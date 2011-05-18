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

#include <unistd.h>
#include <stdio.h>

#include "keys.hh"
#include "ui.hh"
#include "msg.h"
#include "timeout.hh"
#include "utf8.hh"

#include <FL/Fl.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Box.H>

// Include image data
#include "pixmaps.h"
#include "uicmd.hh"

struct iconset {
   Fl_Image *ImgMeterOK, *ImgMeterBug,
            *ImgHome, *ImgReload, *ImgSave, *ImgBook, *ImgTools,
            *ImgClear,*ImgSearch, *ImgHelp, *ImgLeft, *ImgLeftIn,
            *ImgRight, *ImgRightIn, *ImgStop, *ImgStopIn;
};

static struct iconset standard_icons = {
   new Fl_Pixmap(mini_ok_xpm),
   new Fl_Pixmap(mini_bug_xpm),
   new Fl_Pixmap(home_xpm),
   new Fl_Pixmap(reload_xpm),
   new Fl_Pixmap(save_xpm),
   new Fl_Pixmap(bm_xpm),
   new Fl_Pixmap(tools_xpm),
   new Fl_Pixmap(new_s_xpm),
   new Fl_Pixmap(search_xpm),
   new Fl_Pixmap(help_xpm),
   new Fl_Pixmap(left_xpm),
   new Fl_Pixmap(left_i_xpm),
   new Fl_Pixmap(right_xpm),
   new Fl_Pixmap(right_i_xpm),
   new Fl_Pixmap(stop_xpm),
   new Fl_Pixmap(stop_i_xpm),
};

static struct iconset small_icons = {
   standard_icons.ImgMeterOK,
   standard_icons.ImgMeterBug,
   new Fl_Pixmap(home_s_xpm),
   new Fl_Pixmap(reload_s_xpm),
   new Fl_Pixmap(save_s_xpm),
   new Fl_Pixmap(bm_s_xpm),
   new Fl_Pixmap(tools_s_xpm),
   new Fl_Pixmap(new_s_xpm),
   standard_icons.ImgSearch,
   standard_icons.ImgHelp,
   new Fl_Pixmap(left_s_xpm),
   new Fl_Pixmap(left_si_xpm),
   new Fl_Pixmap(right_i_xpm),
   new Fl_Pixmap(right_si_xpm),
   new Fl_Pixmap(stop_s_xpm),
   new Fl_Pixmap(stop_si_xpm),
};


static struct iconset *icons = &standard_icons;

/*
 * Local sub classes
 */

//----------------------------------------------------------------------------

/*
 * (Used to avoid certain shortcuts in the location bar)
 */
class CustInput : public Fl_Input {
public:
   CustInput (int x, int y, int w, int h, const char* l=0) :
      Fl_Input(x,y,w,h,l) {};
   int handle(int e);
};

/*
 * Disable keys: Up, Down, Page_Up, Page_Down, Tab and
 * CTRL+{o,r,Home,End}  SHIFT+{Left,Right}.
 */
int CustInput::handle(int e)
{
   int k = Fl::event_key();

   _MSG("CustInput::handle event=%d\n", e);

   // We're only interested in some flags
   unsigned modifier = Fl::event_state() & (FL_SHIFT | FL_CTRL | FL_ALT);

   // Don't focus with arrow keys
   if (e == FL_FOCUS &&
       (k == FL_Up || k == FL_Down || k == FL_Left || k == FL_Right)) {
      return 0;
   } else if (e == FL_KEYBOARD) {
      if (k == FL_Escape && modifier == 0) {
         // Let the parent group handle this Esc key
         return 0;
      } else if (modifier == FL_SHIFT) {
         if (k == FL_Left || k == FL_Right) {
            // Let these keys get to the UI
            return 0;
         }
      } else if (modifier == FL_CTRL) {
         if (k == 'a' || k == 'e') {
            position(k == 'a' ? 0 : size());
            return 1;
         } else if (k == 'k') {
            cut(position(), size());
            return 1;
         } else if (k == 'd') {
            cut(position(), position()+1);
            return 1;
         } else if (k == 'l') {
            // Make text selected when already focused.
            position(size(), 0);
            return 1;
         } else if (k == 'h' || k == 'o' || k == 'r' ||
                    k == FL_Home || k == FL_End) {
            // Let these keys get to the UI
            return 0;
         }
      } else if (modifier == 0) {
         if (k == FL_Down || k == FL_Up ||
             k == FL_Page_Down || k == FL_Page_Up || k == FL_Tab) {
            // Give up focus and honor the key
            a_UIcmd_focus_main_area(a_UIcmd_get_bw_by_widget(this));
            return 0;
         }
      }
   }

   return Fl_Input::handle(e);
}

