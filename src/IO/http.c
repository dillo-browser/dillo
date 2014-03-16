/*
 * File: http.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * HTTP connect functions
 */


#include <config.h>

#include <ctype.h>              /* isdigit */
#include <unistd.h>
#include <errno.h>              /* for errno */
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>         /* for lots of socket stuff */
#include <netinet/in.h>         /* for ntohl and stuff */
#include <arpa/inet.h>          /* for inet_ntop */

#include "IO.h"
#include "Url.h"
#include "../msg.h"
#include "../klist.h"
#include "../dns.h"
#include "../web.hh"
#include "../cookies.h"
#include "../auth.h"
#include "../prefs.h"
#include "../misc.h"

#include "../uicmd.hh"

/* Used to send a message to the bw's status bar */
#define MSG_BW(web, root, ...)                                        \
D_STMT_START {                                                        \
   if (a_Web_valid((web)) && (!(root) || (web)->flags & WEB_RootUrl)) \
      a_UIcmd_set_msg((web)->bw, __VA_ARGS__);                        \
} D_STMT_END

#define _MSG_BW(web, root, ...)

static const int HTTP_SOCKET_USE_PROXY   = 0x1;
static const int HTTP_SOCKET_QUEUED      = 0x4;
static const int HTTP_SOCKET_TO_BE_FREED = 0x8;

/* 'Url' and 'web' are just references (no need to deallocate them here). */
typedef struct {
   int SockFD;
   uint_t port;            /* need a separate port in order to support PROXY */
   uint_t flags;
   DilloWeb *web;          /* reference to client's web structure */
   Dlist *addr_list;       /* Holds the DNS answer */
   int Err;                /* Holds the errno of the connect() call */
   ChainLink *Info;        /* Used for CCC asynchronous operations */
   char *connected_to;     /* Used for per-host connection limit */
} SocketData_t;

/* Data structures and functions to queue sockets that need to be
 * delayed due to the per host connection limit.
 */
typedef struct SocketQueueEntry {
   SocketData_t* sock;
   struct SocketQueueEntry *next ;
} SocketQueueEntry_t;

typedef struct {
   SocketQueueEntry_t *head;
   SocketQueueEntry_t *tail;
} SocketQueue_t;

typedef struct {
  char *host;
  int active_connections;
  SocketQueue_t queue;
} HostConnection_t;

static void Http_socket_queue_init(SocketQueue_t *sq);
static void Http_socket_enqueue(SocketQueue_t *sq, SocketData_t* sock);
static SocketData_t* Http_socket_dequeue(SocketQueue_t *sq);
static HostConnection_t *Http_host_connection_get(const char *host);
static void Http_host_connection_remove(HostConnection_t *hc);
static int Http_connect_socket(ChainLink *Info);
static void Http_socket_free(int SKey);

/*
 * Local data
 */
static Klist_t *ValidSocks = NULL; /* Active sockets list. It holds pointers to
                                    * SocketData_t structures. */
static DilloUrl *HTTP_Proxy = NULL;
static char *HTTP_Proxy_Auth_base64 = NULL;
static char *HTTP_Language_hdr = NULL;
static Dlist *host_connections;

/*
 * Initialize proxy vars and Accept-Language header
 */
int a_Http_init(void)
{
   char *env_proxy = getenv("http_proxy");

   HTTP_Language_hdr = prefs.http_language ?
      dStrconcat("Accept-Language: ", prefs.http_language, "\r\n", NULL) :
      dStrdup("");

   if (env_proxy && strlen(env_proxy))
      HTTP_Proxy = a_Url_new(env_proxy, NULL);
   if (!HTTP_Proxy && prefs.http_proxy)
      HTTP_Proxy = a_Url_dup(prefs.http_proxy);

/*  This allows for storing the proxy password in "user:passwd" format
 * in dillorc, but as this constitutes a security problem, it was disabled.
 *
   if (HTTP_Proxy && prefs.http_proxyuser && strchr(prefs.http_proxyuser, ':'))
      HTTP_Proxy_Auth_base64 = a_Misc_encode_base64(prefs.http_proxyuser);
 */

   host_connections = dList_new(5);

   return 0;
}

/*
 * Tell whether the proxy auth is already set (user:password)
 * Return: 1 Yes, 0 No
 */
