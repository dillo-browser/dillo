/*
 * Preferences
 *
 * Copyright (C) 2006-2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "prefs.h"

#define PREFS_START_PAGE      "about:splash"
#define PREFS_HOME            "http://www.dillo.org/"
#define PREFS_FONT_SERIF      "DejaVu Serif"
#define PREFS_FONT_SANS_SERIF "DejaVu Sans"
#define PREFS_FONT_CURSIVE    "URW Chancery L"
#define PREFS_FONT_FANTASY    "DejaVu Sans" /* TODO: find good default */
#define PREFS_FONT_MONOSPACE  "DejaVu Sans Mono"
#define PREFS_SEARCH_URL      "dd http://duckduckgo.com/lite/?kp=-1&q=%s"
#define PREFS_NO_PROXY        "localhost 127.0.0.1"
#define PREFS_SAVE_DIR        "/tmp/"
#define PREFS_HTTP_REFERER    "host"
#define PREFS_HTTP_USER_AGENT "Dillo/" VERSION
#define PREFS_THEME           "none"

/*-----------------------------------------------------------------------------
 * Global Data
 *---------------------------------------------------------------------------*/
DilloPrefs prefs;

/*
 * Sets the default settings.
 */

void a_Prefs_init(void)
{
   prefs.allow_white_bg = TRUE;
   prefs.white_bg_replacement = 0xe0e0a3; // 0xdcd1ba;
   prefs.bg_color = 0xdcd1ba;
   prefs.buffered_drawing = 1;
   prefs.contrast_visited_color = TRUE;
   prefs.enterpress_forces_submit = FALSE;
   prefs.focus_new_tab = TRUE;
   prefs.font_cursive = dStrdup(PREFS_FONT_CURSIVE);
   prefs.font_factor = 1.0;
   prefs.font_max_size = 100;
   prefs.font_min_size = 6;
   prefs.font_fantasy = dStrdup(PREFS_FONT_FANTASY);
   prefs.font_monospace = dStrdup(PREFS_FONT_MONOSPACE);
   prefs.font_sans_serif = dStrdup(PREFS_FONT_SANS_SERIF);
   prefs.font_serif = dStrdup(PREFS_FONT_SERIF);
   prefs.fullwindow_start = FALSE;

   /* these four constitute the geometry */
   prefs.width = PREFS_GEOMETRY_DEFAULT_WIDTH;
   prefs.height = PREFS_GEOMETRY_DEFAULT_HEIGHT;
   prefs.xpos = PREFS_GEOMETRY_DEFAULT_XPOS;
   prefs.ypos = PREFS_GEOMETRY_DEFAULT_YPOS;

   prefs.home = a_Url_new(PREFS_HOME, NULL);
   prefs.http_language = NULL;
   prefs.http_proxy = NULL;
   prefs.http_max_conns = 6;
   prefs.http_proxyuser = NULL;
   prefs.http_referer = dStrdup(PREFS_HTTP_REFERER);
   prefs.http_user_agent = dStrdup(PREFS_HTTP_USER_AGENT);
   prefs.limit_text_width = FALSE;
   prefs.load_images=TRUE;
   prefs.load_background_images=FALSE;
   prefs.load_stylesheets=TRUE;
   prefs.middle_click_drags_page = TRUE;
   prefs.middle_click_opens_new_tab = TRUE;
   prefs.right_click_closes_tab = FALSE;
   prefs.no_proxy = dStrdup(PREFS_NO_PROXY);
   prefs.panel_size = P_medium;
   prefs.parse_embedded_css=TRUE;
   prefs.save_dir = dStrdup(PREFS_SAVE_DIR);
   prefs.search_urls = dList_new(16);
   dList_append(prefs.search_urls, dStrdup(PREFS_SEARCH_URL));
   prefs.search_url_idx = 0;
   prefs.show_back = TRUE;
   prefs.show_bookmarks = TRUE;
   prefs.show_clear_url = TRUE;
   prefs.show_extra_warnings = FALSE;
   prefs.show_filemenu=TRUE;
   prefs.show_forw = TRUE;
   prefs.show_help = TRUE;
   prefs.show_home = TRUE;
   prefs.show_msg = TRUE;
   prefs.show_progress_box = TRUE;
   prefs.show_quit_dialog = TRUE;
   prefs.show_reload = TRUE;
   prefs.show_save = TRUE;
   prefs.show_url = TRUE;
   prefs.show_search = TRUE;
   prefs.show_stop = TRUE;
   prefs.show_tools = TRUE;
   prefs.show_tooltip = TRUE;
   prefs.show_ui_tooltip = TRUE;
   prefs.small_icons = FALSE;
   prefs.start_page = a_Url_new(PREFS_START_PAGE, NULL);
   prefs.theme = dStrdup(PREFS_THEME);
   prefs.ui_button_highlight_color = -1;
   prefs.ui_fg_color = -1;
   prefs.ui_main_bg_color = -1;
   prefs.ui_selection_color = -1;
   prefs.ui_tab_active_bg_color = -1;
   prefs.ui_tab_bg_color = -1;
   prefs.ui_tab_active_fg_color = -1;
   prefs.ui_tab_fg_color = -1;
   prefs.ui_text_bg_color = -1;
   prefs.w3c_plus_heuristics = TRUE;

   prefs.penalty_hyphen = 100;
   prefs.penalty_hyphen_2 = 800;
   prefs.penalty_em_dash_left = 800;
   prefs.penalty_em_dash_right = 100;
   prefs.penalty_em_dash_right_2 = 800;
   prefs.stretchability_factor = 100;
}

/*
 *  memory-deallocation
 *  (Call this one at exit time)
 */
void a_Prefs_freeall(void)
{
   int i;

   dFree(prefs.font_cursive);
   dFree(prefs.font_fantasy);
   dFree(prefs.font_monospace);
   dFree(prefs.font_sans_serif);
   dFree(prefs.font_serif);
   a_Url_free(prefs.home);
   dFree(prefs.http_language);
   a_Url_free(prefs.http_proxy);
   dFree(prefs.http_proxyuser);
   dFree(prefs.http_referer);
   dFree(prefs.http_user_agent);
   dFree(prefs.no_proxy);
   dFree(prefs.save_dir);
   for (i = 0; i < dList_length(prefs.search_urls); ++i)
      dFree(dList_nth_data(prefs.search_urls, i));
   dList_free(prefs.search_urls);
   a_Url_free(prefs.start_page);
   dFree(prefs.theme);
}
