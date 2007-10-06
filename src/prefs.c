/*
 * Preferences for dillo
 *
 * Copyright (C) 2006 Jorge Arellano Cid <jcid@dillo.org>
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

#define RCNAME "dillorc2"


/*
 * Global Data
 */
DilloPrefs prefs;

typedef struct SymNode_ SymNode_t;

struct SymNode_ {
   char *name;
   RcToken_t token;
};

/* Symbol array, sorted alphabetically */
SymNode_t symbols[] = {
   { "allow_white_bg", DRC_TOKEN_ALLOW_WHITE_BG },
   { "bg_color", DRC_TOKEN_BG_COLOR },
   { "contrast_visited_color", DRC_TOKEN_CONTRAST_VISITED_COLOR },
   { "enterpress_forces_submit", DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT },
   { "font_factor", DRC_TOKEN_FONT_FACTOR },
   { "force_my_colors", DRC_TOKEN_FORCE_MY_COLORS },
   { "fullwindow_start", DRC_TOKEN_FULLWINDOW_START },
   { "fw_fontname", DRC_TOKEN_FW_FONT },
   { "generate_submit", DRC_TOKEN_GENERATE_SUBMIT },
   { "geometry", DRC_TOKEN_GEOMETRY },
   { "home", DRC_TOKEN_HOME },
   { "http_proxy", DRC_TOKEN_PROXY },
   { "http_proxyuser", DRC_TOKEN_PROXYUSER },
   { "limit_text_width", DRC_TOKEN_LIMIT_TEXT_WIDTH },
   { "link_color", DRC_TOKEN_LINK_COLOR },
   { "no_proxy", DRC_TOKEN_NOPROXY },
   { "panel_size", DRC_TOKEN_PANEL_SIZE },
   { "search_url", DRC_TOKEN_SEARCH_URL },
   { "show_back", DRC_TOKEN_SHOW_BACK },
   { "show_bookmarks", DRC_TOKEN_SHOW_BOOKMARKS },
   { "show_clear_url", DRC_TOKEN_SHOW_CLEAR_URL },
   { "show_extra_warnings", DRC_TOKEN_SHOW_EXTRA_WARNINGS },
   { "show_forw", DRC_TOKEN_SHOW_FORW },
   { "show_home", DRC_TOKEN_SHOW_HOME },
   { "show_menubar", DRC_TOKEN_SHOW_MENUBAR },
   { "show_msg", DRC_TOKEN_SHOW_MSG },
   { "show_progress_box", DRC_TOKEN_SHOW_PROGRESS_BOX },
   { "show_reload", DRC_TOKEN_SHOW_RELOAD },
   { "show_save", DRC_TOKEN_SHOW_SAVE },
   { "show_search", DRC_TOKEN_SHOW_SEARCH },
   { "show_stop", DRC_TOKEN_SHOW_STOP },
   { "show_tooltip", DRC_TOKEN_SHOW_TOOLTIP },
   { "show_url", DRC_TOKEN_SHOW_URL },
   { "small_icons", DRC_TOKEN_SMALL_ICONS },
   { "start_page", DRC_TOKEN_START_PAGE },
   { "text_color", DRC_TOKEN_TEXT_COLOR },
   { "transient_dialogs", DRC_TOKEN_TRANSIENT_DIALOGS },
   { "use_dicache", DRC_TOKEN_USE_DICACHE },
   { "use_oblique", DRC_TOKEN_USE_OBLIQUE },
   { "visited_color", DRC_TOKEN_VISITED_COLOR, },
   { "vw_fontname", DRC_TOKEN_VW_FONT },
   { "w3c_plus_heuristics", DRC_TOKEN_W3C_PLUS_HEURISTICS }
};

static const uint_t n_symbols = sizeof (symbols) / sizeof (symbols[0]);

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
 * Take a dillo rc line and return 'name' and 'value' pointers to it.
 * Notes:
 *    - line is modified!
 *    - it skips blank lines and lines starting with '#'
 *
 * Return value: 0 on successful value/pair, -1 otherwise
 */