//----------------------------------------------------------------------------

/*
 * A button that highlights on mouse over
 */
class CustLightButton : public Fl_Button {
   Fl_Color norm_color;
public:
   CustLightButton(int x, int y, int w, int h, const char *l=0) :
      Fl_Button(x,y,w,h,l) { norm_color = color(); };
   virtual int handle(int e);
};

int CustLightButton::handle(int e)
{
   if (active()) {
      if (e == FL_ENTER) {
         color(51); // {17,26,51}
         redraw();
      } else if (e == FL_LEAVE || e == FL_RELEASE) {
         color(norm_color);
         redraw();
      }
   }
   return Fl_Button::handle(e);
}

//----------------------------------------------------------------------------

/*
 * Used to handle "paste" within the toolbar's Clear button.
 */
class CustPasteButton : public CustLightButton {
public:
   CustPasteButton(int x, int y, int w, int h, const char *l=0) :
      CustLightButton(x,y,w,h,l) {};
   int handle(int e);
};

int CustPasteButton::handle(int e)
{
   if (e == FL_PASTE) {
      const char* t = Fl::event_text();
      if (t && *t) {
         a_UIcmd_set_location_text(a_UIcmd_get_bw_by_widget(this), t);
         a_UIcmd_open_urlstr(a_UIcmd_get_bw_by_widget(this), t);
         return 1;
      }
   }
   return CustLightButton::handle(e);
}

//----------------------------------------------------------------------------

/*
 * Used to resize the progress boxes automatically.
 */
class CustProgressBox : public Fl_Box {
   int padding;
public:
   CustProgressBox(int x, int y, int w, int h, const char *l=0) :
      Fl_Box(x,y,w,h,l) { padding = 0; };
   void update_label(const char *lbl) {
      int w = 0, h = 0;
      if (!padding) {
         copy_label("W");
         measure_label(w, h);
         padding = w > 2 ? w/2 : 1;
      }
      copy_label(lbl);
      //measure_label(w,h);
      //size(w+padding,this->h());
   }
};

//
// Toolbar buttons -----------------------------------------------------------
//
//static const char *button_names[] = {
//   "Back", "Forward", "Home", "Reload", "Save", "Stop", "Bookmarks", "Tools",
//   "Clear", "Search"
//};


//
// Callback functions --------------------------------------------------------
//

/*
 * Callback for the search button.
 */
static void search_cb(Fl_Widget *wid, void *data)
{
   int b = Fl::event_button();

   if (b == FL_LEFT_MOUSE) {
      a_UIcmd_search_dialog(a_UIcmd_get_bw_by_widget(wid));
   } else if (b == FL_MIDDLE_MOUSE) {
      ((UI*)data)->color_change_cb_i();
   } else if (b == FL_RIGHT_MOUSE) {
      // nothing ATM
   }
}

/*
 * Callback for the help button.
 */
static void help_cb(Fl_Widget *w, void *)
{
   char *path = dStrconcat(DILLO_DOCDIR, "user_help.html", NULL);
   BrowserWindow *bw = a_UIcmd_get_bw_by_widget(w);

   if (access(path, R_OK) == 0) {
      char *urlstr = dStrconcat("file:", path, NULL);
      a_UIcmd_open_urlstr(bw, urlstr);
      dFree(urlstr);
   } else {
      MSG("Can't read local help file at \"%s\"."
          " Getting remote help...\n", path);
      a_UIcmd_open_urlstr(bw, "http://www.dillo.org/dillo2-help.html");
   }
   dFree(path);
}

/*
 * Callback for the File menu button.
 */
static void filemenu_cb(Fl_Widget *wid, void *)
{
   int b = Fl::event_button();
   if (b == FL_LEFT_MOUSE || b == FL_RIGHT_MOUSE) {
      a_UIcmd_file_popup(a_UIcmd_get_bw_by_widget(wid), wid);
   }
}

