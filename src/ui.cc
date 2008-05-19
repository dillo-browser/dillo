/*
 * File: ui.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// UI for Dillo

#include <stdlib.h>
#include <stdio.h>

#include <fltk/HighlightButton.h>
#include <fltk/run.h>
#include <fltk/damage.h>
#include <fltk/xpmImage.h>
#include <fltk/MultiImage.h>
#include <fltk/events.h>        // for mouse buttons
#include <fltk/InvisibleBox.h>
#include <fltk/PopupMenu.h>
#include <fltk/Item.h>
#include <fltk/Divider.h>

#include "ui.hh"
#include "msg.h"

using namespace fltk;


// Include image data
#include "pixmaps.h"

#include "uicmd.hh"

/*
 * Local sub class
 * (Used to avoid certain shortcuts in the location bar)
 */

class NewInput : public Input {
public:
   NewInput (int x, int y, int w, int h, const char* l=0) :
      Input(x,y,w,h,l) {};
   int handle(int e);
};

/*
 * Disable: UpKey, DownKey, PageUpKey, PageDownKey and 
 * CTRL+{o,r,HomeKey,EndKey}
 */
int NewInput::handle(int e)
{
   int k = event_key();

   _MSG("NewInput::handle event=%d\n", e);
   if (event_state(CTRL)) {
      if (e == KEY && k == 'l') {
         // Trick to make text selected when already focused.
         throw_focus(); take_focus();
         return 0;
      } else if (k == 'o' || k == 'r' || k == HomeKey || k == EndKey)
         return 0;
   }
   if ((e == KEY || e == KEYUP) &&
       k == UpKey || k == DownKey || k == PageUpKey || k == PageDownKey) {
      _MSG(" {Up|Down|PgUp|PgDn} ret = 1\n");
      // todo: one way to handle this is to send(SHORTCUT) to the viewport
      // and return 1 here. A cleaner approach may be to ignore the event and
      // only allow the location and render area to take focus.
      // 
      // Currently, zero is returned, so the parent gets the event and moves
      // focus to a button widget that's not interested in these keys;
      // once this new widget is focused, Up and Down start to work.
      return 0;

      //this->window()->send(SHORTCUT);
      //this->window()->send(e);
      //return 1;
   }
   _MSG("\n");

   return Input::handle(e);
}

//----------------------------------------------------------------------------

/*
 * Used to handle "paste" within the toolbar's Clear button.
 */
class NewHighlightButton : public HighlightButton {
public:
   NewHighlightButton(int x, int y, int w, int h, const char *l=0) :
      HighlightButton(x,y,w,h,l) {};
   int handle(int e);
};

int NewHighlightButton::handle(int e)
{
   if (e == PASTE) {
      const char* t = event_text();
      if (t && *t) {
         a_UIcmd_set_location_text(this->window()->user_data(), t);
         a_UIcmd_open_urlstr(this->window()->user_data(), t);
         return 1;
      }
   }
   return HighlightButton::handle(e);
}

//----------------------------------------------------------------------------

/*
 * Used to resize the progress boxes automatically.
 */
class NewProgressBox : public InvisibleBox {
public:
   NewProgressBox(int x, int y, int w, int h, const char *l=0) :
      InvisibleBox(x,y,w,h,l) {};
   void update_label(const char *lbl) {
      static int padding = 0;
      int w,h;
      if (!padding) {
         copy_label("W");
         measure_label(w, h);
         padding = w > 2 ? w/2 : 1;
      }
      copy_label(lbl);
      measure_label(w,h);
      resize(w+padding,h);
      redraw_label();
   }
};

//
// Toolbar buttons -----------------------------------------------------------
//
//static const char *button_names[] = {
//   "Back", "Forward", "Home", "Reload", "Save", "Stop", "Bookmarks",
//   "Clear", "Search"
//};


//
// Callback functions --------------------------------------------------------
//

