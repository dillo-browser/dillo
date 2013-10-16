#ifndef __UI_HH__
#define __UI_HH__

// UI for dillo --------------------------------------------------------------

#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Tabs.H>

#include "tipwin.hh"
#include "findbar.hh"

typedef enum {
   UI_BACK = 0,
   UI_FORW,
   UI_HOME,
   UI_RELOAD,
   UI_SAVE,
   UI_STOP,
   UI_BOOK,
   UI_TOOLS,
   UI_CLEAR,
   UI_SEARCH
} UIButton;

typedef enum {
   UI_NORMAL = 0,     /* make sure it's compatible with bool */
   UI_HIDDEN = 1
} UIPanelmode;


// Min size to fit the full UI
#define UI_MIN_W 600
#define UI_MIN_H 200

// Private classes
class CustProgressBox;
class CustTabs;


// Class definitions ---------------------------------------------------------
/*
 * Used to reposition group's widgets when some of them are hidden.
 * All children get the height of the group but retain their original width.
 * The resizable child get's the remaining space.
 */
class CustGroupHorizontal : public Fl_Group {
   Fl_Widget *rsz;
public:
  CustGroupHorizontal(int x,int y,int w ,int h,const char *l = 0) :
    Fl_Group(x,y,w,h,l) { };

  void rearrange() {
     Fl_Widget*const* a = array();
     int sum = 0, _x = x();
     int children_ = children();

     if (resizable())
        rsz = resizable();

     for (int i=0; i < children_; i++)
        if (a[i] != resizable() && a[i]->visible())
           sum += a[i]->w();

     for (int i=0; i < children_; i++) {
        if (a[i] == rsz) {
           if (w() > sum) {
              a[i]->resize(_x, y(), w()-sum, h());
              if (!resizable())
                 resizable(rsz);
           } else {
              /* widgets overflow width */
              a[i]->resize(_x, y(), 0, h());
              resizable(NULL);
           }
        } else {
           a[i]->resize(_x, y(), a[i]->w(), h());
        }
        if (a[i]->visible())
           _x += a[i]->w();
     }
     init_sizes();
     redraw();
  }
};

class CustGroupVertical : public Fl_Group {
public:
  CustGroupVertical(int x,int y,int w ,int h,const char *l = 0) :
    Fl_Group(x,y,w,h,l) { };

  void rearrange() {
     Fl_Widget*const* a = array();
     int sum = 0, _y = y();
     int children_ = children();

     for (int i=0; i < children_; i++)
        if (a[i] != resizable() && a[i]->visible())
           sum += a[i]->h();

     for (int i=0; i < children_; i++) {
        if (a[i] == resizable()) {
           a[i]->resize(x(), _y, w(), h() - sum);
        } else {
           a[i]->resize(x(), _y, w(), a[i]->h());
        }
        if (a[i]->visible())
           _y += a[i]->h();
     }
     init_sizes();
     redraw();
  }
};


//
// UI class definition -------------------------------------------------------
//
class UI : public CustGroupVertical {
   CustTabs *Tabs;

   CustGroupVertical *TopGroup;
   CustButton *Back, *Forw, *Home, *Reload, *Save, *Stop, *Bookmarks,
              *Tools, *Clear, *Search, *Help, *BugMeter, *FileButton;
   CustGroupHorizontal *LocBar, *NavBar, *StatusBar;
   Fl_Input *Location;
   CustProgressBox *PProg, *IProg;
   Fl_Group *Panel, *Main, *LocationGroup;
   Fl_Output *StatusOutput;
   Findbar *FindBar;

   int MainIdx;
   // Panel customization variables
   int PanelSize, Small_Icons;
   int p_xpos, p_ypos, bw, bh, mh, lh, nh, fh, sh, pw, lbl;
   bool PanelTemporary;

   UIPanelmode Panelmode;
   CustButton *make_button(const char *label, Fl_Image *img, Fl_Image*deimg,
                           int b_n, int start = 0);
   void make_toolbar(int tw, int th);
   void make_location(int ww);
   void make_progress_bars(int wide, int thin_up);
   void make_menubar(int x, int y, int w, int h);
   Fl_Widget *make_filemenu_button();
   void make_panel(int ww);
   void make_status_bar(int ww, int wh);

public:

   UI(int x,int y,int w,int h, const char* label = 0, const UI *cur_ui=NULL);
   ~UI();

   // To manage what events to catch and which to let pass
   int handle(int event);

   const char *get_location();
   void set_location(const char *str);
   void focus_location();
   void focus_main();
   void set_status(const char *str);
   void set_page_prog(size_t nbytes, int cmd);
   void set_img_prog(int n_img, int t_img, int cmd);
   void set_bug_prog(int n_bug);
   void set_render_layout(Fl_Group *nw);
   void customize();
   void button_set_sens(UIButton btn, int sens);
   void paste_url();
   int get_panelsize() { return PanelSize; }
   int get_smallicons() { return Small_Icons; }
   void change_panel(int new_size, int small_icons);
   void findbar_toggle(bool add);
   void panels_toggle();

   CustTabs *tabs() { return Tabs; }
   void tabs(CustTabs *tabs) { Tabs = tabs; }
   bool temporaryPanels() { return PanelTemporary; }
   void temporaryPanels(bool val) { PanelTemporary = val; }

   // Hooks to method callbacks
   void color_change_cb_i();
   void toggle_cb_i();
};

#endif // __UI_HH__
