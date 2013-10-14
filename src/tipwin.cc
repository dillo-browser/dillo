/*
 * File: tipwin.cc
 *
 * Copyright 2012 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * The tipwin idea was derived from the Fl_Slider example [1]
 * by Greg Ercolano, which is in public domain.
 *
 * [1] http://seriss.com/people/erco/fltk/#SliderTooltip
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Button.H>

#include "prefs.h"
#include "tipwin.hh"

/*
 * Forward declarations
 */
static void show_timeout(void*);
static void recent_timeout(void*);

/*
 * Custom tooltip window
 */
TipWin::TipWin() : Fl_Menu_Window(1, 1)     // will autosize
{
   bgcolor = fl_color_cube(FL_NUM_RED - 1, FL_NUM_GREEN - 1, FL_NUM_BLUE - 2);
   recent = 0;
   strcpy(tip, "");
   cur_widget = NULL;
   set_override(); // no border
   end();
}

void TipWin::draw()
{
   draw_box(FL_BORDER_BOX, 0, 0, w(), h(), bgcolor);
   fl_color(FL_BLACK);
   fl_font(labelfont(), labelsize());
   fl_draw(tip, 3, 3, w() - 6, h() - 6,
           //Fl_Align(FL_ALIGN_LEFT | FL_ALIGN_WRAP));
           Fl_Align(FL_ALIGN_LEFT));
}

void TipWin::value(const char *s) {
   // Recalc size of window
   snprintf(tip, sizeof(tip) - 1, "%s", s);
   fl_font(labelfont(), labelsize());
   int W = w(), H = h();
   W = 0;
   fl_measure(tip, W, H, 0);
   W += 8; H += 8;
   size(W, H);
   redraw();
}

void TipWin::do_show(void *wid) {
   cur_widget = wid;  // Keep track of requesting widget
   if (prefs.show_ui_tooltip) {
      Fl::add_timeout(recent ? 0.2f : 0.8f, show_timeout);
   }
}

void TipWin::do_hide() {
   Fl::remove_timeout(show_timeout);
   if (shown()) {
      hide();
      recent = 1;
      Fl::add_timeout(0.8f, recent_timeout);
   }
}

void TipWin::recent_tooltip(int val) {
   recent = val;
}

//--------------------------------------------------------------------------

TipWin *my_tipwin(void)
{
   static TipWin *tw = NULL;

   if (!tw) {
      Fl_Group *save = Fl_Group::current();    // save current widget..
      tw = new TipWin();                       // ..because this trashes it
      tw->hide();                              // start hidden
      Fl_Group::current(save);                 // ..then back to previous.
   }
   return tw;
}

static void show_timeout(void*) {
  // if offscreen, move tip ABOVE mouse instead
  int scr_x, scr_y, scr_w, scr_h;
  Fl::screen_xywh(scr_x, scr_y, scr_w, scr_h);
  int ty = Fl::event_y_root() + 20;
  if (ty + my_tipwin()->h() > scr_h)
     ty = Fl::event_y_root() - 20 - my_tipwin()->h();
  if (ty < 0) ty = 0;

  my_tipwin()->position(Fl::event_x_root(), ty);
  my_tipwin()->show();
  my_tipwin()->recent_tooltip(0);
}

static void recent_timeout(void*) {
  my_tipwin()->recent_tooltip(0);
}


//---------------------------------------------------------------------------

/*
 * A Button sharing a custom tooltip window
 */
TipWinButton::TipWinButton(int x, int y, int w, int h, const char *l) :
    Fl_Button(x, y, w, h, l)
{
   tipwin = my_tipwin();
   mytooltip = strdup("empty");
}

TipWinButton::~TipWinButton(void)
{
   tipwin->cancel(this); // cancel tooltip if shown
   free(mytooltip);
}

int TipWinButton::handle(int e)
{
   switch (e) {
   case FL_ENTER:
      tipwin->value(mytooltip);
      tipwin->do_show(this);
      break;
   case FL_PUSH:            // push mouse
   case FL_RELEASE:         // release mouse
   case FL_HIDE:            // widget goes away
   case FL_LEAVE:           // leave focus
      tipwin->do_hide();
      break;
   }
   return (Fl_Button::handle(e));
}

void TipWinButton::set_tooltip(const char *s)
{
   free(mytooltip);
   mytooltip = strdup(s);
}


//---------------------------------------------------------------------------

/*
 * A Light Button sharing a custom tooltip window
 */
CustButton::CustButton(int x, int y, int w, int h, const char *l) :
   TipWinButton(x,y,w,h,l)
{
   norm_color = color();
   light_color = PREFS_UI_BUTTON_HIGHLIGHT_COLOR;
}

int CustButton::handle(int e)
{
   if (active()) {
      if (e == FL_ENTER) {
         color(light_color);
         redraw();
      } else if (e == FL_LEAVE || e == FL_RELEASE || e == FL_HIDE) {
         color(norm_color);
         redraw();
      }
   } else if (e == FL_DEACTIVATE && color() != norm_color) {
      color(norm_color);
      redraw();
   }
   return TipWinButton::handle(e);
}

void CustButton::hl_color(Fl_Color col)
{
   light_color = col;
}


//---------------------------------------------------------------------------

/*
 * An Input with custom tooltip window
 */
TipWinInput::TipWinInput (int x, int y, int w, int h, const char *l) :
   Fl_Input(x,y,w,h,l)
{
   tipwin = my_tipwin();
   mytooltip = strdup("empty");
}

TipWinInput::~TipWinInput(void)
{
   tipwin->cancel(this); // cancel tooltip if shown
   free(mytooltip);
}

int TipWinInput::handle(int e)
{
   switch (e) {
   case FL_ENTER:
      tipwin->value(mytooltip);
      tipwin->do_show(this);
      break;
   case FL_PUSH:            // push mouse
   case FL_RELEASE:         // release mouse
   case FL_HIDE:            // widget goes away
   case FL_LEAVE:           // leave focus
   case FL_KEYBOARD:        // key press
      tipwin->do_hide();
      break;
   }
   return (Fl_Input::handle(e));
}

void TipWinInput::set_tooltip(const char *s)
{
   free(mytooltip);
   mytooltip = strdup(s);
}

