#ifndef __FINDBAR_HH__
#define __FINDBAR_HH__

#include <fltk/xpmImage.h>
#include <fltk/Widget.h>
#include <fltk/HighlightButton.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/Group.h>
#include <fltk/CheckButton.h>

using namespace fltk;

/*
 * Searchbar to find text in page.
 */
class Findbar : public Group {
   HighlightButton *findb;
   Button *clrb;
   HighlightButton *hidebutton;
   xpmImage *hideImg;
   Input *i;
   CheckButton *cb;
   
   static void search_cb (Widget *, void *);
   static void search_cb2 (Widget *, void *);
   static void hide_cb (Widget *, void *);

public:
   Findbar(int width, int height);
   ~Findbar();
   int handle(int event);
   void show();
   void hide();
};

#endif // __FINDBAR_HH__
