#ifndef __UI_HH__
#define __UI_HH__

// UI for dillo --------------------------------------------------------------

#include <fltk/Window.h>
#include <fltk/Widget.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/PackedGroup.h>
#include <fltk/Output.h>
#include <fltk/Image.h>
#include <fltk/MultiImage.h>
#include <fltk/MenuBuild.h>
#include <fltk/TabGroup.h>

#include "findbar.hh"

using namespace fltk;

typedef enum {
   UI_BACK = 0,
   UI_FORW,
   UI_HOME,
   UI_RELOAD,
   UI_SAVE,
   UI_STOP,
   UI_BOOK,
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
class UI : public fltk::Group {
   CustTabGroup *Tabs;
   char *TabTooltip;

   Group *TopGroup;
   Button *Back, *Forw, *Home, *Reload, *Save, *Stop, *Bookmarks,
          *Clear, *Search, *FullScreen, *ImageLoad, *BugMeter, *FileButton;
   Input  *Location;
   PackedGroup *ProgBox;
   CustProgressBox *PProg, *IProg;
   Group *Panel, *StatusPanel;
   Widget *Main;
   Output *Status;

   int MainIdx;
   // Panel customization variables
   int PanelSize, CuteColor, Small_Icons;
   int xpos, bw, bh, fh, lh, lbl;

   UIPanelmode Panelmode;
   Findbar *findbar;
   int PointerOnLink;

   PackedGroup *make_toolbar(int tw, int th);
   PackedGroup *make_location();
   PackedGroup *make_progress_bars(int wide, int thin_up);
   void make_menubar(int x, int y, int w, int h);
   Widget *make_filemenu_button();
   Group *make_panel(int ww);

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
   void set_render_layout(Widget &nw);
   void set_page_title(const char *label);
   void customize(int flags);
   void button_set_sens(UIButton btn, int sens);
   void paste_url();
   void set_panelmode(UIPanelmode mode);
   UIPanelmode get_panelmode();
   void set_findbar_visibility(bool visible);
   bool images_enabled() { return ImageLoad->state();}
   void images_enabled(int flag) { ImageLoad->state(flag);}
   Widget *fullscreen_button() { return FullScreen; }
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