/*
 * Callback handler for the close window event.
 */
static void close_window_cb(Widget *wid, void *data)
{
   a_UIcmd_close_bw(data);
}

/*
 * Callback for the search button.
 */
static void search_cb(Widget *wid, void *data)
{
   int k = event_key();
   if (k && k <= 7)
      MSG("[Search], mouse button %d was pressed\n", k);

   if (k == 1) {
      a_UIcmd_search_dialog(wid->window()->user_data());
   } else if (k == 2) {
      ((UI*)data)->color_change_cb_i();
   } else if (k == 3) {
      ((UI*)data)->panel_cb_i();
   }
}

/*
 * Callback for the location's clear-button.
 */
static void clear_cb(Widget *w, void *data)
{
   UI *ui = (UI*)data;

   int k = event_key();
   if (k && k <= 7)
      MSG("[Clear], mouse button %d was pressed\n", k);
   if (k == 1) {
      ui->set_location("");
      ui->focus_location();
   } if (k == 2) {
      ui->paste_url();
   }
}

/*
 * Change the color of the location bar.
 *
static void color_change_cb(Widget *wid, void *data)
{
   ((UI*)data)->color_change_cb_i();
}
 */


/*
 * Send the browser to the new URL in the location.
 */
static void location_cb(Widget *wid, void *data)
{
   Input *i = (Input*)wid;
   UI *ui = (UI*)i->window();

   /* This test is necessary because WHEN_ENTER_KEY also includes
    * other events we're not interested in. For instance pressing
    * The Back or Forward, buttons, or the first click on a rendered
    * page. BUG: this must be investigated and reported to FLTK2 team */
   if (event_key() == ReturnKey) {
      a_UIcmd_open_urlstr(i->window()->user_data(), i->value());
   }
   if (ui->get_panelmode() == UI_TEMPORARILY_SHOW_PANELS) {
      ui->set_panelmode(UI_HIDDEN);
   }
}


/*
 * Callback handler for button press on the panel
 */
static void b1_cb(Widget *wid, void *cb_data)
{
   int bn = VOIDP2INT(cb_data);
   int k = event_key();
   if (k && k <= 7) {
      _MSG("[%s], mouse button %d was pressed\n", button_names[bn], k);
      _MSG("mouse button %d was pressed\n", k);
   }
   switch (bn) {
   case UI_BACK:
      if (k == 1) {
         a_UIcmd_back(wid->window()->user_data());
      } else if (k == 3) {
         a_UIcmd_back_popup(wid->window()->user_data());
      }
      break;
   case UI_FORW:
      if (k == 1) {
         a_UIcmd_forw(wid->window()->user_data());
      } else if (k == 3) {
         a_UIcmd_forw_popup(wid->window()->user_data());
      }
      break;
   case UI_HOME:
      if (k == 1) {
         a_UIcmd_home(wid->window()->user_data());
      }
      break;
   case UI_RELOAD:
      if (k == 1) {
         a_UIcmd_reload(wid->window()->user_data());
      }
      break;
   case UI_SAVE:
      if (k == 1) {
         a_UIcmd_save(wid->window()->user_data());
      }
      break;
   case UI_STOP:
      if (k == 1) {
         a_UIcmd_stop(wid->window()->user_data());
      }
      break;
   case UI_BOOK:
      if (k == 1) {
         a_UIcmd_book(wid->window()->user_data());
      }
      break;
   default:
      break;
   }
}

/*
 * Callback handler for fullscreen button press
 */
static void fullscreen_cb(Widget *wid, void *data)
{
   /* todo: do we want to toggle fullscreen or panelmode?
            maybe we need to add another button?*/
   ((UI*)data)->panelmode_cb_i();
}

/*
 * Callback for the bug meter button.
 */
