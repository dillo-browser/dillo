/*
 * File: findbar.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include "findbar.hh"

#include "msg.h"
#include "pixmaps.h"
#include "uicmd.hh"
#include "bw.h"

/*
 * Local sub class
 * (Used to handle escape in the findbar, may also avoid some shortcuts).
 */
class MyInput : public Fl_Input {
public:
   MyInput (int x, int y, int w, int h, const char* l=0) :
      Fl_Input(x,y,w,h,l) {};
   int handle(int e);
};

int MyInput::handle(int e)
{
   _MSG("findbar MyInput::handle()\n");
   int ret = 1, k = Fl::event_key();
   unsigned modifier = Fl::event_state() & (FL_SHIFT| FL_CTRL| FL_ALT|FL_META);

   if (e == FL_KEYBOARD) {
      if (k == FL_Left || k == FL_Right) {
         if (modifier == FL_SHIFT) {
            a_UIcmd_send_event_to_tabs_by_wid(e, this);
            return 1;
         }
      } else if (k == FL_Escape && modifier == 0) {
         // Avoid clearing the text with Esc, just hide the findbar.
         return 0;
      }
   }

   if (ret)
      ret = Fl_Input::handle(e);
   return ret;
};

/*
 * Find next occurrence of input key
 */
void Findbar::search_cb(Fl_Widget *, void *vfb)
{
   Findbar *fb = (Findbar *)vfb;
   const char *key = fb->i->value();
   bool case_sens = fb->check_btn->value();

   if (key[0] != '\0')
      a_UIcmd_findtext_search(a_UIcmd_get_bw_by_widget(fb),
                              key, case_sens, false);
}

/*
 * Find previous occurrence of input key
 */
void Findbar::searchBackwards_cb(Fl_Widget *, void *vfb)
{
   Findbar *fb = (Findbar *)vfb;
   const char *key = fb->i->value();
   bool case_sens = fb->check_btn->value();

   if (key[0] != '\0') {
      a_UIcmd_findtext_search(a_UIcmd_get_bw_by_widget(fb),
                              key, case_sens, true);
   }
}

/*
 * Find next occurrence of input key
 */
void Findbar::search_cb2(Fl_Widget *widget, void *vfb)
{
   /*
    * Somehow fltk even regards the first loss of focus for the
    * window as a WHEN_ENTER_KEY_ALWAYS event.
    */
   if (Fl::event_key() == FL_Enter)
      search_cb(widget, vfb);
}

/*
 * Hide the search bar
 */
void Findbar::hide_cb(Fl_Widget *, void *vfb)
{
   ((Findbar *)vfb)->hide();
}

/*
 * Construct text search bar
 */
Findbar::Findbar(int width, int height) :
   Fl_Group(0, 0, width, height)
{
   int button_width = 70;
   int gap = 2;
   int border = 2;
   int input_width = width - (2 * border + 4 * (button_width + gap));
   int x = border;

   Fl_Group::current(0);

   height -= 2 * border;

   box(FL_PLASTIC_UP_BOX);
   Fl_Group::hide();

    hide_btn = new Fl_Button(x, border, 16, height, 0);
    hideImg = new Fl_Pixmap(new_s_xpm);
    hide_btn->image(hideImg);
    x += 16 + gap;
    hide_btn->callback(hide_cb, this);
    hide_btn->clear_visible_focus();
   add(hide_btn);

    i = new MyInput(x, border, input_width, height);
    x += input_width + gap;
    resizable(i);
    i->color(206);
    i->when(FL_WHEN_ENTER_KEY_ALWAYS);
    i->callback(search_cb2, this);
   add(i);

    next_btn = new Fl_Button(x, border, button_width, height, "Next");
    x += button_width + gap;
    next_btn->shortcut(FL_Enter);
    next_btn->callback(search_cb, this);
    next_btn->clear_visible_focus();
   add(next_btn);

    prev_btn= new Fl_Button(x, border, button_width, height, "Previous");
    x += button_width + gap;
    prev_btn->shortcut(FL_SHIFT+FL_Enter);
    prev_btn->callback(searchBackwards_cb, this);
    prev_btn->clear_visible_focus();
   add(prev_btn);

    check_btn = new Fl_Check_Button(x, border, 2*button_width, height,
                              "Case-sensitive");
    x += 2 * button_width + gap;
    check_btn->clear_visible_focus();
   add(check_btn);

   if (prefs.show_tooltip) {
      hide_btn->tooltip("Hide");
      next_btn->tooltip("Find next occurrence of the search phrase\n"
                        "shortcut: Enter");
      prev_btn->tooltip("Find previous occurrence of the search phrase\n"
                        "shortcut: Shift+Enter");
   }
}

Findbar::~Findbar()
{
   delete hideImg;
}

/*
 * Handle events. Used to catch FL_Escape events.
 */
int Findbar::handle(int event)
{
   int ret = 0;
   int k = Fl::event_key();
   unsigned modifier = Fl::event_state() & (FL_SHIFT| FL_CTRL| FL_ALT|FL_META);

   if (event == FL_KEYBOARD && modifier == 0 && k == FL_Escape) {
      hide();
      ret = 1;
   }

   if (ret == 0)
      ret = Fl_Group::handle(event);

   return ret;
}

/*
 * Show the findbar and focus the input field
 */
void Findbar::show()
{
   Fl_Group::show();
   /* select text even if already focused */
   i->take_focus();
   i->position(i->size(), 0);
}

/*
 * Hide the findbar and reset the search state
 */
void Findbar::hide()
{
   BrowserWindow *bw;

   Fl_Group::hide();
   if ((bw = a_UIcmd_get_bw_by_widget(this)))
      a_UIcmd_findtext_reset(bw);
   a_UIcmd_focus_main_area(bw);
}
