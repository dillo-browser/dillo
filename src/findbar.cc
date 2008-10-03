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

#include <fltk/events.h>
#include <fltk/Window.h>
#include "findbar.hh"

#include "msg.h"
#include "pixmaps.h"
#include "uicmd.hh"
#include "bw.h"

/*
 * Local sub class
 * (Used to handle escape in the findbar, may also avoid some shortcuts).
 */
class MyInput : public Input {
public:
   MyInput (int x, int y, int w, int h, const char* l=0) :
      Input(x,y,w,h,l) {};
   int handle(int e);
};

int MyInput::handle(int e)
{
   _MSG("findbar MyInput::handle()\n");
   int ret = 1, k = event_key();
   unsigned modifier = event_state() & (SHIFT | CTRL | ALT | META);

   if (e == KEY) {
      if (k == LeftKey || k == RightKey) {
         if (modifier == SHIFT) {
            a_UIcmd_send_event_to_tabs_by_wid(e, this);
            return 1;
         }
      } else if (k == EscapeKey && modifier == 0) {
         // Avoid clearing the text with Esc, just hide the findbar.
         return 0;
      }
   }

   if (ret)
      ret = Input::handle(e);
   return ret;
};

/*
 * Find next occurrence of input key
 */
void Findbar::search_cb(Widget *, void *vfb)
{
   Findbar *fb = (Findbar *)vfb;
   const char *key = fb->i->text();
   bool case_sens = fb->check_btn->value();

   if (key[0] != '\0')
      a_UIcmd_findtext_search(a_UIcmd_get_bw_by_widget(fb),
                              key, case_sens);
}

/*
 * Find next occurrence of input key
 */
void Findbar::search_cb2(Widget *widget, void *vfb)
{
   /*
    * Somehow fltk even regards the first loss of focus for the
    * window as a WHEN_ENTER_KEY_ALWAYS event.
    */ 
   if (event_key() == ReturnKey)
      search_cb(widget, vfb);
}

/*
 * Hide the search bar
 */
void Findbar::hide_cb(Widget *, void *vfb)
{
   ((Findbar *)vfb)->hide();
}

/*
 * Construct text search bar
 */
Findbar::Findbar(int width, int height) :
   Group(0, 0, width, height)
{
   int button_width = 70;
   int gap = 2;
   int border = 2;
   int input_width = width - (2 * border + 3 * (button_width + gap));
   int x = border;
   height -= 2 * border;

   box(PLASTIC_UP_BOX);
   Group::hide();

   begin();
    hide_btn = new HighlightButton(x, border, 16, height, 0);
    hideImg = new xpmImage(new_s_xpm);
    hide_btn->image(hideImg);
    hide_btn->tooltip("Hide");
    x += 16 + gap;
    hide_btn->callback(hide_cb, this);
    hide_btn->clear_tab_to_focus();

    i = new MyInput(x, border, input_width, height);
    x += input_width + gap;
    resizable(i);
    i->color(206);
    i->when(WHEN_ENTER_KEY_ALWAYS);
    i->callback(search_cb2, this);
    i->clear_tab_to_focus();

    // TODO: search previous would be nice
    next_btn = new HighlightButton(x, border, button_width, height, "Next");
    x += button_width + gap;
    next_btn->tooltip("Find next occurrence of the search phrase");
    next_btn->add_shortcut(ReturnKey);
    next_btn->add_shortcut(KeypadEnter);
    next_btn->callback(search_cb, this);
    next_btn->clear_tab_to_focus();

    check_btn = new CheckButton(x, border, 2*button_width, height,
                              "Case-sensitive");
    check_btn->clear_tab_to_focus();
    x += 2 * button_width + gap;

   end();
}

Findbar::~Findbar()
{
   delete hideImg;
}

/*
 * Handle events. Used to catch EscapeKey events.
 */
int Findbar::handle(int event)
{
   int ret = 0;
   int k = event_key();
   unsigned modifier = event_state() & (SHIFT | CTRL | ALT | META);

   if (event == KEY && modifier == 0 && k == EscapeKey) {
      hide();
      ret = 1;
   }

   if (ret == 0)
      ret = Group::handle(event);

   return ret;
}

/*
 * Show the findbar and focus the input field
 */
void Findbar::show()
{
   Group::show();
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

   Group::hide();
   if ((bw = a_UIcmd_get_bw_by_widget(this)))
      a_UIcmd_findtext_reset(bw);
   a_UIcmd_focus_main_area(bw);
}
