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
#include "list.h"
#include "history.h"
#include "nav.h"
#include "dpiapi.h"
#include "uicmd.hh"
#include "../dpip/dpip.h"

/* for testing dpi chat */
#include "bookmark.h"

#define DEBUG_LEVEL 5
#include "debug.h"

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
static capi_conn_t **DpiConn = NULL;
static int DpiConnSize;
static int DpiConnMax = 4;


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
   /* nothing to do for capi yet, just for cache */
   a_Cache_init();
}

/* ------------------------------------------------------------------------- */

/*
 * Create a new connection data structure
 */
static capi_conn_t *
 Capi_conn_new(DilloUrl *url, void *bw, char *server, char *datastr)
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
 * Increment the reference count and add to the list if not present
 */
static void Capi_conn_ref(capi_conn_t *conn)
{
   if (++conn->Ref == 1) {
      /* add the connection data to list */
      a_List_add(DpiConn, DpiConnSize, DpiConnMax);
      DpiConn[DpiConnSize] = conn;
      DpiConnSize++;
   }
   _MSG(" Capi_conn_ref #%d %p\n", conn->Ref, conn);
}

/*
 * Decrement the reference count (and remove from list when zero)
 */
static void Capi_conn_unref(capi_conn_t *conn)
{
   int i, j;

   _MSG(" Capi_conn_unref #%d %p\n", conn->Ref - 1, conn);

   if (--conn->Ref == 0) {
      for (i = 0; i < DpiConnSize; ++i) {
         if (DpiConn[i] == conn) {
            /* remove conn preserving the list order */
            for (j = i; j + 1 < DpiConnSize; ++j)
               DpiConn[j] = DpiConn[j + 1];
            --DpiConnSize;
            /* free dynamic memory */
            a_Url_free(conn->url);
            dFree(conn->server);
            dFree(conn->datastr);
            dFree(conn);
            break;
         }
      }
   }
}

/*
 * Find connection data by server
 */
static capi_conn_t *Capi_conn_find(char *server)
{
   int i;

   for (i = 0; i < DpiConnSize; ++i)
      if (strcmp(server, DpiConn[i]->server) == 0)
         return DpiConn[i];

   return NULL;
}

/*
 * Resume connections that were waiting for dpid to start.
 */
static void Capi_conn_resume(void)
{
   int i;
   DataBuf *dbuf;

   for (i = 0; i < DpiConnSize; ++i) {
      if (DpiConn[i]->Flags & PENDING) {
         capi_conn_t *conn = DpiConn[i];
         dbuf = a_Chain_dbuf_new(conn->datastr,(int)strlen(conn->datastr), 0);
         a_Capi_ccc(OpSend, 1, BCK, conn->InfoSend, dbuf, NULL);
         dFree(dbuf);
         conn->Flags &= ~PENDING;
      }
   }
}

/*
 * Abort the connection for a given url, using its CCC.
 */
