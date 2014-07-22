/*
 * Dillo Widget
 *
 * Copyright 2014 Sebastian Geerken <sgeerken@dillo.org>
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



#include "simpletablecell.hh"
#include "table.hh"
#include "../lout/debug.hh"

namespace dw {

int SimpleTableCell::CLASS_ID = -1;

SimpleTableCell::SimpleTableCell (bool limitTextWidth):
   Textblock (limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::SimpleTableCell");
   registerName ("dw::SimpleTableCell", &CLASS_ID);
}

SimpleTableCell::~SimpleTableCell()
{
   DBG_OBJ_DELETE ();
}

bool SimpleTableCell::getAdjustMinWidth ()
{
   return Table::getAdjustTableMinWidth ();
}

bool SimpleTableCell::isBlockLevel ()
{
   return false;
}

int SimpleTableCell::applyPerWidth (int containerWidth,
                                     core::style::Length perWidth)
{
   return core::style::multiplyWithPerLength (containerWidth, perWidth);
}

int SimpleTableCell::applyPerHeight (int containerHeight,
                                     core::style::Length perHeight)
{
   return core::style::multiplyWithPerLength (containerHeight, perHeight);
}

} // namespace dw