int a_Http_proxy_auth(void)
{
   return (HTTP_Proxy_Auth_base64 ? 1 : 0);
}

/*
 * Activate entered proxy password for HTTP.
 */
void a_Http_set_proxy_passwd(const char *str)
{
   char *http_proxyauth = dStrconcat(prefs.http_proxyuser, ":", str, NULL);
   HTTP_Proxy_Auth_base64 = a_Misc_encode_base64(http_proxyauth);
   dFree(http_proxyauth);
}

/*
 * Create and init a new SocketData_t struct, insert into ValidSocks,
 * and return a primary key for it.
 */
static int Http_sock_new(void)
{
   SocketData_t *S = dNew0(SocketData_t, 1);
   return a_Klist_insert(&ValidSocks, S);
}

static void Http_connect_queued_sockets(HostConnection_t *hc)
{
   SocketData_t *sd;
   while (hc->active_connections < prefs.http_max_conns &&
          (sd = Http_socket_dequeue(&hc->queue))) {

      sd->flags &= ~HTTP_SOCKET_QUEUED;

      if (sd->flags & HTTP_SOCKET_TO_BE_FREED) {
          dFree(sd);
      } else if (a_Web_valid(sd->web)) {
         /* start connecting the socket */
         if (Http_connect_socket(sd->Info) < 0) {
            ChainLink *Info = sd->Info;
            MSG_BW(sd->web, 1, "ERROR: %s", dStrerror(sd->Err));
            a_Chain_bfcb(OpAbort, Info, NULL, "Both");
            Http_socket_free(VOIDP2INT(Info->LocalKey)); /* free sd */
            dFree(Info);
         } else {
            sd->connected_to = hc->host;
            hc->active_connections++;
         }
      }
   }
}

/*
 * Free SocketData_t struct
 */
static void Http_socket_free(int SKey)
{
   SocketData_t *S;

   if ((S = a_Klist_get_data(ValidSocks, SKey))) {
      a_Klist_remove(ValidSocks, SKey);

      if (S->flags & HTTP_SOCKET_QUEUED) {
         S->flags |= HTTP_SOCKET_TO_BE_FREED;
      } else {
         if (S->connected_to) {
            HostConnection_t *hc = Http_host_connection_get(S->connected_to);
            hc->active_connections--;
            Http_connect_queued_sockets(hc);
            if (hc->active_connections == 0)
               Http_host_connection_remove(hc);
         }
         dFree(S);
      }
   }
}

/*
 * Make the HTTP header's Referer line according to preferences
 * (default is "host" i.e. "scheme://hostname/" )
 */
static char *Http_get_referer(const DilloUrl *url)
{
   char *referer = NULL;

   if (!strcmp(prefs.http_referer, "host")) {
      referer = dStrconcat("Referer: ", URL_SCHEME(url), "://",
                           URL_AUTHORITY(url), "/", "\r\n", NULL);
   } else if (!strcmp(prefs.http_referer, "path")) {
      referer = dStrconcat("Referer: ", URL_SCHEME(url), "://",
                           URL_AUTHORITY(url),
                           URL_PATH_(url) ? URL_PATH(url) : "/", "\r\n", NULL);
   }
   if (!referer)
      referer = dStrdup("");
   _MSG("http, referer='%s'\n", referer);
   return referer;
}

/*
 * Generate Content-Type header value for a POST query.
 */
static Dstr *Http_make_content_type(const DilloUrl *url)
{
   Dstr *dstr;

   if (URL_FLAGS(url) & URL_MultipartEnc) {
      _MSG("submitting multipart/form-data!\n");
      dstr = dStr_new("multipart/form-data; boundary=\"");
      if (URL_DATA(url)->len > 2) {
         /* boundary lines have "--" prepended. Skip that. */
         const char *start = URL_DATA(url)->str + 2;
         char *eol = strchr(start, '\r');
         if (eol)
            dStr_append_l(dstr, start, eol - start);
      } else {
         /* Zero parts; arbitrary boundary */
         dStr_append_c(dstr, '0');
      }
      dStr_append_c(dstr,'"');
   } else {
      dstr = dStr_new("application/x-www-form-urlencoded");
   }
   return dstr;
}

