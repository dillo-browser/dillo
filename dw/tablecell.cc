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

#include "tablecell.hh"
#include "table.hh"

namespace dw {

/**
 * \brief Provided some common implementations of virtual widget
 *    methods.
 *
 * Once I understand how diamond inheritance works, a class TableCell
 * will be provided, from which SimpleTableCell and AlignedTableCell
 * will inherit, additionaly (in a multiple way).
 */
namespace tablecell {

bool getAdjustMinWidth ()
{
   return Table::getAdjustTableMinWidth ();
}

bool isBlockLevel ()
{
   return false;
}

int correctAvailWidthOfChild (core::Widget *widget, core::Widget *child,
                              int width, bool forceValue)
{
   DBG_OBJ_ENTER_O ("resize", 0, widget, "tablecell/correctAvailWidthOfChild",
                    "%p, %d, %s", child, width, forceValue ? "true" : "false");

   // Make sure that this width does not exceed the width of the table
   // cell (minus margin/border/padding).

   if (width != -1) {
      int thisWidth = widget->getAvailWidth (forceValue);
      DBG_OBJ_MSGF_O ("resize", 1, widget, "thisWidth = %d", thisWidth);
      if (thisWidth != -1)
         width =
            lout::misc::max (lout::misc::min (width,
                                              thisWidth
                                              - widget->boxDiffWidth ()),
                             0);
   }

   DBG_OBJ_MSGF_O ("resize", 1, widget, "=> %d", width);
   DBG_OBJ_LEAVE_O (widget);
   return width;
}

int correctAvailHeightOfChild (core::Widget *widget, core::Widget *child,
                               int height, bool forceValue)
{
   // Something to do?
   return height;
}

int applyPerWidth (core::Widget *widget, int containerWidth,
                   core::style::Length perWidth)
{
   return core::style::multiplyWithPerLength (containerWidth, perWidth);
}

int applyPerHeight (core::Widget *widget, int containerHeight,
                    core::style::Length perHeight)
{
   return core::style::multiplyWithPerLength (containerHeight, perHeight);
}

} // namespace dw

} // namespace dw
