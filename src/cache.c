/*
 * File: cache.c
 *
 * Copyright 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo's cache module
 */

#include <sys/types.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "msg.h"
#include "IO/Url.h"
#include "IO/IO.h"
#include "web.hh"
#include "dicache.h"
#include "nav.h"
#include "cookies.h"
#include "misc.h"
#include "capi.h"
#include "decode.h"
#include "auth.h"
#include "domain.h"
#include "timeout.hh"
#include "uicmd.hh"

/* Maximum initial size for the automatically-growing data buffer */
#define MAX_INIT_BUF  1024*1024
/* Maximum filesize for a URL, before offering a download */
#define HUGE_FILESIZE 15*1024*1024

/*
 *  Local data types
 */

typedef struct {
   const DilloUrl *Url;      /* Cached Url. Url is used as a primary Key */
   char *TypeDet;            /* MIME type string (detected from data) */
   char *TypeHdr;            /* MIME type string as from the HTTP Header */
   char *TypeMeta;           /* MIME type string from META HTTP-EQUIV */
   char *TypeNorm;           /* MIME type string normalized */
   Dstr *Header;             /* HTTP header */
   const DilloUrl *Location; /* New URI for redirects */
   Dlist *Auth;              /* Authentication fields */
   Dstr *Data;               /* Pointer to raw data */
   Dstr *UTF8Data;           /* Data after charset translation */
   int DataRefcount;         /* Reference count */
   Decode *TransferDecoder;  /* Transfer decoder (e.g., chunked) */
   Decode *ContentDecoder;   /* Data decoder (e.g., gzip) */
   Decode *CharsetDecoder;   /* Translates text to UTF-8 encoding */
   int ExpectedSize;         /* Goal size of the HTTP transfer (0 if unknown)*/
   int TransferSize;         /* Actual length of the HTTP transfer */
   uint_t Flags;             /* See Flag Defines in cache.h */
} CacheEntry_t;


/*
 *  Local data
 */
/* A sorted list for cached data. Holds pointers to CacheEntry_t structs */
static Dlist *CachedURLs;

/* A list for cache clients.
 * Although implemented as a list, we'll call it ClientQueue  --Jcid */
static Dlist *ClientQueue;

/* A list for delayed clients (it holds weak pointers to cache entries,
 * which are used to make deferred calls to Cache_process_queue) */
static Dlist *DelayedQueue;
static uint_t DelayedQueueIdleId = 0;


/*
 *  Forward declarations
 */
static CacheEntry_t *Cache_process_queue(CacheEntry_t *entry);
static void Cache_delayed_process_queue(CacheEntry_t *entry);
static void Cache_auth_entry(CacheEntry_t *entry, BrowserWindow *bw);
static void Cache_entry_inject(const DilloUrl *Url, Dstr *data_ds);

/*
 * Determine if two cache entries are equal (used by CachedURLs)
 */
static int Cache_entry_cmp(const void *v1, const void *v2)
{
   const CacheEntry_t *d1 = v1, *d2 = v2;

   return a_Url_cmp(d1->Url, d2->Url);
}

/*
 * Determine if two cache entries are equal, using a URL as key.
 */
static int Cache_entry_by_url_cmp(const void *v1, const void *v2)
{
   const DilloUrl *u1 = ((CacheEntry_t*)v1)->Url;
   const DilloUrl *u2 = v2;

   return a_Url_cmp(u1, u2);
}

/*
 * Initialize cache data
 */
void a_Cache_init(void)
{
   ClientQueue = dList_new(32);
   DelayedQueue = dList_new(32);
   CachedURLs = dList_new(256);

   /* inject the splash screen in the cache */
   {
      DilloUrl *url = a_Url_new("about:splash", NULL);
      Dstr *ds = dStr_new(AboutSplash);
      Cache_entry_inject(url, ds);
      dStr_free(ds, 1);
      a_Url_free(url);
   }
}

/* Client operations ------------------------------------------------------ */

/*
 * Add a client to ClientQueue.
 *  - Every client-field is just a reference (except 'Web').
 *  - Return a unique number for identifying the client.
 */
static int Cache_client_enqueue(const DilloUrl *Url, DilloWeb *Web,
                                 CA_Callback_t Callback, void *CbData)
{
   static int ClientKey = 0; /* Provide a primary key for each client */
   CacheClient_t *NewClient;

   if (ClientKey < INT_MAX) /* check for integer overflow */
      ClientKey++;
   else
      ClientKey = 1;

   NewClient = dNew(CacheClient_t, 1);
   NewClient->Key = ClientKey;
   NewClient->Url = Url;
   NewClient->Version = 0;
   NewClient->Buf = NULL;
   NewClient->BufSize = 0;
   NewClient->Callback = Callback;
   NewClient->CbData = CbData;
   NewClient->Web    = Web;

   dList_append(ClientQueue, NewClient);

   return ClientKey;
}

/*
 * Compare function for searching a Client by its key
 */
static int Cache_client_by_key_cmp(const void *client, const void *key)
{
   return ((CacheClient_t *)client)->Key - VOIDP2INT(key);
}

/*
 * Remove a client from the queue
 */
static void Cache_client_dequeue(CacheClient_t *Client)
{
   if (Client) {
      dList_remove(ClientQueue, Client);
      a_Web_free(Client->Web);
      dFree(Client);
   }
}


/* Entry operations ------------------------------------------------------- */

/*
 * Set safe values for a new cache entry
 */
