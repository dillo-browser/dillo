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
#include <fltk/layout.h>
#include <fltk/events.h>
#include <fltk/Cursor.h>
#include <fltk/run.h>

#include <stdio.h>
#include "../lout/msg.h"

using namespace fltk;
using namespace lout::object;
using namespace lout::container::typed;

namespace dw {
namespace fltk {

::fltk::Image *FltkViewBase::backBuffer;
bool FltkViewBase::backBufferInUse;

FltkViewBase::FltkViewBase (int x, int y, int w, int h, const char *label):
   Group (x, y, w, h, label)
{
   canvasWidth = 1;
   canvasHeight = 1;
   bgColor = WHITE;
   mouse_x = mouse_y = 0;
   exposeArea = NULL;
   if (backBuffer == NULL) {
      backBuffer = new Image ();
   }
}

FltkViewBase::~FltkViewBase ()
{
   cancelQueueDraw ();
}

void FltkViewBase::setBufferedDrawing (bool b) {
   if (b && backBuffer == NULL) {
      backBuffer = new Image ();
   } else if (!b && backBuffer != NULL) {
      delete backBuffer;
      backBuffer = NULL;
   }
}

void FltkViewBase::draw ()
{
   int d = damage ();

   if ((d & DAMAGE_VALUE) && !(d & DAMAGE_EXPOSE)) {
      lout::container::typed::Iterator <core::Rectangle> it;

      for (it = drawRegion.rectangles (); it.hasNext (); ) {
         draw (it.getNext (), DRAW_BUFFERED);
      }

      drawRegion.clear ();
      d &= ~DAMAGE_VALUE;
   }

   if (d & DAMAGE_CHILD) {
      drawChildWidgets ();
      d &= ~DAMAGE_CHILD;
   }

   if (d) {
      dw::core::Rectangle rect (
         translateViewXToCanvasX (0),
         translateViewYToCanvasY (0),
         w (),
         h ());

      if (d == DAMAGE_SCROLL) {
         // a clipping rectangle has already been set by fltk::scrollrect ()
         draw (&rect, DRAW_PLAIN);
      } else {
         draw (&rect, DRAW_CLIPPED);
         drawRegion.clear ();
      }
   }
}

void FltkViewBase::draw (const core::Rectangle *rect,
                         DrawType type)
{
   int offsetX = 0, offsetY = 0;

   /* fltk-clipping does not use widget coordinates */
   transform (offsetX, offsetY);

   ::fltk::Rectangle viewRect (
      translateCanvasXToViewX (rect->x) + offsetX,
      translateCanvasYToViewY (rect->y) + offsetY,
      rect->width, rect->height);

   ::fltk::intersect_with_clip (viewRect);

   viewRect.x (viewRect.x () - offsetX);
   viewRect.y (viewRect.y () - offsetY);

   if (! viewRect.empty ()) {
      dw::core::Rectangle r (
         translateViewXToCanvasX (viewRect.x ()),
         translateViewYToCanvasY (viewRect.y ()),
         viewRect.w (),
         viewRect.h ());

      exposeArea = &viewRect;

      if (type == DRAW_BUFFERED && backBuffer && !backBufferInUse) {
         backBufferInUse = true;
         {
            GSave gsave;

            backBuffer->setsize (viewRect.w (), viewRect.h ());
            backBuffer->make_current ();
            translate (-viewRect.x (), -viewRect.y ());

            setcolor (bgColor);
            fillrect (viewRect);
            theLayout->expose (this, &r);
         }

         backBuffer->draw (Rectangle (0, 0, viewRect.w (), viewRect.h ()),
            viewRect);

         backBufferInUse = false;
      } else if (type == DRAW_BUFFERED || type == DRAW_CLIPPED) {
         // if type == DRAW_BUFFERED but we do not have backBuffer available
         // we fall back to clipped drawing
         push_clip (viewRect);
         setcolor (bgColor);
         fillrect (viewRect);
         theLayout->expose (this, &r);
         pop_clip ();
      } else {
         setcolor (bgColor);
         fillrect (viewRect);
         theLayout->expose (this, &r);
      }

      exposeArea = NULL;
   }
}

void FltkViewBase::drawChildWidgets () {
   for (int i = children () - 1; i >= 0; i--) {
      Widget& w = *child(i);
      if (w.damage() & DAMAGE_CHILD_LABEL) {
         draw_outside_label(w);
         w.set_damage(w.damage() & ~DAMAGE_CHILD_LABEL);
      }
      update_child(w);
   }
}

core::ButtonState getDwButtonState ()
{
   int s1 = event_state ();
   int s2 = (core::ButtonState)0;

   if (s1 & SHIFT)   s2 |= core::SHIFT_MASK;
   if (s1 & CTRL)    s2 |= core::CONTROL_MASK;
   if (s1 & ALT)     s2 |= core::META_MASK;
   if (s1 & BUTTON1) s2 |= core::BUTTON1_MASK;
   if (s1 & BUTTON2) s2 |= core::BUTTON2_MASK;
   if (s1 & BUTTON3) s2 |= core::BUTTON3_MASK;

   return (core::ButtonState)s2;
}

int FltkViewBase::handle (int event)
{
   bool processed;

   /**
    * \todo Consider, whether this from the FLTK documentation has any
    *    impacts: "To receive fltk::RELEASE events you must return non-zero
    *    when passed a fltk::PUSH event. "
    */
   switch(event) {
   case PUSH:
      processed =
         theLayout->buttonPress (this, event_clicks () + 1,
                                 translateViewXToCanvasX (event_x ()),
                                 translateViewYToCanvasY (event_y ()),
                                 getDwButtonState (), event_button ());
      _MSG("PUSH => %s\n", processed ? "true" : "false");
      if (processed) {
         /* pressed dw content; give focus to the view */
         ::fltk::focus(this);
      }
      return processed ? true : Group::handle (event);

   case RELEASE:
      processed =
         theLayout->buttonRelease (this, event_clicks () + 1,
                                   translateViewXToCanvasX (event_x ()),
                                   translateViewYToCanvasY (event_y ()),
                                   getDwButtonState (), event_button ());
      _MSG("RELEASE => %s\n", processed ? "true" : "false");
      return processed ? true : Group::handle (event);

   case MOVE:
      mouse_x = event_x();
      mouse_y = event_y();
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (mouse_x),
                                  translateViewYToCanvasY (mouse_y),
                                  getDwButtonState ());
      _MSG("MOVE => %s\n", processed ? "true" : "false");
      return processed ? true : Group::handle (event);

   case DRAG:
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (event_x ()),
                                  translateViewYToCanvasY (event_y ()),
                                  getDwButtonState ());
      _MSG("DRAG => %s\n", processed ? "true" : "false");
      return processed ? true : Group::handle (event);

