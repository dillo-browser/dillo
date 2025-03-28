/*
 * File: disposition.c
 *
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
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

#include "dlib/dlib.h"
#include "src/misc.h"

#include <stdlib.h>

struct testcase {
   const char *disposition;
   const char *type;
   const char *filename;
};

struct testcase cases[] = {
   /* See http://test.greenbytes.de/tech/tc2231/ */
   { "attachment; filename=\"\\\"quoting\\\" tested.html\"", "attachment", "_\"quoting_\" tested.html" },
   //{ "attachment; filename=\"\\\"quoting\\\" tested.html\"", "attachment", "quoting" }, /* <-- Should be this */
   { "attachment; filename=\"/foo.html\"", "attachment", "_foo.html" },
   { "attachment; filename=/foo", "attachment", "_foo" },
   { "attachment; filename=./../foo", "attachment", "_.._foo" },
   { "attachment; filename=", "attachment", NULL },
   { "attachment; filename= ", "attachment", NULL },
   { "attachment; filename=\"", "attachment", NULL },
   { "attachment; filename=\"foo", "attachment", NULL },
   { "attachment; filename=~/foo", "attachment", "~_foo" },
};

static int equal(const char *a, const char *b)
{
   if (a == NULL && b == NULL)
      return 1;

   if (a == NULL || b == NULL)
      return 0;

   return strcmp(a, b) == 0;
}

int main(void)
{
   char dummy[] = "dummy";
   int ncases = sizeof(cases) / sizeof(cases[0]);
   int rc = 0;

   for (int i = 0; i < ncases; i++) {
      struct testcase *t = &cases[i];
      char *type = dummy;
      char *filename = dummy;
      a_Misc_parse_content_disposition(t->disposition, &type, &filename);
      if (!equal(type, t->type)) {
         fprintf(stderr, "test %d failed, type mismatch:\n", i);
         fprintf(stderr, "  Content-Dispsition: %s\n", t->disposition);
         fprintf(stderr, "  Expected type: %s\n", t->type);
         fprintf(stderr, "  Computed type: %s\n", type);
         rc = 1;
      }

      if (!equal(filename, t->filename)) {
         fprintf(stderr, "test %d failed, filename mismatch:\n", i);
         fprintf(stderr, "  Content-Dispsition: %s\n", t->disposition);
         fprintf(stderr, "  Expected filename: %s\n", t->filename);
         fprintf(stderr, "  Computed filename: %s\n", filename);
         rc = 1;
      }

      dFree(type);
      dFree(filename);
   }

   return rc;
}
