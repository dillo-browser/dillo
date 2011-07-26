// fltkcomplexbutton.cc contains code from FLTK 1.3's src/Fl_Button.cxx
// that is Copyright 1998-2010 by Bill Spitzak and others.

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>

#include "fltkcomplexbutton.hh"

using namespace dw::fltk::ui;

/**
  Sets the current value of the button.
  A non-zero value sets the button to 1 (ON), and zero sets it to 0 (OFF).
  \param[in] v button value.
 */
int ComplexButton::value(int v) {
  v = v ? 1 : 0;
  oldval = v;
  clear_changed();
  if (value_ != v) {
    value_ = v;
    if (box()) redraw();
    return 1;
  } else {
    return 0;
  }
}

void ComplexButton::draw() {
  Fl_Color col = value() ? selection_color() : color();
  draw_box(value() ? (down_box()?down_box():fl_down(box())) : box(), col);
  if (Fl::focus() == this) draw_focus();

  // ComplexButton is a Group; draw its children
  for (int i = children () - 1; i >= 0; i--) {
     // set absolute coordinates for fltk-1.3  --jcid
     child (i)->position(x()+(w()-child(i)->w())/2,y()+(h()-child(i)->h())/2);
     draw_child (*child (i));
  }
}

int ComplexButton::handle(int event) {
  int newval;
  switch (event) {
  case FL_ENTER: /* FALLTHROUGH */
  case FL_LEAVE:
    return 1;
  case FL_PUSH:
    if (Fl::visible_focus() && handle(FL_FOCUS)) Fl::focus(this);
  case FL_DRAG:
    if (Fl::event_inside(this)) {
      newval = !oldval;
    } else
    {
      clear_changed();
      newval = oldval;
    }
    if (newval != value_) {
      value_ = newval;
      set_changed();
      redraw();
      if (when() & FL_WHEN_CHANGED) do_callback();
    }
    return 1;
  case FL_RELEASE:
    if (value_ == oldval) {
      if (when() & FL_WHEN_NOT_CHANGED) do_callback();
      return 1;
    }
    set_changed();
    value(oldval);
    set_changed();
    if (when() & FL_WHEN_CHANGED) {
      Fl_Widget_Tracker wp(this);
      do_callback();
      if (wp.deleted()) return 1;
    }
    if (when() & FL_WHEN_RELEASE) do_callback();
    return 1;
  case FL_FOCUS : /* FALLTHROUGH */
  case FL_UNFOCUS :
    if (Fl::visible_focus()) {
      if (box() == FL_NO_BOX) {
        // Widgets with the FL_NO_BOX boxtype need a parent to
        // redraw, since it is responsible for redrawing the
        // background...
        int X = x() > 0 ? x() - 1 : 0;
        int Y = y() > 0 ? y() - 1 : 0;
        if (window()) window()->damage(FL_DAMAGE_ALL, X, Y, w() + 2, h() + 2);
      } else redraw();
      return 1;
    } else return 0;
  case FL_KEYBOARD :
    if (Fl::focus() == this && Fl::event_key() == ' ' &&
        !(Fl::event_state() & (FL_SHIFT | FL_CTRL | FL_ALT | FL_META))) {
      set_changed();
      Fl_Widget_Tracker wp(this);
      if (wp.deleted()) return 1;
      if (when() & FL_WHEN_RELEASE) do_callback();
      return 1;
    }
  default:
    return 0;
  }
}

/**
  The constructor creates the button using the given position, size and label.
  \param[in] X, Y, W, H position and size of the widget
  \param[in] L widget label, default is no label
 */
ComplexButton::ComplexButton(int X, int Y, int W, int H, const char *L)
: Fl_Group(X,Y,W,H,L) {
  Fl_Group::current(0);
  box(FL_UP_BOX);
  down_box(FL_NO_BOX);
  value_ = oldval = 0;
}

ComplexButton::~ComplexButton() {
   /*
    * The Fl_Group destructor clear()s the children, but layout expects
    * the flat view to be around until it deletes it.
    */
   remove(0);
}