static void Cache_entry_init(CacheEntry_t *NewEntry, const DilloUrl *Url)
{
   NewEntry->Url = a_Url_dup(Url);
   NewEntry->TypeDet = NULL;
   NewEntry->TypeHdr = NULL;
   NewEntry->TypeMeta = NULL;
   NewEntry->TypeNorm = NULL;
   NewEntry->Header = dStr_new("");
   NewEntry->Location = NULL;
   NewEntry->Auth = NULL;
   NewEntry->Data = dStr_sized_new(8*1024);
   NewEntry->UTF8Data = NULL;
   NewEntry->DataRefcount = 0;
   NewEntry->TransferDecoder = NULL;
   NewEntry->ContentDecoder = NULL;
   NewEntry->CharsetDecoder = NULL;
   NewEntry->ExpectedSize = 0;
   NewEntry->TransferSize = 0;
   NewEntry->Flags = CA_IsEmpty;
}

/*
 * Get the data structure for a cached URL (using 'Url' as the search key)
 * If 'Url' isn't cached, return NULL
 */
static CacheEntry_t *Cache_entry_search(const DilloUrl *Url)
{
   return dList_find_sorted(CachedURLs, Url, Cache_entry_by_url_cmp);
}

/*
 * Given a URL, find its cache entry, following redirections.
 */
static CacheEntry_t *Cache_entry_search_with_redirect(const DilloUrl *Url)
{
   int i;
   CacheEntry_t *entry;

   for (i = 0; (entry = Cache_entry_search(Url)); ++i) {

      /* Test for a redirection loop */
      if (entry->Flags & CA_RedirectLoop || i == 3) {
         _MSG_WARN("Redirect loop for URL: >%s<\n", URL_STR_(Url));
         break;
      }
      /* Test for a working redirection */
      if (entry->Flags & CA_Redirect && entry->Location) {
         Url = entry->Location;
      } else
         break;
   }
   return entry;
}

/*
 * Allocate and set a new entry in the cache list
 */
static CacheEntry_t *Cache_entry_add(const DilloUrl *Url)
{
   CacheEntry_t *old_entry, *new_entry;

   if ((old_entry = Cache_entry_search(Url))) {
      MSG_WARN("Cache_entry_add, leaking an entry.\n");
      dList_remove(CachedURLs, old_entry);
   }

   new_entry = dNew(CacheEntry_t, 1);
   Cache_entry_init(new_entry, Url);  /* Set safe values */
   dList_insert_sorted(CachedURLs, new_entry, Cache_entry_cmp);
   return new_entry;
}

/*
 * Inject full page content directly into the cache.
 * Used for "about:splash". May be used for "about:cache" too.
 */
static void Cache_entry_inject(const DilloUrl *Url, Dstr *data_ds)
{
   CacheEntry_t *entry;

   if (!(entry = Cache_entry_search(Url)))
      entry = Cache_entry_add(Url);
   entry->Flags |= CA_GotData + CA_GotHeader + CA_GotLength + CA_InternalUrl;
   if (data_ds->len)
      entry->Flags &= ~CA_IsEmpty;
   dStr_truncate(entry->Data, 0);
   dStr_append_l(entry->Data, data_ds->str, data_ds->len);
   dStr_fit(entry->Data);
   entry->ExpectedSize = entry->TransferSize = entry->Data->len;
}

/*
 *  Free Authentication fields.
 */
static void Cache_auth_free(Dlist *auth)
{
   int i;
   void *auth_field;
   for (i = 0; (auth_field = dList_nth_data(auth, i)); ++i)
      dFree(auth_field);
   dList_free(auth);
}

/*
 *  Free the components of a CacheEntry_t struct.
 */
static void Cache_entry_free(CacheEntry_t *entry)
{
   a_Url_free((DilloUrl *)entry->Url);
   dFree(entry->TypeDet);
   dFree(entry->TypeHdr);
   dFree(entry->TypeMeta);
   dFree(entry->TypeNorm);
   dStr_free(entry->Header, TRUE);
   a_Url_free((DilloUrl *)entry->Location);
   Cache_auth_free(entry->Auth);
   dStr_free(entry->Data, 1);
   dStr_free(entry->UTF8Data, 1);
   if (entry->CharsetDecoder)
      a_Decode_free(entry->CharsetDecoder);
   if (entry->TransferDecoder)
      a_Decode_free(entry->TransferDecoder);
   if (entry->ContentDecoder)
      a_Decode_free(entry->ContentDecoder);
   dFree(entry);
}

/*
 * Remove an entry, from the cache.
 * All the entry clients are removed too! (it may stop rendering of this
 * same resource on other windows, but nothing more).
 */
static void Cache_entry_remove(CacheEntry_t *entry, DilloUrl *url)
{
   int i;
   CacheClient_t *Client;

   if (!entry && !(entry = Cache_entry_search(url)))
      return;
   if (entry->Flags & CA_InternalUrl)
      return;

   /* remove all clients for this entry */
   for (i = 0; (Client = dList_nth_data(ClientQueue, i)); ++i) {
      if (Client->Url == entry->Url) {
         a_Cache_stop_client(Client->Key);
         --i;
      }
   }

   /* remove from DelayedQueue */
   dList_remove(DelayedQueue, entry);

   /* remove from dicache */
   a_Dicache_invalidate_entry(entry->Url);

   /* remove from cache */
   dList_remove(CachedURLs, entry);
   Cache_entry_free(entry);
}

/*
 * Wrapper for capi.
 */
void a_Cache_entry_remove_by_url(DilloUrl *url)
{
   Cache_entry_remove(NULL, url);
}

/* Misc. operations ------------------------------------------------------- */

/*
 * Try finding the url in the cache. If it hits, send the cache contents
 * from there. If it misses, set up a new connection.
 *
 * - 'Web' is an auxiliary data structure with misc. parameters.
 * - 'Call' is the callback that receives the data
 * - 'CbData' is custom data passed to 'Call'
 *   Note: 'Call' and/or 'CbData' can be NULL, in that case they get set
 *   later by a_Web_dispatch_by_type, based on content/type and 'Web' data.
 *
 * Return value: A primary key for identifying the client,
 */
