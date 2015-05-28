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
#include "ssl.h"
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
static const int HTTP_SOCKET_QUEUED      = 0x2;
static const int HTTP_SOCKET_TO_BE_FREED = 0x4;
static const int HTTP_SOCKET_SSL         = 0x8;

/* 'web' is just a reference (no need to deallocate it here). */
typedef struct {
   int SockFD;
   uint_t flags;
   DilloWeb *web;          /* reference to client's web structure */
   DilloUrl *url;
   Dlist *addr_list;       /* Holds the DNS answer */
   ChainLink *Info;        /* Used for CCC asynchronous operations */
   char *connected_to;     /* Used for per-server connection limit */
   uint_t connect_port;
   Dstr *https_proxy_reply;
} SocketData_t;

/* Data structures and functions to queue sockets that need to be
 * delayed due to the per host connection limit.
 */
typedef struct {
  char *host;
  uint_t port;
  bool_t https;

  int active_conns;
  Dlist *queue;
} Server_t;

typedef struct {
   int fd;
   int skey;
} FdMapEntry_t;

static void Http_socket_enqueue(Server_t *srv, SocketData_t* sock);
static Server_t *Http_server_get(const char *host, uint_t port, bool_t https);
static void Http_server_remove(Server_t *srv);
static void Http_connect_socket(ChainLink *Info);
static char *Http_get_connect_str(const DilloUrl *url);
static void Http_send_query(SocketData_t *S);
static void Http_socket_free(int SKey);

/*
 * Local data
 */
static Klist_t *ValidSocks = NULL; /* Active sockets list. It holds pointers to
                                    * SocketData_t structures. */
static DilloUrl *HTTP_Proxy = NULL;
static char *HTTP_Proxy_Auth_base64 = NULL;
static char *HTTP_Language_hdr = NULL;
static Dlist *servers;

/* TODO: If fd_map will stick around in its present form (FDs and SocketData_t)
 * then consider whether having both this and ValidSocks is necessary.
 */
static Dlist *fd_map;

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

   servers = dList_new(5);
   fd_map = dList_new(20);

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
   S->SockFD = -1;
   return a_Klist_insert(&ValidSocks, S);
}

/*
 * Compare by FD.
 */
static int Http_fd_map_cmp(const void *v1, const void *v2)
{
   int fd = VOIDP2INT(v2);
   const FdMapEntry_t *e = v1;

   return (fd != e->fd);
}

static void Http_fd_map_add_entry(SocketData_t *sd)
{
   FdMapEntry_t *e = dNew0(FdMapEntry_t, 1);
   e->fd = sd->SockFD;
   e->skey = VOIDP2INT(sd->Info->LocalKey);

   if (dList_find_custom(fd_map, INT2VOIDP(e->fd), Http_fd_map_cmp)) {
      MSG_ERR("FD ENTRY ALREADY FOUND FOR %d\n", e->fd);
      assert(0);
   }

   dList_append(fd_map, e);
}

/*
 * Remove and free entry from fd_map.
 */
static void Http_fd_map_remove_entry(int fd)
{
   void *data = dList_find_custom(fd_map, INT2VOIDP(fd), Http_fd_map_cmp);

   if (data) {
      dList_remove_fast(fd_map, data);
      dFree(data);
   } else {
      MSG("FD ENTRY NOT FOUND FOR %d\n", fd);
   }
}

void a_Http_connect_done(int fd, bool_t success)
{
   SocketData_t *sd;
   FdMapEntry_t *fme = dList_find_custom(fd_map, INT2VOIDP(fd),
                                            Http_fd_map_cmp);

   if (fme && (sd = a_Klist_get_data(ValidSocks, fme->skey))) {
      ChainLink *info = sd->Info;

      if (success) {
         a_Chain_bfcb(OpSend, info, &sd->SockFD, "FD");
         Http_send_query(sd);
      } else {
         MSG_BW(sd->web, 1, "Could not establish connection.");
         MSG("fd %d is done and failed\n", sd->SockFD);
         dClose(fd);
         Http_socket_free(VOIDP2INT(info->LocalKey)); /* free sd */
         a_Chain_bfcb(OpAbort, info, NULL, "Both");
         dFree(info);
      }
   } else {
      MSG("**** but no luck with fme %p or sd\n", fme);
   }
}