static int Prefs_get_pair(char **line, char **name, char **value)
{
   char *eq, *p;
   int len, ret = -1;

   dReturn_val_if_fail(*line, ret);

   *name = NULL;
   *value = NULL;
   dStrstrip(*line);
   if (*line[0] != '#' && (eq = strchr(*line, '='))) {
      /* get name */
      for (p = *line; *p && *p != '=' && !isspace(*p); ++p);
      *p = 0;
      *name = *line;

      /* get value */
      if (p == eq) {
         for (++p; isspace(*p); ++p);
         len = strlen(p);
         if (len >= 2 && *p == '"' && p[len-1] == '"') {
            p[len-1] = 0;
            ++p;
         }
         *value = p;
         ret = 0;
      }
   }

   if (*line[0] && *line[0] != '#' && (!*name || !*value)) {
      MSG("prefs: Syntax error in %s: name=\"%s\" value=\"%s\"\n",
          RCNAME, *name, *value);
   }
   return ret;
}

/*
 * Parse a name/value pair and set preferences accordingly.
 */
static int Prefs_parse_pair(char *name, char *value)
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

   switch (node->token) {
   case DRC_TOKEN_GEOMETRY:
      a_Misc_parse_geometry(value, &prefs.xpos, &prefs.ypos,
                            &prefs.width, &prefs.height);
      break;
   case DRC_TOKEN_PROXY:
      a_Url_free(prefs.http_proxy);
      prefs.http_proxy = a_Url_new(value, NULL, 0, 0, 0);
      break;
   case DRC_TOKEN_PROXYUSER:
      dFree(prefs.http_proxyuser);
      prefs.http_proxyuser = dStrdup(value);
      break;
   case DRC_TOKEN_NOPROXY:
      dFree(prefs.no_proxy);
      prefs.no_proxy = dStrdup(value);
      break;
   case DRC_TOKEN_LINK_COLOR:
      prefs.link_color = a_Color_parse(value, prefs.link_color, &st);
      break;
   case DRC_TOKEN_VISITED_COLOR:
      prefs.visited_color = a_Color_parse(value, prefs.visited_color, &st);
      break;
   case DRC_TOKEN_TEXT_COLOR:
      prefs.text_color = a_Color_parse(value, prefs.text_color, &st);
      break;
   case DRC_TOKEN_BG_COLOR:
      prefs.bg_color = a_Color_parse(value, prefs.bg_color, &st);
      break;
   case DRC_TOKEN_ALLOW_WHITE_BG:
      prefs.allow_white_bg = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FORCE_MY_COLORS:
      prefs.force_my_colors = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_CONTRAST_VISITED_COLOR:
      prefs.contrast_visited_color = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_USE_OBLIQUE:
      prefs.use_oblique = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_PANEL_SIZE:
      if (!dStrcasecmp(value, "tiny"))
         prefs.panel_size = 1;
      else if (!dStrcasecmp(value, "small"))
         prefs.panel_size = 2;
      else if (!dStrcasecmp(value, "medium"))
         prefs.panel_size = 3;
      else /* default to "large" */
         prefs.panel_size = 4;
      break;
   case DRC_TOKEN_SMALL_ICONS:
      prefs.small_icons = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_START_PAGE:
      a_Url_free(prefs.start_page);
      prefs.start_page = a_Url_new(value, NULL, 0, 0, 0);
      break;
   case DRC_TOKEN_HOME:
      a_Url_free(prefs.home);
      prefs.home = a_Url_new(value, NULL, 0, 0, 0);
      break;
   case DRC_TOKEN_SHOW_TOOLTIP:
      prefs.show_tooltip = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FONT_FACTOR:
      prefs.font_factor = strtod(value, NULL);
      break;
   case DRC_TOKEN_LIMIT_TEXT_WIDTH:
      prefs.limit_text_width = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_W3C_PLUS_HEURISTICS:
      prefs.w3c_plus_heuristics = (strcmp(value,"YES") == 0);
      break;
   case DRC_TOKEN_USE_DICACHE:
      prefs.use_dicache = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_BACK:
      prefs.show_back = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_FORW:
      prefs.show_forw = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_HOME:
      prefs.show_home = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_RELOAD:
      prefs.show_reload = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_SAVE:
      prefs.show_save = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_STOP:
      prefs.show_stop = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_BOOKMARKS:
      prefs.show_bookmarks = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_MENUBAR:
      prefs.show_menubar = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_CLEAR_URL:
      prefs.show_clear_url = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_URL:
      prefs.show_url = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_SEARCH:
      prefs.show_search = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_PROGRESS_BOX:
      prefs.show_progress_box = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FULLWINDOW_START:
      prefs.fullwindow_start = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_TRANSIENT_DIALOGS:
      prefs.transient_dialogs = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FW_FONT:
      dFree(prefs.fw_fontname);
      prefs.fw_fontname = dStrdup(value);
      break;
   case DRC_TOKEN_VW_FONT:
      dFree(prefs.vw_fontname);
      prefs.vw_fontname = dStrdup(value);
      break;
   case DRC_TOKEN_GENERATE_SUBMIT:
      prefs.generate_submit = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT:
      prefs.enterpress_forces_submit = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SEARCH_URL:
      dFree(prefs.search_url);
      prefs.search_url = dStrdup(value);
      break;
   case DRC_TOKEN_SHOW_MSG:
      prefs.show_msg = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_EXTRA_WARNINGS:
      prefs.show_extra_warnings = (strcmp(value, "YES") == 0);
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
         if (Prefs_get_pair(&line, &name, &value) == 0){
            _MSG("{%s}, {%s}\n", name, value);
            Prefs_parse_pair(name, value);
         }
         dFree(line);
      }
      fclose(F_in);
      ret = 0;
   }
   dFree(filename);

   return ret;
}

