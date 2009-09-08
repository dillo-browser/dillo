/*
 * File: dpiutil.c
 *
 * Copyright 2004-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>

#include "dpiutil.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[dpiutil.c]: " __VA_ARGS__)


/* Escaping/De-escaping ---------------------------------------------------*/

/*
 * Escape URI characters in 'esc_set' as %XX sequences.
 * Return value: New escaped string.
 */
char *Escape_uri_str(const char *str, const char *p_esc_set)
{
   static const char *esc_set, *hex = "0123456789ABCDEF";
   char *p;
   Dstr *dstr;
   int i;

   esc_set = (p_esc_set) ? p_esc_set : "%#:' ";
   dstr = dStr_sized_new(64);
   for (i = 0; str[i]; ++i) {
      if (str[i] <= 0x1F || str[i] == 0x7F || strchr(esc_set, str[i])) {
         dStr_append_c(dstr, '%');
         dStr_append_c(dstr, hex[(str[i] >> 4) & 15]);
         dStr_append_c(dstr, hex[str[i] & 15]);
      } else {
         dStr_append_c(dstr, str[i]);
      }
   }
   p = dstr->str;
   dStr_free(dstr, FALSE);

   return p;
}

/*
 * Unescape %XX sequences in a string.
 * Return value: a new unescaped string
 */
char *Unescape_uri_str(const char *s)
{
   char *p, *buf = dStrdup(s);

   if (strchr(s, '%')) {
      for (p = buf; (*p = *s); ++s, ++p) {
         if (*p == '%' && isxdigit(s[1]) && isxdigit(s[2])) {
            *p = (isdigit(s[1]) ? (s[1] - '0') : toupper(s[1]) - 'A' + 10)*16;
            *p += isdigit(s[2]) ? (s[2] - '0') : toupper(s[2]) - 'A' + 10;
            s += 2;
         }
      }
   }

   return buf;
}


static const char *unsafe_chars = "&<>\"'";
static const char *unsafe_rep[] =
  { "&amp;", "&lt;", "&gt;", "&quot;", "&#39;" };
static const int unsafe_rep_len[] =  { 5, 4, 4, 6, 5 };

/*
 * Escape unsafe characters as html entities.
 * Return value: New escaped string.
 */
char *Escape_html_str(const char *str)
{
   int i;
   char *p;
   Dstr *dstr = dStr_sized_new(64);

   for (i = 0; str[i]; ++i) {
      if ((p = strchr(unsafe_chars, str[i])))
         dStr_append(dstr, unsafe_rep[p - unsafe_chars]);
      else
         dStr_append_c(dstr, str[i]);
   }
   p = dstr->str;
   dStr_free(dstr, FALSE);

   return p;
}

/*
 * Unescape a few HTML entities (inverse of Escape_html_str)
 * Return value: New unescaped string.
 */
char *Unescape_html_str(const char *str)
{
   int i, j, k;
   char *u_str = dStrdup(str);

   if (!strchr(str, '&'))
      return u_str;

   for (i = 0, j = 0; str[i]; ++i) {
      if (str[i] == '&') {
         for (k = 0; k < 5; ++k) {
            if (!dStrncasecmp(str + i, unsafe_rep[k], unsafe_rep_len[k])) {
               i += unsafe_rep_len[k] - 1;
               break;
            }
         }
         u_str[j++] = (k < 5) ? unsafe_chars[k] : str[i];
      } else {
         u_str[j++] = str[i];
      }
   }
   u_str[j] = 0;

   return u_str;
}

/*
 * Filter '\n', '\r', "%0D" and "%0A" from the authority part of an FTP url.
 * This helps to avoid a SMTP relaying hack. This filtering could be done
 * only when port == 25, but if the mail server is listening on another
 * port it wouldn't work.
 * Note: AFAIS this should be done by wget.
 */