static void bugmeter_cb(Widget *w, void *data)
{
   int k = event_key();
   if (k && k <= 7)
      MSG("[BugMeter], mouse button %d was pressed\n", k);
   if (k == 1) {
      a_UIcmd_view_page_bugs(((UI*)data)->user_data());
   } else if (k == 3) {
      a_UIcmd_bugmeter_popup(((UI*)data)->user_data());
   }
}

/*
 * Callback for the image loading button.
 */
static void imageload_cb(Widget *w, void *data)
{
   int k = event_key();
   //if (k && k <= 7)
   //   MSG("[ImageLoad], mouse button %d was pressed\n", k);
   if (k == 1) {
      ((UI*)data)->imageload_toggle();
   }
}

/*
 * File menu item callback.
 */
static void menu_cb(Widget* w, void*)
{
  Menu* menu = (Menu*)w;
  Widget* item = menu->get_item();
  MSG("Callback for %s, item is %s\n",
      menu->label() ? menu->label() : "menu bar",
      item ? item->label() ? item->label() : "unnamed" : "none");
  //if (item) item->do_callback();
  menu->value(-1);
}

//////////////////////////////////////////////////////////////////////////////
// UI class methods
//

//----------------------------
// Panel construction methods
//----------------------------

/*
 * Create the archetipic browser buttons
 */
PackedGroup *UI::make_toolbar(int tw, int th)
{
   HighlightButton *b;
   MultiImage *multi;
   PackedGroup *p1=new PackedGroup(0,0,tw,th);
   p1->begin();
    Back = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Back" : 0);
    ImgLeftIns = new xpmImage(Small_Icons ? left_si_xpm : left_i_xpm);
    ImgLeftSens = new xpmImage(Small_Icons ? left_s_xpm : left_xpm);
    multi = new MultiImage(*ImgLeftSens, INACTIVE_R, *ImgLeftIns);
    b->image(multi);
    b->tooltip("Previous page");
    b->callback(b1_cb, (void *)UI_BACK);
    HighlightButton::default_style->highlight_color(CuteColor);

    Forw = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Forw" : 0);
    ImgRightIns = new xpmImage(Small_Icons ? right_si_xpm : right_i_xpm);
    ImgRightSens = new xpmImage(Small_Icons ? right_s_xpm : right_xpm);
    multi = new MultiImage(*ImgRightSens, INACTIVE_R, *ImgRightIns);
    b->image(multi);
    b->tooltip("Next page");
    b->callback(b1_cb, (void *)UI_FORW);
  
    Home = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Home" : 0);
    b->image(new xpmImage(Small_Icons ? home_s_xpm : home_xpm));
    b->tooltip("Go to the Home page");
    b->callback(b1_cb, (void *)UI_HOME);

    Reload = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Reload" : 0);
    b->image(new xpmImage(Small_Icons ? reload_s_xpm : reload_xpm));
    b->tooltip("Reload");
    b->callback(b1_cb, (void *)UI_RELOAD);
  
    Save = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Save" : 0);
    b->image(new xpmImage(Small_Icons ? save_s_xpm : save_xpm));
    b->tooltip("Save this page");
    b->callback(b1_cb, (void *)UI_SAVE);
  
    Stop = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Stop" : 0);
    ImgStopIns = new xpmImage(Small_Icons ? stop_si_xpm : stop_i_xpm);
    ImgStopSens = new xpmImage(Small_Icons ? stop_s_xpm : stop_xpm);
    multi = new MultiImage(*ImgStopSens, INACTIVE_R, *ImgStopIns);
    b->image(multi);
    b->tooltip("Stop loading");
    b->callback(b1_cb, (void *)UI_STOP);

    Bookmarks = b = new HighlightButton(xpos, 0, bw, bh, (lbl) ? "Book" : 0);
    b->image(new xpmImage(Small_Icons ? bm_s_xpm : bm_xpm));
    b->tooltip("View bookmarks");
    b->callback(b1_cb, (void *)UI_BOOK);

   p1->type(PackedGroup::ALL_CHILDREN_VERTICAL);
   p1->end();

   return p1;
}

