/*
 * File: menu.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Functions/Methods for menus

#include <stdio.h>
#include <stdarg.h>
#include <fltk/events.h>
#include <fltk/PopupMenu.h>
#include <fltk/Item.h>
#include <fltk/ToggleItem.h>
#include <fltk/Divider.h>

#include "lout/misc.hh"    /* SimpleVector */
#include "msg.h"
#include "menu.hh"
#include "uicmd.hh"
#include "history.h"
#include "html.hh"
#include "ui.hh" // for (UI *)
#include "timeout.hh"

using namespace fltk;

/*
 * Local data
 */

// (This data can be encapsulated inside a class for each popup, but
//  as popups are modal, there's no need).
static DilloUrl *popup_url = NULL;
// Weak reference to the popup's bw
static BrowserWindow *popup_bw = NULL;
// Where to place the filemenu popup
static int popup_x, popup_y;
// History popup direction (-1 = back, 1 = forward).
static int history_direction = -1;
// History popup, list of URL-indexes.
static int *history_list = NULL;

/*
 * Local sub class.
 * Used to add the hint for history popup menus, and to remember
 * the mouse button pressed over a menu item.
 */
class CustItem : public Item {
   int EventButton;
public:
   CustItem (const char* label) : Item(label) { EventButton = 0; };
   int button () { return EventButton; };
   void draw();
   int handle(int e) {
      EventButton = event_button();
      return Item::handle(e);
   }
};

/*
 * This adds a call to a_UIcmd_set_msg() to show the URL in the status bar
 * TODO: erase the URL on popup close.
 */
void CustItem::draw() {
   DilloUrl *url;

   if (flags() & SELECTED) {
      url = a_History_get_url(history_list[(VOIDP2INT(user_data()))-1]);
      a_UIcmd_set_msg(popup_bw, "%s", URL_STR(url));
   }
   Item::draw();
}


//--------------------------------------------------------------------------
/*
 * Static function for File menu callbacks.
 */
static void filemenu_cb(Widget *wid, void *data)
{
   if (strcmp((char*)data, "nw") == 0) {
      UI *ui = (UI*)popup_bw->ui;
      a_UIcmd_browser_window_new(ui->w(), ui->h(), popup_bw);
   } else if (strcmp((char*)data, "nt") == 0) {
      a_UIcmd_open_url_nt(popup_bw, NULL, 1);
   } else if (strcmp((char*)data, "of") == 0) {
      a_UIcmd_open_file(popup_bw);
   } else if (strcmp((char*)data, "ou") == 0) {
      a_UIcmd_focus_location(popup_bw);
   } else if (strcmp((char*)data, "cw") == 0) {
      a_Timeout_add(0.0, a_UIcmd_close_bw, popup_bw);
   } else if (strcmp((char*)data, "ed") == 0) {
      a_Timeout_add(0.0, a_UIcmd_close_all_bw, NULL);
   }
}


static void Menu_copy_urlstr_cb(Widget *)
{
   if (popup_url)
      a_UIcmd_copy_urlstr(popup_bw, URL_STR(popup_url));
}

static void Menu_link_cb(Widget*, void *user_data)
{
   DilloUrl *url = (DilloUrl *) user_data ;
   MSG("Menu_link_cb: click! :-)\n");

   if (url)
      a_Menu_link_popup(popup_bw, url);
}

/*
 * Open URL
 */
static void Menu_open_url_cb(Widget* )
{
   MSG("Open URL cb: click! :-)\n");
   a_UIcmd_open_url(popup_bw, popup_url);
}

/*
 * Open URL in new window
 */
static void Menu_open_url_nw_cb(Widget* )
{
   _MSG("Open URL in new window cb: click! :-)\n");
   a_UIcmd_open_url_nw(popup_bw, popup_url);
}

/*
 * Open URL in new Tab
 */
static void Menu_open_url_nt_cb(Widget* )
{
   int focus = prefs.focus_new_tab ? 1 : 0;
   if (event_state(SHIFT)) focus = !focus;
   a_UIcmd_open_url_nt(popup_bw, popup_url, focus);
}

/*
 * Add bookmark
 */
