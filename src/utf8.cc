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