/*
 * Callback for the location's clear-button.
 */
static void clear_cb(Fl_Widget *w, void *data)
{
   UI *ui = (UI*)data;

   int b = Fl::event_button();
   if (b == FL_LEFT_MOUSE) {
      ui->set_location("");
      ui->focus_location();
   } if (b == FL_MIDDLE_MOUSE) {
      ui->paste_url();
   }
}

/*
 * Change the color of the location bar.
 *
static void color_change_cb(Fl_Widget *wid, void *data)
{
   ((UI*)data)->color_change_cb_i();
}
 */


/*
 * Send the browser to the new URL in the location.
 */
static void location_cb(Fl_Widget *wid, void *data)
{
   Fl_Input *i = (Fl_Input*)wid;
   UI *ui = (UI*)data;

   _MSG("location_cb()\n");
   /* This test is necessary because WHEN_ENTER_KEY also includes
    * other events we're not interested in. For instance pressing
    * The Back or Forward, buttons, or the first click on a rendered
    * page. BUG: this must be investigated and reported to FLTK2 team */
   if (Fl::event_key() == FL_Enter) {
      a_UIcmd_open_urlstr(a_UIcmd_get_bw_by_widget(i), i->value());
   }
   if (ui->get_panelmode() == UI_TEMPORARILY_SHOW_PANELS) {
      ui->set_panelmode(UI_HIDDEN);
   }
}


/*
 * Callback handler for button press on the panel
 */
