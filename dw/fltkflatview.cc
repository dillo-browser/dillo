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



#include "fltkflatview.hh"
#include "../lout/debug.hh"

#include <stdio.h>

using namespace lout::container::typed;

namespace dw {
namespace fltk {

FltkFlatView::FltkFlatView (int x, int y, int w, int h, const char *label):
   FltkWidgetView (x, y, w, h, label)
{
   DBG_OBJ_CREATE ("dw::fltk::FltkFlatView");
}

FltkFlatView::~FltkFlatView ()
{
}

void FltkFlatView::setCanvasSize (int width, int ascent, int descent)
{
   /**
    * \bug It has to be clarified, who is responsible for setting the
    *      FLTK widget size. In the only used context (complex buttons),
    *      it is done elsewhere.
    */

#if 0
   FltkWidgetView::setCanvasSize (width, ascent, descent);

   w (width);
   h (ascent + descent);
#endif
}

bool FltkFlatView::usesViewport ()
{
   return false;
}

int FltkFlatView::getHScrollbarThickness ()
{
   return 0;
}

int FltkFlatView::getVScrollbarThickness ()
{
   return 0;
}

void FltkFlatView::scrollTo (int x, int y)
{
}

void FltkFlatView::setViewportSize (int width, int height,
                                    int hScrollbarThickness,
                                    int vScrollbarThickness)
{
}

int FltkFlatView::translateViewXToCanvasX (int X)
{
   return X - x ();
}

int FltkFlatView::translateViewYToCanvasY (int Y)
{
   return Y - y ();
}

int FltkFlatView::translateCanvasXToViewX (int X)
{
   return X + x ();
}

int FltkFlatView::translateCanvasYToViewY (int Y)
{
   return Y + y ();
}


} // namespace fltk
} // namespace dw
