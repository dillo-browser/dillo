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
#include "tipwin.hh"

#include <FL/Fl.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Box.H>
#include <FL/names.h>

// Include image data
#include "pixmaps.h"
#include "uicmd.hh"
#include "history.h"
#include "nav.h"

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
   NULL,
   new Fl_Pixmap(right_xpm),
   NULL,
   new Fl_Pixmap(stop_xpm),
   NULL,
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
   NULL,
   new Fl_Pixmap(right_s_xpm),
   NULL,
   new Fl_Pixmap(stop_s_xpm),
   NULL,
};


static struct iconset *icons = &standard_icons;

/*
 * Local sub classes
 */

//----------------------------------------------------------------------------

/*
 * (Used to avoid certain shortcuts in the location bar)
 */
class CustInput : public TipWinInput {
public:
   CustInput (int x, int y, int w, int h, const char* l=0) :
      TipWinInput(x,y,w,h,l) {};
   virtual int handle(int e);
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
      if (k == FL_Page_Down || k == FL_Page_Up) {
         // These do nothing of interest when FL_MULTILINE_INPUT isn't set.
         // Let them through for key commands.
         return 0;
      }
   }

   return TipWinInput::handle(e);
}

//----------------------------------------------------------------------------

/*
 * Used to handle "paste" within the toolbar's Clear button.
 */
