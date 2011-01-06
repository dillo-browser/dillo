#ifndef __FINDBAR_HH__
#define __FINDBAR_HH__

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Check_Button.H>

/*
 * Searchbar to find text in page.
 */
class Findbar : public Fl_Group {
   Fl_Button *clrb;
   Fl_Button *hide_btn, *next_btn, *prev_btn;
   Fl_Check_Button *check_btn;
   Fl_Pixmap *hideImg;
   Fl_Input *i;

   static void search_cb (Fl_Widget *, void *);
   static void searchBackwards_cb (Fl_Widget *, void *);
   static void search_cb2 (Fl_Widget *, void *);
   static void hide_cb (Fl_Widget *, void *);

public:
   Findbar(int width, int height);
   ~Findbar();
   int handle(int event);
   void show();
   void hide();
};

#endif // __FINDBAR_HH__
