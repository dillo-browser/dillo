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



#include "listitem.hh"
#include <stdio.h>

namespace dw {

int ListItem::CLASS_ID = -1;

ListItem::ListItem (ListItem *ref, bool limitTextWidth):
   AlignedTextblock (limitTextWidth)
{
   registerName ("dw::ListItem", &CLASS_ID);
   setRefTextblock (ref);
}

ListItem::~ListItem()
{
}

void ListItem::initWithWidget (core::Widget *widget,
                                core::style::Style *style)
{
   addWidget (widget, style);
   addSpace (style);
   updateValue ();
}

void ListItem::initWithText (char *text, core::style::Style *style)
{
   addText (text, style);
   addSpace (style); 
   updateValue ();
}

int ListItem::getValue ()
{
   if (words->size () == 0)
      return 0;
   else
      return words->get(0).size.width + words->get(0).origSpace;
}

void ListItem::setMaxValue (int maxValue, int value)
{
   innerPadding = maxValue;
   line1Offset = - value;
   redrawY = 0;
   queueResize (0, true);
}

} // namespace dw