static void Http_socket_activate(Server_t *srv, SocketData_t *sd)
{
   dList_remove(srv->queue, sd);
   sd->flags &= ~HTTP_SOCKET_QUEUED;
   srv->active_conns++;
   sd->connected_to = srv->host;
}

static void Http_connect_queued_sockets(Server_t *srv)
{
   SocketData_t *sd;
   int i;

   for (i = 0;
        (i < dList_length(srv->queue) &&
         srv->active_conns < prefs.http_max_conns);
        i++) {
      sd = dList_nth_data(srv->queue, i);

      if (!(sd->flags & HTTP_SOCKET_TO_BE_FREED)) {
         int connect_ready = SSL_CONNECT_READY;

         if (sd->flags & HTTP_SOCKET_SSL)
            connect_ready = a_Ssl_connect_ready(sd->url);

         if (connect_ready == SSL_CONNECT_NEVER || !a_Web_valid(sd->web)) {
            int SKey = VOIDP2INT(sd->Info->LocalKey);

            Http_socket_free(SKey);
         } else if (connect_ready == SSL_CONNECT_READY) {
            i--;
            Http_socket_activate(srv, sd);
            Http_connect_socket(sd->Info);
         }
      }
      if (sd->flags & HTTP_SOCKET_TO_BE_FREED) {
         dList_remove(srv->queue, sd);
         dFree(sd);
         i--;
      }
   }

   _MSG("Queue http%s://%s:%u len %d\n", srv->https ? "s" : "", srv->host,
        srv->port, dList_length(srv->queue));
}

/*
 * Free SocketData_t struct
 */
