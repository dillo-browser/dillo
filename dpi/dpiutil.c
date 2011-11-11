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
            *p = (isdigit(s[1]) ? (s[1] - '0')
                                : D_ASCII_TOUPPER(s[1]) - 'A' + 10) * 16;
            *p += isdigit(s[2]) ? (s[2] - '0')
                                : D_ASCII_TOUPPER(s[2]) - 'A' + 10;
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
            if (!dStrnAsciiCasecmp(str + i, unsafe_rep[k], unsafe_rep_len[k])) {
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
                    (D_ASCII_TOLOWER(url[i+2]) == 'a' ||
                     D_ASCII_TOLOWER(url[i+2]) == 'd')) {
            memmove(url + i, url + i + 3, strlen(url + i + 2));
            --i;
         }
      }
   }
   return url;
}

