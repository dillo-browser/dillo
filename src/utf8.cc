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

#include <fltk/utf.h>

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
 * Write UTF-8 encoding of ucs into buf and return number of bytes written.
 */
int a_Utf8_encode(unsigned int ucs, char *buf)
{
   return utf8encode(ucs, buf);
}

/*
 * Examine first srclen bytes of src.
 * Return 0 if not legal UTF-8, 1 if all ASCII, 2 if all below 0x800,
 * 3 if all below 0x10000, and 4 otherwise.
 */
int a_Utf8_test(const char* src, unsigned int srclen)
{
   return utf8test(src, srclen);
}