   case ENTER:
      theLayout->enterNotify (this, translateViewXToCanvasX (event_x ()),
                              translateViewYToCanvasY (event_y ()),
                              getDwButtonState ());
      return Group::handle (event);

   case LEAVE:
      theLayout->leaveNotify (this, getDwButtonState ());
      return Group::handle (event);

   default:
      return Group::handle (event);
   }
}

// ----------------------------------------------------------------------

void FltkViewBase::setLayout (core::Layout *layout)
{
   theLayout = layout;
}

void FltkViewBase::setCanvasSize (int width, int ascent, int descent)
{
   canvasWidth = width;
   canvasHeight = ascent + descent;
}

void FltkViewBase::setCursor (core::style::Cursor cursor)
{
   static Cursor *mapDwToFltk[] = {
      CURSOR_CROSS,
      CURSOR_DEFAULT,
      CURSOR_HAND,
      CURSOR_MOVE,
      CURSOR_WE,
      CURSOR_NESW,
      CURSOR_NWSE,
      CURSOR_NS,
      CURSOR_NWSE,
      CURSOR_NESW,
      CURSOR_NS,
      CURSOR_WE,
      CURSOR_INSERT,
      CURSOR_WAIT,
      CURSOR_HELP
   };

   /*
   static char *cursorName[] = {
      "CURSOR_CROSS",
      "CURSOR_DEFAULT",
      "CURSOR_HAND",
      "CURSOR_MOVE",
      "CURSOR_WE",
      "CURSOR_NESW",
      "CURSOR_NWSE",
      "CURSOR_NS",
      "CURSOR_NWSE",
      "CURSOR_NESW",
      "CURSOR_NS",
      "CURSOR_WE",
      "CURSOR_INSERT",
      "CURSOR_WAIT",
      "CURSOR_HELP"
   };

   MSG("Cursor changes to '%s'.\n", cursorName[cursor]);
   */

   /** \bug Does not work */
   this->cursor (mapDwToFltk[cursor]);
}

