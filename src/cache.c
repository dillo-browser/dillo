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

#include <ctype.h>              /* for tolower */
#include <sys/types.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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

#include "timeout.hh"
#include "uicmd.hh"

#define NULLKey 0

#define DEBUG_LEVEL 5
#include "debug.h"

/* Maximun initial size for the automatically-growing data buffer */
#define MAX_INIT_BUF  1024*1024
/* Maximum filesize for a URL, before offering a download */
#define HUGE_FILESIZE 5*1024*1024

/*
 *  Local data types
 */

typedef struct {
   const DilloUrl *Url;      /* Cached Url. Url is used as a primary Key */
   char *TypeDet;            /* MIME type string (detected from data) */
   char *TypeHdr;            /* MIME type string as from the HTTP Header */
   Dstr *Header;             /* HTTP header */
   const DilloUrl *Location; /* New URI for redirects */
   Dstr *Data;               /* Pointer to raw data */
   Decode *TransferDecoder;  /* Transfer decoder (e.g., chunked) */
   Decode *ContentDecoder;   /* Data decoder (e.g., gzip) */
   int ExpectedSize;         /* Goal size of the HTTP transfer (0 if unknown)*/
   int TransferSize;         /* Actual length of the HTTP transfer */
   uint_t Flags;             /* Look Flag Defines in cache.h */
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
static void Cache_process_queue(CacheEntry_t *entry);
static void Cache_delayed_process_queue(CacheEntry_t *entry);


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
 * Initialize dicache data
 */
void a_Cache_init(void)
{
   ClientQueue = dList_new(32);
   DelayedQueue = dList_new(32);
   CachedURLs = dList_new(256);

   /* inject the splash screen in the cache */
   {
      DilloUrl *url = a_Url_new("about:splash", NULL, 0, 0, 0);
      Dstr *ds = dStr_new(AboutSplash);
      a_Cache_entry_inject(url, ds);
      dStr_free(ds, 1);
      a_Url_free(url);
   }
}

/* Client operations ------------------------------------------------------ */

/*
 * Make a unique primary-key for cache clients
 */
static int Cache_client_make_key(void)
{
   static int ClientKey = 0; /* Provide a primary key for each client */

   if (++ClientKey < 0)
      ClientKey = 1;
   return ClientKey;
}

/*
 * Add a client to ClientQueue.
 *  - Every client-field is just a reference (except 'Web').
 *  - Return a unique number for identifying the client.
 */
static int Cache_client_enqueue(const DilloUrl *Url, DilloWeb *Web,
                                 CA_Callback_t Callback, void *CbData)
{
   int ClientKey;
   CacheClient_t *NewClient;

   NewClient = dNew(CacheClient_t, 1);
   ClientKey = Cache_client_make_key();
   NewClient->Key = ClientKey;
   NewClient->Url = Url;
   NewClient->Buf = NULL;
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
static void Cache_client_dequeue(CacheClient_t *Client, int Key)
{
   if (!Client) {
      Client = dList_find_custom(ClientQueue, INT2VOIDP(Key),
                                 Cache_client_by_key_cmp);
   }
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
   NewEntry->Header = dStr_new("");
   NewEntry->Location = NULL;
   NewEntry->Data = dStr_sized_new(8*1024);
   NewEntry->TransferDecoder = NULL;
   NewEntry->ContentDecoder = NULL;
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
      if (entry && entry->Flags & CA_Redirect && entry->Location) {
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
void a_Cache_entry_inject(const DilloUrl *Url, Dstr *data_ds)
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
 *  Free the components of a CacheEntry_t struct.
 */
static void Cache_entry_free(CacheEntry_t *entry)
{
   a_Url_free((DilloUrl *)entry->Url);
   dFree(entry->TypeDet);
   dFree(entry->TypeHdr);
   dStr_free(entry->Header, TRUE);
   a_Url_free((DilloUrl *)entry->Location);
   dStr_free(entry->Data, 1);
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
 * - 'Web' is an auxiliar data structure with misc. parameters.
 * - 'Call' is the callback that receives the data
 * - 'CbData' is custom data passed to 'Call'
 *   Note: 'Call' and/or 'CbData' can be NULL, in that case they get set
 *   later by a_Web_dispatch_by_type, based on content/type and 'Web' data.
 *
 * Return value: A primary key for identifying the client,
 *               0 if the client is aborted in the process.
 */
int a_Cache_open_url(void *web, CA_Callback_t Call, void *CbData)
{
   int ClientKey;
   CacheEntry_t *entry;
   DilloWeb *Web = web;
   DilloUrl *Url = Web->url;

   if (URL_FLAGS(Url) & URL_E2EReload) {
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
   CacheEntry_t *entry = Cache_entry_search_with_redirect(url);
   return (entry ? entry->Flags : 0);
}

/*
 * Get the pointer to the URL document, and its size, from the cache entry.
 * Return: 1 cached, 0 not cached.
 */
int a_Cache_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   CacheEntry_t *entry = Cache_entry_search_with_redirect(Url);
   *BufSize = (entry) ? entry->Data->len : 0;
   *PBuf = (entry) ? entry->Data->str : NULL;
   return (entry ? 1 : 0);
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
        if (tolower(fieldname[j]) != tolower(header[i + j]))
           break;
      if (fieldname[j]) {
         /* skip to next line */
         for ( i += j; header[i] != '\n'; i++);
         continue;
      }

      i += j;
      while (header[i] == ' ') i++;
      if (header[i] == ':') {
        /* Field found! */
        while (header[++i] == ' ' || header[i] == '\t');
        for (j = 0; header[i + j] != '\n'; j++);
        field = dStrndup(header + i, j);
        return field;
      }
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
         if (tolower(fieldname[j]) != tolower(header[i + j]))
            break;
      if (fieldname[j]) {
         /* skip to next line */
         for (i += j; header[i] != '\n'; i++);
         continue;
      }

      i += j;
      for ( ; header[i] == ' '; i++);
      if (header[i] == ':') {
         /* Field found! */
         while (header[++i] == ' ' || header[i] == '\t');
         for (j = 0; header[i + j] != '\n'; j++);
         field = dStrndup(header + i, j);
         dList_append(fields, field);
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

   if (entry->Header->len > 12) {
      if (header[9] == '1' && header[10] == '0' && header[11] == '0') {
         /* 100: Continue. The "real" header has not come yet. */
         MSG("An actual 100 Continue header!\n");
         entry->Flags &= ~CA_GotHeader;
         dStr_free(entry->Header, 1);
         entry->Header = dStr_new("");
         return;
      }
      if (header[9] == '3' && header[10] == '0') {
         /* 30x: URL redirection */
         entry->Flags |= CA_Redirect;
         if (header[11] == '1')
            entry->Flags |= CA_ForceRedirect;  /* 301 Moved Permanently */
         else if (header[11] == '2')
            entry->Flags |= CA_TempRedirect;   /* 302 Temporary Redirect */
   
         location_str = Cache_parse_field(header, "Location");
         entry->Location = a_Url_new(location_str,URL_STR_(entry->Url),0,0,0);
         dFree(location_str);

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

   if ((Length = Cache_parse_field(header, "Content-Length")) != NULL) {
      char *tmp;
      if ((tmp = Cache_parse_field(header, "Transfer-Encoding"))) {
         /*
          * BUG: Should test for _presence_ of headers, not whether they
          * have content.
          */
         MSG_HTTP("Both Content-Length and Transfer-Encoding headers"
                  " received.\n");
         dFree(tmp);
      } else {
         entry->Flags |= CA_GotLength;
         entry->ExpectedSize = MAX(strtol(Length, NULL, 10), 0);
      }
      dFree(Length);
   }

#ifndef DISABLE_COOKIES
   /* BUG: If a server feels like mixing Set-Cookie2 and Set-Cookie
    * responses which aren't identical, then we have a problem. I don't
    * know if that is a real issue though. */
   if ((Cookies = Cache_parse_multiple_fields(header, "Set-Cookie2")) ||
       (Cookies = Cache_parse_multiple_fields(header, "Set-Cookie"))) {
      a_Cookies_set(Cookies, entry->Url);
      for (i = 0; (data = dList_nth_data(Cookies, i)); ++i)
         dFree(data);
      dList_free(Cookies);
   }
#endif /* !DISABLE_COOKIES */

   /*
    * Get Transfer-Encoding and initialize decoder
    */
   encoding = Cache_parse_field(header, "Transfer-Encoding");
   entry->TransferDecoder = a_Decode_transfer_init(encoding);
   dFree(encoding);

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
      entry->Data = dStr_sized_new(MIN(entry->ExpectedSize+1, MAX_INIT_BUF));
   }

   /* Get Content-Type */
   if ((Type = Cache_parse_field(header, "Content-Type")) == NULL) {
      if (!((entry->Flags & CA_GotLength) && (entry->ExpectedSize == 0))) {
         /* unless the server sent Content-Length: 0 */
         MSG_HTTP("Server didn't send Content-Type in header.\n");
      }
   } else {
      entry->TypeHdr = Type;
      /* This Content-Type is not trusted. It's checked against real data
       * in Cache_process_queue(); only then CA_GotContentType becomes true.
       */
   }
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
         MSG("Multiple-line header!\n");
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
   int offset = 0;
   int len;
   CacheEntry_t *entry = Cache_entry_search(Url);
   Dstr *dbuf;

   /* Assert a valid entry (not aborted) */
   dReturn_if_fail (entry != NULL);

   if (Op == IOClose) {
      if ((entry->Flags & CA_GotLength) &&
          (entry->ExpectedSize != entry->TransferSize)) {
         MSG_HTTP("Content-Length does NOT match message body,\n"
                  " at: %s\n", URL_STR_(entry->Url));
         MSG("entry->ExpectedSize = %d, entry->TransferSize = %d\n",
             entry->ExpectedSize, entry->TransferSize);
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
      Cache_process_queue(entry);
      return;
   } else if (Op == IOAbort) {
      /* unused */
      MSG("a_Cache_process_dbuf Op = IOAbort; not implemented!\n");
      return;
   }

   /*
    * Cache_get_header() will set CA_GotHeader if it has a full header, and
    * Cache_parse_header() will unset it if the header turns out to have been
    * merely an informational response from the server (i.e., 100 Continue)
    */
   while (!(entry->Flags & CA_GotHeader) &&
          (len = Cache_get_header(entry, buf + offset, buf_size - offset))) {
      offset += len;
      /* Let's scan, allocate, and set things according to header info */
      Cache_parse_header(entry);
   }

   if (!(entry->Flags & CA_GotHeader))
      return;

   entry->TransferSize += buf_size - offset;

   dbuf = dStr_sized_new(buf_size - offset);
   dStr_append_l(dbuf, buf + offset, buf_size - offset);

   /* Assert we have a Decoder.
    * BUG: this is a workaround, more study and a proper design
    * for handling redirects is required */
   if (entry->TransferDecoder != NULL) {
      dbuf = a_Decode_process(entry->TransferDecoder, dbuf);
   }
   if (entry->ContentDecoder != NULL) {
      dbuf = a_Decode_process(entry->ContentDecoder, dbuf);
   }

   dStr_append_l(entry->Data, dbuf->str, dbuf->len);
   if (entry->Data->len)
      entry->Flags &= ~CA_IsEmpty;
   dStr_free(dbuf, 1);

   Cache_process_queue(entry);
}

/*
 * Process redirections (HTTP 30x answers)
 * (This is a work in progress --not finished yet)
 */
static int Cache_redirect(CacheEntry_t *entry, int Flags, BrowserWindow *bw)
{
   DilloUrl *NewUrl;

   _MSG(" Cache_redirect: redirect_level = %d\n", bw->redirect_level);

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

      _MSG(">>>Redirect from: %s\n to %s\n",
           URL_STR_(entry->Url), URL_STR_(entry->Location));
      _MSG("%s", entry->Header->str);

      if (Flags & WEB_RootUrl) {
         /* Redirection of the main page */
         NewUrl = a_Url_new(URL_STR_(entry->Location), URL_STR_(entry->Url),
                            0, 0, 0);
         if (entry->Flags & CA_TempRedirect)
            a_Url_set_flags(NewUrl, URL_FLAGS(NewUrl) | URL_E2EReload);
         a_Nav_push(bw, NewUrl);
         a_Url_free(NewUrl);
      } else {
         /* Sub entity redirection (most probably an image) */
         if (!entry->Data->len) {
            DEBUG_MSG(3,">>>Image redirection without entity-content<<<\n");
         } else {
            DEBUG_MSG(3, ">>>Image redirection with entity-content<<<\n");
         }
      }
   }
   return 0;
}

/*
 * Check whether a URL scheme is downloadable.
 * Return: 1 enabled, 0 disabled.
 */
static int Cache_download_enabled(const DilloUrl *url)
{
   if (!dStrcasecmp(URL_SCHEME(url), "http") ||
       !dStrcasecmp(URL_SCHEME(url), "https") ||
       !dStrcasecmp(URL_SCHEME(url), "ftp"))
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

/*
 * Update cache clients for a single cache-entry
 * Tasks:
 *   - Set the client function (if not already set)
 *   - Look if new data is available and pass it to client functions
 *   - Remove clients when done
 *   - Call redirect handler
 *
 * todo: Implement CA_Abort Op in client callback
 */
static void Cache_process_queue(CacheEntry_t *entry)
{
   uint_t i;
   int st;
   const char *Type;
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
      return;
   if (!(entry->Flags & CA_GotContentType)) {
      st = a_Misc_get_content_type_from_data(
              entry->Data->str, entry->Data->len, &Type);
      if (st == 0 || entry->Flags & CA_GotData) {
         if (a_Misc_content_type_check(entry->TypeHdr, Type) < 0) {
            MSG_HTTP("Content-Type '%s' doesn't match the real data.\n",
                     entry->TypeHdr);
            TypeMismatch = TRUE;
         }
         entry->TypeDet = dStrdup(Type);
         entry->Flags |= CA_GotContentType;
      } else
         return;  /* i.e., wait for more data */
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
            if (TypeMismatch) {
               AbortEntry = TRUE;
            } else {
               st = a_Web_dispatch_by_type(
                       entry->TypeHdr ? entry->TypeHdr : entry->TypeDet,
                       ClientWeb, &Client->Callback, &Client->CbData);
               if (st == -1) {
                  /* MIME type is not viewable */
                  if (ClientWeb->flags & WEB_RootUrl) {
                     /* prepare a download offer... */
                     AbortEntry = OfferDownload = TRUE;
                  } else {
                     /* TODO: Resource Type not handled.
                      * Not aborted to avoid multiple connections on the same
                      * resource. A better idea is to abort the connection and
                      * to keep a failed-resource flag in the cache entry. */
                  }
               }
            }
            if (AbortEntry) {
               a_Bw_remove_client(Client_bw, Client->Key);
               Cache_client_dequeue(Client, NULLKey);
               --i; /* Keep the index value in the next iteration */
               continue;
            }
         }

         /* Send data to our client */
         if ((Client->BufSize = entry->Data->len) > 0) {
            Client->Buf = entry->Data->str;
            (Client->Callback)(CA_Send, Client);
         }

         /* Remove client when done */
         if (entry->Flags & CA_GotData) {
            /* Copy flags to a local var */
            int flags = ClientWeb->flags;
            /* We finished sending data, let the client know */
            (Client->Callback)(CA_Close, Client);
            Cache_client_dequeue(Client, NULLKey);
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
      Cache_entry_remove(entry, NULL);
      if (OfferDownload && Cache_download_enabled(url)) {
         a_UIcmd_save_link(Client_bw, url);
      }
      a_Url_free(url);
   }

   /* Trigger cleanup when there are no cache clients */
   if (dList_length(ClientQueue) == 0) {
      _MSG("  a_Dicache_cleanup()\n");
      a_Dicache_cleanup();
   }

   Busy = FALSE;
   _MSG("QueueSize ====> %d\n", dList_length(ClientQueue));
}

/*
 * Callback function for Cache_delayed_process_queue.
 */
static void Cache_delayed_process_queue_callback(void *data)
{
   void *entry;

   while ((entry = dList_nth_data(DelayedQueue, 0))) {
      Cache_process_queue((CacheEntry_t *)entry);
      /* note that if Cache_process_queue removes the entry,
       * the following dList_remove has no effect. */
      dList_remove(DelayedQueue, entry);
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
 * todo: notify the dicache and upper layers
 */
void a_Cache_stop_client(int Key)
{
   CacheClient_t *Client;

   if ((Client = dList_find_custom(ClientQueue, INT2VOIDP(Key),
                                   Cache_client_by_key_cmp))) {
      Cache_client_dequeue(Client, NULLKey);
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
      Cache_client_dequeue(Client, NULLKey);

   /* Remove every cache entry */
   while ((data = dList_nth_data(CachedURLs, 0))) {
      dList_remove(CachedURLs, data);
      Cache_entry_free(data);
   }
   /* Remove the cache list */
   dList_free(CachedURLs);
}
