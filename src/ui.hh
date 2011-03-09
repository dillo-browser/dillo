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
class CustTabGroup;

//
// UI class definition -------------------------------------------------------
//
class UI : public Fl_Pack {
   CustTabGroup *Tabs;
   char *TabTooltip;

   Fl_Group *TopGroup;
   Fl_Button *Back, *Forw, *Home, *Reload, *Save, *Stop, *Bookmarks, *Tools,
          *Clear, *Search, *Help, *FullScreen, *BugMeter, *FileButton;
   Fl_Input  *Location;
   Fl_Pack *ProgBox;
   CustProgressBox *PProg, *IProg;
   Fl_Group *Panel, *Main, *StatusPanel;
   Fl_Output *StatusOutput;

   int MainIdx;
   // Panel customization variables
   int PanelSize, CuteColor, Small_Icons;
   int p_xpos, p_ypos, bw, bh, fh, lh, pw, lbl;

   UIPanelmode Panelmode;
   Findbar *findbar;
   int PointerOnLink;
   Fl_Button *make_button(const char *label, Fl_Image *img,
                          Fl_Image*deimg, int b_n, int start = 0);
   void make_toolbar(int tw, int th);
   void make_location(int ww);
   void make_progress_bars(int wide, int thin_up);
   void make_menubar(int x, int y, int w, int h);
   Fl_Widget *make_filemenu_button();
   void make_panel(int ww);
   void make_status_panel(int ww);

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
   void set_render_layout(Fl_Group &nw);
   void set_tab_title(const char *label);
   void customize(int flags);
   void button_set_sens(UIButton btn, int sens);
   void paste_url();
   void set_panelmode(UIPanelmode mode);
   UIPanelmode get_panelmode();
   void set_findbar_visibility(bool visible);
   Fl_Widget *fullscreen_button() { return FullScreen; }
   void fullscreen_toggle() { FullScreen->do_callback(); }

   CustTabGroup *tabs() { return Tabs; }
   void tabs(CustTabGroup *tabs) { Tabs = tabs; }
   int pointerOnLink() { return PointerOnLink; }
   void pointerOnLink(int flag) { PointerOnLink = flag; }

   // Hooks to method callbacks
   void panel_cb_i();
   void color_change_cb_i();
   void toggle_cb_i();
   void panelmode_cb_i();
};

#endif // __UI_HH__
