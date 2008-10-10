/*
 * Dillo web browser
 *
 * Copyright 1999-2007 Jorge Arellano Cid <jcid@dillo.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include <fltk/Window.h>
#include <fltk/TabGroup.h>
#include <fltk/run.h>

#include "msg.h"
#include "dir.h"
#include "uicmd.hh"

#include "prefs.h"
#include "bw.h"
#include "misc.h"
#include "nav.h"

#include "dns.h"
#include "web.hh"
#include "IO/Url.h"
#include "IO/mime.h"
#include "capi.h"
#include "dicache.h"
#include "cookies.h"


/*
 * Given a command line argument, build a DilloUrl for it.
 */
static DilloUrl *Dillo_make_start_url(char *str)
{
   char *url_str, *p;
   DilloUrl *start_url;
   int is_file = FALSE;

   /* Relative path to a local file? */
   p = (*str == '/') ? dStrdup(str) : dStrconcat(a_Dir_get_owd(),"/",str,NULL);

   if (access(p, F_OK) == 0) {
      /* absolute path may have non-URL characters */
      url_str = a_Misc_escape_chars(p, "% ");
      is_file = TRUE;
   } else {
      /* Not a file, filter URL string */
      url_str = a_Url_string_strip_delimiters(str);
   }
   dFree(p);

   if (is_file) {
      start_url = a_Url_new(url_str + 1, "file:/");
   } else {
      start_url = a_Url_new(url_str, NULL);
   }
   dFree(url_str);

   return start_url;
}

/*
 * MAIN
 */
int main(int argc, char **argv)
{
   srand((uint_t)(time(0) ^ getpid()));

   // Some OSes exit dillo without this (not GNU/Linux).
   signal(SIGPIPE, SIG_IGN);

   // Initialize internal modules
   a_Dir_init();
   a_Prefs_init();
   a_Dir_check_dillorc_directory(); /* and create if not present */
   a_Dpi_init();
   a_Dns_init();
   a_Web_init();
   a_Http_init();
   a_Mime_init();
   a_Capi_init();
   a_Dicache_init();
   a_Bw_init();
   a_Cookies_init();

   // Sets WM_CLASS hint on X11
   fltk::Window::xclass("dillo");

   // WORKAROUND: sometimes the default pager triggers redraw storms
   fltk::TabGroup::default_pager(fltk::PAGER_SHRINK);

   // Create a new UI/bw pair
   BrowserWindow *bw = a_UIcmd_browser_window_new(0, 0, NULL);

   if (prefs.http_proxyuser && !a_Http_proxy_auth()) {
      const char *passwd = a_UIcmd_get_passwd(prefs.http_proxyuser);
      if (passwd) {
         a_Http_set_proxy_passwd(passwd);
      } else {
         MSG_WARN("Not using proxy authentication.\n");
      }
   }

   if (argc == 2) {
      DilloUrl *url = Dillo_make_start_url(argv[1]);
      a_UIcmd_open_urlstr(bw, URL_STR(url));
      a_Url_free(url);
   } else if (argc == 6) {
      // WORKAROUND: sylpheed execs "dillo -l -f -x XID URL"
      if (strcmp(argv[1], "-l") == 0 && strcmp(argv[2], "-f") == 0 &&
          strcmp(argv[3], "-x") == 0) {
         a_UIcmd_set_images_enabled(bw, FALSE);
         DilloUrl *url = Dillo_make_start_url(argv[5]);
         a_Url_set_flags(url, URL_FLAGS(url) & URL_SpamSafe);
         a_UIcmd_open_urlstr(bw, URL_STR(url));
         a_Url_free(url);
      }
   } else {
      /* Send startup screen */
      a_Nav_push(bw, prefs.start_page);
   }

   return fltk::run();
}