static void Http_socket_free(int SKey)
{
   SocketData_t *S;

   if ((S = a_Klist_get_data(ValidSocks, SKey))) {
      a_Klist_remove(ValidSocks, SKey);

      dStr_free(S->https_proxy_reply, 1);

      if (S->flags & HTTP_SOCKET_QUEUED) {
         S->flags |= HTTP_SOCKET_TO_BE_FREED;
         a_Url_free(S->url);
      } else {
         if (S->SockFD != -1)
            Http_fd_map_remove_entry(S->SockFD);
         a_Ssl_reset_server_state(S->url);
         if (S->connected_to) {
            a_Ssl_close_by_fd(S->SockFD);

            Server_t *srv = Http_server_get(S->connected_to, S->connect_port,
                                            (S->flags & HTTP_SOCKET_SSL));
            srv->active_conns--;
            Http_connect_queued_sockets(srv);
            if (srv->active_conns == 0)
               Http_server_remove(srv);
         }
         a_Url_free(S->url);
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
static Dstr *Http_make_query_str(DilloWeb *web, bool_t use_proxy)
{
   char *ptr, *cookies, *referer, *auth;
   const DilloUrl *url = web->url;
   Dstr *query      = dStr_new(""),
        *request_uri = dStr_new(""),
        *proxy_auth = dStr_new("");

   /* BUG: dillo doesn't actually understand application/xml yet */
   const char *accept_hdr_value =
      web->flags & WEB_Image ? "image/png,image/*;q=0.8,*/*;q=0.5" :
      web->flags & WEB_Stylesheet ? "text/css,*/*;q=0.1" :
      "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

   const char *connection_hdr_val =
      (prefs.http_persistent_conns == TRUE) ? "keep-alive" : "close";

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

   cookies = a_Cookies_get_query(url, web->requester);
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
         "Connection: %s\r\n"
         "Content-Type: %s\r\n"
         "Content-Length: %ld\r\n"
         "%s" /* cookies */
         "\r\n",
         request_uri->str, URL_AUTHORITY(url), prefs.http_user_agent,
         accept_hdr_value, HTTP_Language_hdr, auth ? auth : "",
         proxy_auth->str, referer, connection_hdr_val, content_type->str,
         (long)URL_DATA(url)->len, cookies);
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
         "Connection: %s\r\n"
         "%s" /* cache control */
         "%s" /* cookies */
         "\r\n",
         request_uri->str, URL_AUTHORITY(url), prefs.http_user_agent,
         accept_hdr_value, HTTP_Language_hdr, auth ? auth : "",
         proxy_auth->str, referer, connection_hdr_val,
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
static void Http_send_query(SocketData_t *S)
{
   Dstr *query;
   DataBuf *dbuf;

   /* Create the query */
   query = Http_make_query_str(S->web, S->flags & HTTP_SOCKET_USE_PROXY);
   dbuf = a_Chain_dbuf_new(query->str, query->len, 0);

   /* actually this message is sent too early.
    * It should go when the socket is ready for writing (i.e. connected) */
   _MSG_BW(S->web, 1, "Sending query to %s...", URL_HOST_(S->web->url));

   /* send query */
   a_Chain_bcb(OpSend, S->Info, dbuf, NULL);
   dFree(dbuf);
   dStr_free(query, 1);
}

/*
 * Prepare an HTTPS connection.  If necessary, tunnel it through a proxy.
 * Then perform the SSL handshake.
 */
static void Http_connect_ssl(ChainLink *info)
{
   int SKey = VOIDP2INT(info->LocalKey);
   SocketData_t *S = a_Klist_get_data(ValidSocks, SKey);

   if (S->flags & HTTP_SOCKET_USE_PROXY) {
      char *connect_str = Http_get_connect_str(S->url);
      DataBuf *dbuf = a_Chain_dbuf_new(connect_str, strlen(connect_str), 0);

      a_Chain_bfcb(OpSend, info, &S->SockFD, "FD");
      S->https_proxy_reply = dStr_new(NULL);
      a_Chain_bcb(OpSend, info, dbuf, NULL);

      dFree(dbuf);
      dFree(connect_str);
   } else {
      a_Ssl_handshake(S->SockFD, S->url);
   }
}

/*
 * This function is called after the DNS succeeds in solving a hostname.
 * Task: Finish socket setup and start connecting the socket.
 */
static void Http_connect_socket(ChainLink *Info)
{
   int i, status;
   SocketData_t *S;
   DilloHost *dh;
#ifdef ENABLE_IPV6
   struct sockaddr_in6 name;
#else
   struct sockaddr_in name;
#endif
   socklen_t socket_len = 0;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));

   /* TODO: iterate this address list until success, or end-of-list */
   for (i = 0; (dh = dList_nth_data(S->addr_list, i)); ++i) {
      if ((S->SockFD = socket(dh->af, SOCK_STREAM, IPPROTO_TCP)) < 0) {
         MSG("Http_connect_socket ERROR: %s\n", dStrerror(errno));
         continue;
      }
      Http_fd_map_add_entry(S);

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
         sin->sin_port = htons(S->connect_port);
         memcpy(&sin->sin_addr, dh->data, (size_t)dh->alen);
         if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
            MSG("Connecting to %s:%u\n", inet_ntoa(sin->sin_addr),
                S->connect_port);
         break;
      }
#ifdef ENABLE_IPV6
      case AF_INET6:
      {
         char buf[128];
         struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&name;
         socket_len = sizeof(struct sockaddr_in6);
         sin6->sin6_family = dh->af;
         sin6->sin6_port = htons(S->connect_port);
         memcpy(&sin6->sin6_addr, dh->data, dh->alen);
         inet_ntop(dh->af, dh->data, buf, sizeof(buf));
         if (a_Web_valid(S->web) && (S->web->flags & WEB_RootUrl))
            MSG("Connecting to %s:%u\n", buf, S->connect_port);
         break;
      }
#endif
      }/*switch*/
      MSG_BW(S->web, 1, "Contacting host...");
      status = connect(S->SockFD, (struct sockaddr *)&name, socket_len);
      if (status == -1 && errno != EINPROGRESS) {
         MSG("Http_connect_socket ERROR: %s\n", dStrerror(errno));
         a_Http_connect_done(S->SockFD, FALSE);
      } else if (S->flags & HTTP_SOCKET_SSL) {
         Http_connect_ssl(Info);
      } else {
         a_Http_connect_done(S->SockFD, TRUE);
      }
      return;
   }
}

/*
 * Test proxy settings and check the no_proxy domains list
 * Return value: whether to use proxy or not.
 */
