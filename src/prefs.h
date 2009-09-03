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

#ifndef __PREFS_H__
#define __PREFS_H__

#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PREFS_GEOMETRY_DEFAULT_WIDTH   780
#define PREFS_GEOMETRY_DEFAULT_HEIGHT  580
#define PREFS_GEOMETRY_DEFAULT_XPOS  -9999
#define PREFS_GEOMETRY_DEFAULT_YPOS  -9999
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

/* Panel sizes */
enum { P_tiny = 0, P_small, P_medium, P_large };

typedef struct _DilloPrefs DilloPrefs;

struct _DilloPrefs {
   int width;
   int height;
   int xpos;
   int ypos;
   char *http_language;
   DilloUrl *http_proxy;
   char *http_proxyuser;
   char *http_referer;
   char *no_proxy;
   DilloUrl *start_page;
   DilloUrl *home;
   bool_t allow_white_bg;
   bool_t contrast_visited_color;
   bool_t show_tooltip;
   int panel_size;
   bool_t small_icons;
   bool_t limit_text_width;
   bool_t w3c_plus_heuristics;
   bool_t focus_new_tab;
   double font_factor;
   int32_t font_max_size;
   int32_t font_min_size;
   bool_t show_back;
   bool_t show_forw;
   bool_t show_home;
   bool_t show_reload;
   bool_t show_save;
   bool_t show_stop;
   bool_t show_bookmarks;
   bool_t show_tools;
   bool_t show_filemenu;
   bool_t show_clear_url;
   bool_t show_url;
   bool_t show_search;
   bool_t show_progress_box;
   bool_t fullwindow_start;
   bool_t load_images;
   bool_t load_stylesheets;
   bool_t parse_embedded_css;
   int32_t buffered_drawing;
   char *font_serif;
   char *font_sans_serif;
   char *font_cursive;
   char *font_fantasy;
   char *font_monospace;
   bool_t enterpress_forces_submit;
   bool_t middle_click_opens_new_tab;
   char *search_url;
   char *save_dir;
   bool_t show_msg;
   bool_t show_extra_warnings;
   bool_t middle_click_drags_page;
};

/* Global Data */
extern DilloPrefs prefs;

void a_Prefs_init(void);
void a_Prefs_freeall(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PREFS_H__ */
