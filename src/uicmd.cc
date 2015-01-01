/*
 * File: uicmd.cc
 *
 * Copyright (C) 2005-2011 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Functions/Methods for commands triggered from the UI


#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>     /* for qsort */
#include <math.h>       /* for rint */
#include <limits.h>     /* for UINT_MAX */
#include <sys/stat.h>

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Wizard.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Scroll.H>
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
#include "misc.h"

#include "dw/fltkviewport.hh"

#include "nav.h"

//#define DEFAULT_TAB_LABEL "-.untitled.-"
#define DEFAULT_TAB_LABEL "-.new.-"

// Handy macro
#define BW2UI(bw) ((UI*)((bw)->ui))

// Platform idependent part
using namespace dw::core;
// FLTK related
using namespace dw::fltk;


/*
 * Local data
 */
static const char *save_dir = "";

/*
 * Forward declarations
 */
static BrowserWindow *UIcmd_tab_new(CustTabs *tabs, UI *old_ui, int focus);
static void close_tab_btn_cb (Fl_Widget *w, void *cb_data);
static char *UIcmd_make_search_str(const char *str);
static void UIcmd_set_window_labels(Fl_Window *win, const char *str);

//----------------------------------------------------------------------------

/*
 * CustTabs ---------------------------------------------------------------
 */

/*
 * stores the respective UI pointer
 */
class CustTabButton : public Fl_Button {
   UI *ui_;
   uint_t focus_num_; // Used to choose which tab to focus when closing the
                      // active one (the highest numbered gets focus).
public:
   CustTabButton (int x,int y,int w,int h, const char* label = 0) :
      Fl_Button (x,y,w,h,label) { ui_ = NULL; focus_num_ = 0; };
   void ui(UI *pui) { ui_ = pui; }
   UI *ui(void) { return ui_; }
   void focus_num(uint_t fn) { focus_num_ = fn; }
   uint_t focus_num(void) { return focus_num_; }
};

static int btn_cmp(const void *p1, const void *p2)
{
   return ((*(CustTabButton * const *)p1)->focus_num() -
           (*(CustTabButton * const *)p2)->focus_num() );
}

/*
 * Allows fine control of the tabbed interface
 */
class CustTabs : public Fl_Group {
   uint_t focus_counter; // An increasing counter
   int tab_w, tab_h, ctab_h, btn_w, ctl_w;
   Fl_Wizard *Wizard;
   Fl_Scroll *Scroll;
   Fl_Pack *Pack;
   Fl_Group *Control;
   CustButton *CloseBtn;

