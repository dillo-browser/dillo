/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
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



#include "ruler.hh"
#include "../lout/misc.hh"

#include <stdio.h>

namespace dw {

int Ruler::CLASS_ID = -1;

Ruler::Ruler ()
{
   DBG_OBJ_CREATE ("dw::Ruler");
   registerName ("dw::Ruler", &CLASS_ID);
}

Ruler::~Ruler ()
{
   DBG_OBJ_DELETE ();
}

void Ruler::sizeRequestSimpl (core::Requisition *requisition)
{
   /* The ruler will be drawn by using a 1px border, so we substract the
    * border from the available width when computing the content width. */
   int w = lout::misc::max(0, getAvailWidth(true) - boxDiffWidth());
   requisition->width = w;
   requisition->ascent = boxOffsetY ();
   requisition->descent = boxRestHeight ();
}

void Ruler::getExtremesSimpl (core::Extremes *extremes)
{
   extremes->minWidth = extremes->maxWidth = boxDiffWidth ();
   extremes->minWidthIntrinsic = extremes->minWidth;
   extremes->maxWidthIntrinsic = extremes->maxWidth;
   correctExtremes (extremes, false);
   extremes->adjustmentWidth =
      lout::misc::min (extremes->minWidthIntrinsic, extremes->minWidth);
}

bool Ruler::isBlockLevel ()
{
   return true;
}

void Ruler::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");
   // Nothing to do.
   DBG_OBJ_LEAVE ();
}

bool Ruler::usesAvailWidth ()
{
   return true;
}

void Ruler::draw (core::View *view, core::Rectangle *area,
                  core::DrawingContext *context)
{
   drawWidgetBox (view, area, false);
}

core::Widget *Ruler::getWidgetAtPoint (int x, int y,
                                       core::GettingWidgetAtPointContext
                                       *context)
{
   // Override (complex) implementation OOFAwareWidget::getWidgetAtPoint().

   if (inAllocation (x, y))
      return this;
   else
      return NULL;
}

core::Iterator *Ruler::iterator (core::Content::Type mask, bool atEnd)
{
   /** \todo TextIterator? */
   return new core::EmptyIterator (this, mask, atEnd);
}

} // namespace dw
