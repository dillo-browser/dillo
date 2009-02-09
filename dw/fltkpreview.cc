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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include "fltkpreview.hh"
#include "fltkmisc.hh"

#include <fltk/events.h>
#include <fltk/xbmImage.h>
#include <fltk/draw.h>
#include <stdio.h>

#include "preview.xbm"

using namespace ::fltk;

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
   setfont(ff->font, translateCanvasXToViewX (ff->size));
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
   setcolor(((FltkColor*)color)->colors[shading]);
   drawtext(text, len,
            translateCanvasXToViewX (x), translateCanvasYToViewY (y));
}

void FltkPreview::drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                int x, int y, int width, int height)
{
}

bool FltkPreview::usesFltkWidgets ()
{
   return false;
}

void FltkPreview::drawFltkWidget (Widget *widget,
                                  core::Rectangle *area)
{
}

// ----------------------------------------------------------------------

FltkPreviewWindow::FltkPreviewWindow (dw::core::Layout *layout):
   MenuWindow (1, 1)
{
   box (EMBOSSED_BOX);

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

   get_mouse(mx, my);

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

   preview->w (w () - 2 * BORDER_WIDTH);
   preview->h (h () - 2 * BORDER_WIDTH);
}

void FltkPreviewWindow::hideWindow ()
{
   Window::hide ();
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
   Button (x, y, w, h, label)
{
   image (new xbmImage (preview_bits, preview_width, preview_height));
   window = new FltkPreviewWindow (layout);
}

FltkPreviewButton::~FltkPreviewButton ()
{
}

int FltkPreviewButton::handle (int event)
{
   /** \bug Some parts are missing. */

   switch (event) {
   case PUSH:
      window->showWindow ();
      return Button::handle (event);

   case DRAG:
      if (window->visible ()) {
         window->scrollTo (event_x_root (), event_y_root ());
         return 1;
      }
      return Button::handle (event);

   case RELEASE:
      window->hideWindow ();
      return Button::handle (event);
    
   default:
      return Button::handle (event);
   }
}

} // namespace fltk
} // namespace dw