int a_Cache_open_url(void *web, CA_Callback_t Call, void *CbData)
{
   int ClientKey;
   CacheEntry_t *entry;
   DilloWeb *Web = web;
   DilloUrl *Url = Web->url;

   if (URL_FLAGS(Url) & URL_E2EQuery) {
      /* remove current entry */
      Cache_entry_remove(NULL, Url);
   }

   if ((entry = Cache_entry_search(Url))) {
      /* URL is cached: feed our client with cached data */
      ClientKey = Cache_client_enqueue(entry->Url, Web, Call, CbData);
      Cache_delayed_process_queue(entry);

   } else {
      /* URL not cached: create an entry, send our client to the queue,
       * and open a new connection */
      entry = Cache_entry_add(Url);
      ClientKey = Cache_client_enqueue(entry->Url, Web, Call, CbData);
   }

   return ClientKey;
}

/*
 * Get cache entry status
 */
uint_t a_Cache_get_flags(const DilloUrl *url)
{
   CacheEntry_t *entry = Cache_entry_search(url);
   return (entry ? entry->Flags : 0);
}

/*
 * Get cache entry status (following redirections).
 */
uint_t a_Cache_get_flags_with_redirection(const DilloUrl *url)
{
   CacheEntry_t *entry = Cache_entry_search_with_redirect(url);
   return (entry ? entry->Flags : 0);
}

/*
 * Reference the cache data.
 */
static void Cache_ref_data(CacheEntry_t *entry)
{
   if (entry) {
      entry->DataRefcount++;
      _MSG("DataRefcount++: %d\n", entry->DataRefcount);
      if (entry->CharsetDecoder &&
          (!entry->UTF8Data || entry->DataRefcount == 1)) {
         dStr_free(entry->UTF8Data, 1);
         entry->UTF8Data = a_Decode_process(entry->CharsetDecoder,
                                            entry->Data->str,
                                            entry->Data->len);
      }
   }
}

/*
 * Unreference the cache data.
 */
static void Cache_unref_data(CacheEntry_t *entry)
{
   if (entry) {
      entry->DataRefcount--;
      _MSG("DataRefcount--: %d\n", entry->DataRefcount);

      if (entry->CharsetDecoder) {
         if (entry->DataRefcount == 0) {
            dStr_free(entry->UTF8Data, 1);
            entry->UTF8Data = NULL;
         } else if (entry->DataRefcount < 0) {
            MSG_ERR("Cache_unref_data: negative refcount\n");
            entry->DataRefcount = 0;
         }
      }
   }
}

/*
 * Get current content type.
 */
static const char *Cache_current_content_type(CacheEntry_t *entry)
{
   return entry->TypeNorm ? entry->TypeNorm : entry->TypeMeta ? entry->TypeMeta
          : entry->TypeHdr ? entry->TypeHdr : entry->TypeDet;
}

/*
 * Get current Content-Type for cache entry found by URL.
 */
const char *a_Cache_get_content_type(const DilloUrl *url)
{
   CacheEntry_t *entry = Cache_entry_search_with_redirect(url);

   return (entry) ? Cache_current_content_type(entry) : NULL;
}

/*
 * Get pointer to entry's data.
 */
static Dstr *Cache_data(CacheEntry_t *entry)
{
   return entry->UTF8Data ? entry->UTF8Data : entry->Data;
}

/*
 * Change Content-Type for cache entry found by url.
 * from = { "http" | "meta" }
 * Return new content type.
 */
const char *a_Cache_set_content_type(const DilloUrl *url, const char *ctype,
                                     const char *from)
{
   const char *curr;
   char *major, *minor, *charset;
   CacheEntry_t *entry = Cache_entry_search(url);

   dReturn_val_if_fail (entry != NULL, NULL);

   _MSG("a_Cache_set_content_type {%s} {%s}\n", ctype, URL_STR(url));

   curr = Cache_current_content_type(entry);
   if  (entry->TypeMeta || (*from == 'h' && entry->TypeHdr) ) {
      /* Type is already been set. Do nothing.
       * BTW, META overrides TypeHdr */
   } else {
      if (*from == 'h') {
         /* Content-Type from HTTP header */
         entry->TypeHdr = dStrdup(ctype);
      } else {
         /* Content-Type from META */
         entry->TypeMeta = dStrdup(ctype);
      }
      if (a_Misc_content_type_cmp(curr, ctype)) {
         /* ctype gives one different from current */
         a_Misc_parse_content_type(ctype, &major, &minor, &charset);
         if (*from == 'm' && charset &&
             ((!major || !*major) && (!minor || !*minor))) {
            /* META only gives charset; use detected MIME type too */
            entry->TypeNorm = dStrconcat(entry->TypeDet, ctype, NULL);
         } else if (*from == 'm' &&
                    !dStrnAsciiCasecmp(ctype, "text/xhtml", 10)) {
            /* WORKAROUND: doxygen uses "text/xhtml" in META */
            entry->TypeNorm = dStrdup(entry->TypeDet);
         }
         if (charset) {
            if (entry->CharsetDecoder)
               a_Decode_free(entry->CharsetDecoder);
            entry->CharsetDecoder = a_Decode_charset_init(charset);
            curr = Cache_current_content_type(entry);

            /* Invalidate UTF8Data */
            dStr_free(entry->UTF8Data, 1);
            entry->UTF8Data = NULL;
         }
         dFree(major); dFree(minor); dFree(charset);
      }
   }
   return curr;
}

/*
 * Get the pointer to the URL document, and its size, from the cache entry.
 * Return: 1 cached, 0 not cached.
 */
int a_Cache_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   CacheEntry_t *entry = Cache_entry_search_with_redirect(Url);
   if (entry) {
      Dstr *data;
      Cache_ref_data(entry);
      data = Cache_data(entry);
      *PBuf = data->str;
      *BufSize = data->len;
   } else {
      *PBuf = NULL;
      *BufSize = 0;
   }
   return (entry ? 1 : 0);
}

/*
 * Unreference the data buffer when no longer using it.
 */
