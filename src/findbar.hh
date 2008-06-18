#ifndef __FINDBAR_HH__
#define __FINDBAR_HH__

#include <fltk/Widget.h>
#include <fltk/HighlightButton.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/Group.h>
#include <fltk/CheckButton.h>

// simple declaration to avoid circular include
class UI;

using namespace fltk;

/*
 * Searchbar to find text in page.
 */
class Findbar : public Group {
   HighlightButton *findb;
   Button *clrb;
   HighlightButton *hidebutton;
   UI *ui;
   Input *i;
   CheckButton *cb;
   
   static void search_cb (Widget *, void *);
   static void search_cb2 (Widget *, void *);
   static void hide_cb (Widget *, void *);

public:
   Findbar(int width, int height, UI *ui);
   int handle(int event);
   void show();
   void hide();
};

#endif // __FINDBAR_HH__
