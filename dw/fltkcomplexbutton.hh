
// fltkcomplexbutton.hh is derived from FL/Fl_Button.H from FLTK's 1.3 branch
// at http://fltk.org in early 2011.
// FL/Fl_Button.H is Copyright 1998-2010 by Bill Spitzak and others.

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

#ifndef __FLTK_COMPLEX_BUTTON_HH__
#define __FLTK_COMPLEX_BUTTON_HH__

#include <FL/Fl_Group.H>

namespace dw {
namespace fltk {
namespace ui {

class ComplexButton : public Fl_Group {
  char value_;
  char oldval;
  uchar down_box_;

protected:
  virtual void draw();

public:
  virtual int handle(int);

  ComplexButton(int X, int Y, int W, int H, const char *L = 0);
  ~ComplexButton();

  int value(int v);

  /**
    Returns the current value of the button (0 or 1).
   */
  char value() const {return value_;}

  /**
    Returns the current down box type, which is drawn when value() is non-zero.
    \retval Fl_Boxtype
   */
  Fl_Boxtype down_box() const {return (Fl_Boxtype)down_box_;}

  /**
    Sets the down box type. The default value of 0 causes FLTK to figure out
    the correct matching down version of box().
    \param[in] b down box type
   */
  void down_box(Fl_Boxtype b) {down_box_ = b;}
};

} // namespace ui
} // namespace fltk
} // namespace dw

#endif

//
//
