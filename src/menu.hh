#ifndef __MENU_HH__
#define __MENU_HH__

#include "bw.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Menu_page_popup(BrowserWindow *bw, const DilloUrl *url,
                       bool_t has_bugs, void *v_cssUrls);
void a_Menu_link_popup(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_image_popup(BrowserWindow *bw, const DilloUrl *url,
                        bool_t loaded_img, DilloUrl *page_url,
                        DilloUrl *link_url);
void a_Menu_form_popup(BrowserWindow *bw, const DilloUrl *page_url,
                       void *vform, bool_t showing_hiddens);
void a_Menu_file_popup(BrowserWindow *bw, void *v_wid);
void a_Menu_bugmeter_popup(BrowserWindow *bw, const DilloUrl *url);
void a_Menu_history_popup(BrowserWindow *bw, int x, int y, int direction);
void a_Menu_tools_popup(BrowserWindow *bw, int x, int y);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MENU_HH */

