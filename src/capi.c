/*
 * File: capi.c
 *
 * Copyright 2002-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Cache API
 * This is the module that manages the cache and starts the CCC chains
 * to get the requests served. Kind of a broker.
 */

#include <string.h>

#include "msg.h"
#include "capi.h"
#include "IO/IO.h"    /* for IORead &friends */
#include "IO/Url.h"
#include "chain.h"
#include "history.h"
#include "nav.h"
#include "dpiapi.h"
#include "uicmd.hh"
#include "domain.h"
#include "../dpip/dpip.h"

/* for testing dpi chat */
#include "bookmark.h"

typedef struct {
   DilloUrl *url;           /* local copy of web->url */
   void *bw;
   char *server;
   char *datastr;
   int SockFD;
   int Flags;
   ChainLink *InfoSend;
   ChainLink *InfoRecv;

   int Ref;
} capi_conn_t;

/* Flags for conn */
enum {
   PENDING = 1,
   TIMEOUT = 2,  /* unused */
   ABORTED = 4
};

/*
 * Local data
 */
/* Data list for active dpi connections */
static Dlist *CapiConns;      /* Data list for active connections; it holds
                               * pointers to capi_conn_t structures. */
/* Last URL asked for view source */
static DilloUrl *CapiVsUrl = NULL;

/*
 * Forward declarations
 */
void a_Capi_ccc(int Op, int Branch, int Dir, ChainLink *Info,
                void *Data1, void *Data2);


/* ------------------------------------------------------------------------- */

/*
 * Initialize capi&cache data
 */
void a_Capi_init(void)
{
   /* create an empty list */
   CapiConns = dList_new(32);
   /* init cache */
   a_Cache_init();
}

/* ------------------------------------------------------------------------- */

/*
 * Create a new connection data structure
 */
static capi_conn_t *
 Capi_conn_new(const DilloUrl *url, void *bw, char *server, char *datastr)
{
   capi_conn_t *conn;

   conn = dNew(capi_conn_t, 1);
   conn->url = url ? a_Url_dup(url) : NULL;
   conn->bw = bw;
   conn->server = dStrdup(server);
   conn->datastr = dStrdup(datastr);
   conn->SockFD = -1;
   conn->Flags = (strcmp(server, "http") != 0) ? PENDING : 0;
   conn->InfoSend = NULL;
   conn->InfoRecv = NULL;
   conn->Ref = 0;           /* Reference count */
   return conn;
}

/*
 * Validate a capi_conn_t pointer.
 * Return value: NULL if not valid, conn otherwise.
 */
static capi_conn_t *Capi_conn_valid(capi_conn_t *conn)
{
   return dList_find(CapiConns, conn);
}

/*
 * Increment the reference count and add to the list if not present
 */
static void Capi_conn_ref(capi_conn_t *conn)
{
   if (++conn->Ref == 1) {
      /* add the connection data to list */
      dList_append(CapiConns, (void *)conn);
   }
   _MSG(" Capi_conn_ref #%d %p\n", conn->Ref, conn);
}

/*
 * Decrement the reference count (and remove from list when zero)
 */
static void Capi_conn_unref(capi_conn_t *conn)
{
   _MSG(" Capi_conn_unref #%d %p\n", conn->Ref - 1, conn);

   /* We may validate conn here, but it doesn't *seem* necessary */
   if (--conn->Ref == 0) {
      /* remove conn preserving the list order */
      dList_remove(CapiConns, (void *)conn);
      /* free dynamic memory */
      a_Url_free(conn->url);
      dFree(conn->server);
      dFree(conn->datastr);
      dFree(conn);
   }
   _MSG(" Capi_conn_unref CapiConns=%d\n", dList_length(CapiConns));
}

/*
 * Compare function for searching a conn by server string
 */
static int Capi_conn_by_server_cmp(const void *v1, const void *v2)
{
   const capi_conn_t *node = v1;
   const char *server = v2;
   dReturn_val_if_fail(node && node->server && server, 1);
   return strcmp(node->server, server);
}

/*
 * Find connection data by server
 */
static capi_conn_t *Capi_conn_find(char *server)
{
   return dList_find_custom(CapiConns, (void*)server, Capi_conn_by_server_cmp);
}

