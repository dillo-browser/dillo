/*
 * File: dns.c
 *
 * Copyright (C) 1999-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Non blocking pthread-handled Dns scheme
 */


/*
 * Uncomment the following line for debugging or gprof profiling.
 */
/* #undef D_DNS_THREADED */

#ifdef D_DNS_THREADED
#  include <pthread.h>
#endif


#include <assert.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "msg.h"
#include "dns.h"
#include "list.h"
#include "IO/iowatch.hh"


/* Maximum dns resolving threads */
#ifdef D_DNS_THREADED
#  define D_DNS_MAX_SERVERS 4
#else
#  define D_DNS_MAX_SERVERS 1
#endif

typedef enum {
   DNS_SERVER_IDLE,
   DNS_SERVER_PROCESSING,
   DNS_SERVER_RESOLVED,
} DnsServerState_t;

typedef struct {
   int channel;            /* Index of this channel [0 based] */
   DnsServerState_t state;
   Dlist *addr_list;       /* IP address */
   char *hostname;         /* Address to resolve */
   int status;             /* errno code for resolving function */
#ifdef D_DNS_THREADED
   pthread_t th1;          /* Thread id */
#endif
} DnsServer;

typedef struct {
   char *hostname;         /* host name for cache */
   Dlist *addr_list;       /* addresses of host */
} GDnsCache;

typedef struct {
   int channel;            /* -2 if waiting, otherwise index to dns_server[] */
   char *hostname;         /* The one we're resolving */
   DnsCallback_t cb_func;  /* callback function */
   void *cb_data;          /* extra data for the callback function */
} GDnsQueue;


/*
 * Forward declarations
 */
static void Dns_timeout_client(int fd, void *data);

/*
 * Local Data
 */
static DnsServer dns_server[D_DNS_MAX_SERVERS];
static int num_servers;
static GDnsCache *dns_cache;
static int dns_cache_size, dns_cache_size_max;
static GDnsQueue *dns_queue;
static int dns_queue_size, dns_queue_size_max;
static int dns_notify_pipe[2];


/* ----------------------------------------------------------------------
 *  Dns queue functions
 */
static void Dns_queue_add(int channel, const char *hostname,
                          DnsCallback_t cb_func, void *cb_data)
{
   a_List_add(dns_queue, dns_queue_size, dns_queue_size_max);
   dns_queue[dns_queue_size].channel = channel;
   dns_queue[dns_queue_size].hostname = dStrdup(hostname);
   dns_queue[dns_queue_size].cb_func = cb_func;
   dns_queue[dns_queue_size].cb_data = cb_data;
   dns_queue_size++;
}

/*
 * Find hostname index in dns_queue
 * (if found, returns queue index; -1 if not)
 */
static int Dns_queue_find(const char *hostname)
{
   int i;

   for (i = 0; i < dns_queue_size; i++)
      if (!dStrAsciiCasecmp(hostname, dns_queue[i].hostname))
         return i;

   return -1;
}

/*
 * Given an index, remove an entry from the dns_queue
 */
static void Dns_queue_remove(int index)
{
   int i;

   _MSG("Dns_queue_remove: deleting client [%d] [queue_size=%d]\n",
        index, dns_queue_size);

   if (index < dns_queue_size) {
      dFree(dns_queue[index].hostname);
      --dns_queue_size;         /* you'll find out why ;-) */
      for (i = index; i < dns_queue_size; i++)
         dns_queue[i] = dns_queue[i + 1];
   }
}

/*
 * Debug function
 *
void Dns_queue_print()
{
   int i;

   MSG("Queue: [");
   for (i = 0; i < dns_queue_size; i++)
      MSG("%d:%s ", dns_queue[i].channel, dns_queue[i].hostname);
   MSG("]\n");
}
 */

/*
 *  Add an IP/hostname pair to Dns-cache
 */
static void Dns_cache_add(char *hostname, Dlist *addr_list)
{
   a_List_add(dns_cache, dns_cache_size, dns_cache_size_max);
   dns_cache[dns_cache_size].hostname = dStrdup(hostname);
   dns_cache[dns_cache_size].addr_list = addr_list;
   ++dns_cache_size;
   _MSG("Cache objects: %d\n", dns_cache_size);
}


/*
 *  Initializer function
 */
void a_Dns_init(void)
{
   int res, i;

#ifdef D_DNS_THREADED
   MSG("dillo_dns_init: Here we go! (threaded)\n");
#else
   MSG("dillo_dns_init: Here we go! (not threaded)\n");
#endif

   dns_queue_size = 0;
   dns_queue_size_max = 16;
   dns_queue = dNew(GDnsQueue, dns_queue_size_max);

   dns_cache_size = 0;
   dns_cache_size_max = 16;
   dns_cache = dNew(GDnsCache, dns_cache_size_max);

   num_servers = D_DNS_MAX_SERVERS;

   res = pipe(dns_notify_pipe);
   assert(res == 0);
   fcntl(dns_notify_pipe[0], F_SETFL, O_NONBLOCK);
   a_IOwatch_add_fd(dns_notify_pipe[0], DIO_READ, Dns_timeout_client, NULL);

   /* Initialize servers data */
   for (i = 0; i < num_servers; ++i) {
      dns_server[i].channel = i;
      dns_server[i].state = DNS_SERVER_IDLE;
      dns_server[i].addr_list = NULL;
      dns_server[i].hostname = NULL;
      dns_server[i].status = 0;
#ifdef D_DNS_THREADED
      dns_server[i].th1 = (pthread_t) -1;
#endif
   }

#ifdef ENABLE_IPV6
   /* IPv6 test */
   {
      /* If the IPv6 address family is not available there is no point
         wasting time trying to connect to v6 addresses. */
      int fd = socket(AF_INET6, SOCK_STREAM, 0);
      if (fd >= 0)
         dClose(fd);
   }
#endif
}

