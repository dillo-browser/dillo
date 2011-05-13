/*
 * File: uicmd.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Functions/Methods for commands triggered from the UI


#include <stdio.h>
#include <stdarg.h>
#include <math.h>       /* for rint */

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Wizard.H>
#include <FL/Fl_Box.H>
#include <FL/names.h>

#include "paths.hh"
#include "keys.hh"
#include "ui.hh"
#include "uicmd.hh"
#include "timeout.hh"
#include "utf8.hh"
#include "menu.hh"
#include "dialog.hh"
#include "xembed.hh"
#include "bookmark.h"
#include "history.h"
#include "msg.h"
#include "prefs.h"

#include "dw/fltkviewport.hh"

#include "nav.h"

#define DEFAULT_TAB_LABEL "Dillo"

// Handy macro
#define BW2UI(bw) ((UI*)((bw)->ui))

// Platform idependent part
using namespace dw::core;
// FLTK related
using namespace dw::fltk;


/*
 * Local data
 */
static char *save_dir = NULL;
static UI *Gui;

/*
 * Forward declarations
 */
static BrowserWindow *UIcmd_tab_new(CustTabs *tabs, UI *old_ui, int focus);

//----------------------------------------------------------------------------

/*
 * CustTabs ---------------------------------------------------------------
 */

/*
 * stores the respective UI pointer
 */
class CustTabButton : public Fl_Button {
   UI *ui_;
public:
   CustTabButton (int x,int y,int w,int h, const char* label = 0) :
      Fl_Button (x,y,w,h,label) { ui_ = NULL; };
   void ui(UI *pui) { ui_ = pui; }
   UI *ui(void) { return ui_; }
};

/*
 * Allows fine control of the tabbed interface
 */
class CustTabs : public CustGroup {
   int tab_w, tab_h, tab_n;
   Fl_Wizard *Wizard;
   int tabcolor_inactive, tabcolor_active, curtab_idx;
public:
   CustTabs (int ww, int wh, int th, const char *lbl=0) :
      CustGroup(0,0,ww,th,lbl) {
      tab_w = 80, tab_h = th, tab_n = 0, curtab_idx = -1;
      tabcolor_active = FL_DARK_CYAN; tabcolor_inactive = 206;
      Fl_Box *w = new Fl_Box(0,0,0,0,"i n v i s i b l e");
      w->box(FL_NO_BOX);
      resizable(0);
      box(FL_FLAT_BOX);
      end();

      Wizard = new Fl_Wizard(0,tab_h,ww,wh-tab_h);
      Wizard->end();
   };
   int handle(int e);
   UI *add_new_tab(UI *old_ui, int focus);
   void remove_tab(UI *ui);
   Fl_Wizard *wizard(void) { return Wizard; }
   int get_btn_idx(UI *ui);
   int num_tabs() { return (children() - 1); } // substract invisible box
   void switch_tab(CustTabButton *cbtn);
   void prev_tab(void);
   void next_tab(void);

   void set_tab_label(UI *ui, const char *title);
};

/*
 * Callback for mouse click
 */
static void tab_btn_cb (Fl_Widget *w, void *cb_data)
{
   CustTabButton *btn = (CustTabButton*) w;
   CustTabs *tabs = (CustTabs*) cb_data;
   int b = Fl::event_button();

   if (b == FL_LEFT_MOUSE) {
      tabs->switch_tab(btn);
   } else if ((b == FL_RIGHT_MOUSE && prefs.right_click_closes_tab) ||
              (b == FL_MIDDLE_MOUSE && !prefs.right_click_closes_tab)) {
      // TODO: just an example, not necessarily final
      a_UIcmd_close_bw(a_UIcmd_get_bw_by_widget(btn->ui()));
   }
}

