#ifndef __FINDBAR_HH__
#define __FINDBAR_HH__

#include <fltk/xpmImage.h>
#include <fltk/Widget.h>
#include <fltk/HighlightButton.h>
#include <fltk/Button.h>
#include <fltk/Input.h>
#include <fltk/Group.h>
#include <fltk/CheckButton.h>

/*
 * Searchbar to find text in page.
 */
class Findbar : public fltk::Group {
   fltk::Button *clrb;
   fltk::HighlightButton *hide_btn, *next_btn, *prev_btn;
   fltk::CheckButton *check_btn;
   fltk::xpmImage *hideImg;
   fltk::Input *i;

   static void search_cb (fltk::Widget *, void *);
   static void searchBackwards_cb (fltk::Widget *, void *);
   static void search_cb2 (fltk::Widget *, void *);
   static void hide_cb (fltk::Widget *, void *);

public:
   Findbar(int width, int height);
   ~Findbar();
   int handle(int event);
   void show();
   void hide();
};

#endif // __FINDBAR_HH__
