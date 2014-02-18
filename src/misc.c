/*
 * File: misc.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "utf8.hh"
#include "msg.h"
#include "misc.h"

/*
 * Escape characters as %XX sequences.
 * Return value: New string.
 */
char *a_Misc_escape_chars(const char *str, const char *esc_set)
{
   static const char *const hex = "0123456789ABCDEF";
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
int
a_Misc_expand_tabs(char **start, char *end, char *buf, int buflen)
{
   int j, pos = 0, written = 0, old_pos, char_len;
   uint_t code;
   static const int combining_char_space = 32;

   while (*start < end && written < buflen - TAB_SIZE - combining_char_space) {
      code = a_Utf8_decode(*start, end, &char_len);

      if (code == '\t') {
         /* Fill with whitespaces until the next tab. */
         old_pos = pos;
         pos += TAB_SIZE - (pos % TAB_SIZE);
         for (j = old_pos; j < pos; j++)
            buf[written++] = ' ';
      } else {
         assert(char_len <= 4);
         for (j = 0; j < char_len; j++)
            buf[written++] = (*start)[j];
         pos++;
      }

      *start += char_len;
   }

   /* If following chars are combining chars (e.g. accents) add them to the
    * buffer. We have reserved combining_char_space bytes for this.
    * If there should be more combining chars, we split nevertheless.
    */
   while (*start < end && written < buflen - 4) {
      code = a_Utf8_decode(*start, end, &char_len);

      if (! a_Utf8_combining_char(code))
         break;

      assert(char_len <= 4);
      for (j = 0; j < char_len; j++)
         buf[written++] = (*start)[j];

      *start += char_len;
   }

   return written;
}

/* TODO: could use dStr ADT! */
typedef struct {
   const char *str;
   int len;
} ContentType_t;

static const ContentType_t MimeTypes[] = {
   { "application/octet-stream", 24 },
   { "application/xhtml+xml", 21 },
   { "text/html", 9 },
   { "text/plain", 10 },
   { "image/gif", 9 },
   { "image/png", 9 },
   { "image/jpeg", 10 },
   { NULL, 0 }
};

typedef enum {
   DT_OCTET_STREAM = 0,
   DT_PLACEHOLDER,
   DT_TEXT_HTML,
   DT_TEXT_PLAIN,
   DT_IMAGE_GIF,
   DT_IMAGE_PNG,
   DT_IMAGE_JPG,
} DetectedContentType;

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
   size_t i, non_ascci, non_ascci_text, bin_chars;
   char *p = Data;
   int st = 1;      /* default to "doubt' */
   DetectedContentType Type = DT_OCTET_STREAM; /* default to binary */

   /* HTML try */
   for (i = 0; i < Size && dIsspace(p[i]); ++i);
   if ((Size - i >= 5  && !dStrnAsciiCasecmp(p+i, "<html", 5)) ||
       (Size - i >= 5  && !dStrnAsciiCasecmp(p+i, "<head", 5)) ||
       (Size - i >= 6  && !dStrnAsciiCasecmp(p+i, "<title", 6)) ||
       (Size - i >= 14 && !dStrnAsciiCasecmp(p+i, "<!doctype html", 14)) ||
       /* this line is workaround for FTP through the Squid proxy */
       (Size - i >= 17 && !dStrnAsciiCasecmp(p+i, "<!-- HTML listing", 17))) {

      Type = DT_TEXT_HTML;
      st = 0;
   /* Images */
   } else if (Size >= 4 && !strncmp(p, "GIF8", 4)) {
      Type = DT_IMAGE_GIF;
      st = 0;
   } else if (Size >= 4 && !strncmp(p, "\x89PNG", 4)) {
      Type = DT_IMAGE_PNG;
      st = 0;
   } else if (Size >= 2 && !strncmp(p, "\xff\xd8", 2)) {
      /* JPEG has the first 2 bytes set to 0xffd8 in BigEndian - looking
       * at the character representation should be machine independent. */
      Type = DT_IMAGE_JPG;
      st = 0;

   /* Text */
   } else {
      /* Heuristic for "text/plain"
       * {ASCII, LATIN1, UTF8, KOI8-R, CP-1251}
       * All in the above set regard [00-31] as control characters.
       * LATIN1: [7F-9F] unused
       * CP-1251 {7F,98} unused (two characters).
       *
       * We'll use [0-31] as indicators of non-text content.
       * Better heuristics are welcomed! :-) */

      non_ascci = non_ascci_text = bin_chars = 0;
      Size = MIN (Size, 256);
      for (i = 0; i < Size; i++) {
         int ch = (uchar_t) p[i];
         if (ch < 32 && !dIsspace(ch))
            ++bin_chars;
         if (ch > 126)
            ++non_ascci;
         if (ch > 190)
            ++non_ascci_text;
      }
      if (bin_chars == 0 && (non_ascci - non_ascci_text) <= Size/10) {
         /* Let's say text: if "rare" chars are <= 10% */
         Type = DT_TEXT_PLAIN;
      } else if (Size > 0) {
         /* a special check for UTF-8 */
         Size = a_Utf8_end_of_char(p, Size - 1) + 1;
         if (a_Utf8_test(p, Size) > 0)
            Type = DT_TEXT_PLAIN;
      }
      if (Size >= 256)
         st = 0;
   }

   *PT = MimeTypes[Type].str;
   return st;
}

