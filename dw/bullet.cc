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



#include "bullet.hh"

#include <stdio.h>

namespace dw {

Bullet::Bullet ()
{
}

void Bullet::sizeRequestImpl (core::Requisition *requisition)
{
   requisition->width = lout::misc::max (getStyle()->font->xHeight * 4 / 5, 1);
   requisition->ascent = lout::misc::max (getStyle()->font->xHeight, 1);
   requisition->descent = 0;
}

void Bullet::draw (core::View *view, core::Rectangle *area)
{
   int x, y, l;
   bool filled = true;

   l = lout::misc::min (allocation.width, allocation.ascent);
   x = allocation.x;
   y = allocation.y + allocation.ascent - getStyle()->font->xHeight;

   switch (getStyle()->listStyleType) {
   case core::style::LIST_STYLE_TYPE_SQUARE:
      view->drawRectangle (getStyle()->color,
                           core::style::Color::SHADING_NORMAL,
                           false, x, y, l, l);
      break;
   case core::style::LIST_STYLE_TYPE_CIRCLE:
      filled = false;
      // Fall Through
   case core::style::LIST_STYLE_TYPE_DISC:
   default:
      view->drawArc (getStyle()->color, core::style::Color::SHADING_NORMAL,
                     filled, x + l/2, y + l/2, l, l, 0, 360);
   }
}

core::Iterator *Bullet::iterator (core::Content::Type mask, bool atEnd)
{
   //return new core::TextIterator (this, mask, atEnd, "*");
   /** \bug Not implemented. */
   return new core::EmptyIterator (this, mask, atEnd);
}

} // namespace dw
