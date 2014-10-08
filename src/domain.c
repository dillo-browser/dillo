/*
 * File: domain.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdlib.h>

#include "../dlib/dlib.h"
#include "msg.h"
#include "list.h"
#include "domain.h"

typedef struct {
   char *origin;
   char *destination;
} Rule;

static Rule *exceptions = NULL;
static int num_exceptions = 0;
static int num_exceptions_max = 1;

static bool_t default_deny = FALSE;

/*
 * Parse domainrc.
 */
void a_Domain_parse(FILE *fp)
{
   char *line;
   uint_t lineno = 0;

   _MSG("Reading domainrc...\n");

   while ((line = dGetline(fp)) != NULL) {
      ++lineno;

      /* Remove leading and trailing whitespace */
      dStrstrip(line);

      if (line[0] && line[0] != '#') {
         const char *delim = " \t";
         char *tok1 = strtok(line, delim);
         char *tok2 = strtok(NULL, delim);

         if (strtok(NULL, delim) != NULL) {
            MSG("Domain: Ignoring extraneous text at end of line %u.\n",
                lineno);
         }
         if (!tok2) {
            MSG("Domain: Not enough fields in line %u.\n", lineno);
         } else {
            if (dStrAsciiCasecmp(tok1, "default") == 0) {
               if (dStrAsciiCasecmp(tok2, "deny") == 0) {
                  default_deny = TRUE;
                  MSG("Domain: Default deny.\n");
               } else if (dStrAsciiCasecmp(tok2, "accept") == 0) {
                  default_deny = FALSE;
                  MSG("Domain: Default accept.\n");
               } else {
                  MSG("Domain: Default action \"%s\" not recognised.\n", tok2);
               }
            } else {
               a_List_add(exceptions, num_exceptions, num_exceptions_max);
               exceptions[num_exceptions].origin = dStrdup(tok1);
               exceptions[num_exceptions].destination = dStrdup(tok2);
               num_exceptions++;
               _MSG("Domain: Exception from %s to %s.\n", tok1, tok2);
            }
         }
      }
      dFree(line);
   }
}

void a_Domain_freeall(void)
{
   int i = 0;

   for (i = 0; i < num_exceptions; i++) {
      dFree(exceptions[i].origin);
      dFree(exceptions[i].destination);
   }
   dFree(exceptions);
}

/*
 * Wildcard ('*') pattern always matches.
 * "example.org" pattern matches "example.org".
 * ".example.org" pattern matches "example.org" and "sub.example.org".
 */
static bool_t Domain_match(const char *host, const char *pattern) {
   int cmp = strcmp(pattern, "*");

   if (cmp) {
      if (pattern[0] != '.')
         cmp = dStrAsciiCasecmp(host, pattern);
      else {
         int diff = strlen(host) - strlen(pattern);

         if (diff == -1)
            cmp = dStrAsciiCasecmp(host, pattern + 1);
         else if (diff >= 0)
            cmp = dStrAsciiCasecmp(host + diff, pattern);
      }
   }
   return cmp ? FALSE : TRUE;
}

/*
 * Is the resource at 'source' permitted to request the resource at 'dest'?
 */
bool_t a_Domain_permit(const DilloUrl *source, const DilloUrl *dest)
{
   int i;
   bool_t ret;
   const char *source_host, *dest_host;

   if (default_deny == FALSE && num_exceptions == 0)
      return TRUE;

   source_host = URL_HOST(source);
   dest_host = URL_HOST(dest);

   if (dest_host[0] == '\0') {
      ret = source_host[0] == '\0' ||
            !dStrAsciiCasecmp(URL_SCHEME(dest), "data");
      if (ret == FALSE)
         MSG("Domain: DENIED %s -> %s.\n", source_host, URL_STR(dest));
      return ret;
   }

   if (a_Url_same_organization(source, dest))
      return TRUE;

   ret = default_deny ? FALSE : TRUE;

   for (i = 0; i < num_exceptions; i++) {
      if (Domain_match(source_host, exceptions[i].origin) &&
          Domain_match(dest_host, exceptions[i].destination)) {
         ret = default_deny;
         _MSG("Domain: Matched rule from %s to %s.\n", exceptions[i].origin,
              exceptions[i].destination);
         break;
      }
   }

   if (ret == FALSE) {
      const char *src = source_host[0] ? source_host : URL_STR(source);

      MSG("Domain: DENIED %s -> %s.\n", src, dest_host);
   }
   return ret;
}