   void update_pack_offset(void);
   void resize(int x, int y, int w, int h)
      { Fl_Group::resize(x,y,w,h); update_pack_offset(); }
   int get_btn_idx(UI *ui);
   void increase_focus_counter() {
      if (focus_counter < UINT_MAX) /* check for overflow */
         ++focus_counter;
      else {
         /* wrap & normalize old numbers here */
         CustTabButton **btns = dNew(CustTabButton*, num_tabs());
         for (int i = 0; i < num_tabs(); ++i)
            btns[i] = (CustTabButton*)Pack->child(i);
         qsort(btns, num_tabs(), sizeof(CustTabButton *), btn_cmp);
         focus_counter = 0;
         for (int i = 0; i < num_tabs(); ++i)
            btns[i]->focus_num(focus_counter++);
         dFree(btns);
      }
   }

public:
   CustTabs (int ww, int wh, int th, const char *lbl=0) :
      Fl_Group(0,0,ww,th,lbl) {
      Pack = NULL;
      focus_counter = 0;
      tab_w = 50, tab_h = th, ctab_h = 1, btn_w = 20, ctl_w = 1*btn_w+2;
      resize(0,0,ww,ctab_h);
      /* tab buttons go inside a pack within a scroll */
      Scroll = new Fl_Scroll(0,0,ww-ctl_w,ctab_h);
      Scroll->type(0); /* no scrollbars */
      Scroll->box(FL_NO_BOX);
       Pack = new Fl_Pack(0,0,ww-ctl_w,tab_h);
       Pack->type(Fl_Pack::HORIZONTAL);
       Pack->box(FL_NO_BOX); //FL_THIN_DOWN_FRAME
       Pack->end();
      Scroll->end();
      resizable(Scroll);

      /* control buttons go inside a group */
      Control = new Fl_Group(ww-ctl_w,0,ctl_w,ctab_h);
       CloseBtn = new CustButton(ww-ctl_w+2,0,btn_w,ctab_h, "X");
       CloseBtn->box(FL_THIN_UP_BOX);
       CloseBtn->clear_visible_focus();
       CloseBtn->set_tooltip(prefs.right_click_closes_tab ?
          "Close current tab.\nor Right-click tab label to close." :
          "Close current tab.\nor Middle-click tab label to close.");
       CloseBtn->callback(close_tab_btn_cb, this);
       CloseBtn->hide();
      Control->end();

      box(FL_FLAT_BOX);
      end();

      Wizard = new Fl_Wizard(0,ctab_h,ww,wh-ctab_h);
      Wizard->box(FL_NO_BOX);
      Wizard->end();
   };
   int handle(int e);
   UI *add_new_tab(UI *old_ui, int focus);
   void remove_tab(UI *ui);
   Fl_Wizard *wizard(void) { return Wizard; }
   int num_tabs() { return (Pack ? Pack->children() : 0); }
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

/*
 * Callback for the close-tab button
 */
static void close_tab_btn_cb (Fl_Widget *, void *cb_data)
{
   CustTabs *tabs = (CustTabs*) cb_data;
   int b = Fl::event_button();

   if (b == FL_LEFT_MOUSE) {
      a_UIcmd_close_bw(a_UIcmd_get_bw_by_widget(tabs->wizard()->value()));
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
      if (cmd == KEYS_NOP) {
         // Do nothing
         _MSG("CustTabs::handle KEYS_NOP\n");
      } else if (cmd == KEYS_NEW_TAB) {
         a_UIcmd_open_url_nt(bw, NULL, 1);
         ret = 1;
      } else if (cmd == KEYS_CLOSE_TAB) {
         a_UIcmd_close_bw(bw);
         ret = 1;
      } else if (cmd == KEYS_LEFT_TAB) {
         prev_tab();
         ret = 1;
      } else if (cmd == KEYS_RIGHT_TAB) {
         next_tab();
         ret = 1;
      } else if (cmd == KEYS_NEW_WINDOW) {
         a_UIcmd_open_url_nw(bw, NULL);
         ret = 1;
      } else if (cmd == KEYS_CLOSE_ALL) {
         a_Timeout_add(0.0, a_UIcmd_close_all_bw, NULL);
         ret = 1;
      }
   }

   return (ret) ? ret : Fl_Group::handle(e);
}

/*
 * Create a new tab with its own UI
 */
UI *CustTabs::add_new_tab(UI *old_ui, int focus)
{
   if (num_tabs() == 1) {
      // Show tabbar
      ctab_h = tab_h;
      Wizard->resize(0,ctab_h,Wizard->w(),window()->h()-ctab_h);
      resize(0,0,window()->w(),ctab_h);    // tabbar
      CloseBtn->show();
      {int w = 0, h; Pack->child(0)->measure_label(w, h);
       Pack->child(0)->size(w+14,ctab_h);}
      Pack->child(0)->show(); // first tab button
      window()->init_sizes();
   }

   /* The UI is constructed in a comfortable fitting size, and then resized
    * so FLTK doesn't get confused later with even smaller dimensions! */
   current(0);
   UI *new_ui = new UI(0,0,UI_MIN_W,UI_MIN_H,"Dillo:",old_ui);
   new_ui->resize(0,ctab_h,Wizard->w(),Wizard->h());
   new_ui->tabs(this);
   Wizard->add(new_ui);
   new_ui->show();

   CustTabButton *btn = new CustTabButton(num_tabs()*tab_w,0,tab_w,ctab_h);
   btn->align(FL_ALIGN_INSIDE);
   btn->labelsize(btn->labelsize()-2);
   btn->copy_label(DEFAULT_TAB_LABEL);
   btn->clear_visible_focus();
   btn->box(FL_GTK_THIN_UP_BOX);
   btn->color(focus ? PREFS_UI_TAB_ACTIVE_BG_COLOR : PREFS_UI_TAB_BG_COLOR);
   btn->labelcolor(focus ? PREFS_UI_TAB_ACTIVE_FG_COLOR:PREFS_UI_TAB_FG_COLOR);
   btn->ui(new_ui);
   btn->callback(tab_btn_cb, this);
   Pack->add(btn); // append

   if (focus) {
      switch_tab(btn);
   } else {        // no focus
      // set focus counter
      increase_focus_counter();
      btn->focus_num(focus_counter);

      if (num_tabs() == 2) {
         // tabbar added: redraw current page
         Wizard->redraw();
      }
   }
   if (num_tabs() == 1)
      btn->hide();
   update_pack_offset();

   return new_ui;
}

/*
 * Remove tab by UI
 */
void CustTabs::remove_tab(UI *ui)
{
   CustTabButton *btn;

   // get active tab idx
   int act_idx = get_btn_idx((UI*)Wizard->value());
   // get to-be-removed tab idx
   int rm_idx = get_btn_idx(ui);
   btn = (CustTabButton*)Pack->child(rm_idx);

   if (act_idx == rm_idx) {
      // Active tab is being closed, switch to the "previous" one
      CustTabButton *fbtn = NULL;
      for (int i = 0; i < num_tabs(); ++i) {
         if (i != rm_idx) {
            if (!fbtn)
               fbtn = (CustTabButton*)Pack->child(i);
            CustTabButton *btn = (CustTabButton*)Pack->child(i);
            if (btn->focus_num() > fbtn->focus_num())
               fbtn = btn;
         }
      }
      switch_tab(fbtn);
   }
   Pack->remove(rm_idx);
   update_pack_offset();
   delete btn;

   Wizard->remove(ui);
   delete(ui);

   if (num_tabs() == 1) {
      // hide tabbar
      ctab_h = 1;
      CloseBtn->hide();
      Pack->child(0)->size(0,0);
      Pack->child(0)->hide(); // first tab button
      resize(0,0,window()->w(),ctab_h);    // tabbar
      Wizard->resize(0,ctab_h,Wizard->w(),window()->h()-ctab_h);
      window()->init_sizes();
      window()->redraw();
   }
}

int CustTabs::get_btn_idx(UI *ui)
{
   for (int i = 0; i < num_tabs(); ++i) {
      CustTabButton *btn = (CustTabButton*)Pack->child(i);
      if (btn->ui() == ui)
         return i;
   }
   return -1;
}

/*
 * Keep active tab visible
 * (Pack children have unusable x() coordinate)
 */
void CustTabs::update_pack_offset()
{
   dReturn_if (num_tabs() == 0);

   // get active tab button
   int act_idx = get_btn_idx((UI*)Wizard->value());
   CustTabButton *cbtn = (CustTabButton*)Pack->child(act_idx);

   // calculate tab button's x() coordinates
   int x_i = 0, x_f;
   for (int j=0; j < act_idx; ++j)
      x_i += Pack->child(j)->w();
   x_f = x_i + cbtn->w();

   int scr_x = Scroll->xposition(), scr_y = Scroll->yposition();
   int px_i = x_i - scr_x;
   int px_f = px_i + cbtn->w();
   int pw = Scroll->window()->w() - ctl_w;
   _MSG("  scr_x=%d btn_x=%d px_i=%d btn_w=%d px_f=%d pw=%d",
       Scroll->xposition(),cbtn->x(),px_i,cbtn->w(),px_f,pw);
   if (px_i < 0) {
      Scroll->scroll_to(x_i, scr_y);
   } else if (px_i > pw || (px_i > 0 && px_f > pw)) {
      Scroll->scroll_to(MIN(x_i, x_f-pw), scr_y);
   }
   Scroll->redraw();
   _MSG(" >>scr_x=%d btn0_x=%d\n", scr_x, Pack->child(0)->x());
}

/*
 * Make cbtn's tab the active one
 */
void CustTabs::switch_tab(CustTabButton *cbtn)
{
   int idx;
   CustTabButton *btn;
   BrowserWindow *bw;
   UI *old_ui = (UI*)Wizard->value();

   if (cbtn && cbtn->ui() != old_ui) {
      // Set old tab label to normal color
      if ((idx = get_btn_idx(old_ui)) != -1) {
         btn = (CustTabButton*)Pack->child(idx);
         btn->color(PREFS_UI_TAB_BG_COLOR);
         btn->labelcolor(PREFS_UI_TAB_FG_COLOR);
         btn->redraw();
      }
      /* We make a point of calling show() before value() is changed because
       * the wizard may hide the old one before showing the new one. In that
       * case, the new UI gets focus with Fl::e_keysym set to whatever
       * triggered the switch, and this is a problem when it's Tab/Left/Right/
       * Up/Down because some widgets (notably Fl_Group and Fl_Input) exhibit
       * unwelcome behaviour in that case. If the new widgets are already
       * shown, fl_fix_focus will fix everything with Fl::e_keysym temporarily
       * cleared.
       */
      cbtn->ui()->show();
      Wizard->value(cbtn->ui());
      cbtn->color(PREFS_UI_TAB_ACTIVE_BG_COLOR);
      cbtn->labelcolor(PREFS_UI_TAB_ACTIVE_FG_COLOR);
      cbtn->redraw();
      update_pack_offset();

      // Update window title
      if ((bw = a_UIcmd_get_bw_by_widget(cbtn->ui()))) {
         const char *title = (cbtn->ui())->label();
         UIcmd_set_window_labels(cbtn->window(), title ? title : "");
      }
      // Update focus priority
      increase_focus_counter();
      cbtn->focus_num(focus_counter);
   }
}

void CustTabs::prev_tab()
{
   int idx;

   if ((idx = get_btn_idx((UI*)Wizard->value())) != -1)
      switch_tab((CustTabButton*)Pack->child(idx>0 ? idx-1 : num_tabs()-1));
}

void CustTabs::next_tab()
{
   int idx;

   if ((idx = get_btn_idx((UI*)Wizard->value())) != -1)
      switch_tab((CustTabButton*)Pack->child((idx+1<num_tabs()) ? idx+1 : 0));
}

/*
 * Set this UI's tab button label
 */
void CustTabs::set_tab_label(UI *ui, const char *label)
{
   char title[128];
   int idx = get_btn_idx(ui);

   if (idx != -1) {
      // Make a label for this tab
      size_t tab_chars = 15, label_len = strlen(label);

      if (label_len > tab_chars)
         tab_chars = a_Utf8_end_of_char(label, tab_chars - 1) + 1;
      snprintf(title, tab_chars + 1, "%s", label);
      if (label_len > tab_chars)
         snprintf(title + tab_chars, 4, "...");

      // Avoid unnecessary redraws
      if (strcmp(Pack->child(idx)->label(), title)) {
         int w = 0, h;
         Pack->child(idx)->copy_label(title);
         Pack->child(idx)->measure_label(w, h);
         Pack->child(idx)->size(w+14,ctab_h);
         update_pack_offset();
      }
   }
}


//----------------------------------------------------------------------------

static void win_cb (Fl_Widget *w, void *cb_data) {
   CustTabs *tabs = (CustTabs*) cb_data;
   int choice = 1, ntabs = tabs->num_tabs();

   if (Fl::event_key() == FL_Escape) {
      // Don't let FLTK close a browser window due to unhandled Escape
      // (most likely with modifiers).
      return;
   }

   if (prefs.show_quit_dialog && ntabs > 1)
      choice = a_Dialog_choice("Dillo: Close window?",
                               "Window contains more than one tab.",
                               "Close", "Cancel", NULL);
   if (choice == 1)
      while (ntabs-- > 0)
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
   win->resizable(DilloTabs->wizard());

   int focus = 1;
   new_bw = UIcmd_tab_new(DilloTabs, old_ui, focus);
   win->show();

   if (old_bw == NULL && prefs.xpos >= 0 && prefs.ypos >= 0) {
      // position the first window according to preferences
      DilloTabs->window()->position(prefs.xpos, prefs.ypos);
   }

   win->callback(win_cb, DilloTabs);

   return new_bw;
}

/*
 * Set the window name and icon name.
 */
static void UIcmd_set_window_labels(Fl_Window *win, const char *str)
{
   const char *copy;

   win->Fl_Widget::copy_label(str);
   copy = win->label();
   win->label(copy, copy);
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

   // Now create the Dw render layout and viewport
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);
   style::Color *bgColor = style::Color::create (layout, prefs.bg_color);
   layout->setBgColor (bgColor);
   layout->setBgImage (NULL, style::BACKGROUND_REPEAT,
                       style::BACKGROUND_ATTACHMENT_SCROLL,
                       style::createPerLength (0), style::createPerLength (0));

