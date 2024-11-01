/*
 * Dillo web browser
 *
 * Copyright 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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

#include "config.h"

#ifdef ENABLE_TLS
# ifdef HAVE_OPENSSL
#  include <openssl/crypto.h>    /* OpenSSL_version */
# elif defined(HAVE_MBEDTLS)
#  include <mbedtls/version.h>
# endif
#endif

#ifdef ENABLE_PNG
# ifdef HAVE_LIBPNG_PNG_H
#  include <libpng/png.h>
# else
#  include <png.h>
# endif
#endif

#include <FL/Fl.H>
#include <zlib.h>

#include <stdio.h>

static void print_libs()
{

   printf("Libraries:");

   /* FLTK only offers a single number */
   {
      int fltkver = Fl::api_version();
      int fltk_maj = fltkver / 10000;
      int fltk_min = (fltkver / 100) % 100;
      int fltk_pat = fltkver % 100;
      printf(" fltk/%d.%d.%d", fltk_maj, fltk_min, fltk_pat);
   }

   printf(" zlib/%s", zlibVersion());

#ifdef ENABLE_PNG
   printf(" png/%s", png_get_libpng_ver(NULL));
#endif

#ifdef HAVE_OPENSSL
   {
      /* Ugly hack to replace "OpenSSL 3.4.0 22 Oct 2024" with
       * "OpenSSL/3.4.0". */
      char *ossl = strdup(OpenSSL_version(OPENSSL_VERSION));
      char *sp1 = strchr(ossl, ' ');
      if (sp1) {
         *sp1 = '/';
         char *sp2 = strchr(ossl, ' ');
         if (sp2) {
            *sp2 = '\0';
         }
      }
      printf(" %s", ossl);
   }
#elif defined(HAVE_MBEDTLS)
   {
      char version[128];
      mbedtls_version_get_string_full(version);
      char *sp1 = strchr(version, ' ');
      if (sp1) {
         *sp1 = '_';
         char *sp2 = strchr(sp1, ' ');
         if (sp2) {
            *sp2 = '/';
         }
      }
      printf(" %s", version);
   }
#endif

   printf("\n");
}

static void print_features()
{
   printf("Features:"
#ifdef ENABLE_GIF
         " GIF"
#endif
#ifdef ENABLE_JPEG
         " JPEG"
#endif
#ifdef ENABLE_PNG
         " PNG"
#endif
#ifdef ENABLE_SVG
         " SVG"
#endif
#if !( defined(DISABLE_XEMBED) || defined(WIN32) || defined(__APPLE__) )
         " XEMBED"
#endif
#ifdef ENABLE_TLS
         " TLS"
#endif
         "\n");
}

void version_print_info(void)
{
   printf("Dillo version " VERSION "\n");
   print_libs();
   print_features();
}
