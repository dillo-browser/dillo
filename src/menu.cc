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
#include <fltk/Divider.h>

#include "msg.h"
#include "menu.hh"
#include "uicmd.hh"
#include "history.h"
#include "html.hh"

using namespace fltk;

/*
 * Local data
 */

// (This data can be encapsulated inside a class for each popup, but
//  as popups are modal, there's no need).
// Weak reference to the popup's URL
static const DilloUrl *popup_url = NULL;
// Weak reference to the popup's bw
static BrowserWindow *popup_bw = NULL;
// Weak reference to the page's HTML bugs
static const char *popup_bugs = NULL;
// History popup direction (-1 = back, 1 = forward).
static int history_direction = -1; 
// History popup, list of URL-indexes.
static int *history_list = NULL;

/*
 * Local sub class
 * (Used to add the hint for history popup menus)
 */

class NewItem : public Item {
public:
   NewItem (const char* label) : Item(label) {};
   void draw();
};

/*
 * This adds a call to a_UIcmd_set_msg() to show the URL in the status bar
 * TODO: erase the URL on popup close.
 */
void NewItem::draw() {
   DilloUrl *url;

   if (flags() & SELECTED) {
      url = a_History_get_url(history_list[(VOIDP2INT(user_data()))-1]);
      a_UIcmd_set_msg(popup_bw, "%s", URL_STR(url));
   }
   Item::draw();
}


