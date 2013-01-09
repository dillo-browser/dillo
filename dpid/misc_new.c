/*
 * File: misc_new.c
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <errno.h>      /* errno, err-codes */
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>   /* stat */
#include <stdlib.h>     /* rand, srand */

#include "../dlib/dlib.h"
#include "dpid_common.h"
#include "misc_new.h"   /* for function prototypes */

/*! Reads a dpi tag from a socket
 * \li Continues after a signal interrupt
 * \Return
 * Dstr pointer to tag on success, NULL on failure
 * \important Caller is responsible for freeing the returned Dstr *
 */
Dstr *a_Misc_rdtag(int socket)
{
   char c = '\0';
   ssize_t rdlen;
   Dstr *tag;

   tag = dStr_sized_new(64);

   errno = 0;

   do {
      rdlen = read(socket, &c, 1);
      if (rdlen == -1 && errno != EINTR)
         break;
      dStr_append_c(tag, c);
   } while (c != '>');

   if (rdlen == -1) {
      perror("a_Misc_rdtag");
      dStr_free(tag, TRUE);
      return (NULL);
   }
   return (tag);
}

/*!
 * Read a dpi tag from sock
 * \return
 * pointer to dynamically allocated request tag
 */
char *a_Misc_readtag(int sock)
{
   char *tag, c;
   size_t i;
   size_t taglen = 0, tagmem = 10;
   ssize_t rdln = 1;

   tag = NULL;
   // new start
   tag = (char *) dMalloc(tagmem + 1);
   for (i = 0; (rdln = read(sock, &c, 1)) != 0; i++) {
      if (i == tagmem) {
         tagmem += tagmem;
         tag = (char *) dRealloc(tag, tagmem + 1);
      }
      tag[i] = c;
      taglen += rdln;
      if (c == '>') {
         tag[i + 1] = '\0';
         break;
      }
   }
   // new end
   if (rdln == -1) {
      ERRMSG("a_Misc_readtag", "read", errno);
   }

   return (tag);
}

/*! Reads a dpi tag from a socket without hanging on read.
 * \li Continues after a signal interrupt
 * \Return
 * \li 1 on success
 * \li 0 if input is not available within timeout microseconds.
 * \li -1 on failure
 * \important Caller is responsible for freeing the returned Dstr *
 */
/* Is this useful?
int a_Misc_nohang_rdtag(int socket, int timeout, Dstr **tag)
{
   int n_fd;
   fd_set sock_set, select_set;
   struct timeval tout;

   FD_ZERO(&sock_set);
   FD_SET(socket, &sock_set);

   errno = 0;
   do {
      select_set = sock_set;
      tout.tv_sec = 0;
      tout.tv_usec = timeout;
      n_fd = select(socket + 1, &select_set, NULL, NULL, &tout);
   } while (n_fd == -1 && errno == EINTR);

   if (n_fd == -1) {
      MSG_ERR("%s:%d: a_Misc_nohang_rdtag: %s\n",
              __FILE__, __LINE__, dStrerror(errno));
      return(-1);
   }
   if (n_fd == 0) {
      return(0);
   } else {
      *tag = a_Misc_rdtag(socket);
      return(1);
   }
}
*/

/*
 * Alternative to mkdtemp().
 * Not as strong as mkdtemp, but enough for creating a directory.
 */
char *a_Misc_mkdtemp(char *template)
{
   for (;;) {
      if (a_Misc_mkfname(template) && mkdir(template, 0700) == 0)
         break;
      if (errno == EEXIST)
         continue;
      return 0;
   }
   return template;
}

/*
 * Return a new, nonexistent file name from a template
 * (adapted from dietlibc; alternative to mkdtemp())
 */
char *a_Misc_mkfname(char *template)
{
   char *tmp = template + strlen(template) - 6;
   int i;
   uint_t random;
   struct stat stat_buf;

   if (tmp < template)
      goto error;
   for (i = 0; i < 6; ++i)
      if (tmp[i] != 'X') {
       error:
         errno = EINVAL;
         return 0;
      }
   srand((uint_t)(time(0) ^ getpid()));

   for (;;) {
      random = (unsigned) rand();
      for (i = 0; i < 6; ++i) {
         int hexdigit = (random >> (i * 5)) & 0x1f;

         tmp[i] = hexdigit > 9 ? hexdigit + 'a' - 10 : hexdigit + '0';
      }
      if (stat(template, &stat_buf) == -1 && errno == ENOENT)
         return template;

      MSG_ERR("a_Misc_mkfname: another round for %s \n", template);
   }
}

/*
 * Return a new, random hexadecimal string of 'nchar' characters.
 */
char *a_Misc_mksecret(int nchar)
{
   int i;
   uint_t random;
   char *secret = dNew(char, nchar + 1);

   srand((uint_t)(time(0) ^ getpid()));
   random = (unsigned) rand();
   for (i = 0; i < nchar; ++i) {
      int hexdigit = (random >> (i * 5)) & 0x0f;

      secret[i] = hexdigit > 9 ? hexdigit + 'a' - 10 : hexdigit + '0';
   }
   secret[i] = 0;
   MSG("a_Misc_mksecret: %s\n", secret);

   return secret;
}