/*
 * Resume connections that were waiting for dpid to start.
 */
static void Capi_conn_resume(void)
{
   int i;
   DataBuf *dbuf;
   capi_conn_t *conn;

   for (i = 0; i < dList_length(CapiConns); ++i) {
      conn = dList_nth_data (CapiConns, i);
      if (conn->Flags & PENDING) {
         dbuf = a_Chain_dbuf_new(conn->datastr,(int)strlen(conn->datastr), 0);
         if (conn->InfoSend) {
            a_Capi_ccc(OpSend, 1, BCK, conn->InfoSend, dbuf, NULL);
         }
         dFree(dbuf);
         conn->Flags &= ~PENDING;
      }
   }
}

/*
 * Abort the connection for a given url, using its CCC.
 * (OpAbort 2,BCK removes the cache entry)
 * TODO: when conn is already done, the cache entry isn't removed.
 *       This may be wrong and needs a revision.
 */
void a_Capi_conn_abort_by_url(const DilloUrl *url)
{
   int i;
   capi_conn_t *conn;

   for (i = 0; i < dList_length(CapiConns); ++i) {
      conn = dList_nth_data (CapiConns, i);
      if (a_Url_cmp(conn->url, url) == 0) {
         if (conn->InfoSend) {
            a_Capi_ccc(OpAbort, 1, BCK, conn->InfoSend, NULL, NULL);
         }
         if (conn->InfoRecv) {
            a_Capi_ccc(OpAbort, 2, BCK, conn->InfoRecv, NULL, NULL);
         }
         break;
      }
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Store the last URL requested by "view source"
 */
void a_Capi_set_vsource_url(const DilloUrl *url)
{
   a_Url_free(CapiVsUrl);
   CapiVsUrl = a_Url_dup(url);
}

/*
 * Safety test: only allow GET|POST dpi-urls from dpi-generated pages.
 */
int a_Capi_dpi_verify_request(BrowserWindow *bw, DilloUrl *url)
{
   const DilloUrl *referer;
   int allow = FALSE;

   if (dStrAsciiCasecmp(URL_SCHEME(url), "dpi") == 0) {
      if (!(URL_FLAGS(url) & (URL_Post + URL_Get))) {
         allow = TRUE;
      } else if (!(URL_FLAGS(url) & URL_Post) &&
                 strncmp(URL_PATH(url), "/vsource/", 9) == 0) {
         allow = TRUE;
      } else {
         /* only allow GET&POST dpi-requests from dpi-generated urls */
         if (a_Nav_stack_size(bw)) {
            referer = a_History_get_url(NAV_TOP_UIDX(bw));
            if (dStrAsciiCasecmp(URL_SCHEME(referer), "dpi") == 0) {
               allow = TRUE;
            }
         }
      }
   } else {
      allow = TRUE;
   }

   if (!allow) {
      MSG("a_Capi_dpi_verify_request: Permission Denied!\n");
      MSG("  URL_STR : %s\n", URL_STR(url));
      if (URL_FLAGS(url) & URL_Post) {
         MSG("  URL_DATA: %s\n", dStr_printable(URL_DATA(url), 1024));
      }
   }
   return allow;
}

/*
 * If the url belongs to a dpi server, return its name.
 */
static int Capi_url_uses_dpi(DilloUrl *url, char **server_ptr)
{
   char *p, *server = NULL, *url_str = URL_STR(url);
   Dstr *tmp;

   if ((dStrnAsciiCasecmp(url_str, "http:", 5) == 0) ||
       (dStrnAsciiCasecmp(url_str, "about:", 6) == 0)) {
      /* URL doesn't use dpi (server = NULL) */
   } else if (dStrnAsciiCasecmp(url_str, "dpi:/", 5) == 0) {
      /* dpi prefix, get this server's name */
      if ((p = strchr(url_str + 5, '/')) != NULL) {
         server = dStrndup(url_str + 5, (uint_t)(p - url_str - 5));
      } else {
         server = dStrdup("?");
      }
      if (strcmp(server, "bm") == 0) {
         dFree(server);
         server = dStrdup("bookmarks");
      }
   } else if ((p = strchr(url_str, ':')) != NULL) {
      tmp = dStr_new("proto.");
      dStr_append_l(tmp, url_str, p - url_str);
      server = tmp->str;
      dStr_free(tmp, 0);
   }

   return ((*server_ptr = server) ? 1 : 0);
}

/*
 * Build the dpip command tag, according to URL and server.
 */
static char *Capi_dpi_build_cmd(DilloWeb *web, char *server)
{
   char *cmd;

   if (strcmp(server, "proto.https") == 0) {
      /* Let's be kind and make the HTTP query string for the dpi */
      char *proxy_connect = a_Http_make_connect_str(web->url);
      Dstr *http_query = a_Http_make_query_str(web->url, web->requester,
                                               web->flags, FALSE);

      if ((uint_t) http_query->len > strlen(http_query->str)) {
         /* Can't handle NULLs embedded in query data */
         MSG_ERR("HTTPS query truncated!\n");
      }

      /* BUG: WORKAROUND: request to only check the root URL's certificate.
       *  This avoids the dialog bombing that stems from loading multiple
       * https images/resources in a single page. A proper fix would take
       * either to implement the https-dpi as a server (with state),
       * or to move back https handling into dillo. */
      if (proxy_connect) {
         const char *proxy_urlstr = a_Http_get_proxy_urlstr();
         cmd = a_Dpip_build_cmd("cmd=%s proxy_url=%s proxy_connect=%s "
                                "url=%s query=%s check_cert=%s",
                                "open_url", proxy_urlstr,
                                proxy_connect, URL_STR(web->url),
                                http_query->str,
                                (web->flags & WEB_RootUrl) ? "true" : "false");
      } else {
         cmd = a_Dpip_build_cmd("cmd=%s url=%s query=%s check_cert=%s",
                                "open_url", URL_STR(web->url),http_query->str,
                                (web->flags & WEB_RootUrl) ? "true" : "false");
      }
      dFree(proxy_connect);
      dStr_free(http_query, 1);

   } else if (strcmp(server, "downloads") == 0) {
      /* let the downloads server get it */
      cmd = a_Dpip_build_cmd("cmd=%s url=%s destination=%s",
                             "download", URL_STR(web->url), web->filename);

   } else {
      /* For everyone else, the url string is enough... */
      cmd = a_Dpip_build_cmd("cmd=%s url=%s", "open_url", URL_STR(web->url));
   }
   return cmd;
}

/*
 * Send the requested URL's source to the "view source" dpi
 */
static void Capi_dpi_send_source(BrowserWindow *bw,  DilloUrl *url)
{
   char *p, *buf, *cmd, size_str[32], *server="vsource";
   int buf_size;

   if (!(p = strchr(URL_STR(url), ':')) || !(p = strchr(p + 1, ':')))
      return;

   if (a_Capi_get_buf(CapiVsUrl, &buf, &buf_size)) {
      /* send the page's source to this dpi connection */
      snprintf(size_str, 32, "%d", buf_size);
      cmd = a_Dpip_build_cmd("cmd=%s url=%s data_size=%s",
                             "start_send_page", URL_STR(url), size_str);
      a_Capi_dpi_send_cmd(NULL, bw, cmd, server, 0);
      a_Capi_dpi_send_data(url, bw, buf, buf_size, server, 0);
   } else {
      cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                             "DpiError", "Page is NOT cached");
      a_Capi_dpi_send_cmd(NULL, bw, cmd, server, 0);
   }
   dFree(cmd);
}

/*
 * Most used function for requesting a URL.
 * TODO: clean up the ad-hoc bindings with an API that allows dynamic
 *       addition of new plugins.
 *
 * Return value: A primary key for identifying the client,
 *               0 if the client is aborted in the process.
 */
int a_Capi_open_url(DilloWeb *web, CA_Callback_t Call, void *CbData)
{
   int reload;
   char *cmd, *server;
   capi_conn_t *conn = NULL;
   const char *scheme = URL_SCHEME(web->url);
   int safe = 0, ret = 0, use_cache = 0;

   /* web->requester is NULL if the action is initiated by user */
   if (!(a_Capi_get_flags(web->url) & CAPI_IsCached ||
         web->requester == NULL ||
         a_Domain_permit(web->requester, web->url))) {
      return 0;
   }

   /* reload test */
   reload = (!(a_Capi_get_flags(web->url) & CAPI_IsCached) ||
             (URL_FLAGS(web->url) & URL_E2EQuery));

   if (web->flags & WEB_Download) {
     /* download request: if cached save from cache, else
      * for http, ftp or https, use the downloads dpi */
     if (a_Capi_get_flags_with_redirection(web->url) & CAPI_IsCached) {
        if (web->filename) {
           if ((web->stream = fopen(web->filename, "w"))) {
              use_cache = 1;
           } else {
              MSG_WARN("Cannot open \"%s\" for writing.\n", web->filename);
           }
        }
     } else if (a_Cache_download_enabled(web->url)) {
        server = "downloads";
        cmd = Capi_dpi_build_cmd(web, server);
        a_Capi_dpi_send_cmd(web->url, web->bw, cmd, server, 1);
        dFree(cmd);
     } else {
        MSG_WARN("Ignoring download request for '%s': "
                 "not in cache and not downloadable.\n",
                 URL_STR(web->url));
     }

   } else if (Capi_url_uses_dpi(web->url, &server)) {
      /* dpi request */
      if ((safe = a_Capi_dpi_verify_request(web->bw, web->url))) {
         if (dStrAsciiCasecmp(scheme, "dpi") == 0) {
            if (strcmp(server, "vsource") == 0) {
               /* allow "view source" reload upon user request */
            } else {
               /* make the other "dpi:/" prefixed urls always reload. */
               a_Url_set_flags(web->url, URL_FLAGS(web->url) | URL_E2EQuery);
               reload = 1;
            }
         }
         if (reload) {
            a_Capi_conn_abort_by_url(web->url);
            /* Send dpip command */
            _MSG("a_Capi_open_url, reload url='%s'\n", URL_STR(web->url));
            cmd = Capi_dpi_build_cmd(web, server);
            a_Capi_dpi_send_cmd(web->url, web->bw, cmd, server, 1);
            dFree(cmd);
            if (strcmp(server, "vsource") == 0) {
               Capi_dpi_send_source(web->bw, web->url);
            }
         }
         use_cache = 1;
      }
      dFree(server);

   } else if (!dStrAsciiCasecmp(scheme, "http")) {
      /* http request */
      if (reload) {
         a_Capi_conn_abort_by_url(web->url);
         /* create a new connection and start the CCC operations */
         conn = Capi_conn_new(web->url, web->bw, "http", "none");
         /* start the reception branch before the query one because the DNS
          * may callback immediately. This may avoid a race condition. */
         a_Capi_ccc(OpStart, 2, BCK, a_Chain_new(), conn, "http");
         a_Capi_ccc(OpStart, 1, BCK, a_Chain_new(), conn, web);
      }
      use_cache = 1;

   } else if (!dStrAsciiCasecmp(scheme, "about")) {
      /* internal request */
      use_cache = 1;
   }

   if (use_cache) {
      if (!conn || (conn && Capi_conn_valid(conn))) {
         /* not aborted, let's continue... */
         ret = a_Cache_open_url(web, Call, CbData);
      }
   } else {
      a_Web_free(web);
   }
   return ret;
}

/*
 * Convert cache-defined flags to Capi ones.
 */
static int Capi_map_cache_flags(uint_t flags)
{
   int status = 0;

   if (flags) {
      status |= CAPI_IsCached;
      if (flags & CA_IsEmpty)
         status |= CAPI_IsEmpty;
      if (flags & CA_GotData)
         status |= CAPI_Completed;
      else
         status |= CAPI_InProgress;

      /* CAPI_Aborted is not yet used/defined */
   }
   return status;
}

/*
 * Return status information of an URL's content-transfer process.
 */
int a_Capi_get_flags(const DilloUrl *Url)
{
   uint_t flags = a_Cache_get_flags(Url);
   int status = flags ? Capi_map_cache_flags(flags) : 0;
   return status;
}

/*
 * Same as a_Capi_get_flags() but following redirections.
 */
int a_Capi_get_flags_with_redirection(const DilloUrl *Url)
{
   uint_t flags = a_Cache_get_flags_with_redirection(Url);
   int status = flags ? Capi_map_cache_flags(flags) : 0;
   return status;
}

/*
 * Get the cache's buffer for the URL, and its size.
 * Return: 1 cached, 0 not cached.
 */
int a_Capi_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   return a_Cache_get_buf(Url, PBuf, BufSize);
}

