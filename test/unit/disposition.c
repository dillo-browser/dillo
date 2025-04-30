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
   { "attachment; filename=/foo", "attachment", "_foo" },
   { "attachment; filename=./../foo", "attachment", "_.._foo" },
   { "attachment; filename=", "attachment", NULL },
   { "attachment; filename=\"foo", "attachment", NULL },
   { "attachment; filename= ", "attachment", NULL },
   { "attachment; filename=\"", "attachment", NULL },
   { "attachment; filename=\"\"", "attachment", NULL },
   { "attachment; filename=\"a\"", "attachment", "a" },
   { "attachment; filename=\"foo", "attachment", NULL },
   { "attachment; filename=~/foo", "attachment", "__foo" },

   /* Note that due to the rules for implied linear whitespace (Section 2.1 of
    * [RFC2616]), OPTIONAL whitespace can appear between words (token or
    * quoted-string) and separator characters. */
   { "attachment ; filename=foo", "attachment", "foo" },
   { "attachment; filename =foo", "attachment", "foo" },
   { "attachment; filename= foo", "attachment", "foo" },
   { "attachment; filename = foo", "attachment", "foo" },
   { "attachment ; filename = foo", "attachment", "foo" },

   /* See http://test.greenbytes.de/tech/tc2231/ */
   { "inline", "inline", NULL },
   { "\"inline\"", NULL, NULL },
   { "inline; filename=\"foo.html\"", "inline", "foo.html" },
   { "inline; filename=\"Not an attachment!\"", "inline", "Not an attachment!" },
   { "inline; filename=\"foo.pdf\"", "inline", "foo.pdf" },
   { "\"attachment\"", NULL, NULL },
   { "attachment", "attachment", NULL },
   { "ATTACHMENT", "ATTACHMENT", NULL },
   { "attachment; filename=\"foo.html\"", "attachment", "foo.html" },
   { "attachment; filename=\"0000000000111111111122222\"", "attachment", "0000000000111111111122222" },
   { "attachment; filename=\"00000000001111111111222222222233333\"", "attachment", "00000000001111111111222222222233333" },
   { "attachment; filename=\"f\\oo.html\"", "attachment", "f_oo.html" },
   { "attachment; filename=\"\\\"quoting\\\" tested.html\"", "attachment", "\"quoting\" tested.html" },
   { "attachment; filename=\"Here's a semicolon;.html\"", "attachment", "Here's a semicolon;.html" },
   { "attachment; foo=\"bar\"; filename=\"foo.html\"", "attachment", "foo.html" },
   { "attachment; foo=\"\\\"\\\\\";filename=\"foo.html\"", "attachment", "foo.html" },
   { "attachment; FILENAME=\"foo.html\"", "attachment", "foo.html" },
   { "attachment; filename=foo.html", "attachment", "foo.html" },
   { "attachment; filename=foo,bar.html", "attachment", "foo,bar.html" },
   { "attachment; filename=foo.html ;", "attachment", "foo.html" },
   { "attachment; ;filename=foo", "attachment", "foo" },
   { "attachment; filename=foo bar.html", "attachment", "foo" },
   { "attachment; filename='foo.bar'", "attachment", "'foo.bar'" },
   { "attachment; filename=\"foo-ä.html\"", "attachment", "foo-ä.html" },
   { "attachment; filename=\"foo-Ã¤.html\"", "attachment", "foo-Ã¤.html" },
   { "attachment; filename=\"foo-%41.html\"", "attachment", "foo-%41.html" },
   { "attachment; filename=\"50%.html\"", "attachment", "50%.html" },
   { "attachment; filename=\"foo-%\\41.html\"", "attachment", "foo-%_41.html" },
   { "attachment; name=\"foo-%41.html\"", "attachment", NULL },
   { "attachment; filename=\"ä-%41.html\"", "attachment", "ä-%41.html" },
   { "attachment; filename=\"foo-%c3%a4-%e2%82%ac.html\"", "attachment", "foo-%c3%a4-%e2%82%ac.html" },
   { "attachment; filename =\"foo.html\"", "attachment", "foo.html" },
   { "attachment; filename=\"foo.html\"; filename=\"bar.html\"", "attachment", "foo.html" },
   { "attachment; filename=foo[1](2).html", "attachment", "foo[1](2).html" },
   { "attachment; filename=foo-ä.html", "attachment", NULL },
   { "attachment; filename=filename=foo-Ã¤.html", "attachment", NULL },
   { "filename=foo.html", NULL, NULL },
   { "x=y; filename=foo.html", NULL, NULL },
   {"\"foo; filename=bar;baz\"; filename=qux", NULL, NULL },
   { "filename=foo.html, filename=bar.html", NULL, NULL },
   { "; filename=foo.html", NULL, NULL },
   { ": inline; attachment; filename=foo.html", NULL, NULL },
   { "inline; attachment; filename=foo.html", "inline", "foo.html" },
   { "attachment; inline; filename=foo.html", "attachment", "foo.html" },
   { "attachment; filename=\"foo.html\".txt", "attachment", "foo.html" },
   { "attachment; filename=\"bar", "attachment", NULL },
   { "attachment; filename=foo\"bar;baz\"qux", "attachment", "foo\"bar" },
   { "attachment; filename=foo.html, attachment; filename=bar.html", "attachment", "foo.html," },
   { "attachment; foo=foo filename=bar", "attachment", "bar" },
   { "attachment; filename=bar foo=foo ", "attachment", "bar" },
   { "attachment filename=bar", "attachment", NULL },
   { "filename=foo.html; attachment", NULL, NULL },
   { "attachment; xfilename=foo.html", "attachment", NULL },
   { "attachment; filename=\"/foo.html\"", "attachment", "_foo.html" },
   { "attachment; filename=\"\\\\foo.html\"", "attachment", "__foo.html" },
   { "foobar", "foobar", NULL },
   { "attachment; example=\"filename=example.txt\"", "attachment", NULL },
   { "attachment; filename=\"foo-ae.html\"; filename*=UTF-8''foo-%c3%a4.html", "attachment", "foo-ae.html" },
   { "attachment; filename==?ISO-8859-1?Q?foo-=E4.html?=", "attachment", NULL },
   { "attachment; filename=\"=?ISO-8859-1?Q?foo-=E4.html?=\"", "attachment", "=?ISO-8859-1?Q?foo-=E4.html?=" },
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
