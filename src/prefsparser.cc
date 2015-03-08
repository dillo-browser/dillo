/*
 * Preferences parser
 *
 * Copyright (C) 2006-2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <locale.h>            /* for setlocale */
#include <math.h>              /* for isinf */
#include <limits.h>

#include "prefs.h"
#include "misc.h"
#include "msg.h"
#include "colors.h"

#include "prefsparser.hh"

typedef enum {
   PREFS_BOOL,
   PREFS_COLOR,
   PREFS_STRING,
   PREFS_STRINGS,
   PREFS_URL,
   PREFS_INT32,
   PREFS_DOUBLE,
   PREFS_FRACTION_100,
   PREFS_GEOMETRY,
   PREFS_PANEL_SIZE
} PrefType_t;

typedef struct {
   const char *name;
   void *pref;
   PrefType_t type;
   int count;
} SymNode_t;

/*
 * Parse a name/value pair and set preferences accordingly.
 */
static int parseOption(char *name, char *value,
                       SymNode_t *symbols, int n_symbols)
{
   SymNode_t *node;
   int i;
   int st;

   node = NULL;
   for (i = 0; i < n_symbols; i++) {
      if (!strcmp(symbols[i].name, name)) {
         node = & (symbols[i]);
         break;
      }
   }

   if (!node) {
      MSG("prefs: {%s} is not a recognized token.\n", name);
      return -1;
   }

   switch (node->type) {
   case PREFS_BOOL:
      *(bool_t *)node->pref = (!dStrAsciiCasecmp(value, "yes") ||
                               !dStrAsciiCasecmp(value, "true"));
      break;
   case PREFS_COLOR:
      *(int32_t *)node->pref = a_Color_parse(value, *(int32_t*)node->pref,&st);
      if (st == 1)
         MSG("prefs: Color '%s' not recognized.\n", value);
      break;
   case PREFS_STRING:
      dFree(*(char **)node->pref);
      *(char **)node->pref = dStrdup(value);
      break;
   case PREFS_STRINGS:
   {
      Dlist *lp = *(Dlist **)node->pref;
      if (node->count == 0) {
         /* override the default */
         for (i = 0; i < dList_length(lp); i++) {
            void *data = dList_nth_data(lp, i);
            dList_remove(lp, data);
            dFree(data);
         }
      }
      dList_append(lp, dStrdup(value));
      break;
   }
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
   case PREFS_FRACTION_100:
      {
         double d = strtod (value, NULL);
         if (isinf(d)) {
            if (d > 0)
               *(int*)node->pref = INT_MAX;
            else
               *(int*)node->pref = INT_MIN;
         } else
            *(int*)node->pref = 100 * d;
      }
      break;
   case PREFS_GEOMETRY:
      a_Misc_parse_geometry(value, &prefs.xpos, &prefs.ypos,
                            &prefs.width, &prefs.height);
      break;
   case PREFS_PANEL_SIZE:
      if (!dStrAsciiCasecmp(value, "tiny"))
         prefs.panel_size = P_tiny;
      else if (!dStrAsciiCasecmp(value, "small"))
         prefs.panel_size = P_small;
      else /* default to "medium" */
         prefs.panel_size = P_medium;
      break;
   default:
      MSG_WARN("prefs: {%s} IS recognized but not handled!\n", name);
      break;   /* Not reached */
   }
   node->count++;

   return 0;
}

/*
 * Parses the dillorc and sets the values in the prefs structure.
 */