static void b1_cb(Fl_Widget *wid, void *cb_data)
{
   int bn = VOIDP2INT(cb_data);
   int b = Fl::event_button();
   if (b >= FL_LEFT_MOUSE && b <= FL_RIGHT_MOUSE) {
      _MSG("[%s], mouse button %d was pressed\n", button_names[bn], b);
      _MSG("mouse button %d was pressed\n", b);
   }
   switch (bn) {
   case UI_BACK:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_back(a_UIcmd_get_bw_by_widget(wid));
      } else if (b == FL_RIGHT_MOUSE) {
         a_UIcmd_back_popup(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_FORW:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_forw(a_UIcmd_get_bw_by_widget(wid));
      } else if (b == FL_RIGHT_MOUSE) {
         a_UIcmd_forw_popup(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_HOME:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_home(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_RELOAD:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_reload(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_SAVE:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_save(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_STOP:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_stop(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_BOOK:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_book(a_UIcmd_get_bw_by_widget(wid));
      }
      break;
   case UI_TOOLS:
      if (b == FL_LEFT_MOUSE || b == FL_RIGHT_MOUSE) {
         a_UIcmd_tools(a_UIcmd_get_bw_by_widget(wid), wid);
      }
      break;
   default:
      break;
   }
}

/*
 * Callback for the bug meter button.
 */
static void bugmeter_cb(Fl_Widget *wid, void *data)
{
   int b = Fl::event_button();
   if (b == FL_LEFT_MOUSE) {
      a_UIcmd_view_page_bugs(a_UIcmd_get_bw_by_widget(wid));
   } else if (b == FL_RIGHT_MOUSE) {
      a_UIcmd_bugmeter_popup(a_UIcmd_get_bw_by_widget(wid));
   }
}

//////////////////////////////////////////////////////////////////////////////
// UI class methods
//

//----------------------------
// Panel construction methods
//----------------------------

/*
 * Make a generic navigation button
 */
Fl_Button *UI::make_button(const char *label, Fl_Image *img, Fl_Image *deimg,
                           int b_n, int start)
{
   if (start)
      p_xpos = 0;

   Fl_Button *b = new CustLightButton(p_xpos, 0, bw, bh, (lbl) ? label : NULL);
   if (img)
      b->image(img);
   if (deimg)
      b->deimage(deimg);
   b->callback(b1_cb, (void *)b_n);
   b->clear_visible_focus();
   b->labelsize(12);
   b->box(FL_FLAT_BOX);
   b->down_box(FL_THIN_DOWN_FRAME);
   p_xpos += bw;
   return b;
}

/*
 * Create the archetipic browser buttons
 */
void UI::make_toolbar(int tw, int th)
{
   Back = make_button("Back", icons->ImgLeft, icons->ImgLeftIn, UI_BACK, 1);
   Forw = make_button("Forw", icons->ImgRight, icons->ImgRightIn, UI_FORW);
   Home = make_button("Home", icons->ImgHome, NULL, UI_HOME);
   Reload = make_button("Reload", icons->ImgReload, NULL, UI_RELOAD);
   Save = make_button("Save", icons->ImgSave, NULL, UI_SAVE);
   Stop = make_button("Stop", icons->ImgStop, icons->ImgStopIn, UI_STOP);
   Bookmarks = make_button("Book", icons->ImgBook, NULL, UI_BOOK);
   Tools = make_button("Tools", icons->ImgTools, NULL, UI_TOOLS);

   if (prefs.show_tooltip) {
      Back->tooltip("Previous page");
      Forw->tooltip("Next page");
      Home->tooltip("Go to the Home page");
      Reload->tooltip("Reload");
      Save->tooltip("Save this page");
      Stop->tooltip("Stop loading");
      Bookmarks->tooltip("View bookmarks");
      Tools->tooltip("Settings");
   }
}

/*
 * Create the location box (Clear/Input/Search)
 */
void UI::make_location(int ww)
{
   Fl_Button *b;

    Clear = b = new CustPasteButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgClear);
    b->callback(clear_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    p_xpos += b->w();

    Fl_Input *i = Location = new CustInput(p_xpos,0,ww-p_xpos-32,lh,0);
    i->color(CuteColor);
    i->when(FL_WHEN_ENTER_KEY);
    i->callback(location_cb, this);
    p_xpos += i->w();

    Search = b = new CustLightButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgSearch);
    b->callback(search_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    p_xpos += b->w();

    Help = b = new CustLightButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgHelp);
    b->callback(help_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    p_xpos += b->w();

   if (prefs.show_tooltip) {
      Clear->tooltip("Clear the URL box.\nMiddle-click to paste a URL.");
      Location->tooltip("Location");
      Search->tooltip("Search the Web");
      Help->tooltip("Help");
   }
}

/*
 * Create the progress bars
 */
void UI::make_progress_bars(int wide, int thin_up)
{
    // Images
    IProg = new CustProgressBox(p_xpos,p_ypos,pw,bh);
    IProg->labelsize(12);
    IProg->box(thin_up ? FL_THIN_UP_BOX : FL_EMBOSSED_BOX);
    IProg->labelcolor(FL_GRAY_RAMP + 2);
    IProg->update_label(wide ? "Images\n0 of 0" : "0 of 0");
    p_xpos += pw;
    // Page
    PProg = new CustProgressBox(p_xpos,p_ypos,pw,bh);
    PProg->labelsize(12);
    PProg->box(thin_up ? FL_THIN_UP_BOX : FL_EMBOSSED_BOX);
    PProg->labelcolor(FL_GRAY_RAMP + 2);
    PProg->update_label(wide ? "Page\n0.0KB" : "0.0KB");
}

/*
 * Create the "File" menu
 * Static function for File menu callbacks.
 */
Fl_Widget *UI::make_filemenu_button()
{
   Fl_Button *btn;
   int w = 0, h = 0, padding;

   FileButton = btn = new Fl_Button(p_xpos,0,0,0,"W");
   btn->labeltype(FL_FREE_LABELTYPE);
   btn->measure_label(w, h);
   padding = w;
   btn->copy_label(PanelSize == P_tiny ? "&F" : "&File");
   btn->measure_label(w,h);
   h = (PanelSize == P_large) ? mh : (PanelSize == P_tiny) ? bh : lh;
   btn->size(w+padding, h);
   p_xpos += btn->w();
   _MSG("UI::make_filemenu_button w=%d h=%d padding=%d\n", w, h, padding);
   btn->box(PanelSize == P_large ? FL_THIN_UP_BOX : FL_THIN_UP_BOX);
   btn->callback(filemenu_cb, this);
   if (prefs.show_tooltip)
      btn->tooltip("File menu");
   btn->clear_visible_focus();
   if (!prefs.show_filemenu)
      btn->hide();
   return btn;
}


/*
 * Create the control panel
 */
void UI::make_panel(int ww)
{
   Fl_Widget *w;

   if (Small_Icons)
      icons = &small_icons;
   else
      icons = &standard_icons;

   pw = 70;
   p_xpos = p_ypos = 0;
   if (PanelSize == P_tiny) {
      if (Small_Icons)
         bw = 22, bh = 22, mh = 0, lh = 22, lbl = 0;
      else
         bw = 28, bh = 28, mh = 0, lh = 28, lbl = 0;
   } else if (PanelSize == P_small) {
      if (Small_Icons)
         bw = 20, bh = 20, mh = 0, lh = 20, lbl = 0;
      else
         bw = 28, bh = 28, mh = 0, lh = 28, lbl = 0;
   } else if (PanelSize == P_medium) {
      if (Small_Icons)
         bw = 42, bh = 36, mh = 0, lh = 22, lbl = 1;
      else
         bw = 45, bh = 45, mh = 0, lh = 28, lbl = 1;
   } else {   // P_large
      if (Small_Icons)
         bw = 42, bh = 36, mh = 22, lh = 22, lbl = 1;
      else
         bw = 45, bh = 45, mh = 24, lh = 28, lbl = 1;
   }
   nh = bh, fh = 28; sh = 20;

   current(0);
   if (PanelSize == P_tiny) {
      NavBar = new CustGroup(0,0,ww,nh);
      NavBar->begin();
       make_toolbar(ww,bh);
       make_filemenu_button();
       make_location(ww);
       NavBar->resizable(Location);
       make_progress_bars(0,1);
      NavBar->box(FL_THIN_UP_FRAME);
      NavBar->end();
      TopGroup->insert(*NavBar,0);
   } else {
       if (PanelSize == P_large) {
          MenuBar = new CustGroup(0,0,ww,mh);
          MenuBar->begin();
           MenuBar->box(FL_THIN_UP_BOX);
           Fl_Widget *bn = make_filemenu_button();
           MenuBar->add_resizable(*new Fl_Box(bn->w(),0,ww - bn->w(),mh));
          MenuBar->end();
          TopGroup->insert(*MenuBar,0);

          p_xpos = 0;
          LocBar = new CustGroup(0,0,ww,lh);
          LocBar->begin();
           make_location(ww);
           LocBar->resizable(Location);
          LocBar->end();
          TopGroup->insert(*LocBar,1);
       } else {
          LocBar = new CustGroup(0,0,ww,lh);
          LocBar->begin();
           p_xpos = 0;
           make_filemenu_button();
           make_location(ww);
           LocBar->resizable(Location);
          LocBar->end();
          TopGroup->insert(*LocBar,0);
       }

       // Toolbar
       p_ypos = 0;
       NavBar = new CustGroup(0,0,ww,bh);
       NavBar->begin();
        make_toolbar(ww,bh);
        w = new Fl_Box(p_xpos,0,ww-p_xpos-2*pw,bh);
        w->box(FL_FLAT_BOX);
        NavBar->resizable(w);
        p_xpos = ww - 2*pw;
        if (PanelSize == P_small) {
           make_progress_bars(0,0);
        } else {
           make_progress_bars(1,0);
        }
       NavBar->end();
       TopGroup->insert(*NavBar,(MenuBar ? 2 : 1));
   }
}

/*
 * Create the status panel
 */
void UI::make_status_bar(int ww, int wh)
{
   const int bm_w = 20;
   StatusBar = new CustGroup(0, wh-sh, ww, sh);

    // Status box
    StatusOutput = new Fl_Output(0, wh-sh, ww-bm_w, sh);
    StatusOutput->value("http://www.dillo.org");
    StatusOutput->labelsize(8);
    StatusOutput->box(FL_THIN_DOWN_BOX);
    StatusOutput->clear_visible_focus();
    StatusOutput->color(FL_GRAY_RAMP + 18);

    // Bug Meter
    BugMeter = new CustLightButton(ww-bm_w,wh-sh,bm_w,sh);
    BugMeter->image(icons->ImgMeterOK);
    BugMeter->box(FL_THIN_DOWN_BOX);
    BugMeter->align(FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    if (prefs.show_tooltip)
       BugMeter->tooltip("Show HTML bugs\n(right-click for menu)");
    BugMeter->callback(bugmeter_cb, this);
    BugMeter->clear_visible_focus();

   StatusBar->end();
   StatusBar->resizable(StatusOutput);
}

/*
 * User Interface constructor
 */
UI::UI(int x, int y, int ui_w, int ui_h, const char* label, const UI *cur_ui) :
  Fl_Pack(x, y, ui_w, ui_h, label)
{
   PointerOnLink = FALSE;

   MenuBar = LocBar = NavBar = StatusBar = NULL;

   Tabs = NULL;
   TabTooltip = NULL;
   TopGroup = this;
   TopGroup->type(VERTICAL);
   TopGroup->box(FL_NO_BOX);
   clear_flag(SHORTCUT_LABEL);

   if (cur_ui) {
      PanelSize = cur_ui->PanelSize;
      CuteColor = cur_ui->CuteColor;
      Small_Icons = cur_ui->Small_Icons;
      if (cur_ui->Panelmode == UI_HIDDEN ||
          cur_ui->Panelmode == UI_TEMPORARILY_SHOW_PANELS)
         Panelmode = UI_HIDDEN;
      else
         Panelmode = UI_NORMAL;
   } else {
     // Set some default values
     //PanelSize = P_tiny, CuteColor = 26, Small_Icons = 0;
     PanelSize = prefs.panel_size;
     Small_Icons = prefs.small_icons;
     CuteColor = 206;
     Panelmode = (UIPanelmode) prefs.fullwindow_start;
   }

   // Control panel
   TopGroup->begin();
    make_panel(ui_w);

    // Render area
    int main_h = ui_h - (mh+(LocBar?lh:0)+nh+fh+sh);
    Main = new Fl_Group(0,0,0,main_h,"Welcome...");
    Main->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    Main->box(FL_FLAT_BOX);
    Main->color(FL_GRAY_RAMP + 3);
    Main->labelfont(FL_HELVETICA_BOLD_ITALIC);
    Main->labelsize(36);
    Main->labeltype(FL_SHADOW_LABEL);
    Main->labelcolor(FL_WHITE);
    TopGroup->add(Main);
    TopGroup->resizable(Main);
    MainIdx = TopGroup->find(Main);

    // Find text bar
    FindBarSpace = 1;
    FindBar = new Findbar(ui_w, fh);
    TopGroup->add(FindBar);

    // Status Panel
    make_status_bar(ui_w, ui_h);
    TopGroup->add(StatusBar);

   TopGroup->end();

   // Make the full screen button (to be attached to the viewport later)
   // TODO: attach to the viewport
   //FullScreen = new Fl_Button(0,0,15,15);
   //FullScreen->image(ImgFullScreenOn);
   //FullScreen->tooltip("Hide Controls");
   //FullScreen->callback(fullscreen_cb, this);

   customize(0);

   if (Panelmode) {
      //Panel->hide();
      //StatusBar->hide();
   }
}

/*
 * UI destructor
 */
UI::~UI()
{
   _MSG("UI::~UI()\n");
   dFree(TabTooltip);

   if (!FindBarSpace)
      delete FindBar;
}

/*
 * FLTK event handler for this window.
 */
int UI::handle(int event)
{
   _MSG("UI::handle event=%d (%d,%d)\n", event, Fl::event_x(), Fl::event_y());

   int ret = 0;
   if (event == FL_KEYBOARD) {
      return 0; // Receive as shortcut
   } else if (event == FL_SHORTCUT) {
      KeysCommand_t cmd = Keys::getKeyCmd();
      if (cmd == KEYS_NOP) {
         // Do nothing
      } else if (cmd == KEYS_SCREEN_UP || cmd == KEYS_SCREEN_DOWN ||
                 cmd == KEYS_LINE_UP || cmd == KEYS_LINE_DOWN ||
                 cmd == KEYS_LEFT || cmd == KEYS_RIGHT ||
                 cmd == KEYS_TOP || cmd == KEYS_BOTTOM) {
         a_UIcmd_scroll(a_UIcmd_get_bw_by_widget(this), cmd);
         ret = 1;
      } else if (cmd == KEYS_BACK) {
         a_UIcmd_back(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_FORWARD) {
         a_UIcmd_forw(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_BOOKMARKS) {
         a_UIcmd_book(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_FIND) {
         findbar_toggle(1);
         ret = 1;
      } else if (cmd == KEYS_WEBSEARCH) {
         a_UIcmd_search_dialog(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_GOTO) {
         focus_location();
         ret = 1;
      } else if (cmd == KEYS_HIDE_PANELS) {
         fullscreen_toggle();
         ret = 1;
         //if (get_panelmode() == UI_TEMPORARILY_SHOW_PANELS)
         //   set_panelmode(UI_HIDDEN);
      } else if (cmd == KEYS_OPEN) {
         a_UIcmd_open_file(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_HOME) {
         a_UIcmd_home(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_RELOAD) {
         a_UIcmd_reload(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_STOP) {
         a_UIcmd_stop(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_SAVE) {
         a_UIcmd_save(a_UIcmd_get_bw_by_widget(this));
         ret = 1;
      } else if (cmd == KEYS_FULLSCREEN) {
         fullscreen_toggle();
         ret = 1;
      } else if (cmd == KEYS_FILE_MENU) {
         a_UIcmd_file_popup(a_UIcmd_get_bw_by_widget(this), FileButton);
         ret = 1;
      }
   }
#if 0
   } else if (event == FL_PUSH) {
      if (prefs.middle_click_drags_page == 0 &&
          Fl::event_button() == FL_MIDDLE_MOUSE &&
          !a_UIcmd_pointer_on_link(a_UIcmd_get_bw_by_widget(this))) {
         if (Main->contains(Fl::belowmouse())) {
            /* Offer the event to Main's children (form widgets) */
            /* TODO: Try just offering it to Fl::belowmouse() */
            ret = ((Fl_Group *)Main)->Fl_Group::handle(event);
         }
         if (!ret) {
            /* middle click was not on a link or a form widget */
            paste_url();
            ret = 1;
         }
      }
   }
#endif
   if (!ret) {
      ret = Fl_Group::handle(event);
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
   if (!str) str = "";
   Location->value(str);
   Location->position(strlen(str));
}

/*
 * Focus location entry.
 * If it's not visible, show it until the callback is done.
 */
void UI::focus_location()
{
   if (get_panelmode() == UI_HIDDEN) {
      set_panelmode(UI_TEMPORARILY_SHOW_PANELS);
   }
   Location->take_focus();
   // Make text selected when already focused.
   Location->position(Location->size(), 0);
}

/*
 * Focus Main area.
 */
void UI::focus_main()
{
   Main->take_focus();
}

/*
 * Set a new message in the status bar.
 */
void UI::set_status(const char *str)
{
   StatusOutput->value(str);
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
   int new_w = 20;

   if (n_bug == 0) {
      BugMeter->image(icons->ImgMeterOK);
      BugMeter->label("");
   } else if (n_bug >= 1) {
      if (n_bug == 1)
         BugMeter->image(icons->ImgMeterBug);
      snprintf(str, 32, "%d", n_bug);
      BugMeter->copy_label(str);
      new_w = strlen(str)*9 + 20;
   }
   BugMeter->size(new_w, BugMeter->h());
   StatusBar->rearrange();
}

/*
 * Customize the UI's panel (show/hide buttons)
 */
void UI::customize(int flags)
{
   // flags argument not currently used

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
   if ( !prefs.show_tools )
      Tools->hide();
   if ( !prefs.show_clear_url )
      Clear->hide();
   if ( !prefs.show_url )
      Location->hide();
   if ( !prefs.show_search )
      Search->hide();
   if ( !prefs.show_help )
      Help->hide();
   if ( !prefs.show_progress_box ) {
      IProg->hide();
      PProg->hide();
   }

   if (NavBar)
      NavBar->rearrange();
   if (LocBar)
      LocBar->rearrange();
}

/*
 * On-the-fly panel style change
 */
void UI::change_panel(int new_size, int small_icons)
{
   // Remove current panel's bars
   init_sizes();
   TopGroup->remove(MenuBar);
   Fl::delete_widget(MenuBar);
   TopGroup->remove(LocBar);
   Fl::delete_widget(LocBar);
   TopGroup->remove(NavBar);
   Fl::delete_widget(NavBar);
   MenuBar = LocBar = NavBar = NULL;

   // Set internal vars for panel size
   PanelSize = new_size;
   Small_Icons = small_icons;

   // make a new panel
   make_panel(TopGroup->w());
   customize(0);
   a_UIcmd_set_buttons_sens(a_UIcmd_get_bw_by_widget(this));

   // adjust Main's height
   int main_h = h() - (mh+(LocBar?lh:0)+nh+(FindBarSpace?fh:0)+sh);
   Main->size(Main->w(), main_h);
   redraw();

   Location->take_focus();
}

/*
 * On-the-fly color style change
 */
void UI::color_change_cb_i()
{
   const int cols[] = {7,17,26,51,140,156,205,206,215,-1};
   static int ncolor = 0;

   ncolor = (cols[ncolor+1] < 0) ? 0 : ncolor + 1;
   CuteColor = cols[ncolor];
   MSG("Location color %d\n", CuteColor);
   Location->color(CuteColor);
   Location->redraw();
}

/*
 * Set or remove the Panelmode flag and update the UI accordingly
 */
void UI::set_panelmode(UIPanelmode mode)
{
   if (mode == UI_HIDDEN) {
      //Panel->hide();
      StatusBar->hide();
   } else {
      /* UI_NORMAL or UI_TEMPORARILY_SHOW_PANELS */
      //Panel->show();
      StatusBar->show();
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
 * Set 'nw' as the main render area widget
 */
void UI::set_render_layout(Fl_Group *nw)
{
   // Resize layout widget to current height
   nw->resize(0,0,0,Main->h());

   TopGroup->insert(*nw, Main);
   remove(Main);
   delete(Main);
   Main = nw;
   TopGroup->resizable(Main);
}

/*
 * Set button sensitivity (Back/Forw/Stop)
 */
void UI::button_set_sens(UIButton btn, int sens)
{
   switch (btn) {
   case UI_BACK:
      (sens) ? Back->activate() : Back->deactivate();
//    Back->redraw(DAMAGE_HIGHLIGHT);
      break;
   case UI_FORW:
      (sens) ? Forw->activate() : Forw->deactivate();
//    Forw->redraw(DAMAGE_HIGHLIGHT);
      break;
   case UI_STOP:
      (sens) ? Stop->activate() : Stop->deactivate();
//    Stop->redraw(DAMAGE_HIGHLIGHT);
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
   Fl::paste(*Clear, false);
}

/*
 * Adjust space for the findbar (if necessary) and show or remove it
 */
void UI::findbar_toggle(bool add)
{
   /* WORKAROUND:
    * This is tricky: As fltk-1.3 resizes hidden widgets (which it
    * doesn't resize when visible!). We need to go through hoops to
    * get the desired behaviour.
    * Most probably this is a bug in FLTK and we have to report it.
    */

   if (add) {
      if (!FindBarSpace) {
         // show
         Main->size(Main->w(), Main->h()-FindBar->h());
         insert(*FindBar, StatusBar);
         FindBar->show();
         FindBarSpace = 1;
         redraw();
      } else {
         // select text
         FindBar->show();
      }
   } else if (!add && FindBarSpace) {
      // hide
      Main->size(Main->w(), Main->h()+FindBar->h());
      remove(FindBar);
      FindBarSpace = 0;
      redraw();  /* Main->redraw(); is not enough */
   }
}

/*
 * Make panels disappear growing the render area.
 * WORKAROUND: here we avoid hidden widgets resize by setting their
 *             size to (0,0) while hidden.
 *             (Already reported to FLTK team)
 */
void UI::fullscreen_toggle()
{
   int dh = 0;
   int hide = StatusBar->visible();

   // hide/show panels
   init_sizes();
   if (MenuBar) {
      dh += mh;
      hide ? MenuBar->size(0,0) : MenuBar->size(w(),mh);
      hide ? MenuBar->hide() : MenuBar->show();
   }
   if (LocBar) {
      dh += lh;
      hide ? LocBar->size(0,0) : LocBar->size(w(),lh);
      hide ? LocBar->hide() : LocBar->show();
   }
   if (NavBar) {
      dh += nh;
      hide ? NavBar->size(0,0) : NavBar->size(w(),nh);
      hide ? NavBar->hide() : NavBar->show();
   }
   if (StatusBar) {
      dh += sh;
      hide ? StatusBar->size(0,0) : StatusBar->size(w(),sh);;
      hide ? StatusBar->hide() : StatusBar->show();;
   }

   Main->size(Main->w(), Main->h() + (hide ? dh : -dh));
   redraw();
}
