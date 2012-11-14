#ifndef __TIPWIN_HH__
#define __TIPWIN_HH__

#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>


/*
 * Custom tooltip window
 */
class TipWin : public Fl_Menu_Window {
   char tip[256];
   int bgcolor, recent;
   void *cur_widget;
public:
   TipWin();
   void draw();
   void value(const char *s);
   void do_show(void *wid);
   void do_hide();
   void recent_tooltip(int val);

   void cancel(void *wid) {
      if (wid == cur_widget) { cur_widget = NULL; do_hide(); }
   }
};

extern TipWin *my_tipwin(void);


/*
 * A Button sharing a custom tooltip window
 */
class TipWinButton : public Fl_Button {
   char *mytooltip;
   TipWin *tipwin;
 public:
   TipWinButton(int x, int y, int w, int h, const char *l = 0);
   ~TipWinButton();
   virtual int handle(int e);

   void set_tooltip(const char *s);
};

/*
 * A button that highlights on mouse over
 */
class CustButton : public TipWinButton {
   Fl_Color norm_color, light_color;
public:
   CustButton(int x, int y, int w, int h, const char *l=0);
   virtual int handle(int e);
   void hl_color(Fl_Color col);
};


/*
 * An Input with custom tooltip window
 */
class TipWinInput : public Fl_Input {
   char *mytooltip;
   TipWin *tipwin;
public:
   TipWinInput (int x, int y, int w, int h, const char* l=0);
   ~TipWinInput(void);
   virtual int handle(int e);

   void set_tooltip(const char *s);
};


#endif // __TIPWIN_HH__

