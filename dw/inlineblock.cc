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



#include "../lout/misc.hh"
#include "inlineblock.hh"
#include <stdio.h>

namespace dw {

int InlineBlock::CLASS_ID = -1;

InlineBlock::InlineBlock (bool limitTextWidth):
  Textblock (limitTextWidth)
{
   registerName ("dw::InlineBlock", &CLASS_ID);
   unsetFlags (BLOCK_LEVEL);
}

InlineBlock::~InlineBlock()
{
}

void InlineBlock::sizeRequestImpl (core::Requisition *requisition)
{
   rewrap ();
   if (lines->size () > 0) {
      Line *lastLine = lines->getRef (lines->size () - 1);
      requisition->width =
         lout::misc::max (lastLine->maxLineWidth, lastLineWidth);
      /* Note: the breakSpace of the last line is ignored, so breaks
         at the end of a textblock are not visible. */
      requisition->ascent = lines->getRef(0)->boxAscent;
      requisition->descent = lastLine->top
         + lastLine->boxAscent + lastLine->boxDescent -
         lines->getRef(0)->boxAscent;
   } else {
      requisition->width = lastLineWidth;
      requisition->ascent = 0;
      requisition->descent = 0;
   }

   requisition->width += innerPadding + getStyle()->boxDiffWidth ();
   requisition->ascent += getStyle()->boxOffsetY ();
   requisition->descent += getStyle()->boxRestHeight ();
}


} // namespace dw