/*
 * Create the location box (Clear/Input/Search)
 */
PackedGroup *UI::make_location()
{
   Button *b;
   PackedGroup *pg = new PackedGroup(0,0,0,0);
   pg->begin();
    Clear = b = new NewHighlightButton(2,2,16,22,0);
    b->image(new xpmImage(new_s_xpm));
    b->tooltip("Clear the URL box.\nMiddle-click to paste a URL.");
    //b->callback(b1_cb, (void *)UI_CLEAR);
    b->callback(clear_cb, (void *)this);

    Input *i = Location = new NewInput(0,0,0,0,0);
    i->tooltip("Location");
    i->color(CuteColor);
    i->when(WHEN_ENTER_KEY);
    i->callback(location_cb, this);

    Search = b = new HighlightButton(0,0,16,22,0);
    b->image(new xpmImage(search_xpm));
    b->tooltip("Search the Web");
    //b->callback(b1_cb, (void *)UI_SEARCH);
    b->callback(search_cb, (void *)this);

   pg->type(PackedGroup::ALL_CHILDREN_VERTICAL);
   pg->resizable(i);
   pg->end();

   return pg;
}

/*
 * Create the progress bars
 */
PackedGroup *UI::make_progress_bars(int wide, int thin_up)
{
   ProgBox = new PackedGroup(0,0,0,0);
   ProgBox->begin();
    // Images
    IProg = new NewProgressBox(0,0,0,0);
    IProg->box(thin_up ? THIN_UP_BOX : EMBOSSED_BOX);
    IProg->labelcolor(GRAY10);
    IProg->update_label(wide ? "Images\n0 of 0" : "0 of 0");
    // Page
    PProg = new NewProgressBox(0,0,0,0);
    PProg->box(thin_up ? THIN_UP_BOX : EMBOSSED_BOX);
    PProg->labelcolor(GRAY10);
    PProg->update_label(wide ? "Page\n0.0KB" : "0.0KB");
   ProgBox->type(PackedGroup::ALL_CHILDREN_VERTICAL);
   ProgBox->end();

   return ProgBox;
}

/*
 * Create the "File" menu
 */
Group *UI::make_menu(int tiny)
{
   Item *i;

    PopupMenu *pm = new PopupMenu(2,2,30,fh-4, tiny ? "&F" : "&File");
    pm->callback(menu_cb);
    pm->begin();
     i = new Item("&New Browser");
     i->shortcut(CTRL+'n');
     i = new Item("&Open File...");
     i->shortcut(CTRL+'o');
     i = new Item("Open UR&L...");
     i->shortcut(CTRL+'l');
     i = new Item("Close Window");
     i->shortcut(CTRL+'q');
     new Divider();
     i = new Item("Exit Dillo");
     i->shortcut(ALT+'q');

    pm->end();

   return pm;
}

/*
 * Create the control panel
 */
