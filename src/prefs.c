/*
 * Preferences for dillo
 *
 * Copyright (C) 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>       /* for strchr */
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>       /* for setlocale */
#include <ctype.h>        /* for isspace */
#include "prefs.h"
#include "colors.h"
#include "misc.h"
#include "msg.h"

#define RCNAME "dillorc"

#define DILLO_START_PAGE "about:splash"
#define DILLO_HOME "http://www.dillo.org/"

#define D_FONT_SERIF "DejaVu Serif"
#define D_FONT_SANS_SERIF "DejaVu Sans"
#define D_FONT_CURSIVE "DejaVu Sans" /* \todo find good default */
#define D_FONT_FANTASY "DejaVu Sans" /* \todo find good default */
#define D_FONT_MONOSPACE "DejaVu Sans Mono"
#define D_SEARCH_URL "http://www.google.com/search?ie=UTF-8&oe=UTF-8&q=%s"
#define D_SAVE_DIR "/tmp/"

#define DW_COLOR_DEFAULT_BGND   0xdcd1ba
#define DW_COLOR_DEFAULT_TEXT   0x000000
#define DW_COLOR_DEFAULT_LINK   0x0000ff
#define DW_COLOR_DEFAULT_VLINK  0x800080

/*-----------------------------------------------------------------------------
 * Global Data
 *---------------------------------------------------------------------------*/
DilloPrefs prefs;

/*-----------------------------------------------------------------------------
 * Local types
 *---------------------------------------------------------------------------*/

typedef enum {
   PREFS_BOOL,
   PREFS_STRING,
   PREFS_URL,
   PREFS_INT32,
   PREFS_DOUBLE,
   PREFS_COLOR,
   PREFS_GEOMETRY,
   PREFS_PANEL_SIZE
} PrefType_t;

typedef struct SymNode_ {
   char *name;
   void *pref;
   PrefType_t type;
} SymNode_t;

/*
 *- Mini parser -------------------------------------------------------------
 */

/*
 * Comparison function for binary search
 */
static int Prefs_symbol_cmp(const void *a, const void *b)
{
   return strcmp(((SymNode_t*)a)->name, ((SymNode_t*)b)->name);
}

/*
 * Parse a name/value pair and set preferences accordingly.
 */
static int Prefs_parse_pair(char *name, char *value, const SymNode_t *symbols,
                            int n_symbols)
{
   int st;
   SymNode_t key, *node;

   key.name = name;
   node = bsearch(&key, symbols, n_symbols,
                  sizeof(SymNode_t), Prefs_symbol_cmp);
   if (!node) {
      MSG("prefs: {%s} is not a recognized token.\n", name);
      return -1;
   }

   switch (node->type) {
   case PREFS_BOOL:
      *(bool_t *)node->pref = strcmp(value, "YES") == 0;
      break;
   case PREFS_STRING:
      dFree(*(char **)node->pref);
      *(char **)node->pref = dStrdup(value);
      break;
   case PREFS_URL:
      a_Url_free(*(DilloUrl **)node->pref);
      *(DilloUrl **)node->pref = a_Url_new(value, NULL);
      break;
   case PREFS_INT32:
      *(int32_t *)node->pref = strtol(value, NULL, 10);
      break;
   case PREFS_DOUBLE:
      *(double *)node->pref = strtod(value, NULL);
      break;
   case PREFS_COLOR:
      *(int32_t *)node->pref = a_Color_parse(value, *(int32_t*)node->pref,&st);
      break;
   case PREFS_GEOMETRY:
      a_Misc_parse_geometry(value, &prefs.xpos, &prefs.ypos,
                            &prefs.width, &prefs.height);
      break;
   case PREFS_PANEL_SIZE:
      if (!dStrcasecmp(value, "tiny"))
         prefs.panel_size = P_tiny;
      else if (!dStrcasecmp(value, "small"))
         prefs.panel_size = P_small;
      else if (!dStrcasecmp(value, "large"))
         prefs.panel_size = P_large;
      else /* default to "medium" */
         prefs.panel_size = P_medium;
      break;
   default:
      MSG_WARN("prefs: {%s} IS recognized but not handled!\n", name);
      break;   /* Not reached */
   }

   return 0;
}

/*
 * Parse dillorc and set the values in the prefs structure.
 */