/*
 * Make the http query string
 */
Dstr *a_Http_make_query_str(const DilloUrl *url, const DilloUrl *requester,
                            int web_flags, bool_t use_proxy)
{
   char *ptr, *cookies, *referer, *auth;
   Dstr *query      = dStr_new(""),
        *request_uri = dStr_new(""),
        *proxy_auth = dStr_new("");

   /* BUG: dillo doesn't actually understand application/xml yet */
   const char *accept_hdr_value =
      web_flags & WEB_Image ? "image/png,image/*;q=0.8,*/*;q=0.5" :
      web_flags & WEB_Stylesheet ? "text/css,*/*;q=0.1" :
      "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

   if (use_proxy) {
      dStr_sprintfa(request_uri, "%s%s",
                    URL_STR(url),
                    (URL_PATH_(url) || URL_QUERY_(url)) ? "" : "/");
      if ((ptr = strrchr(request_uri->str, '#')))
         dStr_truncate(request_uri, ptr - request_uri->str);
      if (HTTP_Proxy_Auth_base64)
         dStr_sprintf(proxy_auth, "Proxy-Authorization: Basic %s\r\n",
                      HTTP_Proxy_Auth_base64);
   } else {
      dStr_sprintfa(request_uri, "%s%s%s%s",
                    URL_PATH(url),
                    URL_QUERY_(url) ? "?" : "",
                    URL_QUERY(url),
                    (URL_PATH_(url) || URL_QUERY_(url)) ? "" : "/");
   }

   cookies = a_Cookies_get_query(url, requester);
   auth = a_Auth_get_auth_str(url, request_uri->str);
   referer = Http_get_referer(url);
   if (URL_FLAGS(url) & URL_Post) {
      Dstr *content_type = Http_make_content_type(url);
      dStr_sprintfa(
         query,
         "POST %s HTTP/1.1\r\n"
         "Host: %s\r\n"
         "User-Agent: %s\r\n"
         "Accept: %s\r\n"
         "%s" /* language */
         "Accept-Encoding: gzip, deflate\r\n"
         "%s" /* auth */
         "DNT: 1\r\n"
         "%s" /* proxy auth */
         "%s" /* referer */
         "Connection: close\r\n"
         "Content-Type: %s\r\n"
         "Content-Length: %ld\r\n"
         "%s" /* cookies */
         "\r\n",
         request_uri->str, URL_AUTHORITY(url), prefs.http_user_agent,
         accept_hdr_value, HTTP_Language_hdr, auth ? auth : "",
         proxy_auth->str, referer, content_type->str, (long)URL_DATA(url)->len,
         cookies);
      dStr_append_l(query, URL_DATA(url)->str, URL_DATA(url)->len);
      dStr_free(content_type, TRUE);
   } else {
      dStr_sprintfa(
         query,
         "GET %s HTTP/1.1\r\n"
         "Host: %s\r\n"
         "User-Agent: %s\r\n"
         "Accept: %s\r\n"
         "%s" /* language */
         "Accept-Encoding: gzip, deflate\r\n"
         "%s" /* auth */
         "DNT: 1\r\n"
         "%s" /* proxy auth */
         "%s" /* referer */
         "Connection: close\r\n"
         "%s" /* cache control */
         "%s" /* cookies */
         "\r\n",
         request_uri->str, URL_AUTHORITY(url), prefs.http_user_agent,
         accept_hdr_value, HTTP_Language_hdr, auth ? auth : "",
         proxy_auth->str, referer,
         (URL_FLAGS(url) & URL_E2EQuery) ?
            "Pragma: no-cache\r\nCache-Control: no-cache\r\n" : "",
         cookies);
   }
   dFree(referer);
   dFree(cookies);
   dFree(auth);

   dStr_free(request_uri, TRUE);
   dStr_free(proxy_auth, TRUE);
   _MSG("Query: {%s}\n", dStr_printable(query, 8192));
   return query;
}

/*
 * Create and submit the HTTP query to the IO engine
 */