static void Menu_add_bookmark_cb(Widget* )
{
   a_UIcmd_add_bookmark(popup_bw, popup_url);
}

/*
 * Find text
 */
static void Menu_find_text_cb(Widget* )
{
   ((UI *)popup_bw->ui)->set_findbar_visibility(1);
}

/*
 * Save link
 */
static void Menu_save_link_cb(Widget* )
{
   a_UIcmd_save_link(popup_bw, popup_url);
}

/*
 * Save current page
 */
static void Menu_save_page_cb(Widget* )
{
   a_UIcmd_save(popup_bw);
}

/*
 * View current page source
 */
static void Menu_view_page_source_cb(Widget* )
{
   a_UIcmd_view_page_source(popup_url);
}

/*
 * View current page's bugs
 */
static void Menu_view_page_bugs_cb(Widget* )
{
   a_UIcmd_view_page_bugs(popup_bw);
}

/*
 * Load images on current page that match URL pattern
 */
static void Menu_load_images_cb(Widget*, void *user_data)
{
   DilloUrl *page_url = (DilloUrl *) user_data;
   void *doc = a_Bw_get_url_doc(popup_bw, page_url);

   if (doc)
      a_Html_load_images(doc, popup_url);
}

/*
 * Submit form
 */
static void Menu_form_submit_cb(Widget*, void *v_form)
{
   void *doc = a_Bw_get_url_doc(popup_bw, popup_url);

   if (doc)
      a_Html_form_submit(doc, v_form);
}

/*
 * Reset form
 */
static void Menu_form_reset_cb(Widget*, void *v_form)
{
   void *doc = a_Bw_get_url_doc(popup_bw, popup_url);

   if (doc)
      a_Html_form_reset(doc, v_form);
}

/*
 * Toggle display of 'hidden' form controls.
 */
static void Menu_form_hiddens_cb(Widget *w, void *user_data)
{
   void *v_form = w->parent()->user_data();
   bool visible = *((bool *) user_data);
   void *doc = a_Bw_get_url_doc(popup_bw, popup_url);

   if (doc)
      a_Html_form_display_hiddens(doc, v_form, !visible);
}

static void Menu_stylesheet_cb(Widget *w, void *)
{
   a_UIcmd_open_urlstr(popup_bw, w->label() + 5);
}

/*
 * Validate URL with the W3C
 */
static void Menu_bugmeter_validate_w3c_cb(Widget* )
{
   Dstr *dstr = dStr_sized_new(128);

   dStr_sprintf(dstr, "http://validator.w3.org/check?uri=%s",
                URL_STR(popup_url));
   a_UIcmd_open_urlstr(popup_bw, dstr->str);
   dStr_free(dstr, 1);
}

/*
 * Validate URL with the WDG
 */
static void Menu_bugmeter_validate_wdg_cb(Widget* )
{
   Dstr *dstr = dStr_sized_new(128);

   dStr_sprintf(dstr,
      "http://www.htmlhelp.org/cgi-bin/validate.cgi?url=%s&warnings=yes",
      URL_STR(popup_url));
   a_UIcmd_open_urlstr(popup_bw, dstr->str);
   dStr_free(dstr, 1);
}

/*
 * Show info page for the bug meter
 */
static void Menu_bugmeter_about_cb(Widget* )
{
   a_UIcmd_open_urlstr(popup_bw, "http://www.dillo.org/help/bug_meter.html");
}

/*
 * Navigation History callback.
 * Go to selected URL.
 */
static void Menu_history_cb(Widget *wid, void *data)
{
   int mb = ((CustItem*)wid)->button();
   int offset = history_direction * VOIDP2INT(data);
   DilloUrl *url = a_History_get_url(history_list[VOIDP2INT(data)-1]);

   if (mb == 2) {
      // Middle button, open in a new window/tab
      if (prefs.middle_click_opens_new_tab) {
         int focus = prefs.focus_new_tab ? 1 : 0;
         if (event_state(SHIFT)) focus = !focus;
         a_UIcmd_open_url_nt(popup_bw, url, focus);
      } else {
         a_UIcmd_open_url_nw(popup_bw, url);
      }
   } else {
      a_UIcmd_nav_jump(popup_bw, offset, 0);
   }
}