void FltkViewBase::setBgColor (core::style::Color *color)
{
   bgColor = color ?
      ((FltkColor*)color)->colors[dw::core::style::Color::SHADING_NORMAL] :
      WHITE;
}

void FltkViewBase::startDrawing (core::Rectangle *area)
{
}

void FltkViewBase::finishDrawing (core::Rectangle *area)
{
}

void FltkViewBase::queueDraw (core::Rectangle *area)
{
   drawRegion.addRectangle (area);
   /** DAMAGE_VALUE is just an arbitrary value other than DAMAGE_EXPOSE here */
   redraw (DAMAGE_VALUE);
}

void FltkViewBase::queueDrawTotal ()
{
   redraw (DAMAGE_EXPOSE);
}

void FltkViewBase::cancelQueueDraw ()
{
}

void FltkViewBase::drawPoint (core::style::Color *color,
                              core::style::Color::Shading shading,
                              int x, int y)
{
}

void FltkViewBase::drawLine (core::style::Color *color,
                             core::style::Color::Shading shading,
                             int x1, int y1, int x2, int y2)
{
   setcolor(((FltkColor*)color)->colors[shading]);
   drawline (translateCanvasXToViewX (x1), translateCanvasYToViewY (y1),
             translateCanvasXToViewX (x2), translateCanvasYToViewY (y2));
}

void FltkViewBase::drawRectangle (core::style::Color *color,
                                  core::style::Color::Shading shading,
                                  bool filled,
                                  int x, int y, int width, int height)
{
   setcolor(((FltkColor*)color)->colors[shading]);
   if (width < 0) {
      x += width;
      width = -width;
   }
   if (height < 0) {
      y += height;
      height = -height;
   }

   int x1 = translateCanvasXToViewX (x);
   int y1 = translateCanvasYToViewY (y);
   int x2 = translateCanvasXToViewX (x + width);
   int y2 = translateCanvasYToViewY (y + height);

   // We only support rectangles with line width 1px, so we clip with 
   // a rectangle 1px wider and higher than what we actually expose.
   // This is only really necessary for non-filled rectangles.
   clipPoint (&x1, &y1, 1);
   clipPoint (&x2, &y2, 1);

   ::fltk::Rectangle rect (x1, y1, x2 - x1, y2 - y1);
   if (filled)
      fillrect (rect);
   else
      strokerect (rect);
}

void FltkViewBase::drawArc (core::style::Color *color,
                            core::style::Color::Shading shading, bool filled,
                            int x, int y, int width, int height,
                            int angle1, int angle2)
{
   setcolor(((FltkColor*)color)->colors[shading]);
   int x1 = translateCanvasXToViewX (x);
   int y1 = translateCanvasYToViewY (y);
   ::fltk::Rectangle rect (x1, y1, width, height);
   addchord(rect, angle1, angle2);
   closepath();
   if (filled)
      fillpath();
   else
      strokepath();
}

