/*
 * File: dpip.c
 *
 * Copyright 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../dlib/dlib.h"
#include "dpip.h"
#include "d_size.h"

static char Quote = '\'';

/*
 * Basically the syntax of a dpip tag is:
 *
 *  "<"[*alpha] *(<name>"="Quote<escaped_value>Quote) " "Quote">"
 *
 *   1.- No space is allowed around the "=" sign between a name and its value.
 *   2.- The Quote character is not allowed in <name>.
 *   3.- Attribute values stuff Quote as QuoteQuote.
 *
 * e.g. (with ' as Quote):
 *
 *   <a='b' b='c' '>                 OK
 *   <dpi a='b i' b='12' '>          OK
 *   <a='>' '>                       OK
 *   <a='ain''t no doubt' '>         OK
 *   <a='ain''t b=''no'' b='' doubt' '>    OK
 *   <a = '>' '>                     Wrong
 *
 * Notes:
 *
 *   Restriction #1 is for easy finding of end of tag (EOT=Space+Quote+>).
 *   Restriction #2 can be removed, but what for? ;)
 *   The functions here provide for this functionality.
 */

typedef enum {
   SEEK_NAME,
   MATCH_NAME,
   SKIP_VALUE,
   SKIP_QUOTE,
   FOUND
} DpipTagParsingState;

/* ------------------------------------------------------------------------- */

/*
 * Printf like function for building dpip commands.
 * It takes care of dpip escaping of its arguments.
 * NOTE : It ONLY accepts string parameters, and
 *        only one %s per parameter.
 */
char *a_Dpip_build_cmd(const char *format, ...)
{
   va_list argp;
   char *p, *q, *s;
   Dstr *cmd;

   /* Don't allow Quote characters in attribute names */
   if (strchr(format, Quote))
      return NULL;

   cmd = dStr_sized_new(64);
   dStr_append_c(cmd, '<');
   va_start(argp, format);
   for (p = q = (char*)format; *q;  ) {
      p = strstr(q, "%s");
      if (!p) {
         dStr_append(cmd, q);
         break;
      } else {
         /* Copy format's part */
         while (q != p)
            dStr_append_c(cmd, *q++);
         q += 2;

         dStr_append_c(cmd, Quote);
         /* Stuff-copy of argument */
         s = va_arg (argp, char *);
         for (  ; *s; ++s) {
            dStr_append_c(cmd, *s);
            if (*s == Quote)
               dStr_append_c(cmd, *s);
         }
         dStr_append_c(cmd, Quote);
      }
   }
   va_end(argp);
   dStr_append_c(cmd, ' ');
   dStr_append_c(cmd, Quote);
   dStr_append_c(cmd, '>');

   p = cmd->str;
   dStr_free(cmd, FALSE);
   return p;
}

/*
 * Task: given a tag and an attribute name, return its value.
 *       (stuffing of ' is removed here)
 * Return value: the attribute value, or NULL if not present or malformed.
 */
char *a_Dpip_get_attr(char *tag, size_t tagsize, const char *attrname)
{
   uint_t i, n = 0, found = 0;
   char *p, *q, *start, *val = NULL;
   DpipTagParsingState state = SEEK_NAME;

   if (!attrname || !*attrname)
      return NULL;

   for (i = 1; i < tagsize && !found; ++i) {
      switch (state) {
      case SEEK_NAME:
         if (tag[i] == attrname[0] && (tag[i-1] == ' ' || tag[i-1] == '<')) {
            n = 1;
            state = MATCH_NAME;
         } else if (tag[i] == Quote && tag[i-1] == '=')
            state = SKIP_VALUE;
         break;
      case MATCH_NAME:
         if (tag[i] == attrname[n])
            ++n;
         else if (tag[i] == '=' && !attrname[n])
            state = FOUND;
         else
            state = SEEK_NAME;
         break;
      case SKIP_VALUE:
         if (tag[i] == Quote)
            state = (tag[i+1] == Quote) ? SKIP_QUOTE : SEEK_NAME;
         break;
      case SKIP_QUOTE:
         state = SKIP_VALUE;
         break;
      case FOUND:
         found = 1;
         break;
      }
   }

   if (found) {
      p = start = tag + i;
      while ((q = strchr(p, Quote)) && q[1] == Quote)
         p = q + 2;
      if (q && q[1] == ' ') {
         val = dStrndup(start, (uint_t)(q - start));
         for (p = q = val; (*q = *p); ++p, ++q)
            if (*p == Quote && p[1] == p[0])
               ++p;
      }
   }
   return val;
}

/* ------------------------------------------------------------------------- */