/*
 * Menus are popped-up from this timeout callback so the events
 * associated with the button are gone when it pops. This way we
 * avoid a segfault when a new page replaces the page that issued
 * the popup menu.
 */
static void Menu_popup_cb(void *data)
{
   ((PopupMenu *)data)->popup();
   a_Timeout_remove();
}

/*
 * Same as above but with coordinates.
 */
static void Menu_popup_cb2(void *data)
{
   Menu *m = (Menu *)data;
   m->value(-1);
   m->popup(Rectangle(popup_x,popup_y,m->w(),m->h()), m->label());
   a_Timeout_remove();
}

/*
 * Page popup menu (construction & popup)
 */
void a_Menu_page_popup(BrowserWindow *bw, const DilloUrl *url,
                       bool_t has_bugs, void *v_cssUrls)
{
   lout::misc::SimpleVector <DilloUrl*> *cssUrls =
                            (lout::misc::SimpleVector <DilloUrl*> *) v_cssUrls;
   Item *i;
   int j;
   // One menu for every browser window
   static PopupMenu *pm = 0;
   // Active/inactive control.
   static Item *view_page_bugs_item = 0;
   static ItemGroup *stylesheets = 0;

   popup_bw = bw;
   a_Url_free(popup_url);
   popup_url = a_Url_dup(url);

   if (!pm) {
      pm = new PopupMenu(0,0,0,0,"&PAGE OPTIONS");
      pm->begin();
       i = new Item("View page Source");
       i->callback(Menu_view_page_source_cb);
       i = view_page_bugs_item = new Item("View page Bugs");
       i->callback(Menu_view_page_bugs_cb);
       stylesheets = new ItemGroup("View Stylesheets");
       new Divider();
       i = new Item("Bookmark this page");
       i->callback(Menu_add_bookmark_cb);
       new Divider();
       i = new Item("Find Text");
       i->callback(Menu_find_text_cb);
       //i->shortcut(CTRL+'f');
       i = new Item("Jump to...");
       i->deactivate();
       new Divider();
       i = new Item("Save page As...");
       i->callback(Menu_save_page_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   if (has_bugs == TRUE)
      view_page_bugs_item->activate();
   else
      view_page_bugs_item->deactivate();

   int n = stylesheets->children();
   for (j = 0; j < n; j++) {
      /* get rid of the old ones */
      Widget *child = stylesheets->child(0);
      dFree((char *)child->label());
      delete child;
   }

   if (cssUrls && cssUrls->size () > 0) {
      stylesheets->activate();
      for (j = 0; j < cssUrls->size(); j++) {
         DilloUrl *url = cssUrls->get(j);
         const char *action = "View ";
         /* may want ability to Load individual unloaded stylesheets as well */
         char *label = dStrconcat(action, URL_STR(url), NULL);
         i = new Item(label);
         i->set_flag(RAW_LABEL);
         i->callback(Menu_stylesheet_cb);
         stylesheets->add(i);
      }
   } else {
      stylesheets->deactivate();
   }

   a_Timeout_add(0.0, Menu_popup_cb, (void *)pm);
}

/*
 * Link popup menu (construction & popup)
 */
void a_Menu_link_popup(BrowserWindow *bw, const DilloUrl *url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;

   popup_bw = bw;
   a_Url_free(popup_url);
   popup_url = a_Url_dup(url);
   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&LINK OPTIONS");
      //pm->callback(Menu_link_cb, url);
      pm->begin();
       i = new Item("Open Link in New Window");
       i->callback(Menu_open_url_nw_cb);
       i = new Item("Open Link in New Tab");
       i->callback(Menu_open_url_nt_cb);
       new Divider();
       i = new Item("Bookmark this Link");
       i->callback(Menu_add_bookmark_cb);
       i = new Item("Copy Link location");
       i->callback(Menu_copy_urlstr_cb);
       new Divider();
       i = new Item("Save Link As...");
       i->callback(Menu_save_link_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   a_Timeout_add(0.0, Menu_popup_cb, (void *)pm);
}

/*
 * Image popup menu (construction & popup)
 */
void a_Menu_image_popup(BrowserWindow *bw, const DilloUrl *url,
                        bool_t loaded_img, DilloUrl *page_url,
                        DilloUrl *link_url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;
   // Active/inactive control.
   static Item *link_menuitem = 0;
   static Item *load_img_menuitem = 0;
   static DilloUrl *popup_page_url = NULL;
   static DilloUrl *popup_link_url = NULL;

   popup_bw = bw;
   a_Url_free(popup_url);
   popup_url = a_Url_dup(url);
   a_Url_free(popup_page_url);
   popup_page_url = a_Url_dup(page_url);
   a_Url_free(popup_link_url);
   popup_link_url = a_Url_dup(link_url);

   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&IMAGE OPTIONS");
      pm->begin();
       i = new Item("Isolate Image");
       i->callback(Menu_open_url_cb);
       i = new Item("Open Image in New Window");
       i->callback(Menu_open_url_nw_cb);
       i = new Item("Open Image in New Tab");
       i->callback(Menu_open_url_nt_cb);
       new Divider();
       i = load_img_menuitem = new Item("Load image");
       i->callback(Menu_load_images_cb);
       i = new Item("Bookmark this Image");
       i->callback(Menu_add_bookmark_cb);
       i = new Item("Copy Image location");
       i->callback(Menu_copy_urlstr_cb);
       new Divider();
       i = new Item("Save Image As...");
       i->callback(Menu_save_link_cb);
       new Divider();
       i = link_menuitem = new Item("Link menu");
       i->callback(Menu_link_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   if (loaded_img) {
      load_img_menuitem->deactivate();
   } else {
      load_img_menuitem->activate();
      load_img_menuitem->user_data(popup_page_url);
   }

   if (link_url) {
      link_menuitem->user_data(popup_link_url);
      link_menuitem->activate();
   } else {
      link_menuitem->deactivate();
   }

   a_Timeout_add(0.0, Menu_popup_cb, (void *)pm);
}

/*
 * Form popup menu (construction & popup)
 */
void a_Menu_form_popup(BrowserWindow *bw, const DilloUrl *page_url,
                       void *formptr, bool_t hidvis)
{
   static PopupMenu *pm = 0;
   static Item *hiddens_item = 0;
   static bool hiddens_visible;

   popup_bw = bw;
   a_Url_free(popup_url);
   popup_url = a_Url_dup(page_url);
   if (!pm) {
     Item *i;
      pm = new PopupMenu(0,0,0,0,"FORM OPTIONS");
      pm->add(i = new Item("Submit form"));
      i->callback(Menu_form_submit_cb);
      pm->add(i = new Item("Reset form"));
      i->callback(Menu_form_reset_cb);
      pm->add(hiddens_item = new Item(""));
      hiddens_item->callback(Menu_form_hiddens_cb);
      hiddens_item->user_data(&hiddens_visible);
      pm->type(PopupMenu::POPUP123);
   }
   pm->user_data(formptr);

   hiddens_visible = hidvis;
   hiddens_item->label(hiddens_visible ? "Hide hiddens": "Show hiddens");

   a_Timeout_add(0.0, Menu_popup_cb, (void *)pm);
}

/*
 * File popup menu (construction & popup)
 */
void a_Menu_file_popup(BrowserWindow *bw, void *v_wid)
{
   UI *ui = (UI *)bw->ui;
   Widget *wid = (Widget*)v_wid;
   // One menu for every browser window
   static PopupMenu *pm = 0;

   popup_bw = bw;
   popup_x = wid->x();
   popup_y = wid->y() + wid->h() +
             // WORKAROUND: ?? wid->y() doesn't count tabs ??
             (((Group*)ui->tabs())->children() > 1 ? 20 : 0);
   a_Url_free(popup_url);
   popup_url = NULL;

   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"File");
      pm->begin();
       i = new Item("New Window", CTRL+'n', filemenu_cb, (void*)"nw");
       i = new Item("New Tab", CTRL+'t', filemenu_cb, (void*)"nt");
       new Divider();
       i = new Item("Open File...", CTRL+'o', filemenu_cb, (void*)"of");
       i = new Item("Open URL...", CTRL+'l', filemenu_cb, (void*)"ou");
       i = new Item("Close", CTRL+'q', filemenu_cb, (void*)"cw");
       new Divider();
       i = new Item("Exit Dillo", ALT+'q', filemenu_cb, (void*)"ed");
      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   pm->label(wid->visible() ? NULL : "File");
   a_Timeout_add(0.0, Menu_popup_cb2, (void *)pm);
}

/*
 * Bugmeter popup menu (construction & popup)
 */
void a_Menu_bugmeter_popup(BrowserWindow *bw, const DilloUrl *url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;

   popup_bw = bw;
   a_Url_free(popup_url);
   popup_url = a_Url_dup(url);
   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&BUG METER OPTIONS");
      pm->begin();
       i = new Item("Validate URL with W3C");
       i->callback(Menu_bugmeter_validate_w3c_cb);
       i = new Item("Validate URL with WDG");
       i->callback(Menu_bugmeter_validate_wdg_cb);
       new Divider();
       i = new Item("About Bug Meter...");
       i->callback(Menu_bugmeter_about_cb);
      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   pm->popup();
}

/*
 * Navigation History popup menu (construction & popup)
 *
 * direction: {backward = -1, forward = 1}
 */
void a_Menu_history_popup(BrowserWindow *bw, int direction)
{
   static PopupMenu *pm = 0;
   Item *it;
   int i;

   popup_bw = bw;
   history_direction = direction;

   // TODO: hook popdown event with delete or similar.
   if (pm)
      delete(pm);
   if (history_list)
      dFree(history_list);

   if (direction == -1) {
      pm = new PopupMenu(0,0,0,0, "&PREVIOUS PAGES");
   } else {
      pm = new PopupMenu(0,0,0,0, "&FOLLOWING PAGES");
   }

   // Get a list of URLs for this popup
   history_list = a_UIcmd_get_history(bw, direction);

   pm->begin();
    for (i = 0; history_list[i] != -1; i += 1) {
       // TODO: restrict title size
       it = new CustItem(a_History_get_title(history_list[i], 1));
       it->callback(Menu_history_cb, INT2VOIDP(i+1));
    }
   pm->type(PopupMenu::POPUP123);
   pm->end();

   pm->popup();
}

/*
 * Toggle use of remote stylesheets
 */
static void Menu_remote_css_cb(Widget *wid)
{
   _MSG("Menu_remote_css_cb\n");
   prefs.load_stylesheets = wid->state() ? 1 : 0;
   a_UIcmd_repush(popup_bw);
}

/*
 * Toggle use of embedded CSS style
 */
static void Menu_embedded_css_cb(Widget *wid)
{
   prefs.parse_embedded_css = wid->state() ? 1 : 0;
   a_UIcmd_repush(popup_bw);
}

/*
 * Toggle loading of images -- and load them if enabling.
 */
static void Menu_imgload_toggle_cb(Widget *wid)
{
   if ((prefs.load_images = wid->state() ? 1 : 0)) {
      void *doc = a_Bw_get_current_doc(popup_bw);

      if (doc) {
         DilloUrl *pattern = NULL;
         a_Html_load_images(doc, pattern);
      }
   }
}

/*
 * Tools popup menu (construction & popup)
 */
void a_Menu_tools_popup(BrowserWindow *bw, void *v_wid)
{
   // One menu shared by every browser window
   static PopupMenu *pm = NULL;
   Widget *wid = (Widget*)v_wid;
   Item *it;

   popup_bw = bw;

   if (!pm) {
      pm = new PopupMenu(0,0,0,0, "TOOLS");
      pm->begin();
       it = new ToggleItem("Use remote CSS");
       it->callback(Menu_remote_css_cb);
       it->state(prefs.load_stylesheets);
       it = new ToggleItem("Use embedded CSS");
       it->callback(Menu_embedded_css_cb);
       it->state(prefs.parse_embedded_css);
       new Divider();
       it = new ToggleItem("Load images");
       it->callback(Menu_imgload_toggle_cb);
       it->state(prefs.load_images);
       pm->type(PopupMenu::POPUP13);
      pm->end();
   }
   //pm->popup();
   pm->value(-1);
   ((Menu*)pm)->popup(Rectangle(0,wid->h(),pm->w(),pm->h()));
}

