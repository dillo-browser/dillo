
// fltkcomplexbutton.hh contains code from FLTK 1.3's FL/Fl_Button.H
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

#ifndef __FLTK_COMPLEX_BUTTON_HH__
#define __FLTK_COMPLEX_BUTTON_HH__

#include <FL/Fl_Group.H>

// values for type()
#define FL_NORMAL_BUTTON	0   /**< value() will be set to 1 during the press of the button and 
                                         reverts back to 0 when the button is released */
#define FL_TOGGLE_BUTTON	1   ///< value() toggles between 0 and 1 at every click of the button
#define FL_RADIO_BUTTON		(FL_RESERVED_TYPE+2) /**< is set to 1 at button press, and all other
				         buttons in the same group with <tt>type() == FL_RADIO_BUTTON</tt>
				         are set to zero.*/
#define FL_HIDDEN_BUTTON	3   ///< for Forms compatibility

extern FL_EXPORT Fl_Shortcut fl_old_shortcut(const char*);

namespace dw {
namespace fltk {
namespace ui {

class FL_EXPORT ComplexButton : public Fl_Group {

  int shortcut_;
  char value_;
  char oldval;
  uchar down_box_;

protected:

  static Fl_Widget_Tracker *key_release_tracker;
  static void key_release_timeout(void*);
  void simulate_key_action();
  
  virtual void draw();

public:

  virtual int handle(int);

  ComplexButton(int X, int Y, int W, int H, const char *L = 0);

  int value(int v);

  /**
    Returns the current value of the button (0 or 1).
   */
  char value() const {return value_;}

  /**
    Same as \c value(1).
    \see value(int v)
   */
  int set() {return value(1);}

  /**
    Same as \c value(0).
    \see value(int v)
   */
  int clear() {return value(0);}

  void setonly(); // this should only be called on FL_RADIO_BUTTONs

  /**
    Returns the current shortcut key for the button.
    \retval int
   */
  int shortcut() const {return shortcut_;}

  /**
    Sets the shortcut key to \c s.
    Setting this overrides the use of '\&' in the label().
    The value is a bitwise OR of a key and a set of shift flags, for example:
    <tt>FL_ALT | 'a'</tt>, or
    <tt>FL_ALT | (FL_F + 10)</tt>, or just
    <tt>'a'</tt>.
    A value of 0 disables the shortcut.

    The key can be any value returned by Fl::event_key(), but will usually be
    an ASCII letter.  Use a lower-case letter unless you require the shift key
    to be held down.

    The shift flags can be any set of values accepted by Fl::event_state().
    If the bit is on, that shift key must be pushed.  Meta, Alt, Ctrl, and
    Shift must be off if they are not in the shift flags (zero for the other
    bits indicates a "don't care" setting).
    \param[in] s bitwise OR of key and shift flags
   */
  void shortcut(int s) {shortcut_ = s;}

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

  /// (for backwards compatibility)
  void shortcut(const char *s) {shortcut(fl_old_shortcut(s));}

  /// (for backwards compatibility)
  Fl_Color down_color() const {return selection_color();}

  /// (for backwards compatibility)
  void down_color(unsigned c) {selection_color(c);}
};

} // namespace ui
} // namespace fltk
} // namespace dw

#endif

//
//