static int Prefs_parse_dillorc(void)
{
   /* Symbol array, sorted alphabetically */
   const SymNode_t symbols[] = {
   { "allow_white_bg", &prefs.allow_white_bg, PREFS_BOOL },
   { "bg_color", &prefs.bg_color, PREFS_COLOR },
   { "buffered_drawing", &prefs.buffered_drawing, PREFS_INT32 },
   { "contrast_visited_color", &prefs.contrast_visited_color, PREFS_BOOL },
   { "enterpress_forces_submit", &prefs.enterpress_forces_submit, PREFS_BOOL },
   { "focus_new_tab", &prefs.focus_new_tab, PREFS_BOOL },
   { "font_cursive", &prefs.font_cursive, PREFS_STRING },
   { "font_factor", &prefs.font_factor, PREFS_DOUBLE },
   { "font_fantasy", &prefs.font_fantasy, PREFS_STRING },
   { "font_min_size", &prefs.font_min_size, PREFS_INT32 },
   { "font_monospace", &prefs.font_monospace, PREFS_STRING },
   { "font_sans_serif", &prefs.font_sans_serif, PREFS_STRING },
   { "font_serif", &prefs.font_serif, PREFS_STRING },
   { "force_my_colors", &prefs.force_my_colors, PREFS_BOOL },
   { "fullwindow_start", &prefs.fullwindow_start, PREFS_BOOL },
   { "geometry", NULL, PREFS_GEOMETRY },
   { "home", &prefs.home, PREFS_URL },
   { "http_language", &prefs.http_language, PREFS_STRING },
   { "http_proxy", &prefs.http_proxy, PREFS_URL },
   { "http_proxyuser", &prefs.http_proxyuser, PREFS_STRING },
   { "http_referer", &prefs.http_referer, PREFS_STRING },
   { "limit_text_width", &prefs.limit_text_width, PREFS_BOOL },
   { "link_color", &prefs.link_color, PREFS_COLOR },
   { "load_images", &prefs.load_images, PREFS_BOOL },
   { "load_stylesheets", &prefs.load_stylesheets, PREFS_BOOL },
   { "middle_click_drags_page", &prefs.middle_click_drags_page, PREFS_BOOL },
   { "middle_click_opens_new_tab", &prefs.middle_click_opens_new_tab,
     PREFS_BOOL },
   { "no_proxy", &prefs.no_proxy, PREFS_STRING },
   { "panel_size", &prefs.panel_size, PREFS_PANEL_SIZE },
   { "parse_embedded_css", &prefs.parse_embedded_css, PREFS_BOOL },
   { "save_dir", &prefs.save_dir, PREFS_STRING },
   { "search_url", &prefs.search_url, PREFS_STRING },
   { "show_back", &prefs.show_back, PREFS_BOOL },
   { "show_bookmarks", &prefs.show_bookmarks, PREFS_BOOL },
   { "show_clear_url", &prefs.show_clear_url, PREFS_BOOL },
   { "show_extra_warnings", &prefs.show_extra_warnings, PREFS_BOOL },
   { "show_filemenu", &prefs.show_filemenu, PREFS_BOOL },
   { "show_forw", &prefs.show_forw, PREFS_BOOL },
   { "show_home", &prefs.show_home, PREFS_BOOL },
   { "show_msg", &prefs.show_msg, PREFS_BOOL },
   { "show_progress_box", &prefs.show_progress_box, PREFS_BOOL },
   { "show_reload", &prefs.show_reload, PREFS_BOOL },
   { "show_save", &prefs.show_save, PREFS_BOOL },
   { "show_search", &prefs.show_search, PREFS_BOOL },
   { "show_stop", &prefs.show_stop, PREFS_BOOL },
   { "show_tools", &prefs.show_tools, PREFS_BOOL },
   { "show_tooltip", &prefs.show_tooltip, PREFS_BOOL },
   { "show_url", &prefs.show_url, PREFS_BOOL },
   { "small_icons", &prefs.small_icons, PREFS_BOOL },
   { "standard_widget_colors", &prefs.standard_widget_colors, PREFS_BOOL },
   { "start_page", &prefs.start_page, PREFS_URL },
   { "text_color", &prefs.text_color, PREFS_COLOR },
   { "visited_color", &prefs.visited_color, PREFS_COLOR },
   { "w3c_plus_heuristics", &prefs.w3c_plus_heuristics, PREFS_BOOL }
   };

   const uint_t n_symbols = sizeof (symbols) / sizeof (symbols[0]);

   FILE *F_in;
   char *filename, *line, *name, *value;
   int ret = -1;

   filename = dStrconcat(dGethomedir(), "/.dillo/", RCNAME, NULL);
   if (!(F_in = fopen(filename, "r"))) {
      MSG("prefs: Can't open %s file: %s\n", RCNAME, filename);
      if (!(F_in = fopen(DILLORC_SYS, "r"))) {
         MSG("prefs: Can't open %s file: %s\n", RCNAME, DILLORC_SYS);
         MSG("prefs: Using internal defaults.\n");
      } else {
         MSG("prefs: Using %s\n", DILLORC_SYS);
      }
   }

   if (F_in) {
      /* scan dillorc line by line */
      while ((line = dGetline(F_in)) != NULL) {
         if (dParser_get_rc_pair(&line, &name, &value) == 0) {
            _MSG("{%s}, {%s}\n", name, value);
            Prefs_parse_pair(name, value, symbols, n_symbols);
         } else if (line[0] && line[0] != '#' && (!name || !value)) {
            MSG("prefs: Syntax error in %s: name=\"%s\" value=\"%s\"\n",
                RCNAME, name, value);
         }
         dFree(line);
      }
      fclose(F_in);
      ret = 0;
   }
   dFree(filename);

   if (prefs.limit_text_width) {
      /* BUG: causes 100% CPU usage with <button> or <input type="image"> */
      MSG_WARN("Disabling limit_text_width preference (currently broken).\n");
      prefs.limit_text_width = FALSE;
   }

   return ret;
}