void a_Cache_unref_buf(const DilloUrl *Url)
{
   Cache_unref_data(Cache_entry_search_with_redirect(Url));
}


/*
 * Extract a single field from the header, allocating and storing the value
 * in 'field'. ('fieldname' must not include the trailing ':')
 * Return a new string with the field-content if found (NULL on error)
 * (This function expects a '\r'-stripped header, with one-line header fields)
 */
static char *Cache_parse_field(const char *header, const char *fieldname)
{
   char *field;
   uint_t i, j;

   for (i = 0; header[i]; i++) {
      /* Search fieldname */
      for (j = 0; fieldname[j]; j++)
        if (D_ASCII_TOLOWER(fieldname[j]) != D_ASCII_TOLOWER(header[i + j]))
           break;
      if (fieldname[j]) {
         /* skip to next line */
         for ( i += j; header[i] != '\n'; i++);
         continue;
      }

      i += j;
      if (header[i] == ':') {
        /* Field found! */
        while (header[++i] == ' ' || header[i] == '\t');
        for (j = 0; header[i + j] != '\n'; j++);
        while (j && (header[i + j - 1] == ' ' || header[i + j - 1] == '\t'))
           j--;
        field = dStrndup(header + i, j);
        return field;
      }
      while (header[i] != '\n') i++;
   }
   return NULL;
}

/*
 * Extract multiple fields from the header.
 */
static Dlist *Cache_parse_multiple_fields(const char *header,
                                          const char *fieldname)
{
   uint_t i, j;
   Dlist *fields = dList_new(8);
   char *field;

   for (i = 0; header[i]; i++) {
      /* Search fieldname */
      for (j = 0; fieldname[j]; j++)
         if (D_ASCII_TOLOWER(fieldname[j]) != D_ASCII_TOLOWER(header[i + j]))
            break;
      if (fieldname[j]) {
         /* skip to next line */
         for (i += j; header[i] != '\n'; i++);
         continue;
      }

      i += j;
      if (header[i] == ':') {
         /* Field found! */
         while (header[++i] == ' ' || header[i] == '\t');
         for (j = 0; header[i + j] != '\n'; j++);
         while (j && (header[i + j - 1] == ' ' || header[i + j - 1] == '\t'))
            j--;
         field = dStrndup(header + i, j);
         dList_append(fields, field);
      } else {
         while (header[i] != '\n') i++;
      }
   }

   if (dList_length(fields) == 0) {
      dList_free(fields);
      fields = NULL;
   }
   return fields;
}

/*
 * Scan, allocate, and set things according to header info.
 * (This function needs the whole header to work)
 */
static void Cache_parse_header(CacheEntry_t *entry)
{
   char *header = entry->Header->str;
   char *Length, *Type, *location_str, *encoding;
#ifndef DISABLE_COOKIES
   Dlist *Cookies;
#endif
   Dlist *warnings;
   void *data;
   int i;

   _MSG("Cache_parse_header\n");

   if (entry->Header->len > 12) {
      if (header[9] == '1' && header[10] == '0' && header[11] == '0') {
         /* 100: Continue. The "real" header has not come yet. */
         MSG("An actual 100 Continue header!\n");
         entry->Flags &= ~CA_GotHeader;
         dStr_free(entry->Header, 1);
         entry->Header = dStr_new("");
         return;
      }
      if (header[9] == '3' && header[10] == '0' &&
          (location_str = Cache_parse_field(header, "Location"))) {
         /* 30x: URL redirection */
         entry->Location = a_Url_new(location_str, URL_STR_(entry->Url));

         if (!a_Domain_permit(entry->Url, entry->Location) ||
             (URL_FLAGS(entry->Location) & (URL_Post + URL_Get) &&
              dStrAsciiCasecmp(URL_SCHEME(entry->Location), "dpi") == 0 &&
              dStrAsciiCasecmp(URL_SCHEME(entry->Url), "dpi") != 0)) {
            /* Domain test, and forbid dpi GET and POST from non dpi-generated
             * urls.
             */
            MSG("Redirection not followed from %s to %s\n",
                URL_HOST(entry->Url), URL_STR(entry->Location));
         } else {
            entry->Flags |= CA_Redirect;
            if (header[11] == '1')
               entry->Flags |= CA_ForceRedirect;  /* 301 Moved Permanently */
            else if (header[11] == '2')
               entry->Flags |= CA_TempRedirect;   /* 302 Temporary Redirect */
         }
         dFree(location_str);
      } else if (strncmp(header + 9, "401", 3) == 0) {
         entry->Auth =
            Cache_parse_multiple_fields(header, "WWW-Authenticate");
      } else if (strncmp(header + 9, "404", 3) == 0) {
         entry->Flags |= CA_NotFound;
      }
   }

   if ((warnings = Cache_parse_multiple_fields(header, "Warning"))) {
      for (i = 0; (data = dList_nth_data(warnings, i)); ++i) {
         MSG_HTTP("%s\n", (char *)data);
         dFree(data);
      }
      dList_free(warnings);
   }

   /*
    * Get Transfer-Encoding and initialize decoder
    */
   encoding = Cache_parse_field(header, "Transfer-Encoding");
   entry->TransferDecoder = a_Decode_transfer_init(encoding);


   if ((Length = Cache_parse_field(header, "Content-Length")) != NULL) {
      if (encoding) {
         /*
          * If Transfer-Encoding is present, Content-Length must be ignored.
          * If the Transfer-Encoding is non-identity, it is an error.
          */
         if (dStrAsciiCasecmp(encoding, "identity"))
            MSG_HTTP("Content-Length and non-identity Transfer-Encoding "
                     "headers both present.\n");
      } else {
         entry->Flags |= CA_GotLength;
         entry->ExpectedSize = MAX(strtol(Length, NULL, 10), 0);
      }
      dFree(Length);
   }

   dFree(encoding); /* free Transfer-Encoding */

#ifndef DISABLE_COOKIES
   if ((Cookies = Cache_parse_multiple_fields(header, "Set-Cookie"))) {
      CacheClient_t *client;

      for (i = 0; (client = dList_nth_data(ClientQueue, i)); ++i) {
         if (client->Url == entry->Url) {
            DilloWeb *web = client->Web;

            if (!web->requester ||
                a_Url_same_organization(entry->Url, web->requester)) {
               /* If cookies are third party, don't even consider them. */
               char *server_date = Cache_parse_field(header, "Date");

               a_Cookies_set(Cookies, entry->Url, server_date);
               dFree(server_date);
               break;
            }
         }
      }
      for (i = 0; (data = dList_nth_data(Cookies, i)); ++i)
         dFree(data);
      dList_free(Cookies);
   }
#endif /* !DISABLE_COOKIES */

   /*
    * Get Content-Encoding and initialize decoder
    */
   encoding = Cache_parse_field(header, "Content-Encoding");
   entry->ContentDecoder = a_Decode_content_init(encoding);
   dFree(encoding);

   if (entry->ExpectedSize > 0) {
      if (entry->ExpectedSize > HUGE_FILESIZE) {
         entry->Flags |= CA_HugeFile;
      }
      /* Avoid some reallocs. With MAX_INIT_BUF we avoid a SEGFAULT
       * with huge files (e.g. iso files).
       * Note: the buffer grows automatically. */
      dStr_free(entry->Data, 1);
      entry->Data = dStr_sized_new(MIN(entry->ExpectedSize, MAX_INIT_BUF));
   }

   /* Get Content-Type */
   if ((Type = Cache_parse_field(header, "Content-Type"))) {
      /* This HTTP Content-Type is not trusted. It's checked against real data
       * in Cache_process_queue(); only then CA_GotContentType becomes true. */
      a_Cache_set_content_type(entry->Url, Type, "http");
      _MSG("TypeHdr  {%s} {%s}\n", Type, URL_STR(entry->Url));
      _MSG("TypeMeta {%s}\n", entry->TypeMeta);
      dFree(Type);
   }
   Cache_ref_data(entry);
}

