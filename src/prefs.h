#ifndef __PREFS_H__
#define __PREFS_H__

#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define D_GEOMETRY_DEFAULT_WIDTH   780
#define D_GEOMETRY_DEFAULT_HEIGHT  580
#define D_GEOMETRY_DEFAULT_XPOS  -9999
#define D_GEOMETRY_DEFAULT_YPOS  -9999

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
   int32_t link_color;
   int32_t visited_color;
   int32_t bg_color;
   int32_t text_color;
   bool_t allow_white_bg;
   bool_t force_my_colors;
   bool_t contrast_visited_color;
   bool_t standard_widget_colors;
   bool_t show_tooltip;
   int panel_size;
   bool_t small_icons;
   bool_t limit_text_width;
   bool_t w3c_plus_heuristics;
   bool_t focus_new_tab;
   double font_factor;
   bool_t show_back;
   bool_t show_forw;
   bool_t show_home;
   bool_t show_reload;
   bool_t show_save;
   bool_t show_stop;
   bool_t show_bookmarks;
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
