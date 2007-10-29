#ifndef __MENU_HH__
#define __MENU_HH__

#include "bw.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Menu_page_popup(BrowserWindow *bw, const DilloUrl *url,
                       const char *bugs_txt);
void a_Menu_link_popup(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_image_popup(BrowserWindow *bw, const DilloUrl *url,
                        DilloUrl *link_url);
void a_Menu_bugmeter_popup(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_history_popup(BrowserWindow *bw, int direction);

//---------------------
void a_Menu_popup_set_url(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_popup_set_url2(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_popup_clear_url2(void *menu_popup);

DilloUrl *a_Menu_popup_get_url(BrowserWindow *bw);

void a_Menu_pagemarks_new (BrowserWindow *bw);
void a_Menu_pagemarks_destroy (BrowserWindow *bw);
void a_Menu_pagemarks_add(BrowserWindow *bw, void *page, void *style,
                          int level);
void a_Menu_pagemarks_set_text(BrowserWindow *bw, const char *str);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MENU_HH */