int CustTabs::handle(int e)
{
   int ret = 0;

   _MSG("CustTabs::handle e=%s\n", fl_eventnames[e]);
   if (e == FL_KEYBOARD) {
      return 0; // Receive as shortcut
   } else if (e == FL_SHORTCUT) {
      UI *ui = (UI*)wizard()->value();
      BrowserWindow *bw = a_UIcmd_get_bw_by_widget(ui);
      KeysCommand_t cmd = Keys::getKeyCmd();
      if (Fl::event_key() == FL_Escape) {
         // Hide findbar if present
         ui->findbar_toggle(0);
         ret = 1;
      } else if (cmd == KEYS_NOP) {
         // Do nothing
      } else if (cmd == KEYS_NEW_TAB) {
         a_UIcmd_open_url_nt(bw, NULL, 1);
         ret = 1;
      } else if (cmd == KEYS_CLOSE_TAB) {
         a_UIcmd_close_bw(bw);
         ret = 1;
      } else if (cmd == KEYS_LEFT_TAB) {
         MSG("CustTabs::handle KEYS_LEFT_TAB\n");
         ret = 1;
      } else if (cmd == KEYS_RIGHT_TAB) {
         MSG("CustTabs::handle KEYS_RIGHT_TAB\n");
         ret = 1;
      } else if (cmd == KEYS_NEW_WINDOW) {
         a_UIcmd_open_url_nw(bw, NULL);
         ret = 1;
      } else if (cmd == KEYS_FULLSCREEN) {
         MSG("CustTabs::handle KEYS_FULLSCREEN\n");
         ret = 1;
      } else if (cmd == KEYS_CLOSE_ALL) {
         a_Timeout_add(0.0, a_UIcmd_close_all_bw, NULL);
         ret = 1;
      }

   } else if (e == FL_KEYUP) {
      int k = Fl::event_key();
      // We're only interested in some flags
      unsigned modifier = Fl::event_state() & (FL_SHIFT | FL_CTRL | FL_ALT);
      if (k == FL_Up || k == FL_Down || k == FL_Tab) {
         ;
      } else if (k == FL_Left || k == FL_Right) {
         if (modifier == FL_SHIFT) {
            (k == FL_Left) ? prev_tab() : next_tab();
            ret = 1;
         }
      }
   }

   return (ret) ? ret : CustGroup::handle(e);
}

/*
 * Create a new tab with its own UI
 */
UI *CustTabs::add_new_tab(UI *old_ui, int focus)
{
   char tab_label[64];

   current(0);
   UI *new_ui = new UI(0,tab_h,Wizard->w(),Wizard->h(),0,old_ui);
   new_ui->tabs(this);
   Wizard->add(new_ui);
   new_ui->show();

   snprintf(tab_label, 64,"ctab%d", ++tab_n);
   CustTabButton *btn = new CustTabButton(num_tabs()*tab_w,0,tab_w,tab_h);
   btn->align(FL_ALIGN_INSIDE|FL_ALIGN_CLIP);
   btn->copy_label(tab_label);
   btn->clear_visible_focus();
   btn->box(FL_PLASTIC_ROUND_UP_BOX);
   btn->color(focus ? tabcolor_active : tabcolor_inactive);
   btn->ui(new_ui);
   add(btn);
   btn->redraw();
   btn->callback(tab_btn_cb, this);

   if (focus)
      switch_tab(btn);
   rearrange();

   return new_ui;
}

/*
 * Remove tab by UI
 */
void CustTabs::remove_tab(UI *ui)
{
   CustTabButton *btn;

   // remove label button
   int idx = get_btn_idx(ui);
   btn = (CustTabButton*)child(idx);
   idx > 1 ? prev_tab() : next_tab();

   // WORKAROUND: with two tabs, closing the non-focused one, doesn't
   // delete it from screen. This hide() call makes it work.  --Jcid
   btn->hide();

   remove(idx);
   delete btn;
   rearrange();
   redraw();

   Wizard->remove(ui);
   delete(ui);

   if (num_tabs() == 0) {
      window()->hide();
      // TODO: free memory
      //delete window();
   }
}

int CustTabs::get_btn_idx(UI *ui)
{
   for (int i = 1; i <= num_tabs(); ++i) {
      CustTabButton *btn = (CustTabButton*)child(i);
      if (btn->ui() == ui)
         return i;
   }
   return -1;
}

void CustTabs::switch_tab(CustTabButton *cbtn)
{
   int idx;
   CustTabButton *btn;
   UI *old_ui = (UI*)Wizard->value();

   if (cbtn->ui() != old_ui) {
      // Set old tab label to normal color
      if ((idx = get_btn_idx(old_ui)) > 0) {
         btn = (CustTabButton*)child(idx);
         btn->color(tabcolor_inactive);
         btn->redraw();
      }
      Wizard->value(cbtn->ui());
      cbtn->color(tabcolor_active);
      cbtn->redraw();
   }
}

void CustTabs::prev_tab()
{
   int idx;

   if ((idx = get_btn_idx((UI*)Wizard->value())) > 1)
      switch_tab( (CustTabButton*)child(idx-1) );
}

void CustTabs::next_tab()
{
   int idx;

   if ((idx = get_btn_idx((UI*)Wizard->value())) > 0 && idx < num_tabs())
      switch_tab( (CustTabButton*)child(idx+1) );
}

/*
 * Set this UI's tab button label
 */