static int Http_must_use_proxy(const char *hostname)
{
   char *np, *p, *tok;
   int ret = 0;

   if (HTTP_Proxy) {
      ret = 1;
      if (prefs.no_proxy) {
         size_t host_len = strlen(hostname);

         np = dStrdup(prefs.no_proxy);
         for (p = np; (tok = dStrsep(&p, " "));  ) {
            int start = host_len - strlen(tok);

            if (start >= 0 && dStrAsciiCasecmp(hostname + start, tok) == 0) {
               /* no_proxy token is suffix of host string */
               ret = 0;
               break;
            }
         }
         dFree(np);
      }
   }
   _MSG("Http_must_use_proxy: %s\n  %s\n", hostname, ret ? "YES":"NO");
   return ret;
}

/*
 * Return a new string for the request used to tunnel HTTPS through a proxy.
 */
static char *Http_get_connect_str(const DilloUrl *url)
{
   Dstr *dstr;
   const char *auth1;
   int auth_len;
   char *auth2, *proxy_auth, *retstr;

   dReturn_val_if_fail(Http_must_use_proxy(URL_HOST(url)), NULL);

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
 * Callback function for the DNS resolver.
 * Continue connecting the socket, or abort upon error condition.
 * S->web is checked to assert the operation wasn't aborted while waiting.
 */
static void Http_dns_cb(int Status, Dlist *addr_list, void *data)
{
   int SKey = VOIDP2INT(data);
   bool_t clean_up = TRUE;
   SocketData_t *S;
   Server_t *srv;

   S = a_Klist_get_data(ValidSocks, SKey);
   if (S) {
      const char *host = URL_HOST((S->flags & HTTP_SOCKET_USE_PROXY) ?
                                  HTTP_Proxy : S->url);
      if (a_Web_valid(S->web)) {
         if (Status == 0 && addr_list) {

            /* Successful DNS answer; save the IP */
            S->addr_list = addr_list;
            clean_up = FALSE;
            srv = Http_server_get(host, S->connect_port,
                                 (S->flags & HTTP_SOCKET_SSL));
            Http_socket_enqueue(srv, S);
            Http_connect_queued_sockets(srv);
         } else {
            /* DNS wasn't able to resolve the hostname */
            MSG_BW(S->web, 0, "ERROR: DNS can't resolve %s", host);
         }
      }
      if (clean_up) {
         ChainLink *info = S->Info;

         Http_socket_free(SKey);
         a_Chain_bfcb(OpAbort, info, NULL, "Both");
         dFree(info);
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
   const DilloUrl *url;

   S = a_Klist_get_data(ValidSocks, VOIDP2INT(Info->LocalKey));
   /* Reference Web data */
   S->web = Data1;
   /* Reference Info data */
   S->Info = Info;

   /* Proxy support */
   if (Http_must_use_proxy(URL_HOST(S->web->url))) {
      url = HTTP_Proxy;
      S->flags |= HTTP_SOCKET_USE_PROXY;
   } else {
      url = S->web->url;
   }
   hostname = dStrdup(URL_HOST(url));
   S->connect_port = URL_PORT(url);
   S->url = a_Url_dup(S->web->url);
   if (!dStrAsciiCasecmp(URL_SCHEME(S->url), "https"))
      S->flags |= HTTP_SOCKET_SSL;

   /* Let the user know what we'll do */
   MSG_BW(S->web, 1, "DNS resolving %s", hostname);

   /* Let the DNS engine resolve the hostname, and when done,
    * we'll try to connect the socket from the callback function */
   a_Dns_resolve(hostname, Http_dns_cb, Info->LocalKey);

   dFree(hostname);
   return 0;
}

/*
 * Can the old socket's fd be reused for the new socket?
 *
 * NOTE: old and new must come from the same Server_t.
 * This is not built to accept arbitrary sockets.
 */
static bool_t Http_socket_reuse_compatible(SocketData_t *old,
                                           SocketData_t *new)
{
   /*
    * If we are using SSL through a proxy, we need to ensure that old and new
    * are going through to the same host:port.
    */
   if (a_Web_valid(new->web) &&
       ((old->flags & HTTP_SOCKET_SSL) == 0 ||
        (old->flags & HTTP_SOCKET_USE_PROXY) == 0 ||
        ((URL_PORT(old->url) == URL_PORT(new->url)) &&
         !dStrAsciiCasecmp(URL_HOST(old->url), URL_HOST(new->url)))))
      return TRUE;
   return FALSE;
}

/*
 * If any entry in the socket data queue can reuse our connection, set it up
 * and send off a new query.
 */
static void Http_socket_reuse(int SKey)
{
   SocketData_t *new_sd, *old_sd = a_Klist_get_data(ValidSocks, SKey);

   if (old_sd) {
      Server_t *srv = Http_server_get(old_sd->connected_to,
                                      old_sd->connect_port,
                                      (old_sd->flags & HTTP_SOCKET_SSL));
      int i, n = dList_length(srv->queue);

      for (i = 0; i < n; i++) {
         new_sd = dList_nth_data(srv->queue, i);

         if (!(new_sd->flags & HTTP_SOCKET_TO_BE_FREED) &&
             Http_socket_reuse_compatible(old_sd, new_sd)) {
            const bool_t success = TRUE;

            new_sd->SockFD = old_sd->SockFD;

            old_sd->connected_to = NULL;
            srv->active_conns--;
            Http_socket_free(SKey);

            MSG("Reusing fd %d for %s\n", new_sd->SockFD,URL_STR(new_sd->url));
            Http_socket_activate(srv, new_sd);
            Http_fd_map_add_entry(new_sd);
            a_Http_connect_done(new_sd->SockFD, success);
            return;
         }
      }
      dClose(old_sd->SockFD);
      Http_socket_free(SKey);
   }
}

/*
 * CCC function for the HTTP module
 */
void a_Http_ccc(int Op, int Branch, int Dir, ChainLink *Info,
                void *Data1, void *Data2)
{
   int SKey = VOIDP2INT(Info->LocalKey);
   SocketData_t *sd;
   DataBuf *dbuf;

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
            dFree(Info);
            break;
         case OpAbort:
            MSG("ABORT 1B\n");
            Http_socket_free(SKey);
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC 1B Op %d\n", Op);
            break;
         }
      } else {  /* 1 FWD */
         /* HTTP send-query status branch */
         switch (Op) {
         case OpAbort:
            MSG("ABORT 1F\n");
            if ((sd = a_Klist_get_data(ValidSocks, SKey)))
               MSG_BW(sd->web, 1, "Can't get %s", URL_STR(sd->url));
            Http_socket_free(SKey);
            a_Chain_fcb(OpAbort, Info, NULL, "Both");
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC 1F Op %d\n", Op);
            break;
         }
      }
   } else if (Branch == 2) {
      if (Dir == FWD) {
         sd = a_Klist_get_data(ValidSocks, SKey);
         assert(sd);
         /* Receiving from server */
         switch (Op) {
         case OpSend:
            if (sd->https_proxy_reply) {
               dbuf = Data1;
               dStr_append(sd->https_proxy_reply, dbuf->Buf);
               if (strstr(sd->https_proxy_reply->str, "\r\n\r\n")) {
                  if (sd->https_proxy_reply->len >= 12 &&
                      sd->https_proxy_reply->str[9] == '2') {
                     /* e.g. "HTTP/1.1 200 Connection established[...]" */
                     MSG("CONNECT through proxy succeeded. Reply:\n%s\n",
                         sd->https_proxy_reply->str);
                     dStr_free(sd->https_proxy_reply, 1);
                     sd->https_proxy_reply = NULL;
                     a_Ssl_handshake(sd->SockFD, sd->url);
                  } else {
                     MSG_BW(sd->web, 1, "Can't connect through proxy to %s",
                            URL_HOST(sd->url));
                     MSG("CONNECT through proxy failed. Server sent:\n%s\n",
                         sd->https_proxy_reply->str);
                     Http_socket_free(SKey);
                     a_Chain_bfcb(OpAbort, Info, NULL, "Both");
                     dFree(Info);
                  }
               }
            } else {
               /* Data1 = dbuf */
               a_Chain_fcb(OpSend, Info, Data1, "send_page_2eof");
            }
            break;
         case OpEnd:
            if (sd->https_proxy_reply) {
               MSG("CONNECT through proxy failed. "
                   "Full reply not received:\n%s\n",
                   sd->https_proxy_reply->len ? sd->https_proxy_reply->str :
                   "(nothing)");
               Http_socket_free(SKey);
               a_Chain_bfcb(OpAbort, Info, NULL, "Both");
            } else {
               Http_socket_free(SKey);
               a_Chain_fcb(OpEnd, Info, NULL, NULL);
            }
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC 2F Op %d\n", Op);
            break;
         }
      } else {  /* 2 BCK */
         switch (Op) {
         case OpStart:
            a_Chain_link_new(Info, a_Http_ccc, BCK, a_IO_ccc, 2, 2);
            a_Chain_bcb(OpStart, Info, NULL, NULL); /* IORead */
            break;
         case OpSend:
            if (Data2) {
               if (!strcmp(Data2, "FD")) {
                  int fd = *(int*)Data1;
                  FdMapEntry_t *fme = dList_find_custom(fd_map, INT2VOIDP(fd),
                                                        Http_fd_map_cmp);
                  Info->LocalKey = INT2VOIDP(fme->skey);
                  a_Chain_bcb(OpSend, Info, Data1, Data2);
               } else if (!strcmp(Data2, "reply_complete")) {
                  a_Chain_bfcb(OpEnd, Info, NULL, NULL);
                  Http_socket_reuse(SKey);
                  dFree(Info);
               }
            }
            break;
         case OpAbort:
            Http_socket_free(SKey);
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC 2B Op %d\n", Op);
            break;
         }
      }
   }
}