/*
 * Unref the cache's buffer when no longer using it.
 */
void a_Capi_unref_buf(const DilloUrl *Url)
{
   a_Cache_unref_buf(Url);
}

/*
 * Get the Content-Type associated with the URL
 */
const char *a_Capi_get_content_type(const DilloUrl *url)
{
   return a_Cache_get_content_type(url);
}

/*
 * Set the Content-Type for the URL.
 */
const char *a_Capi_set_content_type(const DilloUrl *url, const char *ctype,
                                    const char *from)
{
   return a_Cache_set_content_type(url, ctype, from);
}

/*
 * Send data to a dpi (e.g. add_bookmark, open_url, send_preferences, ...)
 * Most of the time we send dpi commands, but it also serves for raw data
 * as with "view source".
 */
int a_Capi_dpi_send_data(const DilloUrl *url, void *bw,
                         char *data, int data_sz, char *server, int flags)
{
   capi_conn_t *conn;
   DataBuf *dbuf;

   if (flags & 1) {
      /* open a new connection to server */

      /* Create a new connection data struct and add it to the list */
      conn = Capi_conn_new(url, bw, server, data);
      /* start the CCC operations */
      a_Capi_ccc(OpStart, 2, BCK, a_Chain_new(), conn, server);
      a_Capi_ccc(OpStart, 1, BCK, a_Chain_new(), conn, server);

   } else {
      /* Re-use an open connection */
      conn = Capi_conn_find(server);
      if (conn && conn->InfoSend) {
         /* found & active */
         dbuf = a_Chain_dbuf_new(data, data_sz, 0);
         a_Capi_ccc(OpSend, 1, BCK, conn->InfoSend, dbuf, NULL);
         dFree(dbuf);
      } else {
         MSG(" ERROR: [a_Capi_dpi_send_data] No open connection found\n");
      }
   }

   return 0;
}