//--------------------------------------------------------------------------
static void Menu_unimplemented_cb(Widget*)
{
   MSG("Menu_unimplemented_cb: click! :-)\n");
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
   MSG("Open URL in new window cb: click! :-)\n");
   a_UIcmd_open_url_nw(popup_bw, popup_url);
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
   a_UIcmd_fullscreen_toggle(popup_bw);
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
 * Save current page
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
 *
 * BUG: assumes that the document is a DilloHtml.
 */
static void Menu_load_images_cb(Widget*, void *user_data)
{
   DilloUrl *pattern = (DilloUrl *) user_data ;

   if (popup_bw && popup_bw->Docs) {
      int i, n;
      n = dList_length(popup_bw->Docs);
      for (i = 0; i < n; i++) {
         a_Html_load_images(dList_nth_data(popup_bw->Docs, i), pattern);
      }
   }
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
   int k = event_button();
   int offset = history_direction * VOIDP2INT(data);

   if (k == 2) {
      /* middle button, open in a new window */
      a_UIcmd_nav_jump(popup_bw, offset, 1);
   } else {
      a_UIcmd_nav_jump(popup_bw, offset, 0);
   }
}

/*
 * Page popup menu (construction & popup)
 */
void a_Menu_page_popup(BrowserWindow *bw, const DilloUrl *url, 
                       const char *bugs_txt, int prefs_load_images)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;
   // Active/inactive control.
   static Item *view_page_bugs_item = 0;
   static Item *load_images_item = 0;

   popup_bw = bw;
   popup_url = url;
   popup_bugs = bugs_txt;

   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&PAGE OPTIONS");
      pm->begin();
       i = new Item("View page Source");
       i->callback(Menu_view_page_source_cb);
       //i->shortcut(CTRL+'n');
       i = view_page_bugs_item = new Item("View page Bugs");
       i->callback(Menu_view_page_bugs_cb);
       i = load_images_item = new Item("Load images");
       i->callback(Menu_load_images_cb);
       i = new Item("Bookmark this page");
       i->callback(Menu_add_bookmark_cb);
       new Divider();    
       i = new Item("&Find Text");
       i->callback(Menu_find_text_cb);
       i->shortcut(CTRL+'f');
       i = new Item("Jump to...");
       i->deactivate();
       new Divider();    
       i = new Item("Save page As...");
       i->callback(Menu_save_page_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   if (bugs_txt == NULL)
      view_page_bugs_item->deactivate();
   else
      view_page_bugs_item->activate();

   if (prefs_load_images == 1)
      load_images_item->deactivate();
   else
      load_images_item->activate();

   // NULL is wildcard
   load_images_item->user_data(NULL);

   // Make the popup a child of the calling UI object
   ((Group *)bw->ui)->add(pm);

   pm->popup();
}

/*
 * Link popup menu (construction & popup)
 */
void a_Menu_link_popup(BrowserWindow *bw, const DilloUrl *url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;

   popup_bw = bw;
   popup_url = url;
   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&LINK OPTIONS");
      //pm->callback(Menu_link_cb, url);
      pm->begin();
       i = new Item("Open Link in New Window");
       i->callback(Menu_open_url_nw_cb);
       i = new Item("Bookmark this Link");
       i->callback(Menu_add_bookmark_cb);
       i = new Item("Copy Link location");
       i->callback(Menu_unimplemented_cb);
       i->deactivate();
       new Divider();    
       i = new Item("Save Link As...");
       i->callback(Menu_save_link_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   // Make the popup a child of the calling UI object
   ((Group *)bw->ui)->add(pm);

   pm->popup();
}

/*
 * Image popup menu (construction & popup)
 */
void a_Menu_image_popup(BrowserWindow *bw, const DilloUrl *url,
                        DilloUrl *link_url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;
   // Active/inactive control.
   static Item *link_menuitem = 0;
   static Item *load_menuitem = 0;

   DilloUrl *userdata_url = a_Url_dup(url);

   popup_bw = bw;
   popup_url = url;
   
   if (!pm) {
      Item *i;
      pm = new PopupMenu(0,0,0,0,"&IMAGE OPTIONS");
      pm->begin();
       i = new Item("Isolate Image");
       i->callback(Menu_open_url_cb);
       i = new Item("Open Image in New Window");
       i->callback(Menu_open_url_nw_cb);
       i = load_menuitem = new Item("Load image");
       i->callback(Menu_load_images_cb);
       i = new Item("Bookmark this Image");
       i->callback(Menu_add_bookmark_cb);
       i = new Item("Copy Image location");
       i->callback(Menu_unimplemented_cb);
       i->deactivate();
       new Divider();
       i = new Item("Save Image As...");
       i->callback(Menu_save_link_cb);
       new Divider();
       i = link_menuitem = new Item("Link menu");
       i->callback(Menu_link_cb);

      pm->type(PopupMenu::POPUP123);
      pm->end();
   }

   // point to this item initially
   pm->item(load_menuitem);

   if (link_url) {
      link_menuitem->user_data(link_url);
      link_menuitem->activate();
   } else {
      link_menuitem->deactivate();
   }

   load_menuitem->user_data(userdata_url);
 
   // Make the popup a child of the calling UI object
   ((Group *)bw->ui)->add(pm);

   pm->popup();

   a_Url_free(userdata_url);
}

/*
 * Bugmeter popup menu (construction & popup)
 */
void a_Menu_bugmeter_popup(BrowserWindow *bw, const DilloUrl *url)
{
   // One menu for every browser window
   static PopupMenu *pm = 0;

   popup_bw = bw;
   popup_url = url;
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

   // Make the popup a child of the calling UI object
   //((Group *)bw->ui)->add(pm);

   pm->popup();
}

/*
 * Navigation History popup menu (construction & popup)
 *
 * direction: {backward = -1, forward = 1}
 */
void a_Menu_history_popup(BrowserWindow *bw, int direction)
{
   // One menu for every browser window
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
       it = new NewItem(a_History_get_title(history_list[i], 1));
       it->callback(Menu_history_cb, (void*)(i+1));
    }
   pm->type(PopupMenu::POPUP123);
   pm->end();

   // Make the popup a child of the calling UI object
   // I don't know whether this is necessary...
   ((Group *)bw->ui)->add(pm);

   pm->popup();
}


void a_Menu_popup_set_url(BrowserWindow *bw, const DilloUrl *url) { }
void a_Menu_popup_set_url2(BrowserWindow *bw, const DilloUrl *url) { }
void a_Menu_popup_clear_url2(void *menu_popup) { }

DilloUrl *a_Menu_popup_get_url(BrowserWindow *bw) { return NULL; }

void a_Menu_pagemarks_new (BrowserWindow *bw) { }
void a_Menu_pagemarks_destroy (BrowserWindow *bw) { }
void a_Menu_pagemarks_add(BrowserWindow *bw, void *page, void *style,
                          int level) { }
void a_Menu_pagemarks_set_text(BrowserWindow *bw, const char *str) { }

