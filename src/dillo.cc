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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <locale.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include "msg.h"
#include "paths.hh"
#include "uicmd.hh"

#include "prefs.h"
#include "prefsparser.hh"
#include "keys.hh"
#include "bw.h"
#include "misc.h"
#include "history.h"

#include "dns.h"
#include "web.hh"
#include "IO/Url.h"
#include "IO/mime.h"
#include "capi.h"
#include "dicache.h"
#include "cookies.h"
#include "auth.h"

#include "dw/fltkcore.hh"

/*
 * Command line options structure
 */
typedef enum {
   DILLO_CLI_NONE          = 0,
   DILLO_CLI_XID           = 1 << 0,
   DILLO_CLI_FULLWINDOW    = 1 << 1,
   DILLO_CLI_HELP          = 1 << 2,
   DILLO_CLI_VERSION       = 1 << 3,
   DILLO_CLI_LOCAL         = 1 << 4,
   DILLO_CLI_GEOMETRY      = 1 << 5,
   DILLO_CLI_ERROR         = 1 << 15,
} OptID;

typedef struct {
   const char *shortopt;
   const char *longopt;
   int opt_argc; /* positive: mandatory, negative: optional */
   OptID id;
   const char *help;
} CLI_options;

static const CLI_options Options[] = {
   {"-f", "--fullwindow", 0, DILLO_CLI_FULLWINDOW,
    "  -f, --fullwindow       Start in full window mode: hide address bar,\n"
    "                         navigation buttons, menu, and status bar."},
   {"-g", "-geometry",    1, DILLO_CLI_GEOMETRY,
    "  -g, -geometry GEO      Set initial window position where GEO is\n"
    "                         WxH[{+-}X{+-}Y]"},
   {"-h", "--help",       0, DILLO_CLI_HELP,
    "  -h, --help             Display this help text and exit."},
   {"-l", "--local",      0, DILLO_CLI_LOCAL,
    "  -l, --local            Don't load images for these URL(s)."},
   {"-v", "--version",    0, DILLO_CLI_VERSION,
    "  -v, --version          Display version info and exit."},
   {"-x", "--xid",        1, DILLO_CLI_XID,
    "  -x, --xid XID          Open first Dillo window in an existing\n"
    "                         window whose window ID is XID."},
   {NULL, NULL, 0, DILLO_CLI_NONE, NULL}
};

/*
 * Print help text generated from the options structure
 */
static void printHelp(const char *cmdname, const CLI_options *options)
{
   printf("Usage: %s [OPTION]... [--] [URL|FILE]...\n"
          "Options:\n", cmdname);
   while (options && options->help) {
      printf("%s\n", options->help);
      options++;
   }
   printf("  URL                    URL to browse.\n"
          "  FILE                   Local FILE to view.\n"
          "\n");
}

/*
 * Return the maximum number of option arguments
 */
static int numOptions(const CLI_options *options)
{
   int i, max;

   for (i = 0, max = 0; options[i].shortopt; i++)
      if (abs(options[i].opt_argc) > max)
         max = abs(options[i].opt_argc);
   return max;
}

/*
 * Get next command line option.
 */
static OptID getCmdOption(const CLI_options *options, int argc, char **argv,
                           char **opt_argv, int *idx)
{
   typedef enum { O_SEARCH, O_FOUND, O_NOTFOUND, O_DONE } State;
   OptID opt_id = DILLO_CLI_NONE;
   int i = 0;
   State state = O_SEARCH;

   if (*idx >= argc) {
      state = O_DONE;
   } else {
      state = O_NOTFOUND;
      for (i = 0; options[i].shortopt; i++) {
         if (strcmp(options[i].shortopt, argv[*idx]) == 0 ||
             strcmp(options[i].longopt, argv[*idx]) == 0) {
            state = O_FOUND;
            ++*idx;
            break;
         }
      }
   }
   if (state == O_FOUND) {
      int n_arg = options[i].opt_argc;
      opt_id  = options[i].id;
      /* Find the required/optional arguments of the option */
      for (i = 0; *idx < argc && i < abs(n_arg) && argv[*idx][0] != '-'; i++)
         opt_argv[i] = argv[(*idx)++];
      opt_argv[i] = NULL;

      /* Optional arguments have opt_argc < 0 */
      if (i < n_arg) {
         fprintf(stderr, "Option %s requires %d argument%s\n",
                 argv[*idx-i-1], n_arg, (n_arg == 1) ? "" : "s");
         opt_id = DILLO_CLI_ERROR;
      }
   }
   if (state == O_NOTFOUND) {
      if (strcmp(argv[*idx], "--") == 0)
         (*idx)++;
      else if (argv[*idx][0] == '-') {
         fprintf(stderr, "Command line option \"%s\" not recognized.\n",
                 argv[*idx]);
         opt_id = DILLO_CLI_ERROR;
      }
   }
   return opt_id;
}