void CustTabs::set_tab_label(UI *ui, const char *label)
{
   char title[128];
   int idx = get_btn_idx(ui);
 
   if (idx > 0) {
      // Make a label for this tab
      size_t tab_chars = 7, label_len = strlen(label);

      if (label_len > tab_chars)
         tab_chars = a_Utf8_end_of_char(label, tab_chars - 1) + 1;
      snprintf(title, tab_chars + 1, "%s", label);
      if (label_len > tab_chars)
         snprintf(title + tab_chars, 4, "...");

      // Avoid unnecessary redraws
      if (strcmp(child(idx)->label(), title)) {
         child(idx)->copy_label(title);
      }
   }
}


//----------------------------------------------------------------------------

static void win_cb (Fl_Widget *w, void *cb_data) {
   int choice = 1;
   CustTabs *tabs = (CustTabs*) cb_data;

   if (tabs->num_tabs() > 1)
      choice = a_Dialog_choice5("Window contains more than one tab.",
                                "Close", "Cancel", NULL, NULL, NULL);
   if (choice == 1)
      while (tabs->num_tabs())
         a_UIcmd_close_bw(a_UIcmd_get_bw_by_widget(tabs->wizard()->value()));
}

/*
 * Given a UI or UI child widget, return its bw.
 */
BrowserWindow *a_UIcmd_get_bw_by_widget(void *v_wid)
{
   BrowserWindow *bw;
   for (int i = 0; i < a_Bw_num(); ++i) {
      bw = a_Bw_get(i);
      if (((UI*)bw->ui)->contains((Fl_Widget*)v_wid)) {
         return bw;
      }
   }
   return NULL;
}

/*
 * FLTK regards SHIFT + {Left, Right} as navigation keys.
 * Special handling is required to override it. Here we route
 * these events directly to the recipient.
 * TODO: focus is not remembered correctly.
 */
void a_UIcmd_send_event_to_tabs_by_wid(int e, void *v_wid)
{
   BrowserWindow *bw = a_UIcmd_get_bw_by_widget(v_wid);
   UI *ui = (UI*)bw->ui;
   if (ui->tabs())
      ui->tabs()->handle(e);
}

/*
 * Create a new UI and its associated BrowserWindow data structure.
 * Use style from v_ui. If non-NULL it must be of type UI*.
 */
BrowserWindow *a_UIcmd_browser_window_new(int ww, int wh,
                                          uint32_t xid, const void *vbw)
{
   BrowserWindow *old_bw = (BrowserWindow*)vbw;
   BrowserWindow *new_bw = NULL;
   UI *old_ui = old_bw ? BW2UI(old_bw) : NULL;
   Fl_Window *win;

   if (ww <= 0 || wh <= 0) {
      // Set default geometry from dillorc.
      ww = prefs.width;
      wh = prefs.height;
   }

   if (xid)
      win = new Xembed(xid, ww, wh);
   else if (prefs.buffered_drawing != 2)
      win = new Fl_Window(ww, wh);
   else
      win = new Fl_Double_Window(ww, wh);

   win->box(FL_NO_BOX);
   CustTabs *DilloTabs = new CustTabs(ww, wh, 16);
   win->end();

   int focus = 1;
   new_bw = UIcmd_tab_new(DilloTabs, old_ui, focus);
   win->resizable(Gui);
   win->show();

   if (old_bw == NULL && prefs.xpos >= 0 && prefs.ypos >= 0) {
      // position the first window according to preferences
      DilloTabs->window()->position(prefs.xpos, prefs.ypos);
   }

   win->callback(win_cb, DilloTabs);

   return new_bw;
}

/*
 * Create a new Tab button, UI and its associated BrowserWindow data
 * structure.
 */
static BrowserWindow *UIcmd_tab_new(CustTabs *tabs, UI *old_ui, int focus)
{
   _MSG(" UIcmd_tab_new\n");

   // Create and set the UI
   UI *new_ui = tabs->add_new_tab(old_ui, focus);
   Gui = new_ui;

   // Now create the Dw render layout and viewport
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);
   style::Color *bgColor = style::Color::create (layout, prefs.bg_color);
   layout->setBgColor (bgColor);

   // set_render_layout() sets the proper viewport size
   FltkViewport *viewport = new FltkViewport (0, 0, 0, 1);
   viewport->setBufferedDrawing (prefs.buffered_drawing ? true : false);
   layout->attachView (viewport);
   new_ui->set_render_layout(viewport);
   viewport->setScrollStep((int) rint(28.0 * prefs.font_factor));

   // Now, create a new browser window structure
   BrowserWindow *new_bw = a_Bw_new();

   // Reference the UI from the bw
   new_bw->ui = (void *)new_ui;
   // Copy the layout pointer into the bw data
   new_bw->render_layout = (void*)layout;

   // WORKAROUND: see findbar_toggle()
   new_ui->findbar_toggle(0);

   return new_bw;
}