class CustPasteButton : public CustButton {
public:
   CustPasteButton(int x, int y, int w, int h, const char *l=0) :
      CustButton(x,y,w,h,l) {};
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
   return CustButton::handle(e);
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
      a_UIcmd_open_urlstr(bw, "http://www.dillo.org/dillo3-help.html");
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
 * Send the browser to the new URL in the location.
 */
static void location_cb(Fl_Widget *wid, void *data)
{
   Fl_Input *i = (Fl_Input*)wid;
   UI *ui = (UI*)data;

   _MSG("location_cb()\n");
   a_UIcmd_open_urlstr(a_UIcmd_get_bw_by_widget(i), i->value());

   if (ui->temporaryPanels())
      ui->panels_toggle();
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
         a_UIcmd_back_popup(a_UIcmd_get_bw_by_widget(wid), wid->x(),
                            wid->y() + wid->h());
      }
      break;
   case UI_FORW:
      if (b == FL_LEFT_MOUSE) {
         a_UIcmd_forw(a_UIcmd_get_bw_by_widget(wid));
      } else if (b == FL_RIGHT_MOUSE) {
         a_UIcmd_forw_popup(a_UIcmd_get_bw_by_widget(wid), wid->x(),
                            wid->y() + wid->h());
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
         a_UIcmd_tools(a_UIcmd_get_bw_by_widget(wid), wid->x(),
                       wid->y() + wid->h());
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
CustButton *UI::make_button(const char *label, Fl_Image *img, Fl_Image *deimg,
                            int b_n, int start)
{
   if (start)
      p_xpos = 0;

   CustButton *b = new CustButton(p_xpos, 0, bw, bh, (lbl) ? label : NULL);
   if (img)
      b->image(img);
   if (deimg)
      b->deimage(deimg);
   b->callback(b1_cb, INT2VOIDP(b_n));
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
   if (!icons->ImgLeftIn) {
      icons->ImgLeftIn = icons->ImgLeft->copy();
      icons->ImgLeftIn->desaturate();
      icons->ImgLeftIn->color_average(FL_BACKGROUND_COLOR, .14f);
   }
   if (!icons->ImgRightIn) {
      icons->ImgRightIn = icons->ImgRight->copy();
      icons->ImgRightIn->desaturate();
      icons->ImgRightIn->color_average(FL_BACKGROUND_COLOR, .14f);
   }
   if (!icons->ImgStopIn) {
      icons->ImgStopIn = icons->ImgStop->copy();
      icons->ImgStopIn->desaturate();
      icons->ImgStopIn->color_average(FL_BACKGROUND_COLOR, .14f);
   }
   Back = make_button("Back", icons->ImgLeft, icons->ImgLeftIn, UI_BACK, 1);
   Forw = make_button("Forw", icons->ImgRight, icons->ImgRightIn, UI_FORW);
   Home = make_button("Home", icons->ImgHome, NULL, UI_HOME);
   Reload = make_button("Reload", icons->ImgReload, NULL, UI_RELOAD);
   Save = make_button("Save", icons->ImgSave, NULL, UI_SAVE);
   Stop = make_button("Stop", icons->ImgStop, icons->ImgStopIn, UI_STOP);
   Bookmarks = make_button("Book", icons->ImgBook, NULL, UI_BOOK);
   Tools = make_button("Tools", icons->ImgTools, NULL, UI_TOOLS);

   Back->set_tooltip("Previous page");
   Forw->set_tooltip("Next page");
   Home->set_tooltip("Go to the Home page");
   Reload->set_tooltip("Reload");
   Save->set_tooltip("Save this page");
   Stop->set_tooltip("Stop loading");
   Bookmarks->set_tooltip("View bookmarks");
   Tools->set_tooltip("Settings");
}

/*
 * Create the location box (Clear/Input/Search)
 */
void UI::make_location(int ww)
{
   CustButton *b;

    b = Clear = (CustButton*) new CustPasteButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgClear);
    b->callback(clear_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    b->set_tooltip("Clear the URL box.\nMiddle-click to paste a URL.");
    p_xpos += b->w();

    LocationGroup = new Fl_Group(p_xpos,0,ww-p_xpos-32,lh,0);
    LocationGroup->begin();
     CustInput *i = new CustInput(p_xpos,0,ww-p_xpos-32,lh,0);
     Location = i;
     i->when(FL_WHEN_ENTER_KEY);
     i->callback(location_cb, this);
     i->set_tooltip("Location");
     p_xpos += i->w();
    LocationGroup->box(FL_THIN_UP_BOX);   // or FL_FLAT_BOX
    LocationGroup->end();

    Search = b = new CustButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgSearch);
    b->callback(search_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    b->set_tooltip("Search the Web");
    p_xpos += b->w();

    Help = b = new CustButton(p_xpos,0,16,lh,0);
    b->image(icons->ImgHelp);
    b->callback(help_cb, this);
    b->clear_visible_focus();
    b->box(FL_THIN_UP_BOX);
    b->set_tooltip("Help");
    p_xpos += b->w();

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
    IProg->update_label(wide ? "Images\n0 of 0" : "0 of 0");
    p_xpos += pw;
    // Page
    PProg = new CustProgressBox(p_xpos,p_ypos,pw,bh);
    PProg->labelsize(12);
    PProg->box(thin_up ? FL_THIN_UP_BOX : FL_EMBOSSED_BOX);
    PProg->update_label(wide ? "Page\n0.0 KB" : "0.0 KB");
}

/*
 * Create the "File" menu
 * Static function for File menu callbacks.
 */
Fl_Widget *UI::make_filemenu_button()
{
   CustButton *btn;
   int w = 0, h = 0, padding;

   FileButton = btn = new CustButton(p_xpos,0,bw,bh,"W");
   btn->labeltype(FL_FREE_LABELTYPE);
   btn->measure_label(w, h);
   padding = w;
   btn->copy_label(PanelSize == P_tiny ? "&F" : "&File");
   btn->measure_label(w,h);
   h = (PanelSize == P_tiny) ? bh : lh;
   btn->size(w+padding, h);
   p_xpos += btn->w();
   _MSG("UI::make_filemenu_button w=%d h=%d padding=%d\n", w, h, padding);
   btn->box(FL_THIN_UP_BOX);
   btn->callback(filemenu_cb, this);
   btn->set_tooltip("File menu");
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
   }
   nh = bh, fh = 28; sh = 20;

   current(0);
   if (PanelSize == P_tiny) {
      NavBar = new CustGroupHorizontal(0,0,ww,nh);
      NavBar->box(FL_NO_BOX);
      NavBar->begin();
       make_toolbar(ww,bh);
       make_filemenu_button();
       make_location(ww);
       NavBar->resizable(LocationGroup);
       make_progress_bars(0,1);
      NavBar->box(FL_THIN_UP_FRAME);
      NavBar->end();
      NavBar->rearrange();
      TopGroup->insert(*NavBar,0);
   } else {
       // Location
       LocBar = new CustGroupHorizontal(0,0,ww,lh);
       LocBar->box(FL_NO_BOX);
       LocBar->begin();
        p_xpos = 0;
        make_filemenu_button();
        make_location(ww);
        LocBar->resizable(LocationGroup);
       LocBar->end();
       LocBar->rearrange();
       TopGroup->insert(*LocBar,0);

       // Toolbar
       p_ypos = 0;
       NavBar = new CustGroupHorizontal(0,0,ww,bh);
       NavBar->box(FL_NO_BOX);
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
       NavBar->rearrange();
       TopGroup->insert(*NavBar,1);
   }
}

/*
 * Create the status panel
 */
void UI::make_status_bar(int ww, int wh)
{
   const int bm_w = 20;
   StatusBar = new CustGroupHorizontal(0, wh-sh, ww, sh);
   StatusBar->box(FL_NO_BOX);

    // Status box
    StatusOutput = new Fl_Output(0, wh-sh, ww-bm_w, sh);
    StatusOutput->value("http://www.dillo.org");
    StatusOutput->labelsize(8);
    StatusOutput->box(FL_THIN_DOWN_BOX);
    StatusOutput->clear_visible_focus();
    StatusOutput->color(FL_BACKGROUND_COLOR);

    // Bug Meter
    BugMeter = new CustButton(ww-bm_w,wh-sh,bm_w,sh);
    BugMeter->image(icons->ImgMeterOK);
    BugMeter->box(FL_THIN_DOWN_BOX);
    BugMeter->align(FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    BugMeter->set_tooltip("Show HTML bugs\n(right-click for menu)");
    BugMeter->callback(bugmeter_cb, this);
    BugMeter->clear_visible_focus();

   StatusBar->end();
   StatusBar->resizable(StatusOutput);
   StatusBar->rearrange();
}

/*
 * User Interface constructor
 */
UI::UI(int x, int y, int ui_w, int ui_h, const char* label, const UI *cur_ui) :
  CustGroupVertical(x, y, ui_w, ui_h, label)
{
   LocBar = NavBar = StatusBar = NULL;

   Tabs = NULL;
   TopGroup = this;
   TopGroup->box(FL_NO_BOX);
   clear_flag(SHORTCUT_LABEL);

   PanelTemporary = false;
   if (cur_ui) {
      PanelSize = cur_ui->PanelSize;
      Small_Icons = cur_ui->Small_Icons;
      Panelmode = cur_ui->Panelmode;
   } else {
     // Set some default values
     PanelSize = prefs.panel_size;
     Small_Icons = prefs.small_icons;
     Panelmode = (prefs.fullwindow_start) ? UI_HIDDEN : UI_NORMAL;
   }

   // Control panel
   TopGroup->begin();
    make_panel(ui_w);

    // Render area
    Main = new Fl_Group(0,0,0,0,"Welcome..."); // size is set by rearrange()
    Main->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    Main->box(FL_FLAT_BOX);
    Main->labelfont(FL_HELVETICA_BOLD_ITALIC);
    Main->labelsize(36);
    Main->labeltype(FL_SHADOW_LABEL);
    TopGroup->add(Main);
    TopGroup->resizable(Main);
    MainIdx = TopGroup->find(Main);

    // Find text bar
    FindBar = new Findbar(ui_w, fh);
    TopGroup->add(FindBar);

    // Status Panel
    make_status_bar(ui_w, ui_h);
    TopGroup->add(StatusBar);
   TopGroup->end();
   TopGroup->rearrange();

   customize();

   if (Panelmode == UI_HIDDEN) {
      panels_toggle();
   }
}

/*
 * UI destructor
 */
UI::~UI()
{
   _MSG("UI::~UI()\n");
}

/*
 * FLTK event handler for this window.
 */
int UI::handle(int event)
{
   _MSG("UI::handle event=%s\n", fl_eventnames[event]);

   int ret = 0;
   if (event == FL_KEYBOARD) {
      return 0; // Receive as shortcut
   } else if (event == FL_SHORTCUT) {
      KeysCommand_t cmd = Keys::getKeyCmd();
      if (cmd == KEYS_NOP) {
         // Do nothing
      } else if (cmd == KEYS_SCREEN_UP || cmd == KEYS_SCREEN_DOWN ||
                 cmd == KEYS_SCREEN_LEFT || cmd == KEYS_SCREEN_RIGHT ||
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
         /* Hide findbar if present, hide panels if not */
         (FindBar->visible()) ? findbar_toggle(0) : panels_toggle();
         temporaryPanels(false);
         ret = 1;
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
      } else if (cmd == KEYS_FILE_MENU) {
         a_UIcmd_file_popup(a_UIcmd_get_bw_by_widget(this), FileButton);
         ret = 1;
      } else if (cmd == KEYS_VIEW_SOURCE) {
         BrowserWindow *bw = a_UIcmd_get_bw_by_widget(this);
         const DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));
         a_UIcmd_view_page_source(bw, url);
         ret = 1;
      }
   } else if (event == FL_RELEASE) {
      if (Fl::event_button() == FL_MIDDLE_MOUSE &&
          prefs.middle_click_drags_page == 0) {
         /* nobody claimed the event; try paste */
         paste_url();
         ret = 1;
      }
   }

