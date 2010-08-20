
// fltkcomplexbutton.hh contains code from FLTK2's fltk/Button.h
// that is Copyright 2002 by Bill Spitzak and others.
// (see http://svn.easysw.com/public/fltk/fltk/trunk/fltk/Button.h)

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

#include <fltk/Group.h>

namespace dw {
namespace fltk {
namespace ui {

class ComplexButton: public ::fltk::Group
{
public:
  enum {HIDDEN=3}; // back-comptability value to hide the button

  bool  value() const { return state(); }
  bool  value(bool v) { return state(v); }

  int handle(int);
  int handle(int event, const Rectangle&);
  ComplexButton(int,int,int,int,const char * = 0);
  ~ComplexButton() { remove_all ();};
  static ::fltk::NamedStyle* default_style;

  virtual void draw();
  void draw(int glyph_width) const;
};

} // namespace ui
} // namespace fltk
} // namespace dw

#endif // __FLTK_COMPLEX_BUTTON_HH__

//
//