/*
 * Close one browser window
 */
void a_UIcmd_close_bw(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;
   UI *ui = BW2UI(bw);
   Layout *layout = (Layout*)bw->render_layout;

   MSG("a_UIcmd_close_bw\n");
   a_Bw_stop_clients(bw, BW_Root + BW_Img + BW_Force);
   delete(layout);
   if (ui->tabs()) {
      ui->tabs()->remove_tab(ui);
   }
   a_Bw_free(bw);
}

/*
 * Close all the browser windows
 */
void a_UIcmd_close_all_bw(void *)
{
   BrowserWindow *bw;
   int choice = 1;

   if (a_Bw_num() > 1)
      choice = a_Dialog_choice5("More than one open tab or window.",
         "Quit", "Cancel", NULL, NULL, NULL);
   if (choice == 1)
      while ((bw = a_Bw_get(0)))
         a_UIcmd_close_bw((void*)bw);
}

/*
 * Open a new URL in the given browser window.
 *
 * our custom "file:" URIs are normalized here too.
 */
void a_UIcmd_open_urlstr(void *vbw, const char *urlstr)
{
   char *new_urlstr;
   DilloUrl *url;
   int ch;
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if (urlstr && *urlstr) {
      /* Filter URL string */
      new_urlstr = a_Url_string_strip_delimiters(urlstr);

      if (!dStrncasecmp(new_urlstr, "file:", 5)) {
         /* file URI */
         ch = new_urlstr[5];
         if (!ch || ch == '.') {
            url = a_Url_new(Paths::getOldWorkingDir(), "file:");
         } else if (ch == '~') {
            url = a_Url_new(dGethomedir(), "file:");
         } else {
            url = a_Url_new(new_urlstr, "file:");
         }

      } else {
         /* common case */
         url = a_Url_new(new_urlstr, NULL);
      }
      dFree(new_urlstr);

      if (url) {
         a_UIcmd_open_url(bw, url);
         a_Url_free(url);
      }
   }
}

/*
 * Open a new URL in the given browser window
 */
void a_UIcmd_open_url(BrowserWindow *bw, const DilloUrl *url)
{
   if (url) {
      a_Nav_push(bw, url, NULL);
   } else {
      // Used to start a bw with a blank screen
      BW2UI(bw)->focus_location();
      a_UIcmd_set_buttons_sens(bw);
   }
#if 0
   if (BW2UI(bw)->get_panelmode() == UI_TEMPORARILY_SHOW_PANELS)
      BW2UI(bw)->set_panelmode(UI_HIDDEN);
   a_UIcmd_focus_main_area(bw);
#endif
}

static void UIcmd_open_url_nbw(BrowserWindow *new_bw, const DilloUrl *url)
{
   /* When opening a new BrowserWindow (tab or real window) we focus
    * Location if we don't yet have an URL, main otherwise.
    */
   if (url) {
      a_Nav_push(new_bw, url, NULL);
      BW2UI(new_bw)->focus_main();
   } else {
      BW2UI(new_bw)->focus_location();
      a_UIcmd_set_buttons_sens(new_bw);
   }
}

/*
 * Open a new URL in a new browser window
 */
void a_UIcmd_open_url_nw(BrowserWindow *bw, const DilloUrl *url)
{
   int w, h;
   BrowserWindow *new_bw;

   a_UIcmd_get_wh(bw, &w, &h);
   new_bw = a_UIcmd_browser_window_new(w, h, 0, bw);

   UIcmd_open_url_nbw(new_bw, url);
}

/*
 * Open a new URL in a new tab in the same browser window
 */
void a_UIcmd_open_url_nt(void *vbw, const DilloUrl *url, int focus)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;
   BrowserWindow *new_bw = UIcmd_tab_new(BW2UI(bw)->tabs(),
                                         bw ? BW2UI(bw) : NULL, focus);
   UIcmd_open_url_nbw(new_bw, url);
}

/*
 * Send the browser back to previous page
 */
void a_UIcmd_back(void *vbw)
{
   a_Nav_back((BrowserWindow*)vbw);
}

/*
 * Popup the navigation menu of the Back button
 */
void a_UIcmd_back_popup(void *vbw)
{
   a_Menu_history_popup((BrowserWindow*)vbw, -1);
}

/*
 * Send the browser to next page in the history list
 */
void a_UIcmd_forw(void *vbw)
{
   a_Nav_forw((BrowserWindow*)vbw);
}