/*
 * Consume bytes until the whole header is got (up to a "\r\n\r\n" sequence)
 * (Also unfold multi-line fields and strip '\r' chars from header)
 */
static int Cache_get_header(CacheEntry_t *entry,
                            const char *buf, size_t buf_size)
{
   size_t N, i;
   Dstr *hdr = entry->Header;

   /* Header finishes when N = 2 */
   N = (hdr->len && hdr->str[hdr->len - 1] == '\n');
   for (i = 0; i < buf_size && N < 2; ++i) {
      if (buf[i] == '\r' || !buf[i])
         continue;
      if (N == 1 && (buf[i] == ' ' || buf[i] == '\t')) {
         /* unfold multiple-line header */
         _MSG("Multiple-line header!\n");
         dStr_erase(hdr, hdr->len - 1, 1);
      }
      N = (buf[i] == '\n') ? N + 1 : 0;
      dStr_append_c(hdr, buf[i]);
   }

   if (N == 2) {
      /* Got whole header */
      _MSG("Header [buf_size=%d]\n%s", i, hdr->str);
      entry->Flags |= CA_GotHeader;
      dStr_fit(hdr);
      /* Return number of header bytes in 'buf' [1 based] */
      return i;
   }
   return 0;
}

/*
 * Receive new data, update the reception buffer (for next read), update the
 * cache, and service the client queue.
 *
 * This function gets called whenever the IO has new data.
 *  'Op' is the operation to perform
 *  'VPtr' is a (void) pointer to the IO control structure
 */
void a_Cache_process_dbuf(int Op, const char *buf, size_t buf_size,
                          const DilloUrl *Url)
{
   int offset, len;
   const char *str;
   Dstr *dstr1, *dstr2, *dstr3;
   CacheEntry_t *entry = Cache_entry_search(Url);

   /* Assert a valid entry (not aborted) */
   dReturn_if_fail (entry != NULL);

   _MSG("__a_Cache_process_dbuf__\n");

   if (Op == IORead) {
      /*
       * Cache_get_header() will set CA_GotHeader if it has a full header, and
       * Cache_parse_header() will unset it if the header ends being
       * merely an informational response from the server (i.e., 100 Continue)
       */
      for (offset = 0; !(entry->Flags & CA_GotHeader) &&
           (len = Cache_get_header(entry, buf + offset, buf_size - offset));
           Cache_parse_header(entry) ) {
         offset += len;
      }

      if (entry->Flags & CA_GotHeader) {
         str = buf + offset;
         len = buf_size - offset;
         entry->TransferSize += len;
         dstr1 = dstr2 = dstr3 = NULL;

         /* Decode arrived data (<= 3 stages) */
         if (entry->TransferDecoder) {
            dstr1 = a_Decode_process(entry->TransferDecoder, str, len);
            str = dstr1->str;
            len = dstr1->len;
         }
         if (entry->ContentDecoder) {
            dstr2 = a_Decode_process(entry->ContentDecoder, str, len);
            str = dstr2->str;
            len = dstr2->len;
         }
         dStr_append_l(entry->Data, str, len);
         if (entry->CharsetDecoder && entry->UTF8Data) {
            dstr3 = a_Decode_process(entry->CharsetDecoder, str, len);
            dStr_append_l(entry->UTF8Data, dstr3->str, dstr3->len);
         }
         dStr_free(dstr1, 1);
         dStr_free(dstr2, 1);
         dStr_free(dstr3, 1);

         if (entry->Data->len)
            entry->Flags &= ~CA_IsEmpty;

         entry = Cache_process_queue(entry);
      }
   } else if (Op == IOClose) {
      if ((entry->ExpectedSize || entry->TransferSize) &&
          entry->TypeHdr == NULL) {
         MSG_HTTP("Message with a body lacked Content-Type header.\n");
      }
      if ((entry->Flags & CA_GotLength) &&
          (entry->ExpectedSize != entry->TransferSize)) {
         MSG_HTTP("Content-Length does NOT match message body at\n"
                  "%s\n", URL_STR_(entry->Url));
         MSG("Expected size: %d, Transfer size: %d\n",
             entry->ExpectedSize, entry->TransferSize);
      }
      if (!entry->TransferSize && !(entry->Flags & CA_Redirect) &&
          (entry->Flags & WEB_RootUrl)) {
         char *eol = strchr(entry->Header->str, '\n');
         if (eol) {
            char *status_line = dStrndup(entry->Header->str,
                                         eol - entry->Header->str);
            MSG_HTTP("Body was empty. Server sent status: %s\n", status_line);
            dFree(status_line);
         }
      }
      entry->Flags |= CA_GotData;
      entry->Flags &= ~CA_Stopped;          /* it may catch up! */
      if (entry->TransferDecoder) {
         a_Decode_free(entry->TransferDecoder);
         entry->TransferDecoder = NULL;
      }
      if (entry->ContentDecoder) {
         a_Decode_free(entry->ContentDecoder);
         entry->ContentDecoder = NULL;
      }
      dStr_fit(entry->Data);                /* fit buffer size! */

      if ((entry = Cache_process_queue(entry))) {
         if (entry->Flags & CA_GotHeader) {
            Cache_unref_data(entry);
         }
      }
   } else if (Op == IOAbort) {
      /* unused */
      MSG("a_Cache_process_dbuf Op = IOAbort; not implemented!\n");
   }
}

