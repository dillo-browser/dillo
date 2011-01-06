/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
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



#include "fltkviewport.hh"

#include <fltk/draw.h>
#include <fltk/damage.h>
#include <fltk/events.h>

#include <stdio.h>
#include "../lout/msg.h"

using namespace fltk;
using namespace lout;
using namespace lout::object;
using namespace lout::container::typed;

namespace dw {
namespace fltk {

FltkViewport::FltkViewport (int x, int y, int w, int h, const char *label):
   FltkWidgetView (x, y, w, h, label)
{
   hscrollbar = new Scrollbar (0, 0, 1, 1);
   hscrollbar->set_horizontal();
   hscrollbar->callback (hscrollbarCallback, this);
   add (hscrollbar);

   vscrollbar = new Scrollbar (0, 0, 1, 1);
   vscrollbar->set_vertical();
   vscrollbar->callback (vscrollbarCallback, this);
   add (vscrollbar);

   scrollX = scrollY = scrollDX = scrollDY = 0;
   dragScrolling = 0;

   gadgetOrientation[0] = GADGET_HORIZONTAL;
   gadgetOrientation[1] = GADGET_HORIZONTAL;
   gadgetOrientation[2] = GADGET_VERTICAL;
   gadgetOrientation[3] = GADGET_HORIZONTAL;

   gadgets =
      new container::typed::List <object::TypedPointer < ::fltk::Widget> >
      (true);
}

FltkViewport::~FltkViewport ()
{
   delete gadgets;
}

void FltkViewport::adjustScrollbarsAndGadgetsAllocation ()
{
   int hdiff = 0, vdiff = 0;
   int visibility = 0;

   if (hscrollbar->visible ())
      visibility |= 1;
   if (vscrollbar->visible ())
      visibility |= 2;

   if (gadgets->size () > 0) {
      switch (gadgetOrientation [visibility]) {
      case GADGET_VERTICAL:
         hdiff = SCROLLBAR_THICKNESS;
         vdiff = SCROLLBAR_THICKNESS * gadgets->size ();
         break;

      case GADGET_HORIZONTAL:
         hdiff = SCROLLBAR_THICKNESS * gadgets->size ();
         vdiff = SCROLLBAR_THICKNESS;
         break;
      }
   } else {
      hdiff = vscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
      vdiff = hscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   }

   hscrollbar->x (0);
   hscrollbar->y (0 + h () - SCROLLBAR_THICKNESS);
   hscrollbar->w (w () - hdiff);
   hscrollbar->h (SCROLLBAR_THICKNESS);

   vscrollbar->x (0 + w () - SCROLLBAR_THICKNESS);
   vscrollbar->y (0);
   vscrollbar->h (h () - vdiff);
   vscrollbar->w (SCROLLBAR_THICKNESS);

   int x = w () - SCROLLBAR_THICKNESS, y = h () - SCROLLBAR_THICKNESS;
   for (Iterator <TypedPointer < ::fltk::Widget> > it = gadgets->iterator ();
        it.hasNext (); ) {
      ::fltk::Widget *widget = it.getNext()->getTypedValue ();
      widget->x (0);
      widget->y (0);
      widget->w (SCROLLBAR_THICKNESS);
      widget->h (SCROLLBAR_THICKNESS);

      switch (gadgetOrientation [visibility]) {
      case GADGET_VERTICAL:
         y -= SCROLLBAR_THICKNESS;
         break;

      case GADGET_HORIZONTAL:
         x -= SCROLLBAR_THICKNESS;
         break;
      }
   }
}

void FltkViewport::adjustScrollbarValues ()
{
   hscrollbar->value (scrollX, hscrollbar->w (), 0, canvasWidth);
   vscrollbar->value (scrollY, vscrollbar->h (), 0, canvasHeight);
}

void FltkViewport::hscrollbarChanged ()
{
   scroll (hscrollbar->value () - scrollX, 0);
}

void FltkViewport::vscrollbarChanged ()
{
   scroll (0, vscrollbar->value () - scrollY);
}

void FltkViewport::vscrollbarCallback (Widget *vscrollbar, void *viewportPtr)
{
   ((FltkViewport*)viewportPtr)->vscrollbarChanged ();
}

void FltkViewport::hscrollbarCallback (Widget *hscrollbar, void *viewportPtr)
{
   ((FltkViewport*)viewportPtr)->hscrollbarChanged ();
}

// ----------------------------------------------------------------------

void FltkViewport::layout ()
{
   theLayout->viewportSizeChanged (this, w(), h());
   adjustScrollbarsAndGadgetsAllocation ();

   FltkWidgetView::layout ();
}

void FltkViewport::draw_area (void *data, const Rectangle& cr )
{
  FltkViewport *vp = (FltkViewport*) data;
  push_clip(cr);

  vp->FltkWidgetView::draw ();

  for (Iterator <TypedPointer < ::fltk::Widget> > it = vp->gadgets->iterator();
       it.hasNext (); ) {
     ::fltk::Widget *widget = it.getNext()->getTypedValue ();
     vp->draw_child (*widget);
  }

  pop_clip();

}

void FltkViewport::draw ()
{
   int hdiff = vscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   int vdiff = hscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   Rectangle cr (0, 0, w () - hdiff, h () - vdiff);
   int d = damage();

   if (d & DAMAGE_SCROLL) {
      set_damage (DAMAGE_SCROLL);
      scrollrect(cr, -scrollDX, -scrollDY, draw_area, this);
      d &= ~DAMAGE_SCROLL;
      set_damage (d);
   }

   if (d) {
      draw_area(this, cr);

      if (d == DAMAGE_CHILD) {
         if (hscrollbar->damage ())
            draw_child (*hscrollbar);
         if (vscrollbar->damage ())
            draw_child (*vscrollbar);
      } else {
         draw_child (*hscrollbar);
         draw_child (*vscrollbar);
      }
   }

   scrollDX = 0;
   scrollDY = 0;
}

int FltkViewport::handle (int event)
{
   _MSG("FltkViewport::handle %d\n", event);

   if (hscrollbar->Rectangle::contains (Fl::event_x (), Fl::event_y ()) &&
       !(Fl::event_state() & (SHIFT | CTRL | ALT)) &&
       hscrollbar->send (event)) {
      return 1;
   }

   if (vscrollbar->Rectangle::contains (Fl::event_x (), Fl::event_y ()) &&
      vscrollbar->send (event)) {
      return 1;
   }

   switch(event) {
   case FL_KEYBOARD:
      /* Tell fltk we want to receive KEYBOARD events as SHORTCUT.
       * As we don't know the exact keybindings set by the user, we ask
       * for all of them (except Tab to keep form navigation). */
      if (Fl::event_key() != FL_Tab)
         return 0;
      break;

   case FL_FOCUS:
      /** \bug Draw focus box. */

      /* If the user clicks with the left button we take focus
       * and thereby unfocus any form widgets.
       * Otherwise we let fltk do the focus handling.
       */
      if (Fl::event_button() == FL_LEFT_MOUSE || focus_index() < 0) {
         focus_index(-1);
         return 1;
      }
      break;

   case FL_UNFOCUS:
      /** \bug Undraw focus box. */
      break;

   case FL_PUSH:
      take_focus();
      if (Fl::event_button() == FL_MIDDLE_MOUSE) {
         /* pass event so that middle click can open link in new window */
         if (FltkWidgetView::handle (event) == 0) {
            dragScrolling = 1;
            dragX = Fl::event_x();
            dragY = Fl::event_y();
            setCursor (core::style::CURSOR_MOVE);
         }
         return 1;
      }
      break;

   case FL_DRAG:
      if (Fl::event_button() == FL_MIDDLE_MOUSE) {
         if (dragScrolling) {
            scroll(dragX - Fl::event_x(), dragY - Fl::event_y());
            dragX = Fl::event_x();
            dragY = Fl::event_y();
            return 1;
         }
      }
      break;

   case FL_MOUSEWHEEL:
      return (Fl::event_dx() ? hscrollbar : vscrollbar)->handle(event);
      break;

   case FL_RELEASE:
      if (Fl::event_button() == FL_MIDDLE_MOUSE) {
         dragScrolling = 0;
         setCursor (core::style::CURSOR_DEFAULT);
      }
      break;

   case FL_ENTER:
      /* could be the result of, e.g., closing another window. */
      mouse_x = Fl::event_x();
      mouse_y = Fl::event_y();
      positionChanged();
      break;

   case FL_LEAVE:
      mouse_x = mouse_y = -1;
      break;
   }

   return FltkWidgetView::handle (event);
}

// ----------------------------------------------------------------------

void FltkViewport::setCanvasSize (int width, int ascent, int descent)
{
   FltkWidgetView::setCanvasSize (width, ascent, descent);
   adjustScrollbarValues ();
}

/*
 * This is used to simulate mouse motion (e.g., when scrolling).
 */
void FltkViewport::positionChanged ()
{
   if (mouse_x != -1)
      (void)theLayout->motionNotify (this,
                                     translateViewXToCanvasX (mouse_x),
                                     translateViewYToCanvasY (mouse_y),
                                     (core::ButtonState)0);
}

/*
 * For scrollbars, this currently sets the same step to both vertical and
 * horizontal. It may me differentiated if necessary.
 */
void FltkViewport::setScrollStep(int step)
{
   vscrollbar->linesize(step);
   hscrollbar->linesize(step);
}

bool FltkViewport::usesViewport ()
{
   return true;
}

int FltkViewport::getHScrollbarThickness ()
{
   return SCROLLBAR_THICKNESS;
}

int FltkViewport::getVScrollbarThickness ()
{
   return SCROLLBAR_THICKNESS;
}

void FltkViewport::scrollTo (int x, int y)
{
   int hdiff = vscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   int vdiff = hscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;

   x = misc::min (x, canvasWidth - w() + hdiff);
   x = misc::max (x, 0);

   y = misc::min (y, canvasHeight - h() + vdiff);
   y = misc::max (y, 0);

   if (x == scrollX && y == scrollY) {
      return;
   }

   /* multiple calls to scroll can happen before a redraw occurs.
    * scrollDX / scrollDY can therefore be non-zero here.
    */
   updateCanvasWidgets (x - scrollX, y - scrollY);
   scrollDX += x - scrollX;
   scrollDY += y - scrollY;

   scrollX = x;
   scrollY = y;

   adjustScrollbarValues ();
   redraw (DAMAGE_SCROLL);
   theLayout->scrollPosChanged (this, scrollX, scrollY);
   positionChanged();
}

void FltkViewport::scroll (int dx, int dy)
{
   scrollTo (scrollX + dx, scrollY + dy);
}

void FltkViewport::scroll (core::ScrollCommand cmd)
{
   if (cmd == core::SCREEN_UP_CMD) {
      scroll (0, -vscrollbar->pagesize ());
   } else if (cmd == core::SCREEN_DOWN_CMD) {
      scroll (0, vscrollbar->pagesize ());
   } else if (cmd == core::LINE_UP_CMD) {
      scroll (0, (int) -vscrollbar->linesize ());
   } else if (cmd == core::LINE_DOWN_CMD) {
      scroll (0, (int) vscrollbar->linesize ());
   } else if (cmd == core::LEFT_CMD) {
      scroll ((int) -hscrollbar->linesize (), 0);
   } else if (cmd == core::RIGHT_CMD) {
      scroll ((int) hscrollbar->linesize (), 0);
   } else if (cmd == core::TOP_CMD) {
      scrollTo (scrollX, 0);
   } else if (cmd == core::BOTTOM_CMD) {
      scrollTo (scrollX, canvasHeight); /* gets adjusted in scrollTo () */
   }
}

void FltkViewport::setViewportSize (int width, int height,
                                    int hScrollbarThickness,
                                    int vScrollbarThickness)
{
   if (hScrollbarThickness > 0)
      hscrollbar->show ();
   else
      hscrollbar->hide ();
   if (vScrollbarThickness > 0)
      vscrollbar->show ();
   else
      vscrollbar->hide ();

   /* If no scrollbar, go to the beginning */
   scroll(hScrollbarThickness ? 0 : -scrollX,
          vScrollbarThickness ? 0 : -scrollY);
}

void FltkViewport::updateCanvasWidgets (int dx, int dy)
{
   // scroll all child widgets except scroll bars
   for (int i = children () - 1; i > 0; i--) {
      ::fltk::Widget *widget = child (i);

      if (widget == hscrollbar || widget == vscrollbar)
         continue;

      widget->x (widget->x () - dx);
      widget->y (widget->y () - dy);
   }
}

int FltkViewport::translateViewXToCanvasX (int x)
{
   return x + scrollX;
}

int FltkViewport::translateViewYToCanvasY (int y)
{
   return y + scrollY;
}

int FltkViewport::translateCanvasXToViewX (int x)
{
   return x - scrollX;
}

int FltkViewport::translateCanvasYToViewY (int y)
{
   return y - scrollY;
}

// ----------------------------------------------------------------------

void FltkViewport::setGadgetOrientation (bool hscrollbarVisible,
                                         bool vscrollbarVisible,
                                         FltkViewport::GadgetOrientation
                                         gadgetOrientation)
{
   this->gadgetOrientation[(hscrollbarVisible ? 0 : 1) |
                           (vscrollbarVisible ? 0 : 2)] = gadgetOrientation;
   adjustScrollbarsAndGadgetsAllocation ();
}

void FltkViewport::addGadget (::fltk::Widget *gadget)
{
   /** \bug Reparent? */

   gadgets->append (new TypedPointer < ::fltk::Widget> (gadget));
   adjustScrollbarsAndGadgetsAllocation ();
}


} // namespace fltk
} // namespace dw
