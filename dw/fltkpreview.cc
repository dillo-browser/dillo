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

#include "../lout/msg.h"

#include "fltkpreview.hh"
#include "fltkmisc.hh"

#include <FL/Fl.H>
#include <FL/Fl_Bitmap.H>
#include <FL/fl_draw.H>
#include <stdio.h>

#include "preview.xbm"

namespace dw {
namespace fltk {

FltkPreview::FltkPreview (int x, int y, int w, int h,
                          dw::core::Layout *layout, const char *label):
   FltkViewBase (x, y, w, h, label)
{
   layout->attachView (this);

   scrollX = 0;
   scrollY = 0;
   scrollWidth = 1;
   scrollHeight = 1;
}

FltkPreview::~FltkPreview ()
{
}

int FltkPreview::handle (int event)
{
   return FltkViewBase::handle (event);
}

int FltkPreview::translateViewXToCanvasX (int x)
{
   return x * canvasWidth / w ();
}

int FltkPreview::translateViewYToCanvasY (int y)
{
   return y * canvasHeight / h ();
}

int FltkPreview::translateCanvasXToViewX (int x)
{
   return x * w () / canvasWidth;
}

int FltkPreview::translateCanvasYToViewY (int y)
{
   return y * h () / canvasHeight;
}

void FltkPreview::setCanvasSize (int width, int ascent, int descent)
{
   FltkViewBase::setCanvasSize (width, ascent, descent);
   if (parent() && parent()->visible ())
      ((FltkPreviewWindow*)parent())->reallocate ();
}

bool FltkPreview::usesViewport ()
{
   return true;
}

int FltkPreview::getHScrollbarThickness ()
{
   return 0;
}

int FltkPreview::getVScrollbarThickness ()
{
   return 0;
}

void FltkPreview::scrollTo (int x, int y)
{
   scrollX = x;
   scrollY = y;
}

void FltkPreview::scroll (dw::core::ScrollCommand cmd)
{
   MSG_ERR("FltkPreview::scroll not implemented\n");
}

void FltkPreview::setViewportSize (int width, int height,
                                   int hScrollbarThickness,
                                   int vScrollbarThickness)
{
   scrollWidth = width - vScrollbarThickness;
   scrollHeight = height - hScrollbarThickness;
}

void FltkPreview::drawText (core::style::Font *font,
                            core::style::Color *color,
                            core::style::Color::Shading shading,
                            int x, int y, const char *text, int len)
{
   /*
    * We must call setfont() before calling getwidth() (or anything
    * else that measures text).
    */
   FltkFont *ff = (FltkFont*)font;
   Fl::set_font(ff->font, translateCanvasXToViewX (ff->size));
#if 0
   /**
    * \todo Normally, this should already be known, maybe it
    * should be passed?
    */
   int width = (int)getwidth (text, len);
   int height = font->ascent; // No descent, this would look to "bold".

   int x1 = translateCanvasXToViewX (x);
   int y1 = translateCanvasYToViewY (y);
   int x2 = translateCanvasXToViewX (x + width);
   int y2 = translateCanvasYToViewY (y + height);
   Rectangle rect (x1, y1, x2 - x1, y2 - y1);

   setcolor(((FltkColor*)color)->colors[shading]);
   fillrect (rect);
#endif
   fl_color(((FltkColor*)color)->colors[shading]);
   fl_draw(text, len, translateCanvasXToViewX (x), translateCanvasYToViewY(y));
}

void FltkPreview::drawSimpleWrappedText (core::style::Font *font,
                                         core::style::Color *color,
                                         core::style::Color::Shading shading,
                                         int x, int y, int w, int h,
                                         const char *text)
{
}

void FltkPreview::drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                int x, int y, int width, int height)
{
}

bool FltkPreview::usesFltkWidgets ()
{
   return false;
}

void FltkPreview::drawFltkWidget (Fl_Widget *widget,
                                  core::Rectangle *area)
{
}

// ----------------------------------------------------------------------

FltkPreviewWindow::FltkPreviewWindow (dw::core::Layout *layout):
   Fl_Menu_Window (1, 1)
{
   box (FL_EMBOSSED_BOX);

   begin ();
   preview = new FltkPreview (BORDER_WIDTH, BORDER_WIDTH, 1, 1, layout);
   end ();

   hide ();
}

FltkPreviewWindow::~FltkPreviewWindow ()
{
}

void FltkPreviewWindow::showWindow ()
{
   reallocate ();
   show ();
}

void FltkPreviewWindow::reallocate ()
{
   int maxWidth = misc::screenWidth () / 2;
   int maxHeight = misc::screenHeight () * 4 / 5;
   int mx, my, width, height;
   bool warp = false;

   if (preview->canvasHeight * maxWidth > maxHeight * preview->canvasWidth) {
      // Expand to maximal height (most likely case).
      width = preview->canvasWidth * maxHeight / preview->canvasHeight;
      height = maxHeight;
   } else {
      // Expand to maximal width.
      width = maxWidth;
      height = preview->canvasHeight * maxWidth / preview->canvasWidth;
   }

   Fl::get_mouse(mx, my);

   posX = mx - preview->translateCanvasXToViewX (preview->scrollX
                                                 + preview->scrollWidth / 2);
   posY = my - preview->translateCanvasYToViewY (preview->scrollY
                                                 + preview->scrollHeight / 2);

   if (posX < 0) {
      mx -= posX;
      posX = 0;
      warp = true;
   } else if (posX + width > misc::screenWidth ()) {
      mx -= (posX - (misc::screenWidth () - width));
      posX = misc::screenWidth () - width;
      warp = true;
   }

   if (posY < 0) {
      my -= posY;
      posY = 0;
      warp = true;
   } else if (posY + height > misc::screenHeight ()) {
      my -= (posY - (misc::screenHeight () - height));
      posY = misc::screenHeight () - height;
      warp = true;
   }

   if (warp)
      misc::warpPointer (mx, my);

   resize (posX, posY, width, height);

   preview->size(w () - 2 * BORDER_WIDTH, h () - 2 * BORDER_WIDTH);
}

void FltkPreviewWindow::hideWindow ()
{
   Fl_Window::hide ();
}

void FltkPreviewWindow::scrollTo (int mouseX, int mouseY)
{
   preview->scrollX =
      preview->translateViewXToCanvasX (mouseX - posX - BORDER_WIDTH)
      - preview->scrollWidth / 2;
   preview->scrollY =
      preview->translateViewYToCanvasY (mouseY - posY - BORDER_WIDTH)
      - preview->scrollHeight / 2;
   preview->theLayout->scrollPosChanged (preview,
                                         preview->scrollX, preview->scrollY);
}

// ----------------------------------------------------------------------

FltkPreviewButton::FltkPreviewButton (int x, int y, int w, int h,
                                      dw::core::Layout *layout,
                                      const char *label):
   Fl_Button (x, y, w, h, label)
{
   image (new Fl_Bitmap (preview_bits, preview_width, preview_height));
   window = new FltkPreviewWindow (layout);
}

FltkPreviewButton::~FltkPreviewButton ()
{
}

int FltkPreviewButton::handle (int event)
{
   /** \bug Some parts are missing. */

   switch (event) {
   case FL_PUSH:
      window->showWindow ();
      return Fl_Button::handle (event);

   case FL_DRAG:
      if (window->visible ()) {
         window->scrollTo (Fl::event_x_root (), Fl::event_y_root ());
         return 1;
      }
      return Fl_Button::handle (event);

   case FL_RELEASE:
      window->hideWindow ();
      return Fl_Button::handle (event);

   default:
      return Fl_Button::handle (event);
   }
}

} // namespace fltk
} // namespace dw