/*
 * Add socket data to the queue. Pages/stylesheets/etc. have higher priority
 * than images.
 */
static void Http_socket_enqueue(Server_t *srv, SocketData_t* sock)
{
   sock->flags |= HTTP_SOCKET_QUEUED;

   if ((sock->web->flags & WEB_Image) == 0) {
      int i, n = dList_length(srv->queue);

      for (i = 0; i < n; i++) {
         SocketData_t *curr = dList_nth_data(srv->queue, i);

         if (a_Web_valid(curr->web) && (curr->web->flags & WEB_Image)) {
            dList_insert_pos(srv->queue, sock, i);
            return;
         }
      }
   }
   dList_append(srv->queue, sock);
}

static Server_t *Http_server_get(const char *host, uint_t port, bool_t https)
{
   int i;
   Server_t *srv;

   for (i = 0; i < dList_length(servers); i++) {
      srv = (Server_t*) dList_nth_data(servers, i);

      if (port == srv->port && https == srv->https &&
          !dStrAsciiCasecmp(host, srv->host))
         return srv;
   }

   srv = dNew0(Server_t, 1);
   srv->queue = dList_new(10);
   srv->host = dStrdup(host);
   srv->port = port;
   srv->https = https;
   dList_append(servers, srv);

   return srv;
}

