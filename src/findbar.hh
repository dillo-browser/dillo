#ifndef __FINDBAR_HH__
#define __FINDBAR_HH__

//#include <fltk/Window.h>
#include <fltk/Widget.h>
#include <fltk/HighlightButton.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/Group.h>
#include <fltk/CheckButton.h>
#include <fltk/ReturnButton.h>

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

public:
   /* the callback functions need those */
   UI *ui;
   Input *i;
   CheckButton *cb;

   Findbar(int width, int height, UI *ui);
   int handle(int event);
   void show();
   void hide();
};

#endif // __FINDBAR_HH__