/*
 * Popup the navigation menu of the Forward button
 */
void a_UIcmd_forw_popup(void *vbw)
{
   a_Menu_history_popup((BrowserWindow*)vbw, 1);
}

/*
 * Send the browser to home URL
 */
void a_UIcmd_home(void *vbw)
{
   a_UIcmd_open_url((BrowserWindow*)vbw, prefs.home);
}

/*
 * Reload current URL
 */
void a_UIcmd_reload(void *vbw)
{
   a_Nav_reload((BrowserWindow*)vbw);
}

/*
 * Repush current URL
 */
void a_UIcmd_repush(void *vbw)
{
   a_Nav_repush((BrowserWindow*)vbw);
}

/*
 * Zero-delay URL redirection.
 */
void a_UIcmd_redirection0(void *vbw, const DilloUrl *url)
{
   a_Nav_redirection0((BrowserWindow*)vbw, url);
}

/*
 * Return a suitable filename for a given URL path.
 */
static char *UIcmd_make_save_filename(const char *pathstr)
{
   size_t MaxLen = 64;
   char *FileName, *newname, *o, *n;
   const char *name, *dir = a_UIcmd_get_save_dir();

   if ((name = strrchr(pathstr, '/'))) {
      if (strlen(++name) > MaxLen) {
         name = name + strlen(name) - MaxLen;
      }
      /* Replace %20 and ' ' with '_' in Filename */
      o = n = newname = dStrdup(name);
      for (int i = 0; o[i]; i++) {
         *n++ = (o[i] == ' ') ? '_' :
                (o[i] == '%' && o[i+1] == '2' && o[i+2] == '0') ?
                i+=2, '_' : o[i];
      }
      *n = 0;
      FileName = dStrconcat(dir ? dir : "", newname, NULL);
      dFree(newname);
   } else {
      FileName = dStrconcat(dir ? dir : "", pathstr, NULL);
   }
   return FileName;
}

/*
 * Get the default directory for saving files.
 */
const char *a_UIcmd_get_save_dir()
{
   return save_dir;
}

/*
 * Set the default directory for saving files.
 */
void a_UIcmd_set_save_dir(const char *dir)
{
   const char *p;

   if (dir && (p = strrchr(dir, '/'))) {
      dFree(save_dir);
      // assert a trailing '/'
      save_dir = dStrconcat(dir, (p[1] != 0) ? "/" : "", NULL);
   }
}

/*
 * Save current URL
 */
void a_UIcmd_save(void *vbw)
{
   const char *name;
   char *SuggestedName;
   BrowserWindow *bw = (BrowserWindow *)vbw;
   const DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));

   if (url) {
      a_UIcmd_set_save_dir(prefs.save_dir);
      SuggestedName = UIcmd_make_save_filename(URL_PATH(url));
      name = a_Dialog_save_file("Save Page as File", NULL, SuggestedName);
      MSG("a_UIcmd_save: %s\n", name);
      dFree(SuggestedName);

      if (name) {
         a_Nav_save_url(bw, url, name);
      }
   }
}

/*
 * Select a file
 */
const char *a_UIcmd_select_file()
{
   return a_Dialog_select_file("Select a File", NULL, NULL);
}

/*
 * Stop network activity on this bw.
 * The stop button was pressed: stop page (and images) downloads.
 */
void a_UIcmd_stop(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;

   MSG("a_UIcmd_stop()\n");
   a_Nav_cancel_expect(bw);
   a_Bw_stop_clients(bw, BW_Root + BW_Img + BW_Force);
   a_UIcmd_set_buttons_sens(bw);
}

/*
 * Popup the tools menu
 */
void a_UIcmd_tools(void *vbw, void *v_wid)
{
   a_Menu_tools_popup((BrowserWindow*)vbw, v_wid);
}

/*
 * Open URL with dialog chooser
 */
void a_UIcmd_open_file(void *vbw)
{
   char *name;
   DilloUrl *url;

   name = a_Dialog_open_file("Open File", NULL, "");

   if (name) {
      url = a_Url_new(name, "file:");
      a_UIcmd_open_url((BrowserWindow*)vbw, url);
      a_Url_free(url);
      dFree(name);
   }
}

/*
 * Returns a newly allocated string holding a search url generated from
 * a string of keywords (separarated by blanks) and prefs.search_url.
 * The search string is urlencoded.
 */
