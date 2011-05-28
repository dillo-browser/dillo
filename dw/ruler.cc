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



#include "ruler.hh"
#include "../lout/misc.hh"

#include <stdio.h>

namespace dw {

Ruler::Ruler ()
{
   setFlags (BLOCK_LEVEL);
   unsetFlags (HAS_CONTENTS);
}

void Ruler::sizeRequestImpl (core::Requisition *requisition)
{
   requisition->width = getStyle()->boxDiffWidth ();
   requisition->ascent = getStyle()->boxOffsetY ();
   requisition->descent = getStyle()->boxRestHeight ();
}

void Ruler::draw (core::View *view, core::Rectangle *area)
{
   drawWidgetBox (view, area, false);
}

core::Iterator *Ruler::iterator (core::Content::Type mask, bool atEnd)
{
   /** \todo TextIterator? */
   return new core::EmptyIterator (this, mask, atEnd);
}

} // namespace dw