   // set_render_layout() sets the proper viewport size
   FltkViewport *viewport = new FltkViewport (0, 0, 0, 1);
   viewport->box(FL_NO_BOX);
   viewport->setBufferedDrawing (prefs.buffered_drawing ? true : false);
   viewport->setDragScroll (prefs.middle_click_drags_page ? true : false);
   layout->attachView (viewport);
   new_ui->set_render_layout(viewport);
   viewport->setScrollStep((int) rint(28.0 * prefs.font_factor));

   // Now, create a new browser window structure
   BrowserWindow *new_bw = a_Bw_new();

   // Reference the UI from the bw
   new_bw->ui = (void *)new_ui;
   // Copy the layout pointer into the bw data
   new_bw->render_layout = (void*)layout;

   // Clear the window title
   if (focus)
      UIcmd_set_window_labels(new_ui->window(), new_ui->label());

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
   CustTabs *tabs = ui->tabs();
   Layout *layout = (Layout*)bw->render_layout;

   _MSG("a_UIcmd_close_bw\n");
   a_Bw_stop_clients(bw, BW_Root + BW_Img + BW_Force);
   delete(layout);
   if (tabs) {
      tabs->remove_tab(ui);
      if (tabs->num_tabs() == 0)
         delete tabs->window();
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

   if (prefs.show_quit_dialog && a_Bw_num() > 1)
      choice = a_Dialog_choice("Dillo: Quit?",
                               "More than one open tab or window.",
                               "Quit", "Cancel", NULL);
   if (choice == 1)
      while ((bw = a_Bw_get(0)))
         a_UIcmd_close_bw((void*)bw);
}

/*
 * Return a search string of the suffix if str starts with a
 * prefix of a search engine name and a blank
 */
static char *UIcmd_find_search_str(const char *str)
{
   int p;
   char *url = NULL;
   int len = strcspn(str, " ");

   if (len > 0 && str[len] != '\0') {
      /* we found a ' ' in str, check whether the first part of str
       * is a prefix of a search_url label
       */
      for (p = 0; p < dList_length(prefs.search_urls); p++) {
         const char *search =
            (const char *)dList_nth_data(prefs.search_urls, p);
         if (search && strncasecmp(str, search, len) == 0) {
            prefs.search_url_idx = p;
            url = UIcmd_make_search_str(str + len + 1);
            break;
         }
      }
   }
   return url;
}

/*
 * Open a new URL in the given browser window.
 *
 * our custom "file:" URIs are normalized here too.
 */
void a_UIcmd_open_urlstr(void *vbw, const char *urlstr)
{
   char *new_urlstr;
   char *search_urlstr = NULL;
   DilloUrl *url;
   int ch;
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if ((search_urlstr = UIcmd_find_search_str(urlstr))) {
      urlstr = search_urlstr;
   }
   if (urlstr && *urlstr) {
      /* Filter URL string */
      new_urlstr = a_Url_string_strip_delimiters(urlstr);

      if (!dStrnAsciiCasecmp(new_urlstr, "file:", 5)) {
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
   dFree(search_urlstr);
}

/*
 * Open a new URL in the given browser window
 */
void a_UIcmd_open_url(BrowserWindow *bw, const DilloUrl *url)
{
   if (url) {
      a_Nav_push(bw, url, NULL);
      BW2UI(bw)->focus_main();
   } else {
      // Used to start a bw with a blank screen
      BW2UI(bw)->focus_location();
      a_UIcmd_set_buttons_sens(bw);
   }
}

static void UIcmd_open_url_nbw(BrowserWindow *new_bw, const DilloUrl *url)
{
   /* When opening a new BrowserWindow (tab or real window) we focus
    * Location if we don't yet have an URL, main otherwise.
    */
   if (url) {
      a_Nav_push(new_bw, url, NULL);
      a_UIcmd_set_location_text(new_bw, URL_STR(url));
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
void a_UIcmd_back_popup(void *vbw, int x, int y)
{
   a_Menu_history_popup((BrowserWindow*)vbw, x, y, -1);
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
void a_UIcmd_forw_popup(void *vbw, int x, int y)
{
   a_Menu_history_popup((BrowserWindow*)vbw, x, y, 1);
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
static char *UIcmd_make_save_filename(const DilloUrl *url)
{
   size_t MaxLen = 64;
   const char *dir = save_dir, *path, *path2, *query;
   char *name, *free1, *free2, *n1, *n2;

   free1 = free2 = NULL;

   /* get the last component of the path */
   path = URL_PATH(url);
   path2 = strrchr(path, '/');
   path = path2 ? path2 + 1 : path;

   /* truncate the path if necessary */
   if (strlen(path) > MaxLen) {
      path = free1 = dStrndup(path, MaxLen);
   }

   /* is there a query? */
   query = URL_QUERY(url);
   if (*query) {
      /* truncate the query if necessary */
      if (strlen(query) > MaxLen) {
         query = free2 = dStrndup(query, MaxLen);
      }
      name = dStrconcat(dir, path, "?", query, NULL);
   } else {
      name = dStrconcat(dir, path, NULL);
   }

   dFree(free1);
   dFree(free2);

   /* Replace %20 and ' ' with '_' */
   for (n1 = n2 = name; *n1; n1++, n2++) {
      *n2 =
         (n1[0] == ' ')
         ? '_' :
         (n1[0] == '%' && n1[1] == '2' && n1[2] == '0')
         ? (n1 += 2, '_') :
         n1[0];
   }
   *n2 = 0;

   return name;
}

/*
 * Set the default directory for saving files.
 */
void a_UIcmd_init(void)
{
   const char *dir = prefs.save_dir;

   if (dir && *dir) {
      // assert a trailing '/'
      save_dir =
         (dir[strlen(dir)-1] == '/')
         ? dStrdup(dir)
         : dStrconcat(dir, "/", NULL);
   }
}

/*
 * Check a file to save to.
 */
static int UIcmd_save_file_check(const char *name)
{
   struct stat ss;
   if (stat(name, &ss) == 0) {
      Dstr *ds;
      int ch;
      ds = dStr_sized_new(128);
      dStr_sprintf(ds,
                  "The file:\n  %s (%d Bytes)\nalready exists. What do we do?",
                   name, (int)ss.st_size);
      ch = a_Dialog_choice("Dillo Save: File exists!", ds->str,
                           "Abort", "Continue", "Rename", NULL);
      dStr_free(ds, 1);
      return ch;
   } else {
      return 2; /* assume the file does not exist, so Continue */
   }
}

/*
 * Save a URL
 */
static void UIcmd_save(BrowserWindow *bw, const DilloUrl *url,
                       const char *title)
{
   char *SuggestedName = UIcmd_make_save_filename(url);

   while (1) {
      const char *name = a_Dialog_save_file(title, NULL, SuggestedName);
      dFree(SuggestedName);

      if (name) {
         switch (UIcmd_save_file_check(name)) {
         case 0:
         case 1:
            /* Abort */
            return;
         case 2:
            /* Continue */
            MSG("UIcmd_save: %s\n", name);
            a_Nav_save_url(bw, url, name);
            return;
         default:
            /* Rename */
            break; /* prompt again */
         }
      } else {
         return; /* no name, so Abort */
      }

      SuggestedName = dStrdup(name);
   }
}

/*
 * Save current URL
 */
void a_UIcmd_save(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;
   const DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));

   if (url) {
      UIcmd_save(bw, url, "Save Page as File");
   }
}

/*
 * Select a file
 */
const char *a_UIcmd_select_file()
{
   return a_Dialog_select_file("Dillo: Select a File", NULL, NULL);
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
void a_UIcmd_tools(void *vbw, int x, int y)
{
   a_Menu_tools_popup((BrowserWindow*)vbw, x, y);
}

/*
 * Open URL with dialog chooser
 */
void a_UIcmd_open_file(void *vbw)
{
   char *name;
   DilloUrl *url;

   name = a_Dialog_open_file("Dillo: Open File", NULL, "");

   if (name) {
      url = a_Url_new(name, "file:");
      a_UIcmd_open_url((BrowserWindow*)vbw, url);
      a_Url_free(url);
      dFree(name);
   }
}

/*
 * Returns a newly allocated string holding a search url generated from
 * a string of keywords (separated by blanks) and the current search_url.
 * The search string is urlencoded.
 */
static char *UIcmd_make_search_str(const char *str)
{
   char *search_url, *l, *u, *c;
   char *keys = a_Url_encode_hex_str(str),
        *src = (char*)dList_nth_data(prefs.search_urls, prefs.search_url_idx);
   Dstr *ds = dStr_sized_new(128);

   /* parse search_url into label and url */
   if (a_Misc_parse_search_url(src, &l, &u) == 0) {
      for (c = u; *c; c++) {
         if (*c == '%')
            switch(*++c) {
            case 's':
               dStr_append(ds, keys); break;
            case '%':
               dStr_append_c(ds, '%'); break;
            case 0:
               MSG_WARN("search_url ends with '%%'\n"); c--; break;
            default:
               MSG_WARN("illegal specifier '%%%c' in search_url\n", *c);
            }
         else
            dStr_append_c(ds, *c);
      }
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

   if ((query = a_Dialog_input("Dillo: Search", "Search the Web:"))) {
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
   const char *title = "Dillo: Password";
   char *msg = dStrconcat("Password for user \"", user, "\"", NULL);
   passwd = a_Dialog_passwd(title, msg);
   dFree(msg);
   return passwd;
}

/*
 * Save link URL
 */
void a_UIcmd_save_link(BrowserWindow *bw, const DilloUrl *url)
{
   UIcmd_save(bw, url, "Dillo: Save Link as File");
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
   char *buf, *major;
   int buf_size;
   Dstr *dstr_url;
   DilloUrl *vs_url;
   static int post_id = 0;
   char tag[8];
   const char *content_type = a_Nav_get_content_type(url);

   a_Misc_parse_content_type(content_type, &major, NULL, NULL);

   if (major && dStrAsciiCasecmp(major, "image") &&
       a_Nav_get_buf(url, &buf, &buf_size)) {
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
   dFree(major);
}

/*
 * Show the browser window's HTML errors in a text window
 */
void a_UIcmd_view_page_bugs(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if (bw->num_page_bugs > 0) {
      a_Dialog_text_window("Dillo: Detected HTML errors", bw->page_bugs->str);
   } else {
      a_Dialog_msg("Dillo: Valid HTML!", "Zero detected HTML errors!");
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
         {KEYS_SCREEN_LEFT, SCREEN_LEFT_CMD},
         {KEYS_SCREEN_RIGHT, SCREEN_RIGHT_CMD},
         {KEYS_LINE_UP, LINE_UP_CMD},
         {KEYS_LINE_DOWN, LINE_DOWN_CMD},
         {KEYS_LEFT, LEFT_CMD},
         {KEYS_RIGHT, RIGHT_CMD},
         {KEYS_TOP, TOP_CMD},
         {KEYS_BOTTOM, BOTTOM_CMD},
      };
      KeysCommand_t keycmd = (KeysCommand_t)icmd;

      for (uint_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
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
 * Set the page title in the tab label and window titlebar.
 * (Update window titlebar for the current tab only)
 */
void a_UIcmd_set_page_title(BrowserWindow *bw, const char *label)
{
   const int size = 128;
   char title[size];

   if (snprintf(title, size, "Dillo: %s", label ? label : "") >= size) {
      uint_t i = MIN(size - 4, 1 + a_Utf8_end_of_char(title, size - 8));
      snprintf(title + i, 4, "...");
   }
   BW2UI(bw)->copy_label(title);
   BW2UI(bw)->tabs()->set_tab_label(BW2UI(bw), label ? label : "");

   if (a_UIcmd_get_bw_by_widget(BW2UI(bw)->tabs()->wizard()->value()) == bw) {
      // This is the focused bw, set window title
      UIcmd_set_window_labels(BW2UI(bw)->window(), title);
   }
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
           !a_Bw_expecting(bw));
   BW2UI(bw)->button_set_sens(UI_FORW, sens);
}

/*
 * Toggle control panel
 */
void a_UIcmd_panels_toggle(BrowserWindow *bw)
{
   BW2UI(bw)->panels_toggle();
}

/*
 * Search for next/previous occurrence of key.
 */
void a_UIcmd_findtext_search(BrowserWindow *bw, const char *key,
                             int case_sens, int backward)
{
   Layout *l = (Layout *)bw->render_layout;

   switch (l->search(key, case_sens, backward)) {
   case FindtextState::RESTART:
      a_UIcmd_set_msg(bw, backward?"Top reached; restarting from the bottom."
                                  :"Bottom reached; restarting from the top.");
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

