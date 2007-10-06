#ifndef __PREFS_H__
#define __PREFS_H__

#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DILLO_START_PAGE "about:splash"
#define DILLO_HOME "http://www.dillo.org/"
#define D_GEOMETRY_DEFAULT_WIDTH   640
#define D_GEOMETRY_DEFAULT_HEIGHT  550
#define D_GEOMETRY_DEFAULT_XPOS  -9999
#define D_GEOMETRY_DEFAULT_YPOS  -9999

#define DW_COLOR_DEFAULT_GREY   0xd6d6d6
#define DW_COLOR_DEFAULT_BLACK  0x000000
#define DW_COLOR_DEFAULT_BLUE   0x0000ff
#define DW_COLOR_DEFAULT_PURPLE 0x800080
#define DW_COLOR_DEFAULT_BGND   0xd6d6c0


/* define enumeration values to be returned for specific symbols */
typedef enum {
   DRC_TOKEN_ALLOW_WHITE_BG,
   DRC_TOKEN_BG_COLOR,
   DRC_TOKEN_CONTRAST_VISITED_COLOR,
   DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT,
   DRC_TOKEN_FONT_FACTOR,
   DRC_TOKEN_FORCE_MY_COLORS,
   DRC_TOKEN_FULLWINDOW_START,
   DRC_TOKEN_FW_FONT,
   DRC_TOKEN_GENERATE_SUBMIT,
   DRC_TOKEN_GEOMETRY,
   DRC_TOKEN_HOME,
   DRC_TOKEN_LIMIT_TEXT_WIDTH,
   DRC_TOKEN_LINK_COLOR,
   DRC_TOKEN_NOPROXY,
   DRC_TOKEN_PANEL_SIZE,
   DRC_TOKEN_PROXY,
   DRC_TOKEN_PROXYUSER,
   DRC_TOKEN_SEARCH_URL,
   DRC_TOKEN_SHOW_BACK,
   DRC_TOKEN_SHOW_BOOKMARKS,
   DRC_TOKEN_SHOW_CLEAR_URL,
   DRC_TOKEN_SHOW_EXTRA_WARNINGS,
   DRC_TOKEN_SHOW_FORW,
   DRC_TOKEN_SHOW_HOME,
   DRC_TOKEN_SHOW_MENUBAR,
   DRC_TOKEN_SHOW_MSG,
   DRC_TOKEN_SHOW_PROGRESS_BOX,
   DRC_TOKEN_SHOW_RELOAD,
   DRC_TOKEN_SHOW_SAVE,
   DRC_TOKEN_SHOW_SEARCH,
   DRC_TOKEN_SHOW_STOP,
   DRC_TOKEN_SHOW_TOOLTIP,
   DRC_TOKEN_SHOW_URL,
   DRC_TOKEN_SMALL_ICONS,
   DRC_TOKEN_START_PAGE,
   DRC_TOKEN_TEXT_COLOR,
   DRC_TOKEN_TRANSIENT_DIALOGS,
   DRC_TOKEN_USE_DICACHE,
   DRC_TOKEN_USE_OBLIQUE,
   DRC_TOKEN_VISITED_COLOR,
   DRC_TOKEN_VW_FONT,
   DRC_TOKEN_W3C_PLUS_HEURISTICS
} RcToken_t;

typedef struct _DilloPrefs DilloPrefs;

struct _DilloPrefs {
   int width;
   int height;
   int xpos;
   int ypos;
   DilloUrl *http_proxy;
   char *http_proxyuser;
   char *no_proxy;
   DilloUrl *start_page;
   DilloUrl *home;
   int32_t link_color;
   int32_t visited_color;
   int32_t bg_color;
   int32_t text_color;
   bool_t allow_white_bg;
   bool_t use_oblique;
   bool_t force_my_colors;
   bool_t contrast_visited_color;
   bool_t show_tooltip;
   int panel_size;
   bool_t small_icons;
   bool_t limit_text_width;
   bool_t w3c_plus_heuristics;
   double font_factor;
   bool_t use_dicache;
   bool_t show_back;
   bool_t show_forw;
   bool_t show_home;
   bool_t show_reload;
   bool_t show_save;
   bool_t show_stop;
   bool_t show_bookmarks;
   bool_t show_menubar;
   bool_t show_clear_url;
   bool_t show_url;
   bool_t show_search;
   bool_t show_progress_box;
   bool_t fullwindow_start;
   bool_t transient_dialogs;
   char *vw_fontname;
   char *fw_fontname;
   bool_t generate_submit;
   bool_t enterpress_forces_submit;
   char *search_url;
   bool_t show_msg;
   bool_t show_extra_warnings;
};

/* Global Data */
extern DilloPrefs prefs;

void a_Prefs_init(void);
void a_Prefs_freeall(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PREFS_H__ */
