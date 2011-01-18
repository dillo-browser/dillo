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

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include "../lout/msg.h"

using namespace lout::object;
using namespace lout::container::typed;

namespace dw {
namespace fltk {

Fl_Image *FltkViewBase::backBuffer;
bool FltkViewBase::backBufferInUse;

FltkViewBase::FltkViewBase (int x, int y, int w, int h, const char *label):
   Fl_Group (x, y, w, h, label)
{
   canvasWidth = 1;
   canvasHeight = 1;
   bgColor = FL_WHITE;
   mouse_x = mouse_y = 0;
#if 0
PORT1.3
   exposeArea = NULL;
   if (backBuffer == NULL) {
      backBuffer = new Fl_Image ();
   }
#endif
}

FltkViewBase::~FltkViewBase ()
{
   cancelQueueDraw ();
}

void FltkViewBase::setBufferedDrawing (bool b) {
   if (b && backBuffer == NULL) {
#if 0
PORT1.3 
      backBuffer = new Fl_Image ();
#endif
   } else if (!b && backBuffer != NULL) {
      delete backBuffer;
      backBuffer = NULL;
   }
}

void FltkViewBase::draw ()
{
   int d = damage ();

   if ((d & FL_DAMAGE_USER1) && !(d & FL_DAMAGE_EXPOSE)) {
      lout::container::typed::Iterator <core::Rectangle> it;

      for (it = drawRegion.rectangles (); it.hasNext (); ) {
         draw (it.getNext (), DRAW_BUFFERED);
      }

      drawRegion.clear ();
      d &= ~FL_DAMAGE_USER1;
   }

   if (d & FL_DAMAGE_CHILD) {
      drawChildWidgets ();
      d &= ~FL_DAMAGE_CHILD;
   }

   if (d) {
      dw::core::Rectangle rect (
         translateViewXToCanvasX (0),
         translateViewYToCanvasY (0),
         w (),
         h ());

      if (d == FL_DAMAGE_SCROLL) {
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
#if 0
PORT1.3
   int offsetX = 0, offsetY = 0;

   /* fltk-clipping does not use widget coordinates */
   transform (offsetX, offsetY);

   ::fltk::Rectangle viewRect (
      translateCanvasXToViewX (rect->x) + offsetX,
      translateCanvasYToViewY (rect->y) + offsetY,
      rect->width, rect->height);

   ::fltk::intersect_with_clip (viewRect);

   viewRect.x (viewRect.x () - offsetX);
   viewRect.y (viewRect.y () - offsetY);A

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
         fl_push_clip (viewRect);
         setcolor (bgColor);
         fillrect (viewRect);
         theLayout->expose (this, &r);
         fl_pop_clip ();
      } else {
         setcolor (bgColor);
         fillrect (viewRect);
         theLayout->expose (this, &r);
      }

      exposeArea = NULL;
   }
#endif
   core::Rectangle r (rect->x, rect->y, rect->width, rect->height);
   fl_color(bgColor);
   fl_rectf(translateViewXToCanvasX (rect->x),
      translateCanvasYToViewY (rect->y), rect->width, rect->height);
   theLayout->expose (this, &r);
}

void FltkViewBase::drawChildWidgets () {
   for (int i = children () - 1; i >= 0; i--) {
      Fl_Widget& w = *child(i);
#if 0
PORT1.3
      if (w.damage() & DAMAGE_CHILD_LABEL) {
         draw_outside_label(w);
         w.set_damage(w.damage() & ~DAMAGE_CHILD_LABEL);
      }
#endif
      update_child(w);
   }
}

core::ButtonState getDwButtonState ()
{
   int s1 = Fl::event_state ();
   int s2 = (core::ButtonState)0;

   if (s1 & FL_SHIFT)   s2 |= core::SHIFT_MASK;
   if (s1 & FL_CTRL)    s2 |= core::CONTROL_MASK;
   if (s1 & FL_ALT)     s2 |= core::META_MASK;
   if (s1 & FL_BUTTON1) s2 |= core::BUTTON1_MASK;
   if (s1 & FL_BUTTON2) s2 |= core::BUTTON2_MASK;
   if (s1 & FL_BUTTON3) s2 |= core::BUTTON3_MASK;

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
   case FL_PUSH:
      processed =
         theLayout->buttonPress (this, Fl::event_clicks () + 1,
                                 translateViewXToCanvasX (Fl::event_x ()),
                                 translateViewYToCanvasY (Fl::event_y ()),
                                 getDwButtonState (), Fl::event_button ());
      _MSG("PUSH => %s\n", processed ? "true" : "false");
      if (processed) {
         /* pressed dw content; give focus to the view */
         Fl::focus(this);
      }
      return processed ? true : Fl_Group::handle (event);

   case FL_RELEASE:
      processed =
         theLayout->buttonRelease (this, Fl::event_clicks () + 1,
                                   translateViewXToCanvasX (Fl::event_x ()),
                                   translateViewYToCanvasY (Fl::event_y ()),
                                   getDwButtonState (), Fl::event_button ());
      _MSG("RELEASE => %s\n", processed ? "true" : "false");
      return processed ? true : Fl_Group::handle (event);

   case FL_MOVE:
      mouse_x = Fl::event_x();
      mouse_y = Fl::event_y();
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (mouse_x),
                                  translateViewYToCanvasY (mouse_y),
                                  getDwButtonState ());
      _MSG("MOVE => %s\n", processed ? "true" : "false");
      return processed ? true : Fl_Group::handle (event);

