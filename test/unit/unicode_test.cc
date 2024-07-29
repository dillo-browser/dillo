/*
 * File: unicode_test.cc
 *
 * Copyright 2012 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2023 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>
#include <FL/fl_utf8.h>
#include "lout/unicode.hh"

using namespace lout::unicode;

int main (int argc, char *argv[])
{
   // 0-terminated string
   const char *t1 = "abcäöüабв−‐";

   // not 0-terminated; copy from 0-terminated
   int t2len = strlen (t1);
   char *t2 = new char[t2len];
   for (int i = 0; i < t2len; i++)
      t2[i] = t1[i];

   puts ("===== misc::unicode, 0-terminated =====");
   for (const char *s = t1; s; s = nextUtf8Char (s))
      printf ("%3d -> U+%04x ('%s')\n", (int)(s - t1), decodeUtf8(s), s);

   puts ("===== Fltk, 0-terminated =====");
   for (const char *s = t1; *s; s = fl_utf8fwd (s + 1, t1, t1 + strlen (t1)))
      printf ("%3d -> U+%04x ('%s')\n", (int)(s - t1), decodeUtf8(s), s);

   puts ("===== misc::unicode, not 0-terminated =====");
   for (const char *s = t2; s; s = nextUtf8Char (s, t2len - (s - t2)))
      printf ("%3d -> U+%04x\n", (int)(s - t2),
              decodeUtf8(s, t2len - (s - t2)));

   puts ("===== Fltk, not 0-terminated =====");
   for (const char *s = t2; s - t2 < t2len; s = fl_utf8fwd (s + 1, t2, t2 + t2len))
      printf ("%3d -> U+%04x\n", (int)(s - t2),
              decodeUtf8(s, t2len - (s - t2)));

   delete[] t2;

   return 0;
}