Group *UI::make_panel(int ww)
{
   Widget *w;
   Group *g1, *g2, *g3;
   PackedGroup *pg;

   if (PanelSize > P_large) {
      PanelSize = P_tiny;
      Small_Icons = !Small_Icons;
   }
   
   if (PanelSize == P_tiny) {
      if (Small_Icons)
         xpos = 0, bw = 22, bh = 22, fh = 0, lh = 22, lbl = 0;
      else
         xpos = 0, bw = 28, bh = 28, fh = 0, lh = 28, lbl = 0;
   } else if (PanelSize == P_small) {
      if (Small_Icons)
         xpos = 0, bw = 20, bh = 20, fh = 0, lh = 20, lbl = 0;
      else
         xpos = 0, bw = 28, bh = 28, fh = 0, lh = 28, lbl = 0;
   } else if (PanelSize == P_medium) {
      if (Small_Icons)
         xpos = 0, bw = 42, bh = 36, fh = 0, lh = 22, lbl = 1;
      else
         xpos = 0, bw = 45, bh = 45, fh = 0, lh = 28, lbl = 1;
   } else {   // P_large
      if (Small_Icons)
         xpos = 0, bw = 42, bh = 36, fh = 22, lh = 22, lbl = 1;
      else
         xpos = 0, bw = 45, bh = 45, fh = 28, lh = 28, lbl = 1;
   }

   if (PanelSize == P_tiny) {
      g1 = new Group(0,0,ww,bh);
      g1->begin();
       // Toolbar
       pg = make_toolbar(ww,bh);
       pg->box(EMBOSSED_BOX);
       //pg->box(BORDER_FRAME);
        w = make_location();
       pg->add(w);
       pg->resizable(w);
        w = make_progress_bars(0,1);
       pg->add(w);

      g1->resizable(pg);
      g1->end();

   } else {
      g1 = new Group(0,0,ww,fh+lh+bh);
      g1->begin();
       // File menu
       if (PanelSize == P_large) {
          g2 = new Group(0,0,ww,fh);
          g2->begin();
           make_menu(0);
          g2->box(EMBOSSED_BOX);
          g2->end();
       }

       // Location
       g2 = new Group(0,fh,ww,lh);
       g2->begin();
        pg = make_location();
        pg->resize(ww,lh);
       g2->resizable(pg);
       g2->end();
   
       // Toolbar
       g3 = new Group(0,fh+lh,ww,bh);
       g3->begin();
        pg = make_toolbar(ww,bh);
        //w = new InvisibleBox(0,0,0,0,"i n v i s i b l e");
        w = new InvisibleBox(0,0,0,0,0);
        pg->add(w);
        pg->resizable(w);

        if (PanelSize == P_small) {
           w = make_progress_bars(0,0);
        } else {
           w = make_progress_bars(1,0);
        }
        pg->add(w);
   
       g3->resizable(pg); // Better than 'w3' and it also works
       pg->box(BORDER_FRAME);
       //g3->box(EMBOSSED_BOX);
       g3->end();
   
      g1->resizable(g3);
      g1->end();
   }

   return g1;
}

/*
 * User Interface constructor
 */ 