char *Filter_smtp_hack(char *url)
{
   int i;
   char c;

   if (strlen(url) > 6) { /* ftp:// */
      for (i = 6; (c = url[i]) && c != '/'; ++i) {
         if (c == '\n' || c == '\r') {
            memmove(url + i, url + i + 1, strlen(url + i));
            --i;
         } else if (c == '%' && url[i+1] == '0' &&
                    (tolower(url[i+2]) == 'a' || tolower(url[i+2]) == 'd')) {
            memmove(url + i, url + i + 3, strlen(url + i + 2));
            --i;
         }
      }
   }
   return url;
}


/* Streamed Sockets API (not mandatory)  ----------------------------------*/

/*
 * Create and initialize the SockHandler structure
 */
SockHandler *sock_handler_new(int fd_in, int fd_out, int flush_sz)
{
   SockHandler *sh = dNew(SockHandler, 1);

   /* init descriptors and streams */
   sh->fd_in  = fd_in;
   sh->fd_out = fd_out;
   sh->out = fdopen(fd_out, "w");

   /* init buffer */
   sh->buf_max = 8 * 1024;
   sh->buf = dNew(char, sh->buf_max);
   sh->buf_sz = 0;
   sh->flush_sz = flush_sz;

   return sh;
}

/*
 * Streamed write to socket
 * Return: 0 on success, 1 on error.
 */
int sock_handler_write(SockHandler *sh, int flush,
                       const char *Data, size_t DataSize)
{
   int retval = 1;

   /* append to buf */
   while (sh->buf_max < sh->buf_sz + DataSize) {
      sh->buf_max <<= 1;
      sh->buf = dRealloc(sh->buf, sh->buf_max);
   }
   memcpy(sh->buf + sh->buf_sz, Data, DataSize);
   sh->buf_sz += DataSize;
/*
   MSG("sh->buf=%p, sh->buf_sz=%d, sh->buf_max=%d, sh->flush_sz=%d\n",
       sh->buf, sh->buf_sz, sh->buf_max, sh->flush_sz);
*/
/**/
#if 0
{
   uint_t i;
   /* Test dpip's stream handling by chopping data into characters */
   for (i = 0; i < sh->buf_sz; ++i) {
      fputc(sh->buf[i], sh->out);
      fflush(sh->out);
      usleep(50);
   }
   if (i == sh->buf_sz) {
      sh->buf_sz = 0;
      retval = 0;
   }
}
#else
   /* flush data if necessary */
   if (flush || sh->buf_sz >= sh->flush_sz) {
      if (sh->buf_sz && fwrite (sh->buf, sh->buf_sz, 1, sh->out) != 1) {
         perror("[sock_handler_write]");
      } else {
         fflush(sh->out);
         sh->buf_sz = 0;
         retval = 0;
      }

   } else {
      retval = 0;
   }
#endif
   return retval;
}

/*
 * Convenience function.
 */
int sock_handler_write_str(SockHandler *sh, int flush, const char *str)
{
   return sock_handler_write(sh, flush, str, strlen(str));
}

/*
 * Return a newlly allocated string with the contents read from the socket.
 */
char *sock_handler_read(SockHandler *sh)
{
   ssize_t st;
   char buf[16384];

   /* can't use fread() */
   do
      st = read(sh->fd_in, buf, 16384);
   while (st < 0 && errno == EINTR);

   if (st == -1)
      perror("[sock_handler_read]");

   return (st > 0) ? dStrndup(buf, (uint_t)st) : NULL;
}

/*
 * Close this socket for reading and writing.
 */
void sock_handler_close(SockHandler *sh)
{
   /* flush before closing */
   sock_handler_write(sh, 1, "", 0);

   fclose(sh->out);
   close(sh->fd_out);
}

/*
 * Free the SockHandler structure
 */
void sock_handler_free(SockHandler *sh)
{
   dFree(sh->buf);
   dFree(sh);
}

/* ------------------------------------------------------------------------ */