static void Http_server_remove(Server_t *srv)
{
    assert(dList_length(srv->queue) == 0);
    dList_free(srv->queue);
    dList_remove_fast(servers, srv);
    dFree(srv->host);
    dFree(srv);
}

static void Http_servers_remove_all()
{
   Server_t *srv;
   SocketData_t *sd;

   while (dList_length(servers) > 0) {
      srv = (Server_t*) dList_nth_data(servers, 0);
      while ((sd = dList_nth_data(srv->queue, 0))) {
         dList_remove(srv->queue, sd);
         dFree(sd);
      }
      Http_server_remove(srv);
   }
   dList_free(servers);
}

static void Http_fd_map_remove_all()
{
   FdMapEntry_t *fme;
   int i, n = dList_length(fd_map);

   for (i = 0; i < n; i++) {
      fme = (FdMapEntry_t *) dList_nth_data(fd_map, i);
      dFree(fme);
   }
   dList_free(fd_map);
}

/*
 * Deallocate memory used by http module
 * (Call this one at exit time)
 */
void a_Http_freeall(void)
{
   Http_servers_remove_all();
   Http_fd_map_remove_all();
   a_Klist_free(&ValidSocks);
   a_Url_free(HTTP_Proxy);
   dFree(HTTP_Proxy_Auth_base64);
   dFree(HTTP_Language_hdr);
}
