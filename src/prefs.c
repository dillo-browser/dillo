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
#define PREFS_FONT_CURSIVE    "DejaVu Sans" /* TODO: find good default */
#define PREFS_FONT_FANTASY    "DejaVu Sans" /* TODO: find good default */
#define PREFS_FONT_MONOSPACE  "DejaVu Sans Mono"
#define PREFS_SEARCH_URL "http://www.google.com/search?ie=UTF-8&oe=UTF-8&q=%s"
#define PREFS_NO_PROXY        "localhost 127.0.0.1"
#define PREFS_SAVE_DIR        "/tmp/"
#define PREFS_HTTP_REFERER    "host"

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
   prefs.http_proxyuser = NULL;
   prefs.http_referer = dStrdup(PREFS_HTTP_REFERER);
   prefs.limit_text_width = FALSE;
   prefs.load_images=TRUE;
   prefs.load_stylesheets=TRUE;
   prefs.middle_click_drags_page = TRUE;
   prefs.middle_click_opens_new_tab = TRUE;
   prefs.no_proxy = dStrdup(PREFS_NO_PROXY);
   prefs.panel_size = P_medium;
   prefs.parse_embedded_css=TRUE;
   prefs.save_dir = dStrdup(PREFS_SAVE_DIR);
   prefs.search_url = dStrdup(PREFS_SEARCH_URL);
   prefs.show_back = TRUE;
   prefs.show_bookmarks = TRUE;
   prefs.show_clear_url = TRUE;
   prefs.show_extra_warnings = FALSE;
   prefs.show_filemenu=TRUE;
   prefs.show_forw = TRUE;
   prefs.show_home = TRUE;
   prefs.show_msg = TRUE;
   prefs.show_progress_box = TRUE;
   prefs.show_reload = TRUE;
   prefs.show_save = TRUE;
   prefs.show_search = TRUE;
   prefs.show_stop = TRUE;
   prefs.show_tools = TRUE;
   prefs.show_tooltip = TRUE;
   prefs.show_url = TRUE;
   prefs.small_icons = FALSE;
   prefs.start_page = a_Url_new(PREFS_START_PAGE, NULL);
   prefs.w3c_plus_heuristics = TRUE;
}

/*
 *  memory-deallocation
 *  (Call this one at exit time)
 */
void a_Prefs_freeall(void)
{
   dFree(prefs.font_cursive);
   dFree(prefs.font_fantasy);
   dFree(prefs.font_monospace);
   dFree(prefs.font_sans_serif);
   dFree(prefs.font_serif);
   a_Url_free(prefs.home);
   dFree(prefs.http_language);
   dFree(prefs.http_proxy);
   dFree(prefs.http_proxyuser);
   dFree(prefs.http_referer);
   dFree(prefs.no_proxy);
   dFree(prefs.save_dir);
   dFree(prefs.search_url);
   a_Url_free(prefs.start_page);
}
