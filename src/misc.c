/*
 * File: misc.c
 *
 * Copyright (C) 2000 Jorge Arellano Cid <jcid@dillo.org>,
 *                    Jörgen Viksell <vsksga@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "msg.h"
#include "misc.h"


/*
 * Escape characters as %XX sequences.
 * Return value: New string.
 */
char *a_Misc_escape_chars(const char *str, char *esc_set)
{
   static const char *hex = "0123456789ABCDEF";
   char *p = NULL;
   Dstr *dstr;
   int i;

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


#define TAB_SIZE 8
/*
 * Takes a string and converts any tabs to spaces.
 */
char *a_Misc_expand_tabs(const char *str)
{
   Dstr *New = dStr_new("");
   int len, i, j, pos, old_pos;
   char *val;

   if ((len = strlen(str))) {
      for (pos = 0, i = 0; i < len; i++) {
         if (str[i] == '\t') {
            /* Fill with whitespaces until the next tab. */
            old_pos = pos;
            pos += TAB_SIZE - (pos % TAB_SIZE);
            for (j = old_pos; j < pos; j++)
               dStr_append_c(New, ' ');
         } else {
            dStr_append_c(New, str[i]);
            pos++;
         }
      }
   }
   val = New->str;
   dStr_free(New, FALSE);
   return val;
}

/* TODO: could use dStr ADT! */
typedef struct ContentType_ {
   const char *str;
   int len;
} ContentType_t;

static const ContentType_t MimeTypes[] = {
   { "application/octet-stream", 24 },
   { "text/html", 9 },
   { "text/plain", 10 },
   { "image/gif", 9 },
   { "image/png", 9 },
   { "image/jpeg", 10 },
   { NULL, 0 }
};

/*
 * Detects 'Content-Type' from a data stream sample.
 *
 * It uses the magic(5) logic from file(1). Currently, it
 * only checks the few mime types that Dillo supports.
 *
 * 'Data' is a pointer to the first bytes of the raw data.
 *
 * Return value: (0 on success, 1 on doubt, 2 on lack of data).
 */
int a_Misc_get_content_type_from_data(void *Data, size_t Size, const char **PT)
{
   int st = 1;      /* default to "doubt' */
   int Type = 0;    /* default to "application/octet-stream" */
   char *p = Data;
   size_t i, non_ascci;

   /* HTML try */
   for (i = 0; i < Size && isspace(p[i]); ++i);
   if ((Size - i >= 5  && !dStrncasecmp(p+i, "<html", 5)) ||
       (Size - i >= 5  && !dStrncasecmp(p+i, "<head", 5)) ||
       (Size - i >= 6  && !dStrncasecmp(p+i, "<title", 6)) ||
       (Size - i >= 14 && !dStrncasecmp(p+i, "<!doctype html", 14)) ||
       /* this line is workaround for FTP through the Squid proxy */
       (Size - i >= 17 && !dStrncasecmp(p+i, "<!-- HTML listing", 17))) {

      Type = 1;
      st = 0;
   /* Images */
   } else if (Size >= 4 && !dStrncasecmp(p, "GIF8", 4)) {
      Type = 3;
      st = 0;
   } else if (Size >= 4 && !dStrncasecmp(p, "\x89PNG", 4)) {
      Type = 4;
      st = 0;
   } else if (Size >= 2 && !dStrncasecmp(p, "\xff\xd8", 2)) {
      /* JPEG has the first 2 bytes set to 0xffd8 in BigEndian - looking
       * at the character representation should be machine independent. */
      Type = 5;
      st = 0;

   /* Text */
   } else {
      /* We'll assume "text/plain" if the set of chars above 127 is <= 10
       * in a 256-bytes sample.  Better heuristics are welcomed! :-) */
      non_ascci = 0;
      Size = MIN (Size, 256);
      for (i = 0; i < Size; i++)
         if ((uchar_t) p[i] > 127)
            ++non_ascci;
      if (Size == 256) {
         Type = (non_ascci > 10) ? 0 : 2;
         st = 0;
      } else {
         Type = (non_ascci > 0) ? 0 : 2;
      }
   }

   *PT = MimeTypes[Type].str;
   return st;
}

/*
 * Check the server-supplied 'Content-Type' against our detected type.
 * (some servers seem to default to "text/plain").
 *
 * Return value:
 *  0,  if they match
 *  -1, if a mismatch is detected
 *
 * There're many MIME types Dillo doesn't know, they're handled
 * as "application/octet-stream" (as the SPEC says).
 *
 * A mismatch happens when receiving a binary stream as
 * "text/plain" or "text/html", or an image that's not an image of its kind.
 *
 * Note: this is a basic security procedure.
 *
 */
int a_Misc_content_type_check(const char *EntryType, const char *DetectedType)
{
   int i;
   int st = -1;

   _MSG("Type check:  [Srv: %s  Det: %s]\n", EntryType, DetectedType);

   if (!EntryType)
      return 0; /* there's no mismatch without server type */

   for (i = 1; MimeTypes[i].str; ++i)
      if (dStrncasecmp(EntryType, MimeTypes[i].str, MimeTypes[i].len) == 0)
         break;

   if (!MimeTypes[i].str) {
      /* type not found, no mismatch */
      st = 0;
   } else if (dStrncasecmp(EntryType, "image/", 6) == 0 &&
             !dStrncasecmp(DetectedType,MimeTypes[i].str,MimeTypes[i].len)){
      /* An image, and there's an exact match */
      st = 0;
   } else if (dStrncasecmp(EntryType, "text/", 5) ||
              dStrncasecmp(DetectedType, "application/", 12)) {
      /* Not an application sent as text */
      st = 0;
   }

   return st;
}

/*
 * Parse a geometry string.
 */
int a_Misc_parse_geometry(char *str, int *x, int *y, int *w, int *h)
{
   char *p, *t1, *t2;
   int n1, n2;
   int ret = 0;

   if ((p = strchr(str, 'x')) || (p = strchr(str, 'X'))) {
      n1 = strtol(str, &t1, 10);
      n2 = strtol(++p, &t2, 10);
      if (t1 != str && t2 != p) {
         *w = n1;
         *h = n2;
         ret = 1;
         /* parse x,y now */
         p = t2;
         n1 = strtol(p, &t1, 10);
         n2 = strtol(t1, &t2, 10);
         if (t1 != p && t2 != t1) {
            *x = n1;
            *y = n2;
         }
      }
   }
   _MSG("geom: w,h,x,y = (%d,%d,%d,%d)\n", *w, *h, *x, *y);
   return ret;
}

/*
 * Encodes string using base64 encoding.
 * Return value: new string or NULL if input string is empty.
 */
char *a_Misc_encode_base64(const char *in)
{
   static const char *base64_hex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "0123456789+/";
   char *out = NULL;
   int len, i = 0;

   if (in == NULL) return NULL;
   len = strlen(in);

   out = (char *)dMalloc((len + 2) / 3 * 4 + 1);

   for (; len >= 3; len -= 3) {
      out[i++] = base64_hex[in[0] >> 2];
      out[i++] = base64_hex[((in[0]<<4) & 0x30) | (in[1]>>4)];
      out[i++] = base64_hex[((in[1]<<2) & 0x3c) | (in[2]>>6)];
      out[i++] = base64_hex[in[2] & 0x3f];
      in += 3;
   }

   if (len > 0) {
      unsigned char fragment;
      out[i++] = base64_hex[in[0] >> 2];
      fragment = (in[0] << 4) & 0x30;
      if (len > 1) fragment |= in[1] >> 4;
      out[i++] = base64_hex[fragment];
      out[i++] = (len < 2) ? '=' : base64_hex[(in[1] << 2) & 0x3c];
      out[i++] = '=';
   }
   out[i] = '\0';
   return out;
}