UI::UI(int win_w, int win_h, const char* label, const UI *cur_ui) :
  Window(win_w, win_h, label)
{
   int s_h = 20;
   clear_double_buffer();

   TopGroup = new PackedGroup(0, 0, win_w, win_h);
   add(TopGroup);
   resizable(TopGroup);
   
   if (cur_ui) {
      PanelSize = cur_ui->PanelSize;
      CuteColor = cur_ui->CuteColor;
      Small_Icons = cur_ui->Small_Icons;
      Panelmode = cur_ui->Panelmode;
   } else {
     // Set some default values
     //PanelSize = P_tiny, CuteColor = 26, Small_Icons = 0;
     PanelSize = prefs.panel_size;
     Small_Icons = prefs.small_icons;
     CuteColor = 206;
     Panelmode = (UIPanelmode) prefs.fullwindow_start;
   }


   // Set handler for the close window event
   // (the argument is set later via user_data())
   callback(close_window_cb);

   // Control panel
   Panel = make_panel(win_w);
   TopGroup->add(Panel);


   // Render area
   Main = new Widget(0,0,1,1,"Welcome...");
   Main->box(FLAT_BOX);
   Main->color(GRAY15);
   Main->labelfont(HELVETICA_BOLD_ITALIC);
   Main->labelsize(36);
   Main->labeltype(SHADOW_LABEL);
   Main->labelcolor(WHITE);
   TopGroup->add(Main);
   TopGroup->resizable(Main);
   MainIdx = TopGroup->find(Main);

   // Status Panel
   StatusPanel = new Group(0, 0, win_w, s_h, 0);
   // Status box
   int il_w = 16;
   int bm_w = 16;
   Status = new Output(0, 0, win_w-bm_w-il_w, s_h, 0);
   Status->value("");
   //Status->box(UP_BOX);
   Status->box(THIN_DOWN_BOX);
   Status->clear_click_to_focus();
   Status->clear_tab_to_focus();
   StatusPanel->add(Status);
   //Status->throw_focus();

   // Image loading indicator
   ImageLoad = new HighlightButton(win_w-il_w-bm_w,0,il_w,s_h,0);
   ImgImageLoadOn = new xpmImage(imgload_on_xpm);
   ImgImageLoadOff = new xpmImage(imgload_off_xpm);
   if (prefs.load_images) {
      ImageLoad->image(ImgImageLoadOn);
   } else {
      ImageLoad->image(ImgImageLoadOff);
   }
   ImageLoad->box(THIN_DOWN_BOX);
   ImageLoad->align(ALIGN_INSIDE|ALIGN_CLIP|ALIGN_LEFT);
   ImageLoad->tooltip("Click me to toggle image loading.");
   ImageLoad->callback(imageload_cb, (void *)this);
   StatusPanel->add(ImageLoad);

   // Bug Meter
   BugMeter = new HighlightButton(win_w-bm_w,0,bm_w,s_h,0);
   ImgMeterOK = new xpmImage(mini_ok_xpm);
   ImgMeterBug = new xpmImage(mini_bug_xpm);
   BugMeter->image(ImgMeterOK);
   BugMeter->box(THIN_DOWN_BOX);
   BugMeter->align(ALIGN_INSIDE|ALIGN_CLIP|ALIGN_LEFT);
   BugMeter->tooltip("Show HTML bugs\n(right-click for menu)");
   BugMeter->callback(bugmeter_cb, (void *)this);
   StatusPanel->add(BugMeter);

   StatusPanel->resizable(Status);

   TopGroup->add(StatusPanel); 

   // Make the full screen button (to be attached to the viewport later)
   // TODO: attach to the viewport
   FullScreen = new HighlightButton(0,0,15,15);
   ImgFullScreenOn = new xpmImage(full_screen_on_xpm);
   ImgFullScreenOff = new xpmImage(full_screen_off_xpm);
   FullScreen->image(ImgFullScreenOn);
   FullScreen->tooltip("Hide Controls");
   FullScreen->callback(fullscreen_cb, (void *)this);

   customize(0);

   if (Panelmode) {
      Panel->hide();
      StatusPanel->hide();
   }

   //show();
}

/*
 * FLTK event handler for this window.
 */
int UI::handle(int event)
{
   int ret = 0, k = event_key();

   _MSG("UI::handle event=%d\n", event);

   // Let FLTK pass these events to child widgets.
   if (event == KEY) {
      if (k == UpKey || k == DownKey || k == SpaceKey ||
          k == LeftKey || k == RightKey)
         return 0;
      // Ignore Escape for main window.
      if (k == EscapeKey)
         ret = 1;

   } else if (event == SHORTCUT) {
      // Handle these shortcuts here.
      if (event_state(CTRL)) {
         if (k == 'b') {
            a_UIcmd_book(user_data());
            ret = 1;
         } else if (k == 'f') {
            a_UIcmd_findtext_dialog((BrowserWindow*) user_data());
            ret = 1;
         } else if (k == 'l') {
            if (Panelmode == UI_HIDDEN) {
               set_panelmode(UI_TEMPORARILY_SHOW_PANELS);
            }
            focus_location();
            ret = 1;
         } else if (k == 'n') {
            a_UIcmd_browser_window_new(w(), h(), this);
            ret = 1;
         } else if (k == 'o') {
            a_UIcmd_open_file(user_data());
            ret = 1;
         } else if (k == 'q') {
            a_UIcmd_close_bw(user_data());
            ret = 1;
         } else if (k == 'r') {
            a_UIcmd_reload(user_data());
            ret = 1;
         } else if (k == 's') {
            a_UIcmd_search_dialog(user_data());
            ret = 1;
         } else if (k == ' ') {
            panelmode_cb_i();
            ret = 1;
         }
      }

      if (event_key_state(LeftAltKey) && k == 'q') {
         a_UIcmd_close_all_bw();
         ret = 1;
      }

      // Back and Forward navigation shortcuts
      if ((!event_state(SHIFT) && k == BackSpaceKey) || k == ',') {
         a_UIcmd_back(user_data());
         ret = 1;
      } else if ((event_state(SHIFT) && k == BackSpaceKey) || k == '.') {
         a_UIcmd_forw(user_data());
         ret = 1;
      }
   }

   if (ret == 0) {
      ret = Window::handle(event);
   }
   return ret;
}


