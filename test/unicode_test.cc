#include <string.h>
#include <stdio.h>
#include <FL/fl_utf8.h>
#include "../lout/unicode.hh"

using namespace lout::unicode;

int main (int argc, char *argv[])
{
   // 0-terminated string
   const char *t1 = "abcäöüабв−‐";

   // not 0-terminated; copy from 0-terminated
   int t2len = strlen (t1);
   char t2[t2len];
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
   for (const char *s = t2; *s;
        s - t2 < t2len && (s = fl_utf8fwd (s + 1, t2, t2 + t2len)))
      printf ("%3d -> U+%04x\n", (int)(s - t2),
              decodeUtf8(s, t2len - (s - t2)));

   return 0;
}