/*
 * Parse Content-Type string, e.g., "text/html; charset=utf-8".
 * Content-Type is defined in RFC 2045 section 5.1.
 */
void a_Misc_parse_content_type(const char *type, char **major, char **minor,
                               char **charset)
{
   static const char tspecials_space[] = "()<>@,;:\\\"/[]?= ";
   const char *str, *s;

   if (major)
      *major = NULL;
   if (minor)
      *minor = NULL;
   if (charset)
      *charset = NULL;
   if (!(str = type))
      return;

   for (s = str; *s && isascii((uchar_t)*s) && !iscntrl((uchar_t)*s) &&
        !strchr(tspecials_space, *s); s++) ;
   if (major)
      *major = dStrndup(str, s - str);

   if (*s == '/') {
      for (str = ++s; *s && isascii((uchar_t)*s) && !iscntrl((uchar_t)*s) &&
           !strchr(tspecials_space, *s); s++) ;
      if (minor)
         *minor = dStrndup(str, s - str);
   }
   if (charset && *s &&
       (dStrnAsciiCasecmp(type, "text/", 5) == 0 ||
        dStrnAsciiCasecmp(type, "application/xhtml+xml", 21) == 0)) {
      /* "charset" parameter defined for text media type in RFC 2046,
       * application/xhtml+xml in RFC 3236.
       *
       * Note that RFC 3023 lists some main xml media types and provides
       * the convention of using the "+xml" minor type suffix for other
       * xml types, so it would be reasonable to check for that suffix if
       * we have need to care about various xml types someday.
       */
      const char terminators[] = " ;\t";
      const char key[] = "charset";

      if ((s = dStriAsciiStr(str, key)) &&
          (s == str || strchr(terminators, s[-1]))) {
         s += sizeof(key) - 1;
         for ( ; *s == ' ' || *s == '\t'; ++s);
         if (*s == '=') {
            size_t len;
            for (++s; *s == ' ' || *s == '\t'; ++s);
            if ((len = strcspn(s, terminators))) {
               if (*s == '"' && s[len-1] == '"' && len > 1) {
                 /* quoted string */
                 s++;
                 len -= 2;
               }
               *charset = dStrndup(s, len);
            }
         }
      }
   }
}

/*
 * Compare two Content-Type strings.
 * Return 0 if they are equivalent, and 1 otherwise.
 */