/*
 * Set FL_NORMAL_LABEL to interpret neither symbols (@) nor shortcuts (&),
 * and FL_FREE_LABELTYPE to interpret shortcuts.
 */
static void custLabelDraw(const Fl_Label* o, int X, int Y, int W, int H,
                          Fl_Align align)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 0;
   fl_font(o->font, o->size);
   fl_color((Fl_Color)o->color);
   fl_draw(o->value, X, Y, W, H, align, o->image, interpret_symbols);
}

static void custLabelMeasure(const Fl_Label* o, int& W, int& H)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 0;
   fl_font(o->font, o->size);
   fl_measure(o->value, W, H, interpret_symbols);
}

static void custMenuLabelDraw(const Fl_Label* o, int X, int Y, int W, int H,
                              Fl_Align align)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 1;
   fl_font(o->font, o->size);
   fl_color((Fl_Color)o->color);
   fl_draw(o->value, X, Y, W, H, align, o->image, interpret_symbols);
}

static void custMenuLabelMeasure(const Fl_Label* o, int& W, int& H)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 1;
   fl_font(o->font, o->size);
   fl_measure(o->value, W, H, interpret_symbols);
}

/*
 * Tell the user if default/pref fonts can't be found.
 */
static void checkFont(const char *name, const char *type)
{
   if (! dw::fltk::FltkFont::fontExists(name))
      MSG_WARN("preferred %s font \"%s\" not found.\n", type, name);
}

static void checkPreferredFonts()
{
   checkFont(prefs.font_sans_serif, "sans-serif");
   checkFont(prefs.font_serif, "serif");
   checkFont(prefs.font_monospace, "monospace");
   checkFont(prefs.font_cursive, "cursive");
   checkFont(prefs.font_fantasy, "fantasy");
}

/*
 * Given a command line argument, build a DilloUrl for it.
 */
static DilloUrl *makeStartUrl(char *str, bool local)
{
   char *url_str, *p;
   DilloUrl *start_url;

   /* Relative path to a local file? */
   p = (*str == '/') ? dStrdup(str) :
                       dStrconcat(Paths::getOldWorkingDir(), "/", str, NULL);

   if (access(p, F_OK) == 0) {
      /* absolute path may have non-URL characters */
      url_str = a_Misc_escape_chars(p, "% ");
      start_url = a_Url_new(url_str + 1, "file:/");
   } else {
      /* Not a file, filter URL string */
      url_str = a_Url_string_strip_delimiters(str);
      start_url = a_Url_new(url_str, NULL);
   }
   dFree(p);
   dFree(url_str);

   if (local)
      a_Url_set_flags(start_url, URL_FLAGS(start_url) | URL_SpamSafe);

   return start_url;
}

/*
 * MAIN
 */