static char *UIcmd_make_search_str(const char *str)
{
   char *keys = a_Url_encode_hex_str(str), *c = prefs.search_url;
   Dstr *ds = dStr_sized_new(128);
   char *search_url;

   for (; *c; c++) {
      if (*c == '%')
         switch(*++c) {
         case 's':
            dStr_append(ds, keys); break;;
         case '%':
            dStr_append_c(ds, '%'); break;;
         case 0:
            MSG_WARN("search_url ends with '%%'\n"); c--; break;;
         default:
            MSG_WARN("illegal specifier '%%%c' in search_url\n", *c);
         }
      else
         dStr_append_c(ds, *c);
   }
   dFree(keys);

   search_url = ds->str;
   dStr_free(ds, 0);
   return search_url;
}

/*
 * Get a query from a dialog and open it
 */
void a_UIcmd_search_dialog(void *vbw)
{
   const char *query;

   if ((query = a_Dialog_input("Search the Web:"))) {
      char *url_str = UIcmd_make_search_str(query);
      a_UIcmd_open_urlstr(vbw, url_str);
      dFree(url_str);
   }
}

/*
 * Get password for user
 */
const char *a_UIcmd_get_passwd(const char *user)
{
   const char *passwd;
   char *prompt = dStrconcat("Password for user \"", user, "\"", NULL);
   passwd = a_Dialog_passwd(prompt);
   dFree(prompt);
   return passwd;
}

/*
 * Save link URL
 */
void a_UIcmd_save_link(BrowserWindow *bw, const DilloUrl *url)
{
   const char *name;
   char *SuggestedName;

   a_UIcmd_set_save_dir(prefs.save_dir);

   SuggestedName = UIcmd_make_save_filename(URL_STR(url));
   name = a_Dialog_save_file("Save Link as File", NULL, SuggestedName);
   MSG("a_UIcmd_save_link: %s\n", name);
   dFree(SuggestedName);

   if (name) {
      a_Nav_save_url(bw, url, name);
   }
}

/*
 * Request the bookmarks page
 */
void a_UIcmd_book(void *vbw)
{
   DilloUrl *url = a_Url_new("dpi:/bm/", NULL);
   a_UIcmd_open_url((BrowserWindow*)vbw, url);
   a_Url_free(url);
}

/*
 * Add a bookmark for a certain URL
 */
void a_UIcmd_add_bookmark(BrowserWindow *bw, const DilloUrl *url)
{
   a_Bookmarks_add(bw, url);
}


/*
 * Popup the page menu
 */
void a_UIcmd_page_popup(void *vbw, bool_t has_bugs, void *v_cssUrls)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   const DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));
   a_Menu_page_popup(bw, url, has_bugs, v_cssUrls);
}

/*
 * Popup the link menu
 */
void a_UIcmd_link_popup(void *vbw, const DilloUrl *url)
{
   a_Menu_link_popup((BrowserWindow*)vbw, url);
}

/*
 * Pop up the image menu
 */
void a_UIcmd_image_popup(void *vbw, const DilloUrl *url, bool_t loaded_img,
                         DilloUrl *page_url, DilloUrl *link_url)
{
   a_Menu_image_popup((BrowserWindow*)vbw, url, loaded_img, page_url,link_url);
}

/*
 * Pop up the form menu
 */
void a_UIcmd_form_popup(void *vbw, const DilloUrl *url, void *vform,
                        bool_t showing_hiddens)
{
   a_Menu_form_popup((BrowserWindow*)vbw, url, vform, showing_hiddens);
}

/*
 * Pop up the file menu
 */
void a_UIcmd_file_popup(void *vbw, void *v_wid)
{
   a_Menu_file_popup((BrowserWindow*)vbw, v_wid);
}

/*
 * Copy url string to paste buffer
 */
void a_UIcmd_copy_urlstr(BrowserWindow *bw, const char *urlstr)
{
   Layout *layout = (Layout*)bw->render_layout;
   layout->copySelection(urlstr);
}

/*
 * Ask the vsource dpi to show this URL's source
 */
void a_UIcmd_view_page_source(BrowserWindow *bw, const DilloUrl *url)
{
   char *buf;
   int buf_size;
   Dstr *dstr_url;
   DilloUrl *vs_url;
   static int post_id = 0;
   char tag[8];

   if (a_Nav_get_buf(url, &buf, &buf_size)) {
      a_Nav_set_vsource_url(url);
      dstr_url = dStr_new("dpi:/vsource/:");
      dStr_append(dstr_url, URL_STR(url));
      if (URL_FLAGS(url) & URL_Post) {
         /* append a custom string to differentiate POST URLs */
         post_id = (post_id < 9999) ? post_id + 1 : 0;
         snprintf(tag, 8, "_%.4d", post_id);
         dStr_append(dstr_url, tag);
      }
      vs_url = a_Url_new(dstr_url->str, NULL);
      a_UIcmd_open_url_nt(bw, vs_url, 1);
      a_Url_free(vs_url);
      dStr_free(dstr_url, 1);
      a_Nav_unref_buf(url);
   }
}