   case FL_DRAG:
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (Fl::event_x ()),
                                  translateViewYToCanvasY (Fl::event_y ()),
                                  getDwButtonState ());
      _MSG("DRAG => %s\n", processed ? "true" : "false");
      return processed ? true : Fl_Group::handle (event);

   case FL_ENTER:
      theLayout->enterNotify (this, translateViewXToCanvasX (Fl::event_x ()),
                              translateViewYToCanvasY (Fl::event_y ()),
                              getDwButtonState ());
      return Fl_Group::handle (event);

   case FL_LEAVE:
      theLayout->leaveNotify (this, getDwButtonState ());
      return Fl_Group::handle (event);

   default:
      return Fl_Group::handle (event);
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
   static Fl_Cursor mapDwToFltk[] = {
      FL_CURSOR_CROSS,
      FL_CURSOR_DEFAULT,
      FL_CURSOR_HAND,
      FL_CURSOR_MOVE,
      FL_CURSOR_WE,
      FL_CURSOR_NESW,
      FL_CURSOR_NWSE,
      FL_CURSOR_NS,
      FL_CURSOR_NWSE,
      FL_CURSOR_NESW,
      FL_CURSOR_NS,
      FL_CURSOR_WE,
      FL_CURSOR_INSERT,
      FL_CURSOR_WAIT,
      FL_CURSOR_HELP
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
   fl_cursor (mapDwToFltk[cursor]);
}

void FltkViewBase::setBgColor (core::style::Color *color)
{
   bgColor = color ?
      ((FltkColor*)color)->colors[dw::core::style::Color::SHADING_NORMAL] :
      FL_WHITE;
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
   damage (FL_DAMAGE_USER1);
}

void FltkViewBase::queueDrawTotal ()
{
   damage (FL_DAMAGE_EXPOSE);
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
   fl_color(((FltkColor*)color)->colors[shading]);
   fl_line (translateCanvasXToViewX (x1), translateCanvasYToViewY (y1),
            translateCanvasXToViewX (x2), translateCanvasYToViewY (y2));
}

void FltkViewBase::drawTypedLine (core::style::Color *color,
                                  core::style::Color::Shading shading,
                                  core::style::LineType type, int width,
                                  int x1, int y1, int x2, int y2)
{
   char dashes[3], w, ng, d, gap, len;
   const int f = 2;

   w = (width == 1) ? 0 : width;
   if (type == core::style::LINE_DOTTED) {
      /* customized drawing for dotted lines */
      len = (x2 == x1) ? y2 - y1 + 1 : (y2 == y1) ? x2 - x1 + 1 : 0;
      ng = len / f*width;
      d = len % f*width;
      gap = ng ? d/ng + (w > 3 ? 2 : 0) : 0;
      dashes[0] = 1; dashes[1] = f*width-gap; dashes[2] = 0;
      fl_line_style(FL_DASH + FL_CAP_ROUND, w, dashes);

      /* These formulas also work, but ain't pretty ;)
       * fl_line_style(FL_DOT + FL_CAP_ROUND, w);
       * dashes[0] = 1; dashes[1] = 3*width-2; dashes[2] = 0;
       */
   } else if (type == core::style::LINE_DASHED) {
      fl_line_style(FL_DASH + FL_CAP_ROUND, w);
   }

   fl_color(((FltkColor*)color)->colors[shading]);
   drawLine (color, shading, x1, y1, x2, y2);

   if (type != core::style::LINE_NORMAL)
      fl_line_style(FL_SOLID);
}

void FltkViewBase::drawRectangle (core::style::Color *color,
                                  core::style::Color::Shading shading,
                                  bool filled,
                                  int x, int y, int width, int height)
{
   fl_color(((FltkColor*)color)->colors[shading]);
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

#if 0
PORT1.3
   // We only support rectangles with line width 1px, so we clip with
   // a rectangle 1px wider and higher than what we actually expose.
   // This is only really necessary for non-filled rectangles.
   clipPoint (&x1, &y1, 1);
   clipPoint (&x2, &y2, 1);
#endif

   if (filled)
      fl_rectf (x1, y1, x2 - x1, y2 - y1);
   else
      fl_rect (x1, y1, x2 - x1, y2 - y1);
}

void FltkViewBase::drawArc (core::style::Color *color,
                            core::style::Color::Shading shading, bool filled,
                            int centerX, int centerY, int width, int height,
                            int angle1, int angle2)
{
   fl_color(((FltkColor*)color)->colors[shading]);
#if 0
PORT1.3
   int x = translateCanvasXToViewX (centerX) - width / 2;
   int y = translateCanvasYToViewY (centerY) - height / 2;
   ::fltk::Rectangle rect (x, y, width, height);
   addchord(rect, angle1, angle2);
   closepath();
   if (filled)
      fillpath();
   else
      strokepath();
#endif
}

void FltkViewBase::drawPolygon (core::style::Color *color,
                                core::style::Color::Shading shading,
                                bool filled, int points[][2], int npoints)
{
#if 0
PORT1.3
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
#endif
}

core::View *FltkViewBase::getClippingView (int x, int y, int width, int height)
{
   fl_push_clip (translateCanvasXToViewX (x), translateCanvasYToViewY (y),
                 width, height);
   return this;
}

void FltkViewBase::mergeClippingView (core::View *clippingView)
{
   fl_pop_clip ();
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
#if 0
PORT1.3
   /**
    * pass layout to child widgets. This is needed for complex fltk
    * widgets as TextEditor.
    * We can't use Group::layout() as that would rearrange the widgets.
    */
   for (int i = children () - 1; i >= 0; i--) {
      Fl_Widget *widget = child (i);

      if (widget->layout_damage ()) {
         widget->layout ();
      }
   }
#endif
}

void FltkWidgetView::drawText (core::style::Font *font,
                             core::style::Color *color,
                             core::style::Color::Shading shading,
                             int x, int y, const char *text, int len)
{
   FltkFont *ff = (FltkFont*)font;
   fl_font(ff->font, ff->size);
   fl_color(((FltkColor*)color)->colors[shading]);

   if (!font->letterSpacing && !font->fontVariant) {
      fl_draw(text, len,
              translateCanvasXToViewX (x), translateCanvasYToViewY (y));
   } else {
      /* Nonzero letter spacing adjustment, draw each glyph individually */
      int viewX = translateCanvasXToViewX (x),
          viewY = translateCanvasYToViewY (y);
      int curr = 0, next = 0, nb;
      char chbuf[4];
      wchar_t wc, wcu;

      if (font->fontVariant == 1) {
         int sc_fontsize = lout::misc::roundInt(ff->size * 0.78);
         for (curr = 0; next < len; curr = next) {
            next = theLayout->nextGlyph(text, curr);
            wc = fl_utf8decode(text + curr, text + next, &nb);
            if ((wcu = towupper(wc)) == wc) {
               /* already uppercase, just draw the character */
               fl_font(ff->font, ff->size);
               fl_draw(text + curr, next - curr, viewX, viewY);
               viewX += font->letterSpacing;
               viewX += (int)fl_width(text + curr, next - curr);
            } else {
#if 0
PORT1.3
               /* make utf8 string for converted char */
               nb = utf8encode(wcu, chbuf);
#endif
               fl_font(ff->font, sc_fontsize);
               fl_draw(chbuf, nb, viewX, viewY);
               viewX += font->letterSpacing;
               viewX += (int)fl_width(chbuf, nb);
            }
         }
      } else {
         while (next < len) {
            next = theLayout->nextGlyph(text, curr);
            fl_draw(text + curr, next - curr, viewX, viewY);
            viewX += font->letterSpacing +
                     (int)fl_width(text + curr,next - curr);
            curr = next;
         }
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

void FltkWidgetView::addFltkWidget (Fl_Widget *widget,
                                    core::Allocation *allocation)
{
   allocateFltkWidget (widget, allocation);
   add (widget);
}

void FltkWidgetView::removeFltkWidget (Fl_Widget *widget)
{
   remove (widget);
}

void FltkWidgetView::allocateFltkWidget (Fl_Widget *widget,
                                       core::Allocation *allocation)
{
   widget->resize (translateCanvasXToViewX (allocation->x),
      translateCanvasYToViewY (allocation->y),
      allocation->width,
      allocation->ascent + allocation->descent);

#if 0
PORT1.3
   /* widgets created tiny and later resized need this flag to display */
   uchar damage = widget->layout_damage ();
   damage |= LAYOUT_XYWH;
   widget->layout_damage (damage);
#endif
}

void FltkWidgetView::drawFltkWidget (Fl_Widget *widget,
                                   core::Rectangle *area)
{
   draw_child (*widget);
}

} // namespace fltk
} // namespace dw