void a_Capi_conn_abort_by_url(const DilloUrl *url)
{
   int i;

   for (i = 0; i < DpiConnSize; ++i) {
      if (a_Url_cmp(DpiConn[i]->url, url) == 0) {
         capi_conn_t *conn = DpiConn[i];
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
 * Safety test: only allow dpi-urls from dpi-generated pages.
 */
static int Capi_dpi_verify_request(DilloWeb *web)
{
   DilloUrl *referer;
   int allow = FALSE;

   /* test POST and GET */
   if (dStrcasecmp(URL_SCHEME(web->url), "dpi") == 0 &&
       URL_FLAGS(web->url) & (URL_Post + URL_Get)) {
      /* only allow dpi requests from dpi-generated urls */
      if (a_Nav_stack_size(web->bw)) {
         referer = a_History_get_url(NAV_TOP_UIDX(web->bw));
         if (dStrcasecmp(URL_SCHEME(referer), "dpi") == 0) {
            allow = TRUE;
         }
      }
   } else {
      allow = TRUE;
   }

   if (!allow) {
      MSG("Capi_dpi_verify_request: Permission Denied!\n");
      MSG("  URL_STR : %s\n", URL_STR(web->url));
      if (URL_FLAGS(web->url) & URL_Post) {
         MSG("  URL_DATA: %s\n", dStr_printable(URL_DATA(web->url), 1024));
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

   if ((dStrncasecmp(url_str, "http:", 5) == 0) ||
       (dStrncasecmp(url_str, "about:", 6) == 0)) {
      /* URL doesn't use dpi (server = NULL) */
   } else if (dStrncasecmp(url_str, "dpi:/", 5) == 0) {
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
 * todo: make it PROXY-aware (AFAIS, it should be easy)
 */
static char *Capi_dpi_build_cmd(DilloWeb *web, char *server)
{
   char *cmd;

   if (strcmp(server, "proto.https") == 0) {
      /* Let's be kind and make the HTTP query string for the dpi */
      Dstr *http_query = a_Http_make_query_str(web->url, FALSE);
      /* BUG: embedded NULLs in query data will truncate message */
      cmd = a_Dpip_build_cmd("cmd=%s url=%s query=%s",
                             "open_url", URL_STR(web->url), http_query->str);
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
 * Most used function for requesting a URL.
 * todo: clean up the ad-hoc bindings with an API that allows dynamic
 *       addition of new plugins.
 *
 * Return value: A primary key for identifying the client,
 *               0 if the client is aborted in the process.
 */
int a_Capi_open_url(DilloWeb *web, CA_Callback_t Call, void *CbData)
{
   capi_conn_t *conn;
   int reload;
   char *cmd, *server;
   const char *scheme = URL_SCHEME(web->url);
   int safe = 0, ret = 0, use_cache = 0;

   /* reload test */
   reload = (!(a_Capi_get_flags(web->url) & CAPI_IsCached) ||
             (URL_FLAGS(web->url) & URL_E2EReload));

   if (web->flags & WEB_Download) {
     /* download request: if cached save from cache, else
      * for http, ftp or https, use the downloads dpi */
     if (a_Capi_get_flags(web->url) & CAPI_IsCached) {
        if (web->filename && (web->stream = fopen(web->filename, "w"))) {
           use_cache = 1;
        }
     } else {
        if (!dStrcasecmp(scheme, "https") ||
            !dStrcasecmp(scheme, "http") ||
            !dStrcasecmp(scheme, "ftp")) {
           server = "downloads";
           cmd = Capi_dpi_build_cmd(web, server);
           a_Capi_dpi_send_cmd(web->url, web->bw, cmd, server, 1);
           dFree(cmd);
        }
     }

   } else if (Capi_url_uses_dpi(web->url, &server)) {
      /* dpi request */
      if ((safe = Capi_dpi_verify_request(web))) {
         if (dStrcasecmp(scheme, "dpi") == 0) {
            /* make "dpi:/" prefixed urls always reload. */
            a_Url_set_flags(web->url, URL_FLAGS(web->url) | URL_E2EReload);
            reload = 1;
         }
         if (reload) {
            a_Capi_conn_abort_by_url(web->url);
            /* Send dpip command */
            cmd = Capi_dpi_build_cmd(web, server);
            a_Capi_dpi_send_cmd(web->url, web->bw, cmd, server, 1);
            dFree(cmd);
         }
         use_cache = 1;
      }
      dFree(server);

   } else if (!dStrcasecmp(scheme, "http")) {
      /* http request */
      if (reload) {
         a_Capi_conn_abort_by_url(web->url);
         /* create a new connection and start the CCC operations */
         conn = Capi_conn_new(web->url, web->bw, "http", "none");
         a_Capi_ccc(OpStart, 1, BCK, a_Chain_new(), conn, web);
      }
      use_cache = 1;

   } else if (!dStrcasecmp(scheme, "about")) {
      /* internal request */
      use_cache = 1;
   }

   if (use_cache) {
      ret = a_Cache_open_url(web, Call, CbData);
   } else {
      a_Web_free(web);
   }
   return ret;
}

/*
 * Return status information of an URL's content-transfer process.
 */
int a_Capi_get_flags(const DilloUrl *Url)
{
   int status = 0;
   uint_t flags = a_Cache_get_flags(Url);

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
 * Get the cache's buffer for the URL, and its size.
 * Return: 1 cached, 0 not cached.
 */
int a_Capi_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   return a_Cache_get_buf(Url, PBuf, BufSize);
}

/*
 * Send a dpi cmd.
 * (For instance: add_bookmark, open_url, send_preferences, ...)
 */
int a_Capi_dpi_send_cmd(DilloUrl *url, void *bw, char *cmd, char *server,
                        int flags)
{
   capi_conn_t *conn;
   DataBuf *dbuf;

   if (flags & 1) {
      /* open a new connection to server */

      /* Create a new connection data struct and add it to the list */
      conn = Capi_conn_new(url, bw, server, cmd);
      /* start the CCC operations */
      a_Capi_ccc(OpStart, 1, BCK, a_Chain_new(), conn, server);

   } else {
      /* Re-use an open connection */
      conn = Capi_conn_find(server);
      if (conn) {
         /* found */
         dbuf = a_Chain_dbuf_new(cmd, (int)strlen(cmd), 0);
         a_Capi_ccc(OpSend, 1, BCK, conn->InfoSend, dbuf, NULL);
         dFree(dbuf);
      } else {
         MSG(" ERROR: [a_Capi_dpi_send_cmd] No open connection found\n");
      }
   }

   return 0;
}

/*
 * Remove a client from the cache client queue.
 * force = also abort the CCC if this is the last client.
 */
void a_Capi_stop_client(int Key, int force)
{
   CacheClient_t *Client;

   if (force && (Client = a_Cache_client_get_if_unique(Key))) {
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

   a_Chain_debug_msg("a_Capi_ccc", Op, Branch, Dir);

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
      } else {  /* FWD */
         /* Command sending branch (status) */
         switch (Op) {
         case OpSend:
            if (!Data2) {
               MSG_WARN("Capi.c: Opsend [1F] Data2 = NULL\n");
            } else if (strcmp(Data2, "SockFD") == 0) {
               /* start the receiving branch */
               capi_conn_t *conn = Info->LocalKey;
               conn->SockFD = *(int*)Data1;
               a_Capi_ccc(OpStart, 2, BCK, a_Chain_new(), Info->LocalKey,
                          conn->server);
            } else if (strcmp(Data2, "DpidOK") == 0) {
               /* resume pending dpi requests */
               Capi_conn_resume();
            }
            break;
         case OpAbort:
            conn = Info->LocalKey;
            conn->InfoSend = NULL;
            /* remove the cache entry for this URL */
            a_Cache_entry_remove_by_url(conn->url);
            if (Data2 && !strcmp(Data2, "DpidERROR"))
               a_UIcmd_set_msg(conn->bw,
                               "ERROR: can't start dpid daemon "
                               "(URL scheme = '%s')!", 
                               URL_SCHEME(conn->url));
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
         /* Server listening branch (status)
          * (Data1 = conn; Data2 = {"HttpFD" | "DpiFD"}) */
         switch (Op) {
         case OpStart:
            conn = Data1;
            Capi_conn_ref(conn);
            Info->LocalKey = conn;
            conn->InfoRecv = Info;
            a_Chain_link_new(Info, a_Capi_ccc, BCK, a_Dpi_ccc, 2, 2);
            a_Chain_bcb(OpStart, Info, &conn->SockFD, Data2);
            break;
         case OpAbort:
            conn = Info->LocalKey;
            conn->InfoRecv = NULL;
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Capi_conn_unref(conn);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* FWD */
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