/*
 * Allocate a host structure and add it to the list
 */

static void Dns_note_hosts(Dlist *list, struct addrinfo *res0)
{
   struct addrinfo *res;
   DilloHost *dh;

   for (res = res0; res; res = res->ai_next) {

      if (res->ai_family == AF_INET) {
         struct sockaddr_in *in_addr;

         if (res->ai_addrlen < sizeof(struct sockaddr_in)) {
            continue;
         }

         dh = dNew0(DilloHost, 1);
         dh->af = AF_INET;

         in_addr = (struct sockaddr_in*) res->ai_addr;
         dh->alen = sizeof (struct in_addr);
         memcpy(&dh->data[0], &in_addr->sin_addr.s_addr, dh->alen);

         dList_append(list, dh);
#ifdef ENABLE_IPV6
      } else if (res->ai_family == AF_INET6) {
         struct sockaddr_in6 *in6_addr;

         if (res->ai_addrlen < sizeof(struct sockaddr_in6)) {
            continue;
         }

         dh = dNew0(DilloHost, 1);
         dh->af = AF_INET6;

         in6_addr = (struct sockaddr_in6*) res->ai_addr;
         dh->alen = sizeof (struct in6_addr);
         memcpy(&dh->data[0], &in6_addr->sin6_addr.s6_addr, dh->alen);

         dList_append(list, dh);
#endif
      }
   }
}

/*
 *  Server function (runs on its own thread)
 */
static void *Dns_server(void *data)
{
   int channel = VOIDP2INT(data);
   struct addrinfo hints, *res0;
   int error;
   Dlist *hosts;
   size_t length, i;
   char addr_string[40];

   memset(&hints, 0, sizeof(hints));
#ifdef ENABLE_IPV6
   hints.ai_family = AF_UNSPEC;
#else
   hints.ai_family = AF_INET;
#endif
   hints.ai_socktype = SOCK_STREAM;

   hosts = dList_new(2);

   _MSG("Dns_server: starting...\n ch: %d host: %s\n",
        channel, dns_server[channel].hostname);

   error = getaddrinfo(dns_server[channel].hostname, NULL, &hints, &res0);

   if (error != 0) {
      dns_server[channel].status = error;
      if (error == EAI_NONAME)
         MSG("DNS error: HOST_NOT_FOUND\n");
      else if (error == EAI_AGAIN)
         MSG("DNS error: TRY_AGAIN\n");
#ifdef EAI_NODATA
      /* Some FreeBSD don't have this anymore */
      else if (error == EAI_NODATA)
         MSG("DNS error: NO_ADDRESS\n");
#endif
      else if (h_errno == EAI_FAIL)
         MSG("DNS error: NO_RECOVERY\n");
   } else {
      Dns_note_hosts(hosts, res0);
      dns_server[channel].status = 0;
      freeaddrinfo(res0);
   }

   if (dList_length(hosts) > 0) {
      dns_server[channel].status = 0;
   } else {
      dList_free(hosts);
      hosts = NULL;
   }

   /* tell our findings */
   MSG("Dns_server [%d]: %s is", channel,
       dns_server[channel].hostname);
   if ((length = dList_length(hosts))) {
      for (i = 0; i < length; i++) {
         a_Dns_dillohost_to_string(dList_nth_data(hosts, i),
                                   addr_string, sizeof(addr_string));
         MSG(" %s", addr_string);
      }
      MSG("\n");
   } else {
      MSG(" (nil)\n");
   }
   dns_server[channel].addr_list = hosts;
   dns_server[channel].state = DNS_SERVER_RESOLVED;

   write(dns_notify_pipe[1], ".", 1);

   return NULL;                 /* (avoids a compiler warning) */
}


/*
 *  Request function (spawn a server and let it handle the request)
 */
static void Dns_server_req(int channel, const char *hostname)
{
#ifdef D_DNS_THREADED
   static pthread_attr_t thrATTR;
   static int thrATTRInitialized = 0;
#endif

   dns_server[channel].state = DNS_SERVER_PROCESSING;

   dFree(dns_server[channel].hostname);
   dns_server[channel].hostname = dStrdup(hostname);

#ifdef D_DNS_THREADED
   /* set the thread attribute to the detached state */
   if (!thrATTRInitialized) {
      pthread_attr_init(&thrATTR);
      pthread_attr_setdetachstate(&thrATTR, PTHREAD_CREATE_DETACHED);
      thrATTRInitialized = 1;
   }
   /* Spawn thread */
   pthread_create(&dns_server[channel].th1, &thrATTR, Dns_server,
                  INT2VOIDP(dns_server[channel].channel));
#else
   Dns_server(0);
#endif
}

