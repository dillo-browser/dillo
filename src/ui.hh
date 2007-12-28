#ifndef __UI_HH__
#define __UI_HH__

// UI for dillo --------------------------------------------------------------

#include <fltk/Window.h>
#include <fltk/Widget.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/PackedGroup.h>
#include <fltk/Output.h>
#include <fltk/xpmImage.h>

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

// Private class 
class NewProgressBox;

//
// UI class definition -------------------------------------------------------
//
class UI : public fltk::Window {
   Group *TopGroup;
   Button *Back, *Forw, *Home, *Reload, *Save, *Stop, *Bookmarks,
          *Clear, *Search, *FullScreen, *ImageLoad, *BugMeter;
   Input  *Location;
   PackedGroup *ProgBox;
   NewProgressBox *PProg, *IProg;
   Image *ImgLeftIns, *ImgLeftSens, *ImgRightIns, *ImgRightSens,
         *ImgStopIns, *ImgStopSens, *ImgFullScreenOn, *ImgFullScreenOff,
         *ImgImageLoadOn, *ImgImageLoadOff, *ImgMeterOK, *ImgMeterBug;
   Group *Panel, *StatusPanel;
   Widget *Main;
   Output *Status;

   int MainIdx;
   // Panel customization variables
   int PanelSize, CuteColor, Small_Icons;
   int xpos, bw, bh, fh, lh, lbl;

   // TODO: Hack for fullscreen mode
   int Panel_h, Status_h;

   PackedGroup *make_toolbar(int tw, int th);
   PackedGroup *make_location();
   PackedGroup *make_progress_bars(int wide, int thin_up);
   Group *make_menu(int tiny);
   Group *make_panel(int ww);


public:

   UI(int w, int h, const char* label = 0, const UI *cur_ui = NULL);
   ~UI() {} // TODO: implement destructor

   // To manage what events to catch and which to let pass
   int handle(int event);

   const char *get_location();
   void set_location(const char *str);
   void focus_location();
   void set_status(const char *str);
   void set_page_prog(size_t nbytes, int cmd);
   void set_img_prog(int n_img, int t_img, int cmd);
   void set_bug_prog(int n_bug);
   void set_render_layout(Widget &nw);
   void set_page_title(const char *label);
   void customize(int flags);
   void button_set_sens(UIButton btn, int sens);
   void paste_url();

   // Workaround functions for a non-working replace() in FLTK2
   void set_render_layout_begin() {
      TopGroup->remove(MainIdx);
      TopGroup->remove(TopGroup->find(StatusPanel));
      delete(Main);
      Main = NULL;
      TopGroup->begin();
   }
   void set_render_layout_end() {
      TopGroup->resizable(TopGroup->child(MainIdx));
      Main = TopGroup->child(MainIdx);
      TopGroup->add(*StatusPanel);
      TopGroup->end();
   }
   int panel_h() { return Panel->h(); };
   int status_h() { return Status->h(); };
   Widget *fullscreen_button() { return FullScreen; }
   void fullscreen_toggle() { FullScreen->do_callback(); }

   // Hooks to method callbacks
   void panel_cb_i();
   void color_change_cb_i();
   void toggle_cb_i();
   void fullscreen_cb_i();
   void imageload_toggle();
};

#endif // __UI_HH__
