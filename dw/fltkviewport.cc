/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/names.h>

#include <stdio.h>
#include "../lout/msg.h"
#include "../lout/debug.hh"

using namespace lout;
using namespace lout::object;
using namespace lout::container::typed;

namespace dw {
namespace fltk {

/*
 * Lets SHIFT+{Left,Right} go to the parent
 */
class CustScrollbar : public Fl_Scrollbar
{
public:
   CustScrollbar(int x, int y, int w, int h) : Fl_Scrollbar(x,y,w,h) {};
   int handle(int e) {
      if (e == FL_SHORTCUT && Fl::event_state() == FL_SHIFT &&
          (Fl::event_key() == FL_Left || Fl::event_key() == FL_Right))
         return 0;
      return Fl_Scrollbar::handle(e);
   }
};

FltkViewport::FltkViewport (int X, int Y, int W, int H, const char *label):
   FltkWidgetView (X, Y, W, H, label)
{
   DBG_OBJ_CREATE ("dw::fltk::FltkViewport");

   hscrollbar = new CustScrollbar (x (), y (), 1, 1);
   hscrollbar->type(FL_HORIZONTAL);
   hscrollbar->callback (hscrollbarCallback, this);
   hscrollbar->hide();
   add (hscrollbar);

   vscrollbar = new Fl_Scrollbar (x (), y(), 1, 1);
   vscrollbar->type(FL_VERTICAL);
   vscrollbar->callback (vscrollbarCallback, this);
   vscrollbar->hide();
   add (vscrollbar);

   hasDragScroll = 1;
   scrollX = scrollY = scrollDX = scrollDY = 0;
   horScrolling = verScrolling = dragScrolling = 0;
   scrollbarPageMode = false;
   pageOverlap = 50;
   pageScrolling = core::NONE_CMD;

   pageScrollDelay = 0.300;
   pageScrollInterval = 0.100;

   gadgetOrientation[0] = GADGET_HORIZONTAL;
   gadgetOrientation[1] = GADGET_HORIZONTAL;
   gadgetOrientation[2] = GADGET_VERTICAL;
   gadgetOrientation[3] = GADGET_HORIZONTAL;

   gadgets =
      new container::typed::List <object::TypedPointer < Fl_Widget> >
      (true);
}

FltkViewport::~FltkViewport ()
{
   delete gadgets;
   DBG_OBJ_DELETE ();
}

void FltkViewport::adjustScrollbarsAndGadgetsAllocation ()
{
   int hdiff = 0, vdiff = 0;
   int visibility = 0;

   _MSG(" >>FltkViewport::adjustScrollbarsAndGadgetsAllocation\n");
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

   if (scrollbarOnLeft) {
      hscrollbar->resize(x () + hdiff, y () + h () - SCROLLBAR_THICKNESS,
                         w () - hdiff, SCROLLBAR_THICKNESS);
      vscrollbar->resize(x (), y (),
                         SCROLLBAR_THICKNESS, h () - vdiff);
   } else {
      hscrollbar->resize(x (), y () + h () - SCROLLBAR_THICKNESS,
                         w () - hdiff, SCROLLBAR_THICKNESS);
      vscrollbar->resize(x () + w () - SCROLLBAR_THICKNESS, y (),
                         SCROLLBAR_THICKNESS, h () - vdiff);
   }

   //int X = x () + w () - SCROLLBAR_THICKNESS;
   //int Y = y () + h () - SCROLLBAR_THICKNESS;
   for (Iterator <TypedPointer < Fl_Widget> > it = gadgets->iterator ();
        it.hasNext (); ) {
      Fl_Widget *widget = it.getNext()->getTypedValue ();
      widget->resize(x (), y (), SCROLLBAR_THICKNESS, SCROLLBAR_THICKNESS);

      /* FIXME: This has no effect */
#if 0
      switch (gadgetOrientation [visibility]) {
      case GADGET_VERTICAL:
         Y -= SCROLLBAR_THICKNESS;
         break;

      case GADGET_HORIZONTAL:
         X -= SCROLLBAR_THICKNESS;
         break;
      }
#endif
   }

   adjustScrollbarValues();
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

void FltkViewport::vscrollbarCallback (Fl_Widget *vscrollbar,void *viewportPtr)
{
   ((FltkViewport*)viewportPtr)->vscrollbarChanged ();
}

void FltkViewport::hscrollbarCallback (Fl_Widget *hscrollbar,void *viewportPtr)
{
   ((FltkViewport*)viewportPtr)->hscrollbarChanged ();
}

// ----------------------------------------------------------------------

void FltkViewport::resize(int X, int Y, int W, int H)
{
   bool dimension_changed = W != w() || H != h();

   Fl_Group::resize(X, Y, W, H);
   if (dimension_changed) {
      theLayout->viewportSizeChanged (this, W, H);
      adjustScrollbarsAndGadgetsAllocation ();
   }
}

void FltkViewport::draw_area (void *data, int x, int y, int w, int h)
{
  FltkViewport *vp = (FltkViewport*) data;
  fl_push_clip(x, y, w, h);

  vp->FltkWidgetView::draw ();

  for (Iterator <TypedPointer < Fl_Widget> > it = vp->gadgets->iterator();
       it.hasNext (); ) {
     Fl_Widget *widget = it.getNext()->getTypedValue ();
     vp->draw_child (*widget);
  }

  fl_pop_clip();
}

/*
 * Draw the viewport.
 *
 *  + Damage flags come in different ways, draw() should cope with them all.
 *  + Damage flags are alive for visible and hidden widgets.
 *  + FL_DAMAGE_CHILD can flag scroll bars or embedded FLTK widgets.
 */
void FltkViewport::draw ()
{
   const int d = damage(),
             vis_vs = vscrollbar->visible () ? SCROLLBAR_THICKNESS : 0,
             vis_hs = hscrollbar->visible () ? SCROLLBAR_THICKNESS : 0,
             draw = d & (FL_DAMAGE_ALL | FL_DAMAGE_EXPOSE),
             draw_vs = vis_vs && vscrollbar->damage (),
             draw_hs = vis_hs && hscrollbar->damage ();

   _MSG("FltkViewport::draw d=%d  =>  ", d);
   // scrollbars
   if (draw || draw_vs)
      draw_child (*vscrollbar);
   if (draw || draw_hs)
      draw_child (*hscrollbar);
   if (draw && vis_vs && vis_hs) {
      fl_color(FL_BACKGROUND_COLOR);
      if (scrollbarOnLeft) {
         fl_rectf(x(), y()+h()-vis_hs, vis_vs, vis_hs);
      } else {
         fl_rectf(x()+w()-vis_vs, y()+h()-vis_hs, vis_vs, vis_hs);
      }
   }
   // main area
   if (d == FL_DAMAGE_CHILD && (draw_vs || draw_hs)) {
      _MSG("none\n");
   } else if (d == (FL_DAMAGE_SCROLL | FL_DAMAGE_CHILD)) {
      int x = this->x();

      if (scrollbarOnLeft)
         x += vis_vs;
      fl_scroll(x, y(), w() - vis_vs, h() - vis_hs,
                -scrollDX, -scrollDY, draw_area, this);
      _MSG("fl_scroll()\n");
   } else {
      int x = this->x();

      if (scrollbarOnLeft)
         x += vis_vs;
      draw_area(this, x, y(), w() - vis_vs, h() - vis_hs);
      _MSG("draw_area()\n");
   }

   scrollDX = 0;
   scrollDY = 0;
}

int FltkViewport::handle (int event)
{
   int ret = 0;
   _MSG("FltkViewport::handle %s\n", fl_eventnames[event]);

   switch(event) {
   case FL_KEYBOARD:
      /* When the viewport has focus (and not one of its children), FLTK
       * sends the event here. Returning zero tells FLTK to resend the
       * event as SHORTCUT, which we finally route to the parent. */

      /* As we don't know the exact keybindings set by the user, we ask for
       * all of them (except for the minimum needed to keep form navigation).*/
      if (Fl::event_key() != FL_Tab || Fl::event_ctrl())
         return 0;
      break;

   case FL_SHORTCUT:
      /* send it to the parent (UI) */
      return 0;

   case FL_FOCUS:
      /** \bug Draw focus box. */
      break;

   case FL_UNFOCUS:
      /** \bug Undraw focus box. */
      break;

   case FL_PUSH:
      if (vscrollbar->visible() && Fl::event_inside(vscrollbar)) {
         if (scrollbarPageMode ^ (bool) Fl::event_shift()) {
            /* Check top and bottom actions first */
            int yclick = Fl::event_y();
            int ytop = y() + SCROLLBAR_THICKNESS;
            int ybottom = y() + h() - SCROLLBAR_THICKNESS;
            if (hscrollbar->visible())
               ybottom -= SCROLLBAR_THICKNESS;

            if (yclick <= ytop) {
               scroll(core::TOP_CMD);
               return 1;
            } else if (yclick >= ybottom) {
               scroll(core::BOTTOM_CMD);
               return 1;
            }

            if (Fl::event_button() == FL_LEFT_MOUSE)
               pageScrolling = core::SCREEN_DOWN_CMD;
            else if (Fl::event_button() == FL_RIGHT_MOUSE)
               pageScrolling = core::SCREEN_UP_CMD;
            else
               pageScrolling = core::NONE_CMD;

            if (pageScrolling != core::NONE_CMD) {
               scroll(pageScrolling);
               /* Repeat until released */
               if (!Fl::has_timeout(repeatPageScroll, this))
                  Fl::add_timeout(pageScrollDelay, repeatPageScroll, this);

               return 1;
            }
         }
         if (vscrollbar->handle(event)) {
            verScrolling = 1;
         }
      } else if (hscrollbar->visible() && Fl::event_inside(hscrollbar)) {
         if (hscrollbar->handle(event))
            horScrolling = 1;
      } else if (FltkWidgetView::handle(event) == 0 &&
                 Fl::event_button() == FL_MIDDLE_MOUSE) {
         if (!hasDragScroll) {
            /* let the parent widget handle it... */
            return 0;
         } else {
            /* receive FL_DRAG and FL_RELEASE */
            dragScrolling = 1;
            dragX = Fl::event_x();
            dragY = Fl::event_y();
            setCursor (core::style::CURSOR_MOVE);
         }
      }
      return 1;
      break;

   case FL_DRAG:
      if (Fl::event_inside(this))
         Fl::remove_timeout(selectionScroll);
      if (dragScrolling) {
         scroll(dragX - Fl::event_x(), dragY - Fl::event_y());
         dragX = Fl::event_x();
         dragY = Fl::event_y();
         return 1;
      } else if (verScrolling) {
         vscrollbar->handle(event);
         return 1;
      } else if (horScrolling) {
         hscrollbar->handle(event);
         return 1;
      } else if (!Fl::event_inside(this)) {
         mouse_x = Fl::event_x();
         mouse_y = Fl::event_y();
         if (!Fl::has_timeout(selectionScroll, this))
            Fl::add_timeout(0.025, selectionScroll, this);
      }
      break;

   case FL_MOUSEWHEEL:
      if ((vscrollbar->visible() && Fl::event_inside(vscrollbar)) ||
            Fl::event_shift()) {
         if (Fl::event_dy() > 0) {
            scroll(core::SCREEN_DOWN_CMD);
            return 1;
         } else if (Fl::event_dy() < 0) {
            scroll(core::SCREEN_UP_CMD);
            return 1;
         }
      }
      return (Fl::event_dx() ? hscrollbar : vscrollbar)->handle(event);
      break;

   case FL_RELEASE:
      Fl::remove_timeout(repeatPageScroll);
      Fl::remove_timeout(selectionScroll);
      if (Fl::event_button() == FL_MIDDLE_MOUSE) {
         setCursor (core::style::CURSOR_DEFAULT);
      } else if (verScrolling) {
         ret = vscrollbar->handle(event);
      } else if (horScrolling) {
         ret = hscrollbar->handle(event);
      }
      horScrolling = verScrolling = dragScrolling = 0;
      break;

   case FL_ENTER:
      if (vscrollbar->visible() && Fl::event_inside(vscrollbar))
         return vscrollbar->handle(event);
      if (hscrollbar->visible() && Fl::event_inside(hscrollbar))
         return hscrollbar->handle(event);
      /* could be the result of, e.g., closing another window. */
      mouse_x = Fl::event_x();
      mouse_y = Fl::event_y();
      positionChanged();
      break;

   case FL_MOVE:
      /* Use LEAVE in order not to be over a link, etc., anymore. */
      if (vscrollbar->visible() && Fl::event_inside(vscrollbar)) {
         (void)FltkWidgetView::handle(FL_LEAVE);
         return vscrollbar->handle(event);
      }
      if (hscrollbar->visible() && Fl::event_inside(hscrollbar)) {
         (void)FltkWidgetView::handle(FL_LEAVE);
         return hscrollbar->handle(event);
      }
      break;
   case FL_LEAVE:
      mouse_x = mouse_y = -1;
      break;
   }

   return ret ? ret : FltkWidgetView::handle (event);
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
   if (!dragScrolling && mouse_x >= x() && mouse_x < x()+w() && mouse_y >= y()
       && mouse_y < y()+h())
      (void)theLayout->motionNotify (this,
                                     translateViewXToCanvasX (mouse_x),
                                     translateViewYToCanvasY (mouse_y),
                                     (core::ButtonState)0);
}

/*
 * For scrollbars, this currently sets the same step to both vertical and
 * horizontal. It may be differentiated if necessary.
 */
void FltkViewport::setScrollStep(int step)
{
   vscrollbar->linesize(step);
   hscrollbar->linesize(step);
}

void FltkViewport::setPageOverlap(int overlap)
{
   pageOverlap = overlap;
}

void FltkViewport::setScrollbarPageMode(bool enable)
{
   scrollbarPageMode = enable;
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
    * scrollDX and scrollDY can therefore be non-zero here.
    */
   updateCanvasWidgets (x - scrollX, y - scrollY);
   scrollDX += x - scrollX;
   scrollDY += y - scrollY;

   scrollX = x;
   scrollY = y;

   adjustScrollbarValues ();
   damage(FL_DAMAGE_SCROLL);
   theLayout->scrollPosChanged (this, scrollX, scrollY);
   positionChanged();
}

void FltkViewport::scroll (int dx, int dy)
{
   scrollTo (scrollX + dx, scrollY + dy);
}

void FltkViewport::scroll (core::ScrollCommand cmd)
{
   int hdiff = vscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   int vdiff = hscrollbar->visible () ? SCROLLBAR_THICKNESS : 0;
   if (cmd == core::SCREEN_UP_CMD) {
      scroll (0, -h () + pageOverlap + vdiff);
   } else if (cmd == core::SCREEN_DOWN_CMD) {
      scroll (0, h () - pageOverlap - vdiff);
   } else if (cmd == core::SCREEN_LEFT_CMD) {
      scroll (-w() + pageOverlap + hdiff, 0);
   } else if (cmd == core::SCREEN_RIGHT_CMD) {
      scroll (w() - pageOverlap - hdiff, 0);
   } else if (cmd == core::LINE_UP_CMD) {
      scroll (0, -vscrollbar->linesize ());
   } else if (cmd == core::LINE_DOWN_CMD) {
      scroll (0, vscrollbar->linesize ());
   } else if (cmd == core::LEFT_CMD) {
      scroll (-hscrollbar->linesize (), 0);
   } else if (cmd == core::RIGHT_CMD) {
      scroll (hscrollbar->linesize (), 0);
   } else if (cmd == core::TOP_CMD) {
      scrollTo (scrollX, 0);
   } else if (cmd == core::BOTTOM_CMD) {
      scrollTo (scrollX, canvasHeight); /* gets adjusted in scrollTo () */
   }
}

/*
 * Scrolling in response to selection where the cursor is outside the view.
 */
void FltkViewport::selectionScroll ()
{
   int distance;
   int dx = 0, dy = 0;

   if ((distance = x() - mouse_x) > 0)
      dx = -distance * hscrollbar->linesize () / 48 - 1;
   else if ((distance = mouse_x - (x() + w())) > 0)
      dx = distance * hscrollbar->linesize () / 48 + 1;
   if ((distance = y() - mouse_y) > 0)
      dy = -distance * vscrollbar->linesize () / 48 - 1;
   else if ((distance = mouse_y - (y() + h())) > 0)
      dy = distance * vscrollbar->linesize () / 48 + 1;

   scroll (dx, dy);
}

void FltkViewport::selectionScroll (void *data)
{
   ((FltkViewport *)data)->selectionScroll ();
   Fl::repeat_timeout(0.025, selectionScroll, data);
}

void FltkViewport::repeatPageScroll ()
{
   scroll(pageScrolling);
   Fl::repeat_timeout(pageScrollInterval, repeatPageScroll, this);
}

void FltkViewport::repeatPageScroll (void *data)
{
   ((FltkViewport *)data)->repeatPageScroll ();
}

void FltkViewport::setScrollbarOnLeft (bool enable)
{
   scrollbarOnLeft = enable ? 1 : 0;
   adjustScrollbarsAndGadgetsAllocation();
   damage(FL_DAMAGE_ALL);
}

void FltkViewport::setViewportSize (int width, int height,
                                    int hScrollbarThickness,
                                    int vScrollbarThickness)
{
   int adjustReq =
      (hscrollbar->visible() ? !hScrollbarThickness : hScrollbarThickness) ||
      (vscrollbar->visible() ? !vScrollbarThickness : vScrollbarThickness);

   _MSG("FltkViewport::setViewportSize old_w,old_h=%dx%d -> w,h=%dx%d\n"
       "\t hThick=%d hVis=%d, vThick=%d vVis=%d, adjustReq=%d\n",
       w(),h(),width,height,
       hScrollbarThickness,hscrollbar->visible(),
       vScrollbarThickness,vscrollbar->visible(), adjustReq);

   (hScrollbarThickness > 0) ? hscrollbar->show () : hscrollbar->hide ();
   (vScrollbarThickness > 0) ? vscrollbar->show () : vscrollbar->hide ();

   /* If no scrollbar, go to the beginning */
   scroll(hScrollbarThickness ? 0 : -scrollX,
          vScrollbarThickness ? 0 : -scrollY);

   /* Adjust when scrollbar visibility changes */
   if (adjustReq)
      adjustScrollbarsAndGadgetsAllocation ();
}

void FltkViewport::updateCanvasWidgets (int dx, int dy)
{
   // scroll all child widgets except scroll bars
   for (int i = children () - 1; i > 0; i--) {
      Fl_Widget *widget = child (i);

      if (widget == hscrollbar || widget == vscrollbar)
         continue;

      widget->position(widget->x () - dx, widget->y () - dy);
   }
}

int FltkViewport::translateViewXToCanvasX (int X)
{
   return X - x () + scrollX;
}

int FltkViewport::translateViewYToCanvasY (int Y)
{
   return Y - y () + scrollY;
}

int FltkViewport::translateCanvasXToViewX (int X)
{
   return X + x () - scrollX;
}

int FltkViewport::translateCanvasYToViewY (int Y)
{
   return Y + y () - scrollY;
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

void FltkViewport::addGadget (Fl_Widget *gadget)
{
   /** \bug Reparent? */

   gadgets->append (new TypedPointer < Fl_Widget> (gadget));
   adjustScrollbarsAndGadgetsAllocation ();
}


} // namespace fltk
} // namespace dw