//----------------------------
// API for the User Interface
//----------------------------

/*
 * Get the text from the location input-box.
 */
const char *UI::get_location()
{
   return Location->value();
}

/*
 * Set a new URL in the location input-box.
 */
void UI::set_location(const char *str)
{
   Location->static_value("");
   Location->insert(str);
}

/*
 * Focus location entry.
 */
void UI::focus_location()
{
   Location->take_focus();
}

/*
 * Set a new message in the status bar.
 */
void UI::set_status(const char *str)
{
   Status->value(str);
}

/*
 * Set the page progress text
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void UI::set_page_prog(size_t nbytes, int cmd)
{
   char str[32];

   if (cmd == 0) {
      PProg->deactivate();
   } else {
      PProg->activate();
      if (cmd == 1) {
         snprintf(str, 32, "%s%.1f KB",
                  (PanelSize == 0) ? "" : "Page\n", nbytes/1024.0);
      } else if (cmd == 2) {
         str[0] = '\0';
      }
      PProg->update_label(str);
   }
}

/*
 * Set the image progress text
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void UI::set_img_prog(int n_img, int t_img, int cmd)
{
   char str[32];

   if (cmd == 0) {
      IProg->deactivate();
   } else {
      IProg->activate();
      if (cmd == 1) {
         snprintf(str, 32, "%s%d of %d", 
                  (PanelSize == 0) ? "" : "Images\n", n_img, t_img);
      } else if (cmd == 2) {
         str[0] = '\0';
      }
      IProg->update_label(str);
   }
}

/*
 * Set the bug meter progress text
 */
void UI::set_bug_prog(int n_bug)
{
   char str[32];
   int new_w = 16;

   if (n_bug == 0) {
      BugMeter->image(ImgMeterOK);
      BugMeter->label("");
   } else if (n_bug >= 1) {
      if (n_bug == 1)
         BugMeter->image(ImgMeterBug);
      snprintf(str, 32, "%d", n_bug);
      BugMeter->copy_label(str);
      BugMeter->redraw_label();
      new_w = strlen(str)*8 + 20;
   }
   Status->resize(0,0,StatusPanel->w()-ImageLoad->w()-new_w,Status->h());
   ImageLoad->resize(StatusPanel->w()-ImageLoad->w()-new_w, 0, ImageLoad->w(),
                     ImageLoad->h());
   BugMeter->resize(StatusPanel->w()-new_w, 0, new_w, BugMeter->h());
   StatusPanel->init_sizes();
}

/*
 * Customize the UI's panel (show/hide buttons)
 */
void UI::customize(int flags)
{
   // flags argument not currently used

   if ( !prefs.show_menubar )
      MSG("show_menubar preference ignored\n");
   if ( !prefs.show_back )
      Back->hide();
   if ( !prefs.show_forw )
      Forw->hide();
   if ( !prefs.show_home )
      Home->hide();
   if ( !prefs.show_reload )
      Reload->hide();
   if ( !prefs.show_save )
      Save->hide();
   if ( !prefs.show_stop )
      Stop->hide();
   if ( !prefs.show_bookmarks )
      Bookmarks->hide();
   if ( !prefs.show_clear_url )
      Clear->hide();
   if ( !prefs.show_url )
      Location->hide();
   if ( !prefs.show_search )
      Search->hide();
   if ( !prefs.show_progress_box )
      ProgBox->hide();
}