/*
 * Show a text window with the URL's source
 */
void a_UIcmd_view_page_bugs(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if (bw->num_page_bugs > 0) {
      a_Dialog_text_window(bw->page_bugs->str, "Detected HTML errors");
   } else {
      a_Dialog_msg("Zero detected HTML errors!");
   }
}

/*
 * Popup the bug meter menu
 */
void a_UIcmd_bugmeter_popup(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;

   a_Menu_bugmeter_popup(bw, a_History_get_url(NAV_TOP_UIDX(bw)));
}

/*
 * Make a list of URL indexes for the history popup
 * based on direction (-1 = back, 1 = forward)
 */
int *a_UIcmd_get_history(BrowserWindow *bw, int direction)
{
   int i, j, n;
   int *hlist;

   // Count the number of URLs
   i = a_Nav_stack_ptr(bw) + direction;
   for (n = 0 ; i >= 0 && i < a_Nav_stack_size(bw); i+=direction)
      ++n;
   hlist = dNew(int, n + 1);

   // Fill the list
   i = a_Nav_stack_ptr(bw) + direction;
   for (j = 0 ; i >= 0 && i < a_Nav_stack_size(bw); i+=direction, j += 1) {
      hlist[j] = NAV_UIDX(bw,i);
   }
   hlist[j] = -1;

   return hlist;
}

/*
 * Jump to a certain URL in the navigation stack.
 */
void a_UIcmd_nav_jump(BrowserWindow *bw, int offset, int new_bw)
{
   a_Nav_jump(bw, offset, new_bw);
}

// UI binding functions -------------------------------------------------------

/*
 * Return browser window width and height
 */
void a_UIcmd_get_wh(BrowserWindow *bw, int *w, int *h)
{
   *w = BW2UI(bw)->w();
   *h = BW2UI(bw)->h();
   _MSG("a_UIcmd_wh: w=%d, h=%d\n", *w, *h);
}

/*
 * Get the scroll position (x, y offset pair)
 */
void a_UIcmd_get_scroll_xy(BrowserWindow *bw, int *x, int *y)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout) {
     *x = layout->getScrollPosX();
     *y = layout->getScrollPosY();
   }
}

/*
 * Set the scroll position ({x, y} offset pair)
 */
void a_UIcmd_set_scroll_xy(BrowserWindow *bw, int x, int y)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout) {
      layout->scrollTo(HPOS_LEFT, VPOS_TOP, x, y, 0, 0);
   }
}

/*
 * Set the scroll position by fragment (from URL)
 */
void a_UIcmd_set_scroll_by_fragment(BrowserWindow *bw, const char *f)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout && f) {
      layout->setAnchor(f);
   }
}

/*
 * Pass scrolling command to dw.
 */
void a_UIcmd_scroll(BrowserWindow *bw, int icmd)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout) {
      typedef struct {
         KeysCommand_t keys_cmd;
         ScrollCommand dw_cmd;
      } mapping_t;

      const mapping_t map[] = {
         {KEYS_SCREEN_UP, SCREEN_UP_CMD},
         {KEYS_SCREEN_DOWN, SCREEN_DOWN_CMD},
         {KEYS_LINE_UP, LINE_UP_CMD},
         {KEYS_LINE_DOWN, LINE_DOWN_CMD},
         {KEYS_LEFT, LEFT_CMD},
         {KEYS_RIGHT, RIGHT_CMD},
         {KEYS_TOP, TOP_CMD},
         {KEYS_BOTTOM, BOTTOM_CMD},
      };
      KeysCommand_t keycmd = (KeysCommand_t)icmd;

      for (uint_t i = 0; i < (sizeof(map)/sizeof(mapping_t)); i++) {
         if (keycmd == map[i].keys_cmd) {
            layout->scroll(map[i].dw_cmd);
            break;
         }
      }
   }
}

/*
 * Get location's text
 */
char *a_UIcmd_get_location_text(BrowserWindow *bw)
{
   return dStrdup(BW2UI(bw)->get_location());
}

/*
 * Set location's text
 */
void a_UIcmd_set_location_text(void *vbw, const char *text)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   BW2UI(bw)->set_location(text);
}