int a_Misc_content_type_cmp(const char *ct1, const char *ct2)
{
   char *major1, *major2, *minor1, *minor2, *charset1, *charset2;
   int ret;

   if ((!ct1 || !*ct1) && (!ct2 || !*ct2))
      return 0;
   if ((!ct1 || !*ct1) || (!ct2 || !*ct2))
      return 1;

   a_Misc_parse_content_type(ct1, &major1, &minor1, &charset1);
   a_Misc_parse_content_type(ct2, &major2, &minor2, &charset2);

   if (major1 && major2 && !dStrAsciiCasecmp(major1, major2) &&
       minor1 && minor2 && !dStrAsciiCasecmp(minor1, minor2) &&
       ((!charset1 && !charset2) ||
        (charset1 && charset2 && !dStrAsciiCasecmp(charset1, charset2)) ||
        (!charset1 && charset2 && !dStrAsciiCasecmp(charset2, "UTF-8")) ||
        (charset1 && !charset2 && !dStrAsciiCasecmp(charset1, "UTF-8")))) {
      ret = 0;
   } else {
      ret = 1;
   }
   dFree(major1); dFree(major2);
   dFree(minor1); dFree(minor2);
   dFree(charset1); dFree(charset2);

   return ret;
}

/*
 * Check the server-supplied 'Content-Type' against our detected type.
 * (some servers seem to default to "text/plain").
 *
 * Return value:
 *  0,  if they match
 *  -1, if a mismatch is detected
 *
 * There are many MIME types Dillo doesn't know, they're handled
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
      if (dStrnAsciiCasecmp(EntryType, MimeTypes[i].str, MimeTypes[i].len) ==0)
         break;

   if (!MimeTypes[i].str) {
      /* type not found, no mismatch */
      st = 0;
   } else if (dStrnAsciiCasecmp(EntryType, "image/", 6) == 0 &&
             !dStrnAsciiCasecmp(DetectedType, MimeTypes[i].str,
                                MimeTypes[i].len)){
      /* An image, and there's an exact match */
      st = 0;
   } else if (dStrnAsciiCasecmp(EntryType, "text/", 5) ||
              dStrnAsciiCasecmp(DetectedType, "application/", 12)) {
      /* Not an application sent as text */
      st = 0;
   } else if (dStrnAsciiCasecmp(EntryType, "application/xhtml+xml", 21) &&
              dStrnAsciiCasecmp(DetectedType, "text/html", 9)) {
      /* XML version of HTML */
      st = 0;
   }
   _MSG("Type check: %s\n", st == 0 ? "MATCH" : "MISMATCH");

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
 * Parse dillorc's search_url string ("[<label> ]<url>")
 * Return value: -1 on error, 0 on success (and label and urlstr pointers)
 */
int a_Misc_parse_search_url(char *source, char **label, char **urlstr)
{
   static char buf[32];
   char *p, *q;
   int ret = -1;

   if ((p = strrchr(source, ' '))) {
      /* label and url pair */
      strncpy(buf,source,MIN(p-source,31));
      buf[MIN(p-source,31)] = 0;
      source = p+1;
      if ((p = strchr(source, '/')) && p[1] && (q = strchr(p+2,'/'))) {
         *urlstr = source;
         ret = 0;
      }
   } else {
      /* url only, make a custom label */
      if ((p = strchr(source, '/')) && p[1] && (q = strchr(p+2,'/'))) {
         strncpy(buf,p+2,MIN(q-p-2,31));
         buf[MIN(q-p-2,31)] = 0;
         *urlstr = source;
         ret = 0;
      }
   }
   *label = buf;
   if (ret == -1)
      MSG("Invalid search_url: \"%s\"\n", source);
   return ret;
}

/*
 * Encodes string using base64 encoding.
 * Return value: new string or NULL if input string is empty.
 */
char *a_Misc_encode_base64(const char *in)
{
   static const char *const base64_hex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
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

/*
 * Load a local file into a dStr.
 * Return value: dStr on success, NULL on error.
 * TODO: a filesize threshold may be implemented.
 */
Dstr *a_Misc_file2dstr(const char *filename)
{
   FILE *F_in;
   int n;
   char buf[4096];
   Dstr *dstr = NULL;

   if ((F_in = fopen(filename, "r"))) {
      dstr = dStr_sized_new(4096);
      while ((n = fread (buf, 1, 4096, F_in)) > 0) {
         dStr_append_l(dstr, buf, n);
      }
      fclose(F_in);
   }
   return dstr;
}