/*
 * Process redirections (HTTP 30x answers)
 * (This is a work in progress --not finished yet)
 */
static int Cache_redirect(CacheEntry_t *entry, int Flags, BrowserWindow *bw)
{
   DilloUrl *NewUrl;

   _MSG(" Cache_redirect: redirect_level = %d\n", bw->redirect_level);

   /* Don't allow redirection for SpamSafe/local URLs */
   if (URL_FLAGS(entry->Url) & URL_SpamSafe) {
      a_UIcmd_set_msg(bw, "WARNING: local URL with redirection.  Aborting.");
      return 0;
   }

   /* if there's a redirect loop, stop now */
   if (bw->redirect_level >= 5)
      entry->Flags |= CA_RedirectLoop;

   if (entry->Flags & CA_RedirectLoop) {
      a_UIcmd_set_msg(bw, "ERROR: redirect loop for: %s", URL_STR_(entry->Url));
      bw->redirect_level = 0;
      return 0;
   }

   if ((entry->Flags & CA_Redirect && entry->Location) &&
       (entry->Flags & CA_ForceRedirect || entry->Flags & CA_TempRedirect ||
        !entry->Data->len || entry->Data->len < 1024)) {

      _MSG(">>>> Redirect from: %s\n to %s <<<<\n",
           URL_STR_(entry->Url), URL_STR_(entry->Location));
      _MSG("%s", entry->Header->str);

      if (Flags & WEB_RootUrl) {
         /* Redirection of the main page */
         NewUrl = a_Url_new(URL_STR_(entry->Location), URL_STR_(entry->Url));
         if (entry->Flags & CA_TempRedirect)
            a_Url_set_flags(NewUrl, URL_FLAGS(NewUrl) | URL_E2EQuery);
         a_Nav_push(bw, NewUrl, entry->Url);
         a_Url_free(NewUrl);
      } else {
         /* Sub entity redirection (most probably an image) */
         if (!entry->Data->len) {
            _MSG(">>>> Image redirection without entity-content <<<<\n");
         } else {
            _MSG(">>>> Image redirection with entity-content <<<<\n");
         }
      }
   }
   return 0;
}

typedef struct {
   Dlist *auth;
   DilloUrl *url;
   BrowserWindow *bw;
} CacheAuthData_t;

/*
 * Ask for user/password and reload the page.
 */
static void Cache_auth_callback(void *vdata)
{
   CacheAuthData_t *data = (CacheAuthData_t *)vdata;
   if (a_Auth_do_auth(data->auth, data->url))
      a_Nav_reload(data->bw);
   Cache_auth_free(data->auth);
   a_Url_free(data->url);
   dFree(data);
   Cache_auth_entry(NULL, NULL);
   a_Timeout_remove();
}

/*
 * Set a timeout function to ask for user/password.
 */
static void Cache_auth_entry(CacheEntry_t *entry, BrowserWindow *bw)
{
   static int busy = 0;
   CacheAuthData_t *data;

   if (!entry) {
      busy = 0;
   } else if (busy) {
      MSG_WARN("Cache_auth_entry: caught busy!\n");
   } else if (entry->Auth) {
      busy = 1;
      data = dNew(CacheAuthData_t, 1);
      data->auth = entry->Auth;
      data->url = a_Url_dup(entry->Url);
      data->bw = bw;
      entry->Auth = NULL;
      a_Timeout_add(0.0, Cache_auth_callback, data);
   }
}

/*
 * Check whether a URL scheme is downloadable.
 * Return: 1 enabled, 0 disabled.
 */
int a_Cache_download_enabled(const DilloUrl *url)
{
   if (!dStrAsciiCasecmp(URL_SCHEME(url), "http") ||
       !dStrAsciiCasecmp(URL_SCHEME(url), "https") ||
       !dStrAsciiCasecmp(URL_SCHEME(url), "ftp"))
      return 1;
   return 0;
}

/*
 * Don't process data any further, but let the cache fill the entry.
 * (Currently used to handle WEB_RootUrl redirects,
 *  and to ignore image redirects --Jcid)
 */
static void Cache_null_client(int Op, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;

   /* make the stop button insensitive when done */
   if (Op == CA_Close) {
      if (Web->flags & WEB_RootUrl) {
         /* Remove this client from our active list */
         a_Bw_close_client(Web->bw, Client->Key);
      }
   }

   /* else ignore */

   return;
}

