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
#include "../lout/msg.h"

using namespace lout::object;
using namespace lout::container::typed;

namespace dw {
namespace fltk {

FltkViewBase::BackBuffer::BackBuffer ()
{
   w = 0;
   h = 0;
   created = false;
}

FltkViewBase::BackBuffer::~BackBuffer ()
{
   if (created)
      fl_delete_offscreen (offscreen);
}

void FltkViewBase::BackBuffer::setSize (int w, int h)
{
   if (!created || w > this->w || h > this->h) {
      this->w = w;
      this->h = h;
      if (created)
         fl_delete_offscreen (offscreen);
      offscreen = fl_create_offscreen (w, h);
      created = true;
   }
}

FltkViewBase::BackBuffer *FltkViewBase::backBuffer;
bool FltkViewBase::backBufferInUse;

FltkViewBase::FltkViewBase (int x, int y, int w, int h, const char *label):
   Fl_Group (x, y, w, h, label)
{
   Fl_Group::current(0);
   canvasWidth = 1;
   canvasHeight = 1;
   bgColor = FL_WHITE;
   mouse_x = mouse_y = 0;
   focused_child = NULL;
   exposeArea = NULL;
   if (backBuffer == NULL) {
      backBuffer = new BackBuffer ();
   }
   box(FL_NO_BOX);
   resizable(NULL);
}

FltkViewBase::~FltkViewBase ()
{
   cancelQueueDraw ();
}

void FltkViewBase::setBufferedDrawing (bool b) {
   if (b && backBuffer == NULL) {
      backBuffer = new BackBuffer ();
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
         translateViewXToCanvasX (x ()),
         translateViewYToCanvasY (y ()),
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
   int X = translateCanvasXToViewX (rect->x);
   int Y = translateCanvasYToViewY (rect->y);
   int W, H;

   // fl_clip_box() can't handle values greater than SHRT_MAX!
   if (X > x () + w () || Y > y () + h ())
      return;

   W = X + rect->width > x () + w () ? x () + w () - X : rect->width;
   H = Y + rect->height > y () + h () ? y () + h () - Y : rect->height;

   fl_clip_box(X, Y, W, H, X, Y, W, H);

   core::Rectangle r (translateViewXToCanvasX (X),
                      translateViewYToCanvasY (Y), W, H);

   if (r.isEmpty ())
      return;

   exposeArea = &r;

   if (type == DRAW_BUFFERED && backBuffer && !backBufferInUse) {
      backBufferInUse = true;
      backBuffer->setSize (X + W, Y + H); // would be nicer to use (W, H)...
      fl_begin_offscreen (backBuffer->offscreen);
      fl_push_matrix ();
      fl_color (bgColor);
      fl_rectf (X, Y, W, H);
      theLayout->expose (this, &r);
      fl_pop_matrix ();
      fl_end_offscreen ();
      fl_copy_offscreen (X, Y, W, H, backBuffer->offscreen, X, Y);
      backBufferInUse = false;
   } else if (type == DRAW_BUFFERED || type == DRAW_CLIPPED) {
      // if type == DRAW_BUFFERED but we do not have backBuffer available
      // we fall back to clipped drawing
      fl_push_clip (X, Y, W, H);
      fl_color (bgColor);
      fl_rectf (X, Y, W, H);
      theLayout->expose (this, &r);
      fl_pop_clip ();
   } else {
      fl_color (bgColor);
      fl_rectf (X, Y, W, H);
      theLayout->expose (this, &r);
   }
   // DEBUG:
   //fl_color(FL_RED);
   //fl_rect(X, Y, W, H);

   exposeArea = NULL;
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

/*
 * We handle Tab to determine which FLTK widget should get focus.
 *
 * Presumably a proper solution that allows focusing links, etc., would live
 * in Textblock and use iterators.
 */
int FltkViewBase::manageTabToFocus()
{
   int i, ret = 0;
   Fl_Widget *old_child = NULL;

   if (this == Fl::focus()) {
      // if we have focus, give it to a child. Go forward typically,
      // or backward with Shift pressed.
      if (!(Fl::event_state() & FL_SHIFT)) {
         for (i = 0; i < children(); i++) {
            if (child(i)->take_focus()) {
               ret = 1;
               break;
            }
         }
      } else {
         for (i = children() - 1; i >= 0; i--) {
            if (child(i)->take_focus()) {
               ret = 1;
               break;
            }
         }
      }
   } else {
      // tabbing between children
      old_child = Fl::focus();

      if (!(ret = Fl_Group::handle (FL_KEYBOARD))) {
         // group didn't have any more children to focus.
         Fl::focus(this);
         return 1;
      } else {
         // which one did it focus? (Note i == children() if not found)
         i = find(Fl::focus());
      }
   }
   if (ret) {
      if (i >= 0 && i < children()) {
         Fl_Widget *c = child(i);
         int canvasX = translateViewXToCanvasX(c->x()),
             canvasY = translateViewYToCanvasY(c->y());

         theLayout->scrollTo(core::HPOS_INTO_VIEW, core::VPOS_INTO_VIEW,
                             canvasX, canvasY, c->w(), c->h());

         // Draw the children who gained and lost focus. Otherwise a
         // widget that had been only partly visible still shows its old
         // appearance in the previously-visible portion.
         core::Rectangle r(canvasX, canvasY, c->w(), c->h());

         queueDraw(&r);

         if (old_child) {
            r.x = translateViewXToCanvasX(old_child->x());
            r.y = translateViewYToCanvasY(old_child->y());
            r.width = old_child->w();
            r.height = old_child->h();
            queueDraw(&r);
         }
      }
   }
   return ret;
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
      /* Hide the tooltip */
      theLayout->cancelTooltip();

      processed =
         theLayout->buttonPress (this, Fl::event_clicks () + 1,
                                 translateViewXToCanvasX (Fl::event_x ()),
                                 translateViewYToCanvasY (Fl::event_y ()),
                                 getDwButtonState (), Fl::event_button ());
      _MSG("PUSH => %s\n", processed ? "true" : "false");
      if (processed) {
         /* pressed dw content; give focus to the view */
         if (Fl::event_button() != FL_RIGHT_MOUSE)
            Fl::focus(this);
         return true;
      }
      break;
   case FL_RELEASE:
      processed =
         theLayout->buttonRelease (this, Fl::event_clicks () + 1,
                                   translateViewXToCanvasX (Fl::event_x ()),
                                   translateViewYToCanvasY (Fl::event_y ()),
                                   getDwButtonState (), Fl::event_button ());
      _MSG("RELEASE => %s\n", processed ? "true" : "false");
      if (processed)
         return true;
      break;
   case FL_MOVE:
      mouse_x = Fl::event_x();
      mouse_y = Fl::event_y();
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (mouse_x),
                                  translateViewYToCanvasY (mouse_y),
                                  getDwButtonState ());
      _MSG("MOVE => %s\n", processed ? "true" : "false");
      if (processed)
         return true;
      break;
   case FL_DRAG:
      processed =
         theLayout->motionNotify (this,
                                  translateViewXToCanvasX (Fl::event_x ()),
                                  translateViewYToCanvasY (Fl::event_y ()),
                                  getDwButtonState ());
      _MSG("DRAG => %s\n", processed ? "true" : "false");
      if (processed)
         return true;
      break;
   case FL_ENTER:
      theLayout->enterNotify (this,
                              translateViewXToCanvasX (Fl::event_x ()),
                              translateViewYToCanvasY (Fl::event_y ()),
                              getDwButtonState ());
      break;
   case FL_HIDE:
      /* WORKAROUND: strangely, the tooltip window is not automatically hidden
       * with its parent. Here we fake a LEAVE to achieve it. */
   case FL_LEAVE:
      theLayout->leaveNotify (this, getDwButtonState ());
      break;
   case FL_FOCUS:
      if (focused_child && find(focused_child) < children()) {
         /* strangely, find() == children() if the child is not found */
         focused_child->take_focus();
      }
      return 1;
   case FL_UNFOCUS:
      // FLTK delivers UNFOCUS to the previously focused widget
      if (find(Fl::focus()) < children())
         focused_child = Fl::focus(); // remember the focused child!
      else if (Fl::focus() == this)
         focused_child = NULL; // no focused child this time
      return 0;
   case FL_KEYBOARD:
      if (Fl::event_key() == FL_Tab)
         return manageTabToFocus();
      break;
   default:
      break;
   }
   return Fl_Group::handle (event);
}

// ----------------------------------------------------------------------

void FltkViewBase::setLayout (core::Layout *layout)
{
   theLayout = layout;
   if (usesViewport())
      theLayout->viewportSizeChanged(this, w(), h());
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
   // we clip with a large border (5000px), as clipping causes artefacts
   // with non-solid line styles.
   // However it's still better than no clipping at all.
   clipPoint (&x1, &y1, 5000);
   clipPoint (&x2, &y2, 5000);
   fl_line (translateCanvasXToViewX (x1),
            translateCanvasYToViewY (y1),
            translateCanvasXToViewX (x2),
            translateCanvasYToViewY (y2));
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
                                  int X, int Y, int width, int height)
{
   fl_color(((FltkColor*)color)->colors[shading]);
   if (width < 0) {
      X += width;
      width = -width;
   }
   if (height < 0) {
      Y += height;
      height = -height;
   }

   int x1 = X;
   int y1 = Y;
   int x2 = X + width;
   int y2 = Y + height;

   // We only support rectangles with line width 1px, so we clip with
   // a rectangle 1px wider and higher than what we actually expose.
   // This is only really necessary for non-filled rectangles.
   clipPoint (&x1, &y1, 1);
   clipPoint (&x2, &y2, 1);

   x1 = translateCanvasXToViewX (x1);
   y1 = translateCanvasYToViewY (y1);
   x2 = translateCanvasXToViewX (x2);
   y2 = translateCanvasYToViewY (y2);

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
   int x = translateCanvasXToViewX (centerX) - width / 2;
   int y = translateCanvasYToViewY (centerY) - height / 2;

   fl_arc(x, y, width, height, angle1, angle2);
   if (filled) {
      // WORKAROUND: We call both fl_arc and fl_pie due to a FLTK bug
      // (STR #2703) that was present in 1.3.0.
      fl_pie(x, y, width, height, angle1, angle2);
   }
}

void FltkViewBase::drawPolygon (core::style::Color *color,
                                core::style::Color::Shading shading,
                                bool filled, bool convex, core::Point *points,
                                int npoints)
{
   if (npoints > 0) {
      fl_color(((FltkColor*)color)->colors[shading]);

      if (filled) {
         if (convex)
            fl_begin_polygon();
         else
            fl_begin_complex_polygon();
      } else
         fl_begin_loop();

      for (int i = 0; i < npoints; i++) {
         fl_vertex(translateCanvasXToViewX(points[i].x),
                   translateCanvasYToViewY(points[i].y));
      }
      if (filled) {
         if (convex)
            fl_end_polygon();
         else
            fl_end_complex_polygon();
      } else
         fl_end_loop();
   }
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

void FltkWidgetView::drawText (core::style::Font *font,
                               core::style::Color *color,
                               core::style::Color::Shading shading,
                               int X, int Y, const char *text, int len)
{
   //printf ("drawText (..., %d, %d, '", X, Y);
   //for (int i = 0; i < len; i++)
   //   putchar (text[i]);
   //printf ("'\n");

   FltkFont *ff = (FltkFont*)font;
   fl_font(ff->font, ff->size);
   fl_color(((FltkColor*)color)->colors[shading]);

   if (!font->letterSpacing && !font->fontVariant) {
      fl_draw(text, len,
              translateCanvasXToViewX (X), translateCanvasYToViewY (Y));
   } else {
      /* Nonzero letter spacing adjustment, draw each glyph individually */
      int viewX = translateCanvasXToViewX (X),
          viewY = translateCanvasYToViewY (Y);
      int curr = 0, next = 0, nb;
      char chbuf[4];
      int c, cu, width;

      if (font->fontVariant == core::style::FONT_VARIANT_SMALL_CAPS) {
         int sc_fontsize = lout::misc::roundInt(ff->size * 0.78);
         for (curr = 0; next < len; curr = next) {
            next = theLayout->nextGlyph(text, curr);
            c = fl_utf8decode(text + curr, text + next, &nb);
            if ((cu = fl_toupper(c)) == c) {
               /* already uppercase, just draw the character */
               fl_font(ff->font, ff->size);
               width = (int)fl_width(text + curr, next - curr);
               if (curr && width)
                  viewX += font->letterSpacing;
               fl_draw(text + curr, next - curr, viewX, viewY);
               viewX += width;
            } else {
               /* make utf8 string for converted char */
               nb = fl_utf8encode(cu, chbuf);
               fl_font(ff->font, sc_fontsize);
               width = (int)fl_width(chbuf, nb);
               if (curr && width)
                  viewX += font->letterSpacing;
               fl_draw(chbuf, nb, viewX, viewY);
               viewX += width;
            }
         }
      } else {
         while (next < len) {
            next = theLayout->nextGlyph(text, curr);
            width = (int)fl_width(text + curr, next - curr);
            if (curr && width)
               viewX += font->letterSpacing;
            fl_draw(text + curr, next - curr, viewX, viewY);
            viewX += width;
            curr = next;
         }
      }
   }
}

/*
 * "simple" in that it ignores letter-spacing, etc. This was added for image
 * alt text where none of that matters.
 */
void FltkWidgetView::drawSimpleWrappedText (core::style::Font *font,
                                            core::style::Color *color,
                                           core::style::Color::Shading shading,
                                            int X, int Y, int W, int H,
                                            const char *text)
{
   FltkFont *ff = (FltkFont*)font;
   fl_font(ff->font, ff->size);
   fl_color(((FltkColor*)color)->colors[shading]);
   fl_draw(text,
           translateCanvasXToViewX (X), translateCanvasYToViewY (Y),
           W, H, FL_ALIGN_TOP|FL_ALIGN_LEFT|FL_ALIGN_WRAP, NULL, 0);
}

void FltkWidgetView::drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                              int X, int Y, int width, int height)
{
   ((FltkImgbuf*)imgbuf)->draw (this,
                                translateCanvasXToViewX (xRoot),
                                translateCanvasYToViewY (yRoot),
                                X, Y, width, height);
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
}

void FltkWidgetView::drawFltkWidget (Fl_Widget *widget,
                                   core::Rectangle *area)
{
   draw_child (*widget);
   draw_outside_label(*widget);
}

} // namespace fltk
} // namespace dw
