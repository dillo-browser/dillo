#ifndef __UI_HH__
#define __UI_HH__

// UI for dillo --------------------------------------------------------------

#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Tabs.H>

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
   UI_HIDDEN = 1,
   UI_TEMPORARILY_SHOW_PANELS
} UIPanelmode;

// Private classes
class CustProgressBox;
class CustTabs;


// Class definition ----------------------------------------------------------
/*
 * Used to reposition group's widgets when some of them are hidden
 */
class CustGroup : public Fl_Group {
public:
  CustGroup(int x,int y,int w ,int h,const char *l = 0) :
    Fl_Group(x,y,w,h,l) { };
  void rearrange(void) {
     int n = children(), xpos = 0, r_x1, r_i = -1, i;

     init_sizes();
     for (i = 0; i < n; ++i) {
        if (child(i) == resizable()) {
           r_i = i;
           r_x1 = xpos;
           break;
        }
        if (child(i)->visible()) {
           child(i)->position(xpos, child(i)->y());
           xpos += child(i)->w();
        }
     }
     if (r_i < 0)
        return;
     xpos = w();
     for (i = n - 1; i > r_i; --i) {
        if (child(i)->visible()) {
           xpos -= child(i)->w();
           child(i)->position(xpos, child(i)->y());
        }
     }
     child(r_i)->resize(r_x1, child(r_i)->y(), xpos-r_x1, child(r_i)->h());
     redraw();
  }
  void rearrange_y(void) {
     int n = children(), pos = 0, r_pos, r_i = -1, i;

     printf("children = %d\n", n);
     init_sizes();
     for (i = 0; i < n; ++i) {
        if (child(i) == resizable()) {
           r_i = i;
           r_pos = pos;
           break;
        }
        if (child(i)->visible()) {
           printf("child[%d] x=%d y=%d w=%d h=%d\n",
                  i, child(i)->x(), pos, child(i)->w(), child(i)->h());
           child(i)->position(child(i)->x(), pos);
           pos += child(i)->h();
        }
     }
     if (r_i < 0)
        return;
     pos = h();
     for (i = n - 1; i > r_i; --i) {
        if (child(i)->visible()) {
           pos -= child(i)->h();
           printf("child[%d] x=%d y=%d w=%d h=%d\n",
                  i, child(i)->x(), pos, child(i)->w(), child(i)->h());
           child(i)->position(child(i)->x(), pos);
        }
     }
     child(r_i)->resize(child(r_i)->x(), r_pos, child(r_i)->w(), pos-r_pos);
     printf("resizable child[%d] x=%d y=%d w=%d h=%d\n",
            r_i, child(r_i)->x(), r_pos, child(r_i)->w(), child(r_i)->h());
     child(r_i)->hide();
     redraw();
  }
};


//
// UI class definition -------------------------------------------------------
//
class UI : public Fl_Pack {
   CustTabs *Tabs;
   char *TabTooltip;

   Fl_Group *TopGroup;
   Fl_Button *Back, *Forw, *Home, *Reload, *Save, *Stop, *Bookmarks, *Tools,
          *Clear, *Search, *Help, *FullScreen, *BugMeter, *FileButton;
   CustGroup *MenuBar, *LocBar, *NavBar, *StatusBar;
   Fl_Input  *Location;
   Fl_Pack *ProgBox;
   CustProgressBox *PProg, *IProg;
   Fl_Group *Panel, *Main;
   Fl_Output *StatusOutput;
   Findbar *FindBar;

   int FindBarSpace, MainIdx;
   // Panel customization variables
   int PanelSize, CuteColor, Small_Icons;
   int p_xpos, p_ypos, bw, bh, mh, lh, nh, fh, sh, pw, lbl;

   UIPanelmode Panelmode;
   int PointerOnLink;
   Fl_Button *make_button(const char *label, Fl_Image *img,
                          Fl_Image*deimg, int b_n, int start = 0);
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
   void customize(int flags);
   void button_set_sens(UIButton btn, int sens);
   void paste_url();
   void set_panelmode(UIPanelmode mode);
   UIPanelmode get_panelmode();
   int get_panelsize() { return PanelSize; }
   int get_smallicons() { return Small_Icons; }
   void change_panel(int new_size, int small_icons);
   void findbar_toggle(bool add);
   void fullscreen_toggle();

   CustTabs *tabs() { return Tabs; }
   void tabs(CustTabs *tabs) { Tabs = tabs; }
   int pointerOnLink() { return PointerOnLink; }
   void pointerOnLink(int flag) { PointerOnLink = flag; }

   // Hooks to method callbacks
   void color_change_cb_i();
   void toggle_cb_i();
   void panelmode_cb_i();
};

#endif // __UI_HH__