/*---------------------------------------------------------------------------*/

void a_Prefs_init(void)
{
   char *old_locale;

   prefs.width = D_GEOMETRY_DEFAULT_WIDTH;
   prefs.height = D_GEOMETRY_DEFAULT_HEIGHT;
   prefs.xpos = D_GEOMETRY_DEFAULT_XPOS;
   prefs.ypos = D_GEOMETRY_DEFAULT_YPOS;
   prefs.http_proxy = NULL;
   prefs.http_proxyuser = NULL;
   prefs.no_proxy = NULL;
   prefs.link_color = DW_COLOR_DEFAULT_BLUE;
   prefs.visited_color = DW_COLOR_DEFAULT_PURPLE;
   prefs.bg_color = DW_COLOR_DEFAULT_BGND;
   prefs.text_color = DW_COLOR_DEFAULT_BLACK;
   prefs.use_oblique = FALSE;
   prefs.start_page = a_Url_new(DILLO_START_PAGE, NULL, 0, 0, 0);
   prefs.home = a_Url_new(DILLO_HOME, NULL, 0, 0, 0);
   prefs.allow_white_bg = TRUE;
   prefs.force_my_colors = FALSE;
   prefs.contrast_visited_color = FALSE;
   prefs.show_tooltip = FALSE;
   prefs.panel_size = 1;
   prefs.small_icons = FALSE;
   prefs.limit_text_width = FALSE;
   prefs.w3c_plus_heuristics = TRUE;
   prefs.font_factor = 1.0;
   prefs.use_dicache = FALSE;
   prefs.show_back=TRUE;
   prefs.show_forw=TRUE;
   prefs.show_home=TRUE;
   prefs.show_reload=TRUE;
   prefs.show_save=TRUE;
   prefs.show_stop=TRUE;
   prefs.show_bookmarks=TRUE;
   prefs.show_menubar=TRUE;
   prefs.show_clear_url=TRUE;
   prefs.show_url=TRUE;
   prefs.show_search=TRUE;
   prefs.show_progress_box=TRUE;
   prefs.fullwindow_start=FALSE;
   prefs.transient_dialogs=FALSE;
   prefs.vw_fontname = dStrdup("helvetica");
   prefs.fw_fontname = dStrdup("courier");
   prefs.generate_submit = FALSE;
   prefs.enterpress_forces_submit = FALSE;
   prefs.search_url = dStrdup("http://www.google.com/search?q=%s");
   prefs.show_msg = TRUE;
   prefs.show_extra_warnings = FALSE;

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
   dFree(prefs.http_proxyuser);
   dFree(prefs.no_proxy);
   a_Url_free(prefs.http_proxy);
   dFree(prefs.fw_fontname);
   dFree(prefs.vw_fontname);
   a_Url_free(prefs.start_page);
   a_Url_free(prefs.home);
   dFree(prefs.search_url);
}