void FltkViewBase::drawPolygon (core::style::Color *color,
                                core::style::Color::Shading shading,
                                bool filled, int points[][2], int npoints)
{
   if (npoints > 0) {
      for (int i = 0; i < npoints; i++) {
         points[i][0] = translateCanvasXToViewX(points[i][0]);
         points[i][1] = translateCanvasYToViewY(points[i][1]);
      }
      setcolor(((FltkColor*)color)->colors[shading]);
      addvertices(npoints, points);
      closepath();
      if (filled)
         fillpath();
      else
         strokepath();
   }
}

core::View *FltkViewBase::getClippingView (int x, int y, int width, int height)
{
   push_clip (translateCanvasXToViewX (x), translateCanvasYToViewY (y),
              width, height);
   return this;
}

void FltkViewBase::mergeClippingView (core::View *clippingView)
{
   pop_clip ();
}

// ----------------------------------------------------------------------

FltkWidgetView::FltkWidgetView (int x, int y, int w, int h,
                                const char *label):
   FltkViewBase (x, y, w, h, label)
{
}

FltkWidgetView::~FltkWidgetView ()
{
}

void FltkWidgetView::layout () {
   /**
    * pass layout to child widgets. This is needed for complex fltk
    * widgets as TextEditor.
    * We can't use Group::layout() as that would rearrange the widgets.
    */
   for (int i = children () - 1; i >= 0; i--) {
      ::fltk::Widget *widget = child (i);

      if (widget->layout_damage ()) {
         widget->layout ();
      }
   }
}

void FltkWidgetView::drawText (core::style::Font *font,
                             core::style::Color *color,
                             core::style::Color::Shading shading,
                             int x, int y, const char *text, int len)
{
   FltkFont *ff = (FltkFont*)font;
   setfont(ff->font, ff->size);
   setcolor(((FltkColor*)color)->colors[shading]);

   if (!font->letterSpacing) {
      drawtext(text, len,
               translateCanvasXToViewX (x), translateCanvasYToViewY (y));
   } else {
      /* Nonzero letter spacing adjustment, draw each glyph individually */
      int viewX = translateCanvasXToViewX (x),
          viewY = translateCanvasYToViewY (y);
      int curr = 0, next = 0;

      while (next < len) {
         next = theLayout->nextGlyph(text, curr);
         drawtext(text + curr, next - curr, viewX, viewY);
         viewX += font->letterSpacing + (int)getwidth(text + curr,next - curr);
         curr = next;
      }
   }
}

void FltkWidgetView::drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                              int x, int y, int width, int height)
{
   ((FltkImgbuf*)imgbuf)->draw (this,
                                translateCanvasXToViewX (xRoot),
                                translateCanvasYToViewY (yRoot),
                                x, y, width, height);
}

bool FltkWidgetView::usesFltkWidgets ()
{
   return true;
}

void FltkWidgetView::addFltkWidget (::fltk::Widget *widget,
                                  core::Allocation *allocation)
{
   allocateFltkWidget (widget, allocation);
   add (widget);
}

void FltkWidgetView::removeFltkWidget (::fltk::Widget *widget)
{
   remove (widget);
}

void FltkWidgetView::allocateFltkWidget (::fltk::Widget *widget,
                                       core::Allocation *allocation)
{
   widget->x (translateCanvasXToViewX (allocation->x));
   widget->y (translateCanvasYToViewY (allocation->y));
   widget->w (allocation->width);
   widget->h (allocation->ascent + allocation->descent);

   /* widgets created tiny and later resized need this flag to display */
   uchar damage = widget->layout_damage ();
   damage |= LAYOUT_XYWH;
   widget->layout_damage (damage);
}

void FltkWidgetView::drawFltkWidget (::fltk::Widget *widget,
                                   core::Rectangle *area)
{
   draw_child (*widget);
}

} // namespace fltk
} // namespace dw