/*
 * On-the-fly panel style change
 */
void UI::panel_cb_i()
{
   Group *NewPanel;

   // Create a new Panel
   ++PanelSize;
   NewPanel = make_panel(TopGroup->w());
   TopGroup->replace(*Panel, *NewPanel);
   delete(Panel);
   Panel = NewPanel;
   customize(0);

   Location->take_focus();
}

/*
 * On-the-fly color style change
 */
void UI::color_change_cb_i()
{
   static int ncolor = 0, cols[] = {7,17,26,51,140,156,205,206,215,-1};

   ncolor = (cols[ncolor+1] < 0) ? 0 : ncolor + 1;
   CuteColor = cols[ncolor];
   MSG("Location color %d\n", CuteColor);
   Location->color(CuteColor);
   Location->redraw();
   HighlightButton::default_style->highlight_color(CuteColor);
}

/*
 * Set or remove the Panelmode flag and update the UI accordingly
 */
void UI::set_panelmode(UIPanelmode mode)
{
   if (mode == UI_HIDDEN) {
      Panel->hide();
      StatusPanel->hide();
   } else {
      /* UI_NORMAL or UI_TEMPORARILY_SHOW_PANELS */
      Panel->show();
      StatusPanel->show();
   }
   Panelmode = mode;
}

/*
 * Get the value of the panelmode flag
 */
UIPanelmode UI::get_panelmode()
{
   return Panelmode;
}

/*
 * Toggle the Control Panel out of the way
 */
void UI::panelmode_cb_i()
{
   set_panelmode((UIPanelmode) !Panelmode);
}

/*
 * Toggle image loading
 */
void UI::imageload_toggle()
{
   prefs.load_images = !prefs.load_images;
   if (prefs.load_images) {
      ImageLoad->image(ImgImageLoadOn);
   } else {
      ImageLoad->image(ImgImageLoadOff);
   }
}

/*
 * Set 'nw' as the main render area widget
 */
void UI::set_render_layout(Widget &nw)
{
   // BUG: replace() is not working as it should.
   // In our case, replacing the rendering area leaves the vertical
   // scrollbar without events.
   //
   // We'll use a workaround in a_UIcmd_browser_window_new() instead.
   TopGroup->replace(MainIdx, nw);
   delete(Main);
   Main = &nw;
   //TopGroup->box(DOWN_BOX);
   //TopGroup->box(BORDER_FRAME);
   TopGroup->resizable(TopGroup->child(MainIdx));
}

/*
 * Set the window title
 */
void UI::set_page_title(const char *label)
{
   char title[128];

   snprintf(title, 128, "Dillo: %s", label);
   this->copy_label(title);
   this->redraw_label();
}

/*
 * Set button sensitivity (Back/Forw/Stop)
 */
void UI::button_set_sens(UIButton btn, int sens)
{
   switch (btn) {
   case UI_BACK:
      (sens) ? Back->activate() : Back->deactivate();
      Back->redraw(DAMAGE_HIGHLIGHT);
      break;
   case UI_FORW:
      (sens) ? Forw->activate() : Forw->deactivate();
      Forw->redraw(DAMAGE_HIGHLIGHT);
      break;
   case UI_STOP:
      (sens) ? Stop->activate() : Stop->deactivate();
      Stop->redraw(DAMAGE_HIGHLIGHT);
      break;
   default:
      break;
   }
}

/*
 * Paste a middle-click-selection into "Clear" button as URL
 */
void UI::paste_url()
{
   paste(*Clear, false);
}