typedef struct {
   BrowserWindow *bw;
   DilloUrl *url;
} Cache_savelink_t;

/*
 * Save link from behind a timeout so that Cache_process_queue() can
 * get on with its work.
 */
static void Cache_savelink_cb(void *vdata)
{
   Cache_savelink_t *data = (Cache_savelink_t*) vdata;

   a_UIcmd_save_link(data->bw, data->url);
   a_Url_free(data->url);
   dFree(data);
}

/*
 * Let the client know that we're not following a redirection.
 */
static void Cache_provide_redirection_blocked_page(CacheEntry_t *entry,
                                                   CacheClient_t *client)
{
   DilloWeb *clientWeb = client->Web;

   a_Web_dispatch_by_type("text/html", clientWeb, &client->Callback,
                          &client->CbData);
   client->Buf = dStrconcat("<!doctype html><html><body>"
                    "Dillo blocked a redirection attempt from <a href=\"",
                    URL_STR(entry->Url), "\">", URL_STR(entry->Url),
                    "</a> to <a href=\"", URL_STR(entry->Location), "\">",
                    URL_STR(entry->Location), "</a> based on your domainrc "
                    "settings.</body></html>", NULL);
   client->BufSize = strlen(client->Buf);
   (client->Callback)(CA_Send, client);
   dFree(client->Buf);
}

/*
 * Update cache clients for a single cache-entry
 * Tasks:
 *   - Set the client function (if not already set)
 *   - Look if new data is available and pass it to client functions
 *   - Remove clients when done
 *   - Call redirect handler
 *
 * Return: Cache entry, which may be NULL if it has been removed.
 *
 * TODO: Implement CA_Abort Op in client callback
 */
static CacheEntry_t *Cache_process_queue(CacheEntry_t *entry)
{
   uint_t i;
   int st;
   const char *Type;
   Dstr *data;
   CacheClient_t *Client;
   DilloWeb *ClientWeb;
   BrowserWindow *Client_bw = NULL;
   static bool_t Busy = FALSE;
   bool_t AbortEntry = FALSE;
   bool_t OfferDownload = FALSE;
   bool_t TypeMismatch = FALSE;

   if (Busy)
      MSG_ERR("FATAL!: >>>> Cache_process_queue Caught busy!!! <<<<\n");
   if (!(entry->Flags & CA_GotHeader))
      return entry;
   if (!(entry->Flags & CA_GotContentType)) {
      st = a_Misc_get_content_type_from_data(
              entry->Data->str, entry->Data->len, &Type);
      _MSG("Cache: detected Content-Type '%s'\n", Type);
      if (st == 0 || entry->Flags & CA_GotData) {
         if (a_Misc_content_type_check(entry->TypeHdr, Type) < 0) {
            MSG_HTTP("Content-Type '%s' doesn't match the real data.\n",
                     entry->TypeHdr);
            TypeMismatch = TRUE;
         }
         entry->TypeDet = dStrdup(Type);
         entry->Flags |= CA_GotContentType;
      } else
         return entry;  /* i.e., wait for more data */
   }

   Busy = TRUE;
   for (i = 0; (Client = dList_nth_data(ClientQueue, i)); ++i) {
      if (Client->Url == entry->Url) {
         ClientWeb = Client->Web;    /* It was a (void*) */
         Client_bw = ClientWeb->bw;  /* 'bw' in a local var */

         if (ClientWeb->flags & WEB_RootUrl) {
            if (!(entry->Flags & CA_MsgErased)) {
               /* clear the "expecting for reply..." message */
               a_UIcmd_set_msg(Client_bw, "");
               entry->Flags |= CA_MsgErased;
            }
            if (TypeMismatch) {
               a_UIcmd_set_msg(Client_bw,"HTTP warning: Content-Type '%s' "
                               "doesn't match the real data.", entry->TypeHdr);
               OfferDownload = TRUE;
            }
            if (entry->Flags & CA_Redirect) {
               if (!Client->Callback) {
                  Client->Callback = Cache_null_client;
                  Client_bw->redirect_level++;
               }
            } else {
               Client_bw->redirect_level = 0;
            }
            if (entry->Flags & CA_HugeFile) {
               a_UIcmd_set_msg(Client_bw,"Huge file! (%dMB)",
                               entry->ExpectedSize / (1024*1024));
               AbortEntry = OfferDownload = TRUE;
            }
         } else {
            /* For non root URLs, ignore redirections and 404 answers */
            if (entry->Flags & CA_Redirect || entry->Flags & CA_NotFound)
               Client->Callback = Cache_null_client;
         }

         /* Set the client function */
         if (!Client->Callback) {
            Client->Callback = Cache_null_client;

            if (entry->Location && !(entry->Flags & CA_Redirect)) {
               /* Not following redirection, so don't display page body. */
            } else {
               if (TypeMismatch) {
                  AbortEntry = TRUE;
               } else {
                  const char *curr_type = Cache_current_content_type(entry);
                  st = a_Web_dispatch_by_type(curr_type, ClientWeb,
                                              &Client->Callback,
                                              &Client->CbData);
                  if (st == -1) {
                     /* MIME type is not viewable */
                     if (ClientWeb->flags & WEB_RootUrl) {
                        MSG("Content-Type '%s' not viewable.\n", curr_type);
                        /* prepare a download offer... */
                        AbortEntry = OfferDownload = TRUE;
                     } else {
                        /* TODO: Resource Type not handled.
                         * Not aborted to avoid multiple connections on the
                         * same resource. A better idea is to abort the
                         * connection and to keep a failed-resource flag in
                         * the cache entry. */
                     }
                  }
               }
               if (AbortEntry) {
                  if (ClientWeb->flags & WEB_RootUrl)
                     a_Nav_cancel_expect_if_eq(Client_bw, Client->Url);
                  a_Bw_remove_client(Client_bw, Client->Key);
                  Cache_client_dequeue(Client);
                  --i; /* Keep the index value in the next iteration */
                  continue;
               }
            }
         }

         /* Send data to our client */
         if (ClientWeb->flags & WEB_Download) {
            /* for download, always provide original data, not translated */
            data = entry->Data;
         } else {
            data = Cache_data(entry);
         }
         if ((Client->BufSize = data->len) > 0) {
            Client->Buf = data->str;
            (Client->Callback)(CA_Send, Client);
            if (ClientWeb->flags & WEB_RootUrl) {
               /* show size of page received */
               a_UIcmd_set_page_prog(Client_bw, entry->Data->len, 1);
            }
         }

         /* Remove client when done */
         if (entry->Flags & CA_GotData) {
            /* Copy flags to a local var */
            int flags = ClientWeb->flags;

            if (ClientWeb->flags & WEB_RootUrl && entry->Location &&
                !(entry->Flags & CA_Redirect)) {
               Cache_provide_redirection_blocked_page(entry, Client);
            }
            /* We finished sending data, let the client know */
            (Client->Callback)(CA_Close, Client);
            if (ClientWeb->flags & WEB_RootUrl)
               a_UIcmd_set_page_prog(Client_bw, 0, 0);
            Cache_client_dequeue(Client);
            --i; /* Keep the index value in the next iteration */

            /* within CA_GotData, we assert just one redirect call */
            if (entry->Flags & CA_Redirect)
               Cache_redirect(entry, flags, Client_bw);
         }
      }
   } /* for */

   if (AbortEntry) {
      /* Abort the entry, remove it from cache, and maybe offer download. */
      DilloUrl *url = a_Url_dup(entry->Url);
      a_Capi_conn_abort_by_url(url);
      entry = NULL;
      if (OfferDownload) {
         /* Remove entry when 'conn' is already done */
         Cache_entry_remove(NULL, url);
         if (a_Cache_download_enabled(url)) {
            Cache_savelink_t *data = dNew(Cache_savelink_t, 1);
            data->bw = Client_bw;
            data->url = a_Url_dup(url);
            a_Timeout_add(0.0, Cache_savelink_cb, data);
         }
      }
      a_Url_free(url);
   } else if (entry->Auth && (entry->Flags & CA_GotData)) {
      Cache_auth_entry(entry, Client_bw);
   }

   /* Trigger cleanup when there are no cache clients */
   if (dList_length(ClientQueue) == 0) {
      a_Dicache_cleanup();
   }

   Busy = FALSE;
   _MSG("QueueSize ====> %d\n", dList_length(ClientQueue));
   return entry;
}

