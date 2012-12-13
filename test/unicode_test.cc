#include <string.h>
#include <stdio.h>
#include "../lout/unicode.hh"

using namespace lout::unicode;

int main (int argc, char *argv[])
{
   const char *t = "abcäöüабв−‐";

   for (const char *s = t; s; s = nextUtf8Char (s, strlen (s)))
      printf ("%3d -> U+%04x ('%s')\n", (int)(s - t), decodeUtf8(s), s);
   
   return 0;
}