/*
 * Set the page progress bar
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void a_UIcmd_set_page_prog(BrowserWindow *bw, size_t nbytes, int cmd)
{
   BW2UI(bw)->set_page_prog(nbytes, cmd);
}

/*
 * Set the images progress bar
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void a_UIcmd_set_img_prog(BrowserWindow *bw, int n_img, int t_img, int cmd)
{
   BW2UI(bw)->set_img_prog(n_img, t_img, cmd);
#if 0
   if (!cmd)
      a_UIcmd_close_bw(bw);
#endif
}

/*
 * Set the bug meter progress label
 */
void a_UIcmd_set_bug_prog(BrowserWindow *bw, int n_bug)
{
   BW2UI(bw)->set_bug_prog(n_bug);
}

/*
 * Set the page title in the window titlebar and tab label.
 * (Update window titlebar for the current tab only)
 */
void a_UIcmd_set_page_title(BrowserWindow *bw, const char *label)
{
   const int size = 128;
   char title[size];

   if (a_UIcmd_get_bw_by_widget(BW2UI(bw)->tabs()->wizard()->value()) == bw) {
      // This is the focused bw, set window title
      if (snprintf(title, size, "Dillo: %s", label) >= size) {
         uint_t i = MIN(size - 4, 1 + a_Utf8_end_of_char(title, size - 8));
         snprintf(title + i, 4, "...");
      }
      BW2UI(bw)->window()->copy_label(title);
   }
   BW2UI(bw)->tabs()->set_tab_label(BW2UI(bw), label);
}

/*
 * Set a printf-like status string on the bottom of the dillo window.
 * Beware: The safe way to set an arbitrary string is
 *         a_UIcmd_set_msg(bw, "%s", str)
 */
void a_UIcmd_set_msg(BrowserWindow *bw, const char *format, ...)
{
   va_list argp;
   Dstr *ds = dStr_sized_new(128);
   va_start(argp, format);
   dStr_vsprintf(ds, format, argp);
   va_end(argp);
   BW2UI(bw)->set_status(ds->str);
   dStr_free(ds, 1);
}

/*
 * Set the sensitivity of back/forw/stop buttons.
 */
void a_UIcmd_set_buttons_sens(BrowserWindow *bw)
{
   int sens;

   // Stop
   sens = (dList_length(bw->ImageClients) || dList_length(bw->RootClients));
   BW2UI(bw)->button_set_sens(UI_STOP, sens);
   // Back
   sens = (a_Nav_stack_ptr(bw) > 0);
   BW2UI(bw)->button_set_sens(UI_BACK, sens);
   // Forward
   sens = (a_Nav_stack_ptr(bw) < a_Nav_stack_size(bw) - 1 &&
           !bw->nav_expecting);
   BW2UI(bw)->button_set_sens(UI_FORW, sens);
}

/*
 * Keep track of mouse pointer over a link.
 */
void a_UIcmd_set_pointer_on_link(BrowserWindow *bw, int flag)
{
   BW2UI(bw)->pointerOnLink(flag);
}

/*
 * Is the mouse pointer over a link?
 */
int a_UIcmd_pointer_on_link(BrowserWindow *bw)
{
   return BW2UI(bw)->pointerOnLink();
}

/*
 * Toggle control panel (aka. fullscreen)
 */
void a_UIcmd_fullscreen_toggle(BrowserWindow *bw)
{
   BW2UI(bw)->fullscreen_toggle();
}

/*
 * Search for next/previous occurrence of key.
 */
void a_UIcmd_findtext_search(BrowserWindow *bw, const char *key,
                             int case_sens, int backwards)
{
   Layout *l = (Layout *)bw->render_layout;

   switch (l->search(key, case_sens, backwards)) {
   case FindtextState::RESTART:
      a_UIcmd_set_msg(bw, "No further occurrences of \"%s\". "
                          "Restarting from the top.", key);
      break;
   case FindtextState::NOT_FOUND:
      a_UIcmd_set_msg(bw, "\"%s\" not found.", key);
      break;
   case FindtextState::SUCCESS:
   default:
      a_UIcmd_set_msg(bw, "");
   }
}

/*
 * Reset text search state.
 */
void a_UIcmd_findtext_reset(BrowserWindow *bw)
{
   Layout *l = (Layout *)bw->render_layout;
   l->resetSearch();

   a_UIcmd_set_msg(bw, "");
}

/*
 * Tell the UI to hide/show the findbar
 */
void a_UIcmd_findbar_toggle(BrowserWindow *bw, int on)
{
   BW2UI(bw)->findbar_toggle(on);
}

/*
 * Focus the rendered area.
 */
void a_UIcmd_focus_main_area(BrowserWindow *bw)
{
   BW2UI(bw)->focus_main();
}

/*
 * Focus the location bar.
 */
void a_UIcmd_focus_location(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   BW2UI(bw)->focus_location();
}