int main(int argc, char **argv)
{
   uint_t opt_id;
   uint_t options_got = 0;
   uint32_t xid = 0;
   int idx = 1;
   int xpos = PREFS_GEOMETRY_DEFAULT_XPOS, ypos = PREFS_GEOMETRY_DEFAULT_YPOS,
       width = PREFS_GEOMETRY_DEFAULT_WIDTH,
       height = PREFS_GEOMETRY_DEFAULT_HEIGHT;
   char **opt_argv;
   FILE *fp;

   srand((uint_t)(time(0) ^ getpid()));

   // Some OSes exit dillo without this (not GNU/Linux).
   signal(SIGPIPE, SIG_IGN);

   /* Handle command line options */
   opt_argv = dNew0(char*, numOptions(Options) + 1);
   while ((opt_id = getCmdOption(Options, argc, argv, opt_argv, &idx))) {
      options_got |= opt_id;
      switch (opt_id) {
      case DILLO_CLI_FULLWINDOW:
      case DILLO_CLI_LOCAL:
         break;
      case DILLO_CLI_XID:
      {
         char *end;
         xid = strtol(opt_argv[0], &end, 0);
         if (*end) {
            fprintf(stderr, "XID argument \"%s\" not valid.\n",opt_argv[0]);
            return 2;
         }
         break;
      }
      case DILLO_CLI_GEOMETRY:
         if (!a_Misc_parse_geometry(opt_argv[0],&xpos,&ypos,&width,&height)){
            fprintf(stderr, "geometry argument \"%s\" not valid. Must be of "
                            "the form WxH[{+-}X{+-}Y].\n", opt_argv[0]);
            return 2;
         }
         break;
      case DILLO_CLI_VERSION:
         puts("Dillo version " VERSION);
         return 0;
      case DILLO_CLI_HELP:
         printHelp(argv[0], Options);
         return 0;
      default:
         printHelp(argv[0], Options);
         return 2;
      }
   }
   dFree(opt_argv);

   // set the default values for the preferences
   a_Prefs_init();

   // create ~/.dillo if not present
   Paths::init();

   // initialize default key bindings
   Keys::init();

   // parse dillorc
   if ((fp = Paths::getPrefsFP(PATHS_RC_PREFS))) {
      PrefsParser::parse(fp);
   }
   // parse keysrc
   if ((fp = Paths::getPrefsFP(PATHS_RC_KEYS))) {
      Keys::parse(fp);
   }
   dLib_show_messages(prefs.show_msg);

   // initialize internal modules
   a_Dpi_init();
   a_Dns_init();
   a_Web_init();
   a_Http_init();
   a_Mime_init();
   a_Capi_init();
   a_Dicache_init();
   a_Bw_init();
   a_Cookies_init();
   a_Auth_init();

   /* command line options override preferences */
   if (options_got & DILLO_CLI_FULLWINDOW)
      prefs.fullwindow_start = TRUE;
   if (options_got & DILLO_CLI_GEOMETRY) {
       prefs.width = width;
       prefs.height = height;
       prefs.xpos = xpos;
       prefs.ypos = ypos;
   }

   // Sets WM_CLASS hint on X11
   Fl_Window::default_xclass("dillo");

   Fl::scheme(prefs.theme);

   if (!prefs.show_tooltip) {
      // turn off UI tooltips
      Fl::option(Fl::OPTION_SHOW_TOOLTIPS, false);
   }

   // Disable '@' and '&' interpretation in normal labels.
   Fl::set_labeltype(FL_NORMAL_LABEL, custLabelDraw, custLabelMeasure);

   // Use to permit '&' interpretation.
   Fl::set_labeltype(FL_FREE_LABELTYPE,custMenuLabelDraw,custMenuLabelMeasure);

   checkPreferredFonts();

   /* use preferred font for UI */
   Fl_Font defaultFont = dw::fltk::FltkFont::get (prefs.font_sans_serif, 0);
   Fl::set_font(FL_HELVETICA, defaultFont); // this seems to be the
                                            // only way to set the
                                            // default font in fltk1.3

   // Create a new UI/bw pair
   BrowserWindow *bw = a_UIcmd_browser_window_new(0, 0, xid, NULL);

   /* We need this so that fl_text_extents() in dw/fltkplatform.cc can
    * work when FLTK is configured without XFT and Dillo is opening
    * immediately-available URLs from the cmdline (e.g. about:splash).
    */
   ((Fl_Widget *)bw->ui)->window()->make_current();

   /* Proxy authentication */
   if (prefs.http_proxyuser && !a_Http_proxy_auth()) {
      const char *passwd = a_UIcmd_get_passwd(prefs.http_proxyuser);
      if (passwd) {
         a_Http_set_proxy_passwd(passwd);
      } else {
         MSG_WARN("Not using proxy authentication.\n");
      }
   }

   /* Open URLs/files */
   const bool local = options_got & DILLO_CLI_LOCAL;

   if (idx == argc) {
      /* No URLs/files on cmdline. Send startup screen */
      if (strcmp(URL_STR(prefs.start_page), "about:blank") == 0)
         a_UIcmd_open_url(bw, NULL);
      else
         a_UIcmd_open_url(bw, prefs.start_page);
   } else {
      for (int i = idx; i < argc; i++) {
         DilloUrl *start_url = makeStartUrl(argv[i], local);

         if (i > idx) {
            if (prefs.middle_click_opens_new_tab) {
               /* user must prefer tabs */
               const int focus = 1;
               a_UIcmd_open_url_nt(bw, start_url, focus);
            } else {
               a_UIcmd_open_url_nw(bw, start_url);
            }
         } else {
            a_UIcmd_open_url(bw, start_url);
         }
         a_Url_free(start_url);
      }
   }

   Fl::run();

   /*
    * Memory deallocating routines
    * (This can be left to the OS, but we'll do it, with a view to test
    *  and fix our memory management)
    */
   a_Cookies_freeall();
   a_Cache_freeall();
   a_Dicache_freeall();
   a_Http_freeall();
   a_Dns_freeall();
   a_History_freeall();
   a_Prefs_freeall();
   Keys::free();
   Paths::free();
   /* TODO: auth, css */

   //a_Dpi_dillo_exit();
   MSG("Dillo: normal exit!\n");
   return 0;
}