/*
 * Callback function for Cache_delayed_process_queue.
 */
static void Cache_delayed_process_queue_callback()
{
   CacheEntry_t *entry;

   while ((entry = (CacheEntry_t *)dList_nth_data(DelayedQueue, 0))) {
      Cache_ref_data(entry);
      if ((entry = Cache_process_queue(entry))) {
         Cache_unref_data(entry);
         dList_remove(DelayedQueue, entry);
      }
   }
   DelayedQueueIdleId = 0;
   a_Timeout_remove();
}

/*
 * Set a call to Cache_process_queue from the main cycle.
 */
static void Cache_delayed_process_queue(CacheEntry_t *entry)
{
   /* there's no need to repeat entries in the queue */
   if (!dList_find(DelayedQueue, entry))
      dList_append(DelayedQueue, entry);

   if (DelayedQueueIdleId == 0) {
      _MSG("  Setting timeout callback\n");
      a_Timeout_add(0.0, Cache_delayed_process_queue_callback, NULL);
      DelayedQueueIdleId = 1;
   }
}

/*
 * Last Client for this entry?
 * Return: Client if true, NULL otherwise
 * (cache.c has only one call to a capi function. This avoids a second one)
 */
CacheClient_t *a_Cache_client_get_if_unique(int Key)
{
   int i, n = 0;
   CacheClient_t *Client, *iClient;

   if ((Client = dList_find_custom(ClientQueue, INT2VOIDP(Key),
                                   Cache_client_by_key_cmp))) {
      for (i = 0; (iClient = dList_nth_data(ClientQueue, i)); ++i) {
         if (iClient->Url == Client->Url) {
            ++n;
         }
      }
   }
   return (n == 1) ? Client : NULL;
}

/*
 * Remove a client from the client queue
 * TODO: notify the dicache and upper layers
 */
void a_Cache_stop_client(int Key)
{
   CacheClient_t *Client;
   CacheEntry_t *entry;
   DICacheEntry *DicEntry;

   /* The client can be in both queues at the same time */
   if ((Client = dList_find_custom(ClientQueue, INT2VOIDP(Key),
                                   Cache_client_by_key_cmp))) {
      /* Dicache */
      if ((DicEntry = a_Dicache_get_entry(Client->Url, Client->Version)))
         a_Dicache_unref(Client->Url, Client->Version);

      /* DelayedQueue */
      if ((entry = Cache_entry_search(Client->Url)))
         dList_remove(DelayedQueue, entry);

      /* Main queue */
      Cache_client_dequeue(Client);

   } else {
      _MSG("WARNING: Cache_stop_client, nonexistent client\n");
   }
}


/*
 * Memory deallocator (only called at exit time)
 */
void a_Cache_freeall(void)
{
   CacheClient_t *Client;
   void *data;

   /* free the client queue */
   while ((Client = dList_nth_data(ClientQueue, 0)))
      Cache_client_dequeue(Client);

   /* Remove every cache entry */
   while ((data = dList_nth_data(CachedURLs, 0))) {
      dList_remove_fast(CachedURLs, data);
      Cache_entry_free(data);
   }
   /* Remove the cache list */
   dList_free(CachedURLs);
}
