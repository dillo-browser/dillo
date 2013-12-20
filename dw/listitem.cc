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



#include "listitem.hh"
#include "../lout/debug.hh"
#include <stdio.h>

namespace dw {

int ListItem::CLASS_ID = -1;

ListItem::ListItem (ListItem *ref, bool limitTextWidth):
   AlignedTextblock (limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::ListItem");
   registerName ("dw::ListItem", &CLASS_ID);
   setRefTextblock (ref);
}

ListItem::~ListItem()
{
   DBG_OBJ_DELETE ();
}

void ListItem::initWithWidget (core::Widget *widget,
                                core::style::Style *style)
{
   hasListitemValue = true;
   addWidget (widget, style);
   addSpace (style);
   if (style->listStylePosition == core::style::LIST_STYLE_POSITION_OUTSIDE)
      updateValue ();
}

void ListItem::initWithText (const char *text, core::style::Style *style)
{
   hasListitemValue = true;
   addText (text, style);
   addSpace (style);
   if (style->listStylePosition == core::style::LIST_STYLE_POSITION_OUTSIDE)
      updateValue ();
}

int ListItem::getValue ()
{
   if (words->size () == 0)
      return 0;
   else
      return words->getRef(0)->size.width + words->getRef(0)->origSpace;
}

void ListItem::setMaxValue (int maxValue, int value)
{
   innerPadding = maxValue;
   line1Offset = - value;
   redrawY = 0;
   queueResize (0, true);
}

} // namespace dw