static void Http_send_query(ChainLink *Info, SocketData_t *S)
{
   Dstr *query;
   DataBuf *dbuf;

   /* Create the query */
   query = a_Http_make_query_str(S->web->url, S->web->requester, S->web->flags,
                                 S->flags & HTTP_SOCKET_USE_PROXY);
   dbuf = a_Chain_dbuf_new(query->str, query->len, 0);

   /* actually this message is sent too early.
    * It should go when the socket is ready for writing (i.e. connected) */
   _MSG_BW(S->web, 1, "Sending query to %s...", URL_HOST_(S->web->url));

   /* send query */
   a_Chain_bcb(OpSend, Info, dbuf, NULL);
   dFree(dbuf);
   dStr_free(query, 1);
}

/*
 * This function gets called after the DNS succeeds solving a hostname.
 * Task: Finish socket setup and start connecting the socket.
 * Return value: 0 on success;  -1 on error.
 */
static int Http_connect_socket(ChainLink *Info)
{
   int i, status;
#ifdef ENABLE_IPV6
   struct sockaddr_in6 name;
#else
   struct sockaddr_in name;
#endif
   SocketData_t *S;
   DilloHost *dh;
   socklen_t socket_len = 0;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));

   /* TODO: iterate this address list until success, or end-of-list */
   for (i = 0; (dh = dList_nth_data(S->addr_list, i)); ++i) {
      if ((S->SockFD = socket(dh->af, SOCK_STREAM, IPPROTO_TCP)) < 0) {
         S->Err = errno;
         MSG("Http_connect_socket ERROR: %s\n", dStrerror(errno));
         continue;
      }
      /* set NONBLOCKING and close on exec. */
      fcntl(S->SockFD, F_SETFL, O_NONBLOCK | fcntl(S->SockFD, F_GETFL));
      fcntl(S->SockFD, F_SETFD, FD_CLOEXEC | fcntl(S->SockFD, F_GETFD));

      /* Some OSes require this...  */
      memset(&name, 0, sizeof(name));
      /* Set remaining parms. */
      switch (dh->af) {
      case AF_INET:
      {
         struct sockaddr_in *sin = (struct sockaddr_in *)&name;
         socket_len = sizeof(struct sockaddr_in);
         sin->sin_family = dh->af;
         sin->sin_port = S->port ? htons(S->port) : htons(DILLO_URL_HTTP_PORT);
         memcpy(&sin->sin_addr, dh->data, (size_t)dh->alen);
         if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
            MSG("Connecting to %s\n", inet_ntoa(sin->sin_addr));
         break;
      }
#ifdef ENABLE_IPV6
      case AF_INET6:
      {
         char buf[128];
         struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&name;
         socket_len = sizeof(struct sockaddr_in6);
         sin6->sin6_family = dh->af;
         sin6->sin6_port =
            S->port ? htons(S->port) : htons(DILLO_URL_HTTP_PORT);
         memcpy(&sin6->sin6_addr, dh->data, dh->alen);
         inet_ntop(dh->af, dh->data, buf, sizeof(buf));
         if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
            MSG("Connecting to %s\n", buf);
         break;
      }
#endif
      }/*switch*/

      MSG_BW(S->web, 1, "Contacting host...");
      status = connect(S->SockFD, (struct sockaddr *)&name, socket_len);
      if (status == -1 && errno != EINPROGRESS) {
         S->Err = errno;
         dClose(S->SockFD);
         MSG("Http_connect_socket ERROR: %s\n", dStrerror(S->Err));
      } else {
         a_Chain_bcb(OpSend, Info, &S->SockFD, "FD");
         a_Chain_fcb(OpSend, Info, &S->SockFD, "FD");
         Http_send_query(S->Info, S);
         return 0; /* Success */
      }
   }

   return -1;
}

/*
 * Test proxy settings and check the no_proxy domains list
 * Return value: whether to use proxy or not.
 */
static int Http_must_use_proxy(const DilloUrl *url)
{
   char *np, *p, *tok;
   int ret = 0;

   if (HTTP_Proxy) {
      ret = 1;
      if (prefs.no_proxy) {
         const char *host = URL_HOST(url);
         size_t host_len = strlen(host);

         np = dStrdup(prefs.no_proxy);
         for (p = np; (tok = dStrsep(&p, " "));  ) {
            int start = host_len - strlen(tok);

            if (start >= 0 && dStrAsciiCasecmp(host + start, tok) == 0) {
               /* no_proxy token is suffix of host string */
               ret = 0;
               break;
            }
         }
         dFree(np);
      }
   }
   _MSG("Http_must_use_proxy: %s\n  %s\n", URL_STR(url), ret ? "YES":"NO");
   return ret;
}