/*
 * Return the IP for the given hostname using a callback.
 * Side effect: a thread is spawned when hostname is not cached.
 */
void a_Dns_resolve(const char *hostname, DnsCallback_t cb_func, void *cb_data)
{
   int i, channel;

   if (!hostname)
      return;

   /* check for cache hit. */
   for (i = 0; i < dns_cache_size; i++)
      if (!dStrAsciiCasecmp(hostname, dns_cache[i].hostname))
         break;

   if (i < dns_cache_size) {
      /* already resolved, call the Callback immediately. */
      cb_func(0, dns_cache[i].addr_list, cb_data);

   } else if ((i = Dns_queue_find(hostname)) != -1) {
      /* hit in queue, but answer hasn't come back yet. */
      Dns_queue_add(dns_queue[i].channel, hostname, cb_func, cb_data);

   } else {
      /* Never requested before -- we must resolve it! */

      /* Find a channel we can send the request to */
      for (channel = 0; channel < num_servers; channel++)
         if (dns_server[channel].state == DNS_SERVER_IDLE)
            break;
      if (channel < num_servers) {
         /* Found a free channel! */
         Dns_queue_add(channel, hostname, cb_func, cb_data);
         Dns_server_req(channel, hostname);
      } else {
         /* We'll have to wait for a thread to finish... */
         Dns_queue_add(-2, hostname, cb_func, cb_data);
      }
   }
}

/*
 * Give answer to all queued callbacks on this channel
 */
static void Dns_serve_channel(int channel)
{
   int i;
   DnsServer *srv = &dns_server[channel];

   for (i = 0; i < dns_queue_size; i++) {
      if (dns_queue[i].channel == channel) {
         dns_queue[i].cb_func(srv->status, srv->addr_list,
                              dns_queue[i].cb_data);
         Dns_queue_remove(i);
         --i;
      }
   }
}

/*
 * Assign free channels to waiting clients (-2)
 */
static void Dns_assign_channels(void)
{
   int ch, i, j;

   for (ch = 0; ch < num_servers; ++ch) {
      if (dns_server[ch].state == DNS_SERVER_IDLE) {
         /* Find the next query in the queue (we're a FIFO) */
         for (i = 0; i < dns_queue_size; i++)
            if (dns_queue[i].channel == -2)
               break;

         if (i < dns_queue_size) {
            /* assign this channel to every queued request
             * with the same hostname*/
            for (j = i; j < dns_queue_size; j++)
               if (dns_queue[j].channel == -2 &&
                   !dStrAsciiCasecmp(dns_queue[j].hostname,
                                     dns_queue[i].hostname)) {
                  dns_queue[j].channel = ch;
               }
            Dns_server_req(ch, dns_queue[i].hostname);
         } else
            return;
      }
   }
}

/*
 * This function is called on the main thread and
 * reads the DNS results.
 */
static void Dns_timeout_client(int fd, void *data)
{
   int i;
   char buf[16];

   while (read(dns_notify_pipe[0], buf, sizeof(buf)) > 0);

   for (i = 0; i < num_servers; ++i) {
      DnsServer *srv = &dns_server[i];

      if (srv->state == DNS_SERVER_RESOLVED) {
         if (srv->addr_list != NULL) {
            /* DNS succeeded, let's cache it */
            Dns_cache_add(srv->hostname, srv->addr_list);
         }
         Dns_serve_channel(i);
         srv->state = DNS_SERVER_IDLE;
      }
   }
   Dns_assign_channels();
}


/*
 *  Dns memory-deallocation
 *  (Call this one at exit time)
 *  The Dns_queue is deallocated at execution time (no need to do that here)
 *  'dns_cache' is the only one that grows dynamically
 */
void a_Dns_freeall(void)
{
   int i, j;

   for ( i = 0; i < dns_cache_size; ++i ){
      dFree(dns_cache[i].hostname);
      for ( j = 0; j < dList_length(dns_cache[i].addr_list); ++j)
         dFree(dList_nth_data(dns_cache[i].addr_list, j));
      dList_free(dns_cache[i].addr_list);
   }
   a_IOwatch_remove_fd(dns_notify_pipe[0], DIO_READ);
   dClose(dns_notify_pipe[0]);
   dClose(dns_notify_pipe[1]);
   dFree(dns_cache);
}

/*
 *  Writes a string representation of the given DilloHost
 *  into dst. dst will be \0 terminated.
 *  Please note that dst must be at least 40 bytes long for IPv6
 *  addresses.
 */
void a_Dns_dillohost_to_string(DilloHost *host, char *dst, size_t size)
{
   if (!inet_ntop(host->af, host->data, dst, size)) {
      switch (errno) {
         case EAFNOSUPPORT:
            snprintf(dst, size, "Unknown address family");
            break;
         case ENOSPC:
            snprintf(dst, size, "Buffer too small");
            break;
      }
   }
}
