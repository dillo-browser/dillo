/*
 * File: utf8.c
 *
 * Copyright (C) 2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <FL/fl_utf8.h>

#include "../dlib/dlib.h"    /* TRUE/FALSE */
#include "utf8.hh"

// C++ functions with C linkage ----------------------------------------------

/*
 * Return index of the last byte of the UTF-8-encoded character that str + i
 * points to or into.
 */
uint_t a_Utf8_end_of_char(const char *str, uint_t i)
{
   /* We can almost get what we want from utf8fwd(p+1,...)-1, but that
    * does not work for the last character in a string, and the fn makes some
    * assumptions that do not suit us.
    * Here's something very simpleminded instead:
    */
   if (str && *str && (str[i] & 0x80)) {
      int internal_bytes = (str[i] & 0x40) ? 0 : 1;

      while (((str[i + 1] & 0xc0) == 0x80) && (++internal_bytes < 4))
         i++;
   }
   return i;
}

/*
 * Decode a single UTF-8-encoded character starting at p.
 * The resulting Unicode value (in the range 0-0x10ffff) is returned,
 * and len is set to the number of bytes in the UTF-8 encoding.
 * Note that utf8decode(), if given non-UTF-8 data, will interpret
 * it as ISO-8859-1 or CP1252 if possible.
 */
uint_t a_Utf8_decode(const char* str, const char* end, int* len)
{
   return fl_utf8decode(str, end, len);
}

/*
 * Write UTF-8 encoding of ucs into buf and return number of bytes written.
 */
int a_Utf8_encode(unsigned int ucs, char *buf)
{
   return fl_utf8encode(ucs, buf);
}

/*
 * Examine first srclen bytes of src.
 * Return 0 if not legal UTF-8, 1 if all ASCII, 2 if all below 0x800,
 * 3 if all below 0x10000, and 4 otherwise.
 */
int a_Utf8_test(const char* src, unsigned int srclen)
{
   return fl_utf8test(src, srclen);
}

/*
 * Does s point to a UTF-8-encoded ideographic character?
 *
 * This is based on http://unicode.org/reports/tr14/#ID plus some guesses
 * for what might make the most sense for Dillo. Surprisingly, they include
 * Hangul Compatibility Jamo, but they're the experts, so I'll follow along.
 */
bool_t a_Utf8_ideographic(const char *s, const char *end, int *len)
{
   bool_t ret = FALSE;

   if ((uchar_t)*s >= 0xe2) {
      /* Unicode char >= U+2000. */
      unsigned unicode = a_Utf8_decode(s, end, len);

      if (unicode >= 0x2e80 &&
           ((unicode <= 0xa4cf) ||
            (unicode >= 0xf900 && unicode <= 0xfaff) ||
            (unicode >= 0xff00 && unicode <= 0xff9f))) {
         ret = TRUE;
     }
   } else {
      *len = 1 + (int)a_Utf8_end_of_char(s, 0);
   }
   return ret;
}

bool_t a_Utf8_combining_char(int unicode)
{
   return ((unicode >= 0x0300 && unicode <= 0x036f) ||
           (unicode >= 0x1dc0 && unicode <= 0x1dff) ||
           (unicode >= 0x20d0 && unicode <= 0x20ff) ||
           (unicode >= 0xfe20 && unicode <= 0xfe2f));
}

int a_Utf8_char_count(const char *str, int len)
{
   return fl_utf_nb_char((const uchar_t*)str, len);
}