/*
 * Return a new string for the request used to tunnel HTTPS through a proxy.
 * As of 2009, the best reference appears to be section 5 of RFC 2817.
 */
char *a_Http_make_connect_str(const DilloUrl *url)
{
   Dstr *dstr;
   const char *auth1;
   int auth_len;
   char *auth2, *proxy_auth, *retstr;

   dReturn_val_if_fail(Http_must_use_proxy(url), NULL);

   dstr = dStr_new("");
   auth1 = URL_AUTHORITY(url);
   auth_len = strlen(auth1);
   if (auth_len > 0 && !isdigit(auth1[auth_len - 1]))
      /* if no port number, add HTTPS port */
      auth2 = dStrconcat(auth1, ":443", NULL);
   else
      auth2 = dStrdup(auth1);
   proxy_auth = HTTP_Proxy_Auth_base64 ?
                   dStrconcat ("Proxy-Authorization: Basic ",
                               HTTP_Proxy_Auth_base64, "\r\n", NULL) :
                   dStrdup("");
   dStr_sprintfa(
      dstr,
      "CONNECT %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "%s"
      "\r\n",
      auth2,
      auth2,
      proxy_auth);

   dFree(auth2);
   dFree(proxy_auth);
   retstr = dstr->str;
   dStr_free(dstr, 0);
   return retstr;
}

/*
 * Return URL string of HTTP proxy, if any
 */
const char *a_Http_get_proxy_urlstr()
{
   return HTTP_Proxy ? URL_STR(HTTP_Proxy) : NULL;
}

/*
 * Callback function for the DNS resolver.
 * Continue connecting the socket, or abort upon error condition.
 * S->web is checked to assert the operation wasn't aborted while waiting.
 */
static void Http_dns_cb(int Status, Dlist *addr_list, void *data)
{
   int SKey = VOIDP2INT(data);
   SocketData_t *S;
   HostConnection_t *hc;

   S = a_Klist_get_data(ValidSocks, SKey);
   if (S) {
      if (!a_Web_valid(S->web)) {
         a_Chain_bfcb(OpAbort, S->Info, NULL, "Both");
         dFree(S->Info);
         Http_socket_free(SKey);

      } else if (Status == 0 && addr_list) {
         /* Successful DNS answer; save the IP */
         S->addr_list = addr_list;
         S->flags |= HTTP_SOCKET_QUEUED;
         if (S->flags & HTTP_SOCKET_USE_PROXY)
            hc = Http_host_connection_get(URL_HOST(HTTP_Proxy));
         else
            hc = Http_host_connection_get(URL_HOST(S->web->url));
         Http_socket_enqueue(&hc->queue, S);
         Http_connect_queued_sockets(hc);
      } else {
         /* DNS wasn't able to resolve the hostname */
         MSG_BW(S->web, 0, "ERROR: Dns can't resolve %s",
            (S->flags & HTTP_SOCKET_USE_PROXY) ? URL_HOST_(HTTP_Proxy) :
                                                 URL_HOST_(S->web->url));
         a_Chain_bfcb(OpAbort, S->Info, NULL, "Both");
         dFree(S->Info);
         Http_socket_free(SKey);
      }
   }
}

/*
 * Asynchronously create a new http connection for 'Url'
 * We'll set some socket parameters; the rest will be set later
 * when the IP is known.
 * ( Data1 = Web structure )
 * Return value: 0 on success, -1 otherwise
 */
static int Http_get(ChainLink *Info, void *Data1)
{
   SocketData_t *S;
   char *hostname;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));
   /* Reference Web data */
   S->web = Data1;
   /* Reference Info data */
   S->Info = Info;

   /* Proxy support */
   if (Http_must_use_proxy(S->web->url)) {
      hostname = dStrdup(URL_HOST(HTTP_Proxy));
      S->port = URL_PORT(HTTP_Proxy);
      S->flags |= HTTP_SOCKET_USE_PROXY;
   } else {
      hostname = dStrdup(URL_HOST(S->web->url));
      S->port = URL_PORT(S->web->url);
      S->flags &= ~HTTP_SOCKET_USE_PROXY;
   }

   /* Let the user know what we'll do */
   MSG_BW(S->web, 1, "DNS resolving %s", URL_HOST_(S->web->url));

   /* Let the DNS engine resolve the hostname, and when done,
    * we'll try to connect the socket from the callback function */
   a_Dns_resolve(hostname, Http_dns_cb, Info->LocalKey);

   dFree(hostname);
   return 0;
}

