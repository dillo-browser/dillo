/*
 * Dillo web browser
 *
 * Copyright 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
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
#include "commit.h"

#include "djpeg.h"
#include "dpng.h"
#include "dwebp.h"
#include "IO/tls.h"

#include <FL/Fl.H>
#include <zlib.h>

#ifdef ENABLE_BROTLI
#include <brotli/decode.h>
#endif

#include <stdio.h>

static void print_libs()
{
   char buf[256];

   printf("Libraries:");

   /* FLTK only offers a single number */
   {
#if FL_MAJOR_VERSION == 1 && FL_MINOR_VERSION == 3 && FL_PATCH_VERSION <= 3
      /* The version comes in a double like this 1.0302 (1.3.3), so we
       * transform it to a integer as Fl::api_version(): 1.0303 -> 10303 */
      int fltkver = (int) (Fl::version() * 10000.0);
#else
      int fltkver = Fl::api_version();
#endif
      int fltk_maj = fltkver / 10000;
      int fltk_min = (fltkver / 100) % 100;
      int fltk_pat = fltkver % 100;
      printf(" fltk/%d.%d.%d", fltk_maj, fltk_min, fltk_pat);
   }

#ifdef ENABLE_JPEG
   printf(" jpeg/%s", a_Jpeg_version());
#endif

#ifdef ENABLE_PNG
   printf(" png/%s", a_Png_version());
#endif

#ifdef ENABLE_WEBP
   printf(" webp/%s", a_Webp_version(buf, 256));
#endif

   printf(" zlib/%s", zlibVersion());

#ifdef ENABLE_BROTLI
   /* From brotli:
    *
    *   Compose 3 components into a single number. In a hexadecimal
    *   representation B and C components occupy exactly 3 digits:
    *
    *   #define BROTLI_MAKE_HEX_VERSION(A, B, C) ((A << 24) | (B << 12) | C)
    */
   {
      /* Decode the version into each component */
      uint32_t br_ver = BrotliDecoderVersion();
      int br_maj = (br_ver >> 24) & 0x7ff;
      int br_min = (br_ver >> 12) & 0x7ff;
      int br_pat = (br_ver >>  0) & 0x7ff;

      printf(" brotli/%d.%d.%d", br_maj, br_min, br_pat);
   }
#endif


#ifdef ENABLE_TLS
   /* TLS prints the name/version format, as it determines which SSL
    * library is in use */
   printf(" %s", a_Tls_version(buf, 256));
#endif

   printf("\n");
}

static void print_features()
{
   printf("Features:"
#ifdef ENABLE_GIF
         " +GIF"
#else
         " -GIF"
#endif
#ifdef ENABLE_JPEG
         " +JPEG"
#else
         " -JPEG"
#endif
#ifdef ENABLE_PNG
         " +PNG"
#else
         " -PNG"
#endif
#ifdef ENABLE_SVG
         " +SVG"
#else
         " -SVG"
#endif
#ifdef ENABLE_WEBP
         " +WEBP"
#else
         " -WEBP"
#endif
#ifdef ENABLE_BROTLI
         " +BROTLI"
#else
         " -BROTLI"
#endif
#if !( defined(DISABLE_XEMBED) || defined(WIN32) || defined(__APPLE__) )
         " +XEMBED"
#else
         " -XEMBED"
#endif
#ifdef ENABLE_TLS
         " +TLS"
#else
         " -TLS"
#endif
#ifdef ENABLE_IPV6
         " +IPV6"
#else
         " -IPV6"
#endif
         "\n");
}

void a_Version_print_info(void)
{
   const char *version = "v" VERSION;

#ifdef GIT_COMMIT
   version = GIT_COMMIT;
#endif

   printf("Dillo %s\n", version);
   print_libs();
   print_features();
}