/*---------------------------------------------------------------------------*/

void a_Prefs_init(void)
{
   char *old_locale;

   prefs.allow_white_bg = TRUE;
   prefs.bg_color = DW_COLOR_DEFAULT_BGND;
   prefs.buffered_drawing=1;
   prefs.contrast_visited_color = TRUE;
   prefs.enterpress_forces_submit = FALSE;
   prefs.focus_new_tab = TRUE;
   prefs.font_cursive = dStrdup(D_FONT_CURSIVE);
   prefs.font_factor = 1.0;
   prefs.font_min_size = 6;
   prefs.font_fantasy = dStrdup(D_FONT_FANTASY);
   prefs.font_monospace = dStrdup(D_FONT_MONOSPACE);
   prefs.font_sans_serif = dStrdup(D_FONT_SANS_SERIF);
   prefs.font_serif = dStrdup(D_FONT_SERIF);
   prefs.force_my_colors = FALSE;
   prefs.fullwindow_start=FALSE;

   /* these four constitute the geometry */
   prefs.width = D_GEOMETRY_DEFAULT_WIDTH;
   prefs.height = D_GEOMETRY_DEFAULT_HEIGHT;
   prefs.xpos = D_GEOMETRY_DEFAULT_XPOS;
   prefs.ypos = D_GEOMETRY_DEFAULT_YPOS;

   prefs.home = a_Url_new(DILLO_HOME, NULL);
   prefs.http_language = NULL;
   prefs.http_proxy = NULL;
   prefs.http_proxyuser = NULL;
   prefs.http_referer = dStrdup("host");
   prefs.limit_text_width = FALSE;
   prefs.link_color = DW_COLOR_DEFAULT_LINK;
   prefs.load_images=TRUE;
   prefs.load_stylesheets=TRUE;
   prefs.middle_click_drags_page = TRUE;
   prefs.middle_click_opens_new_tab = TRUE;
   prefs.no_proxy = NULL;
   prefs.panel_size = P_medium;
   prefs.parse_embedded_css=TRUE;
   prefs.save_dir = dStrdup(D_SAVE_DIR);
   prefs.search_url = dStrdup(D_SEARCH_URL);
   prefs.show_back=TRUE;
   prefs.show_bookmarks=TRUE;
   prefs.show_clear_url=TRUE;
   prefs.show_extra_warnings = FALSE;
   prefs.show_filemenu=TRUE;
   prefs.show_forw=TRUE;
   prefs.show_home=TRUE;
   prefs.show_msg = TRUE;
   prefs.show_progress_box=TRUE;
   prefs.show_reload=TRUE;
   prefs.show_save=TRUE;
   prefs.show_search=TRUE;
   prefs.show_stop=TRUE;
   prefs.show_tools=TRUE;
   prefs.show_tooltip = TRUE;
   prefs.show_url=TRUE;
   prefs.small_icons = FALSE;
   prefs.standard_widget_colors = FALSE;
   prefs.start_page = a_Url_new(DILLO_START_PAGE, NULL);
   prefs.text_color = DW_COLOR_DEFAULT_TEXT;
   prefs.visited_color = DW_COLOR_DEFAULT_VLINK;
   prefs.w3c_plus_heuristics = TRUE;

   /* this locale stuff is to avoid parsing problems with float numbers */
   old_locale = dStrdup (setlocale (LC_NUMERIC, NULL));
   setlocale (LC_NUMERIC, "C");

   Prefs_parse_dillorc();

   setlocale (LC_NUMERIC, old_locale);
   dFree (old_locale);

}

/*
 *  Preferences memory-deallocation
 *  (Call this one at exit time)
 */
void a_Prefs_freeall(void)
{
   dFree(prefs.font_cursive);
   dFree(prefs.font_fantasy);
   dFree(prefs.font_monospace);
   dFree(prefs.font_sans_serif);
   dFree(prefs.font_serif);
   dFree(prefs.http_language);
   dFree(prefs.http_proxyuser);
   dFree(prefs.http_referer);
   dFree(prefs.no_proxy);
   dFree(prefs.save_dir);
   dFree(prefs.search_url);

   a_Url_free(prefs.home);
   a_Url_free(prefs.http_proxy);
   a_Url_free(prefs.start_page);
}