void PrefsParser::parse(FILE *fp)
{
   char *line, *name, *value, *oldLocale;
   int st;

   /* Symbol array, sorted alphabetically */
   SymNode_t symbols[] = {
      { "allow_white_bg", &prefs.allow_white_bg, PREFS_BOOL, 0 },
      { "white_bg_replacement", &prefs.white_bg_replacement, PREFS_COLOR, 0 },
      { "bg_color", &prefs.bg_color, PREFS_COLOR, 0 },
      { "buffered_drawing", &prefs.buffered_drawing, PREFS_INT32, 0 },
      { "contrast_visited_color", &prefs.contrast_visited_color, PREFS_BOOL, 0 },
      { "enterpress_forces_submit", &prefs.enterpress_forces_submit,
        PREFS_BOOL, 0 },
      { "focus_new_tab", &prefs.focus_new_tab, PREFS_BOOL, 0 },
      { "font_cursive", &prefs.font_cursive, PREFS_STRING, 0 },
      { "font_factor", &prefs.font_factor, PREFS_DOUBLE, 0 },
      { "font_fantasy", &prefs.font_fantasy, PREFS_STRING, 0 },
      { "font_max_size", &prefs.font_max_size, PREFS_INT32, 0 },
      { "font_min_size", &prefs.font_min_size, PREFS_INT32, 0 },
      { "font_monospace", &prefs.font_monospace, PREFS_STRING, 0 },
      { "font_sans_serif", &prefs.font_sans_serif, PREFS_STRING, 0 },
      { "font_serif", &prefs.font_serif, PREFS_STRING, 0 },
      { "fullwindow_start", &prefs.fullwindow_start, PREFS_BOOL, 0 },
      { "geometry", NULL, PREFS_GEOMETRY, 0 },
      { "home", &prefs.home, PREFS_URL, 0 },
      { "http_language", &prefs.http_language, PREFS_STRING, 0 },
      { "http_max_conns", &prefs.http_max_conns, PREFS_INT32, 0 },
      { "http_proxy", &prefs.http_proxy, PREFS_URL, 0 },
      { "http_proxyuser", &prefs.http_proxyuser, PREFS_STRING, 0 },
      { "http_referer", &prefs.http_referer, PREFS_STRING, 0 },
      { "http_user_agent", &prefs.http_user_agent, PREFS_STRING, 0 },
      { "limit_text_width", &prefs.limit_text_width, PREFS_BOOL, 0 },
      { "load_images", &prefs.load_images, PREFS_BOOL, 0 },
      { "load_background_images", &prefs.load_background_images, PREFS_BOOL, 0 },
      { "load_stylesheets", &prefs.load_stylesheets, PREFS_BOOL, 0 },
      { "middle_click_drags_page", &prefs.middle_click_drags_page,
        PREFS_BOOL, 0 },
      { "middle_click_opens_new_tab", &prefs.middle_click_opens_new_tab,
        PREFS_BOOL, 0 },
      { "right_click_closes_tab", &prefs.right_click_closes_tab, PREFS_BOOL, 0 },
      { "no_proxy", &prefs.no_proxy, PREFS_STRING, 0 },
      { "panel_size", &prefs.panel_size, PREFS_PANEL_SIZE, 0 },
      { "parse_embedded_css", &prefs.parse_embedded_css, PREFS_BOOL, 0 },
      { "save_dir", &prefs.save_dir, PREFS_STRING, 0 },
      { "search_url", &prefs.search_urls, PREFS_STRINGS, 0 },
      { "show_back", &prefs.show_back, PREFS_BOOL, 0 },
      { "show_bookmarks", &prefs.show_bookmarks, PREFS_BOOL, 0 },
      { "show_clear_url", &prefs.show_clear_url, PREFS_BOOL, 0 },
      { "show_extra_warnings", &prefs.show_extra_warnings, PREFS_BOOL, 0 },
      { "show_filemenu", &prefs.show_filemenu, PREFS_BOOL, 0 },
      { "show_forw", &prefs.show_forw, PREFS_BOOL, 0 },
      { "show_help", &prefs.show_help, PREFS_BOOL, 0 },
      { "show_home", &prefs.show_home, PREFS_BOOL, 0 },
      { "show_msg", &prefs.show_msg, PREFS_BOOL, 0 },
      { "show_progress_box", &prefs.show_progress_box, PREFS_BOOL, 0 },
      { "show_quit_dialog", &prefs.show_quit_dialog, PREFS_BOOL, 0 },
      { "show_reload", &prefs.show_reload, PREFS_BOOL, 0 },
      { "show_save", &prefs.show_save, PREFS_BOOL, 0 },
      { "show_url", &prefs.show_url, PREFS_BOOL, 0 },
      { "show_search", &prefs.show_search, PREFS_BOOL, 0 },
      { "show_stop", &prefs.show_stop, PREFS_BOOL, 0 },
      { "show_tools", &prefs.show_tools, PREFS_BOOL, 0 },
      { "show_tooltip", &prefs.show_tooltip, PREFS_BOOL, 0 },
      { "show_ui_tooltip", &prefs.show_ui_tooltip, PREFS_BOOL, 0 },
      { "small_icons", &prefs.small_icons, PREFS_BOOL, 0 },
      { "start_page", &prefs.start_page, PREFS_URL, 0 },
      { "theme", &prefs.theme, PREFS_STRING, 0 },
      { "ui_button_highlight_color", &prefs.ui_button_highlight_color,
        PREFS_COLOR, 0 },
      { "ui_fg_color", &prefs.ui_fg_color, PREFS_COLOR, 0 },
      { "ui_main_bg_color", &prefs.ui_main_bg_color, PREFS_COLOR, 0 },
      { "ui_selection_color", &prefs.ui_selection_color, PREFS_COLOR, 0 },
      { "ui_tab_active_bg_color", &prefs.ui_tab_active_bg_color, PREFS_COLOR, 0 },
      { "ui_tab_bg_color", &prefs.ui_tab_bg_color, PREFS_COLOR, 0 },
      { "ui_tab_active_fg_color", &prefs.ui_tab_active_fg_color, PREFS_COLOR, 0 },
      { "ui_tab_fg_color", &prefs.ui_tab_fg_color, PREFS_COLOR, 0 },
      { "ui_text_bg_color", &prefs.ui_text_bg_color, PREFS_COLOR, 0 },
      { "w3c_plus_heuristics", &prefs.w3c_plus_heuristics, PREFS_BOOL, 0 },
      { "penalty_hyphen", &prefs.penalty_hyphen, PREFS_FRACTION_100, 0 },
      { "penalty_hyphen_2", &prefs.penalty_hyphen_2, PREFS_FRACTION_100, 0 },
      { "penalty_em_dash_left", &prefs.penalty_em_dash_left,
        PREFS_FRACTION_100, 0 },
      { "penalty_em_dash_right", &prefs.penalty_em_dash_right,
        PREFS_FRACTION_100, 0 },
      { "penalty_em_dash_right_2", &prefs.penalty_em_dash_right_2,
        PREFS_FRACTION_100, 0 },
      { "stretchability_factor", &prefs.stretchability_factor,
        PREFS_FRACTION_100, 0 }
   };
   // changing the LC_NUMERIC locale (temporarily) to C
   // avoids parsing problems with float numbers
   oldLocale = dStrdup(setlocale(LC_NUMERIC, NULL));
   setlocale(LC_NUMERIC, "C");

   // scan the file line by line
   while ((line = dGetline(fp)) != NULL) {
      st = dParser_parse_rc_line(&line, &name, &value);

      if (st == 0) {
         _MSG("prefsparser: name=%s, value=%s\n", name, value);
         parseOption(name, value, symbols, sizeof(symbols) / sizeof(symbols[0]));
      } else if (st < 0) {
         MSG_ERR("prefsparser: Syntax error in dillorc:"
                 " name=\"%s\" value=\"%s\"\n", name, value);
      }

      dFree(line);
   }
   fclose(fp);

   // restore the old numeric locale
   setlocale(LC_NUMERIC, oldLocale);
   dFree(oldLocale);
}