/*
 * Send a dpi cmd.
 * (For instance: add_bookmark, open_url, send_preferences, ...)
 */
int a_Capi_dpi_send_cmd(DilloUrl *url, void *bw, char *cmd, char *server,
                        int flags)
{
   return a_Capi_dpi_send_data(url, bw, cmd, strlen(cmd), server, flags);
}

/*
 * Remove a client from the cache client queue.
 * force = also abort the CCC if this is the last client.
 */
void a_Capi_stop_client(int Key, int force)
{
   CacheClient_t *Client;

   _MSG("a_Capi_stop_client:  force=%d\n", force);

   Client = a_Cache_client_get_if_unique(Key);
   if (Client && (force || Client->BufSize == 0)) {
      /* remove empty entries too */
      a_Capi_conn_abort_by_url(Client->Url);
   }
   a_Cache_stop_client(Key);
}

/*
 * CCC function for the CAPI module
 */
void a_Capi_ccc(int Op, int Branch, int Dir, ChainLink *Info,
                void *Data1, void *Data2)
{
   capi_conn_t *conn;

   dReturn_if_fail( a_Chain_check("a_Capi_ccc", Op, Branch, Dir, Info) );

   if (Branch == 1) {
      if (Dir == BCK) {
         /* Command sending branch */
         switch (Op) {
         case OpStart:
            /* Data1 = conn; Data2 = {Web | server} */
            conn = Data1;
            Capi_conn_ref(conn);
            Info->LocalKey = conn;
            conn->InfoSend = Info;
            if (strcmp(conn->server, "http") == 0) {
               a_Chain_link_new(Info, a_Capi_ccc, BCK, a_Http_ccc, 1, 1);
               a_Chain_bcb(OpStart, Info, Data2, NULL);
            } else {
               a_Chain_link_new(Info, a_Capi_ccc, BCK, a_Dpi_ccc, 1, 1);
               a_Chain_bcb(OpStart, Info, Data2, NULL);
            }
            break;
         case OpSend:
            /* Data1 = dbuf */
            a_Chain_bcb(OpSend, Info, Data1, NULL);
            break;
         case OpEnd:
            conn = Info->LocalKey;
            conn->InfoSend = NULL;
            a_Chain_bcb(OpEnd, Info, NULL, NULL);
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         case OpAbort:
            conn = Info->LocalKey;
            conn->InfoSend = NULL;
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* 1 FWD */
         /* Command sending branch (status) */
         switch (Op) {
         case OpSend:
            if (!Data2) {
               MSG_WARN("Capi.c: Opsend [1F] Data2 = NULL\n");
            } else if (strcmp(Data2, "FD") == 0) {
               conn = Info->LocalKey;
               conn->SockFD = *(int*)Data1;
               /* communicate the FD through the answer branch */
               a_Capi_ccc(OpSend, 2, BCK, conn->InfoRecv, &conn->SockFD, "FD");
            } else if (strcmp(Data2, "DpidOK") == 0) {
               /* resume pending dpi requests */
               Capi_conn_resume();
            }
            break;
         case OpAbort:
            conn = Info->LocalKey;
            conn->InfoSend = NULL;
            if (Data2) {
               if (!strcmp(Data2, "DpidERROR")) {
                  a_UIcmd_set_msg(conn->bw,
                                  "ERROR: can't start dpid daemon "
                                  "(URL scheme = '%s')!",
                                  conn->url ? URL_SCHEME(conn->url) : "");
               } else if (!strcmp(Data2, "Both") && conn->InfoRecv) {
                  /* abort the other branch too */
                  a_Capi_ccc(OpAbort, 2, BCK, conn->InfoRecv, NULL, NULL);
               }
            }
            /* if URL == expect-url */
            a_Nav_cancel_expect_if_eq(conn->bw, conn->url);
            /* finish conn */
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }

   } else if (Branch == 2) {
      if (Dir == BCK) {
         /* Answer branch */
         switch (Op) {
         case OpStart:
            /* Data1 = conn; Data2 = {"http" | "<dpi server name>"} */
            conn = Data1;
            Capi_conn_ref(conn);
            Info->LocalKey = conn;
            conn->InfoRecv = Info;
            a_Chain_link_new(Info, a_Capi_ccc, BCK, a_Dpi_ccc, 2, 2);
            a_Chain_bcb(OpStart, Info, NULL, Data2);
            break;
         case OpSend:
            /* Data1 = FD */
            if (Data2 && strcmp(Data2, "FD") == 0) {
               a_Chain_bcb(OpSend, Info, Data1, Data2);
            }
            break;
         case OpAbort:
            conn = Info->LocalKey;
            conn->InfoRecv = NULL;
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            /* remove the cache entry for this URL */
            a_Cache_entry_remove_by_url(conn->url);
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* 2 FWD */
         /* Server listening branch */
         switch (Op) {
         case OpSend:
            conn = Info->LocalKey;
            if (strcmp(Data2, "send_page_2eof") == 0) {
               /* Data1 = dbuf */
               DataBuf *dbuf = Data1;
               a_Cache_process_dbuf(IORead, dbuf->Buf, dbuf->Size, conn->url);
            } else if (strcmp(Data2, "send_status_message") == 0) {
               a_UIcmd_set_msg(conn->bw, "%s", Data1);
            } else if (strcmp(Data2, "chat") == 0) {
               a_UIcmd_set_msg(conn->bw, "%s", Data1);
               a_Bookmarks_chat_add(NULL, NULL, Data1);
            } else if (strcmp(Data2, "dialog") == 0) {
               a_Dpiapi_dialog(conn->bw, conn->server, Data1);
            } else if (strcmp(Data2, "reload_request") == 0) {
               a_Nav_reload(conn->bw);
            } else if (strcmp(Data2, "start_send_page") == 0) {
               /* prepare the cache to receive the data stream for this URL
                *
                * a_Capi_open_url() already added a new cache entry,
                * and a client for it.
                */
            }
            break;
         case OpEnd:
            conn = Info->LocalKey;
            conn->InfoRecv = NULL;

            a_Cache_process_dbuf(IOClose, NULL, 0, conn->url);

            if (conn->InfoSend) {
               /* Propagate OpEnd to the sending branch too */
               a_Capi_ccc(OpEnd, 1, BCK, conn->InfoSend, NULL, NULL);
            }
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}