   if (!ret) {
      ret = Fl_Group::handle(event);
   }
   if (!ret && event == FL_PUSH && !prefs.middle_click_drags_page) {
      /* nobody claimed FL_PUSH: ask for FL_RELEASE,
       * which is necessary for middle-click paste URL) */
      ret = 1;
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
   Location->position((Fl::focus() == Location) ? strlen(str) : 0);
}

/*
 * Focus location entry.
 * If it's not visible, show it until the callback is done.
 */
void UI::focus_location()
{
   if (Panelmode == UI_HIDDEN) {
      panels_toggle();
      temporaryPanels(true);
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
         char prefix;
         float magnitude;

         if (nbytes >= 1024 * 1024) {
            prefix = 'M';
            magnitude = nbytes / (1024 * 1024.0);
         } else {
            prefix = 'K';
            magnitude = nbytes / 1024.0;
         }
         snprintf(str, 32, "%s%.1f %cB",
                  (PanelSize == 0) ? "" : "Page\n", magnitude, prefix);
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
void UI::customize()
{
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
   char *loc_text = dStrdup(Location->value());

   // Remove current panel's bars
   init_sizes();
   TopGroup->remove(LocBar);
   Fl::delete_widget(LocBar);
   TopGroup->remove(NavBar);
   Fl::delete_widget(NavBar);
   LocBar = NavBar = NULL;

   // Set internal vars for panel size
   PanelSize = new_size;
   Small_Icons = small_icons;

   // make a new panel
   make_panel(TopGroup->w());
   customize();
   a_UIcmd_set_buttons_sens(a_UIcmd_get_bw_by_widget(this));

   TopGroup->rearrange();
   Location->value(loc_text);
   Location->take_focus();

   dFree(loc_text);
}

/*
 * Set 'nw' as the main render area widget
 */
void UI::set_render_layout(Fl_Group *nw)
{
   // Resize layout widget to current height
   nw->resize(0,Main->y(),Main->w(),Main->h());

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
      break;
   case UI_FORW:
      (sens) ? Forw->activate() : Forw->deactivate();
      break;
   case UI_STOP:
      (sens) ? Stop->activate() : Stop->deactivate();
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
    * doesn't resize when visible!). We need to set the size to (0,0) to
    * get the desired behaviour.
    * (STR#2639 in FLTK bug tracker).
    */

   if (add) {
      if (!FindBar->visible())
         FindBar->size(w(), fh);
      FindBar->show();
   } else {
      // hide
      FindBar->size(0,0);
      FindBar->hide();
      // reset state
      a_UIcmd_findtext_reset(a_UIcmd_get_bw_by_widget(this));
      // focus main area
      focus_main();
   }
   TopGroup->rearrange();
}

/*
 * Make panels disappear growing the render area.
 * WORKAROUND: here we avoid hidden widgets resize by setting their
 *             size to (0,0) while hidden.
 *             (Already reported to FLTK team)
 */
void UI::panels_toggle()
{
   int hide = StatusBar->visible();

   // hide/show panels
   init_sizes();
   if (LocBar) {
      hide ? LocBar->size(0,0) : LocBar->size(w(),lh);
      hide ? LocBar->hide() : LocBar->show();
   }
   if (NavBar) {
      hide ? NavBar->size(0,0) : NavBar->size(w(),nh);
      hide ? NavBar->hide() : NavBar->show();
   }
   if (StatusBar) {
      hide ? StatusBar->size(0,0) : StatusBar->size(w(),sh);
      hide ? StatusBar->hide() : StatusBar->show();
      StatusBar->rearrange();
   }

   TopGroup->rearrange();
   Panelmode = (hide) ? UI_HIDDEN : UI_NORMAL;
}