/*
 * CCC function for the HTTP module
 */
void a_Http_ccc(int Op, int Branch, int Dir, ChainLink *Info,
                void *Data1, void *Data2)
{
   int SKey = VOIDP2INT(Info->LocalKey);

   (void)Data2; /* suppress unused parameter warning */

   dReturn_if_fail( a_Chain_check("a_Http_ccc", Op, Branch, Dir, Info) );

   if (Branch == 1) {
      if (Dir == BCK) {
         /* HTTP query branch */
         switch (Op) {
         case OpStart:
            /* ( Data1 = Web ) */
            SKey = Http_sock_new();
            Info->LocalKey = INT2VOIDP(SKey);
            /* link IO */
            a_Chain_link_new(Info, a_Http_ccc, BCK, a_IO_ccc, 1, 1);
            a_Chain_bcb(OpStart, Info, NULL, NULL);
            /* async. connection */
            Http_get(Info, Data1);
            break;
         case OpEnd:
            /* finished the HTTP query branch */
            a_Chain_bcb(OpEnd, Info, NULL, NULL);
            Http_socket_free(SKey);
            dFree(Info);
            break;
         case OpAbort:
            /* something bad happened... */
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Http_socket_free(SKey);
            dFree(Info);
            break;
         }
      } else {  /* 1 FWD */
         /* HTTP send-query status branch */
         switch (Op) {
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}


static void Http_socket_queue_init(SocketQueue_t *sq)
{
   sq->head = NULL;
   sq->tail = NULL;
}

static void Http_socket_enqueue(SocketQueue_t *sq, SocketData_t* sock)
{
   SocketQueueEntry_t *se = dNew(SocketQueueEntry_t, 1);

   se->sock = sock;
   se->next = NULL;

   if (sq->tail)
      sq->tail->next = se;
   sq->tail = se;

   if (! sq->head)
      sq->head = se;
}

static SocketData_t* Http_socket_dequeue(SocketQueue_t *sq)
{
   SocketQueueEntry_t *se = sq->head;
   SocketData_t *sd = NULL;

   if (se) {
      sq->head = se->next;
      if (sq->tail == se)
         sq->tail = NULL;
      sd = se->sock;
      dFree(se);
   }

   return sd;
}

static HostConnection_t *Http_host_connection_get(const char *host)
{
   int i;
   HostConnection_t *hc;

   for (i = 0; i < dList_length(host_connections); i++) {
      hc = (HostConnection_t*) dList_nth_data(host_connections, i);

      if (dStrAsciiCasecmp(host, hc->host) == 0)
         return hc;
   }

   hc = dNew0(HostConnection_t, 1);
   Http_socket_queue_init(&hc->queue);
   hc->host = dStrdup(host);
   dList_append(host_connections, hc);

   return hc;
}

static void Http_host_connection_remove(HostConnection_t *hc)
{
    assert(hc->queue.head == NULL);
    dList_remove_fast(host_connections, hc);
    dFree(hc->host);
    dFree(hc);
}

static void Http_host_connection_remove_all()
{
   HostConnection_t *hc;

   while (dList_length(host_connections) > 0) {
      hc = (HostConnection_t*) dList_nth_data(host_connections, 0);
      while (Http_socket_dequeue(&hc->queue));
      Http_host_connection_remove(hc);
   }
   dList_free(host_connections);
}

/*
 * Deallocate memory used by http module
 * (Call this one at exit time)
 */
void a_Http_freeall(void)
{
   Http_host_connection_remove_all();
   a_Klist_free(&ValidSocks);
   a_Url_free(HTTP_Proxy);
   dFree(HTTP_Proxy_Auth_base64);
   dFree(HTTP_Language_hdr);
}
