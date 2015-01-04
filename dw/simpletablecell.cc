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
#include "tablecell.hh"
#include "../lout/misc.hh"
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
   return tablecell::getAdjustMinWidth ();
}

bool SimpleTableCell::isBlockLevel ()
{
   return tablecell::isBlockLevel ();
}

bool SimpleTableCell::mustBeWidenedToAvailWidth ()
{
   return tablecell::mustBeWidenedToAvailWidth ();
}

int SimpleTableCell::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "SimpleTableCell/getAvailWidthOfChild",
                  "%p, %s", child, forceValue ? "true" : "false");

   int width = tablecell::correctAvailWidthOfChild
      (this, child, Textblock::getAvailWidthOfChild (child, forceValue),
       forceValue);

   DBG_OBJ_LEAVE ();
   return width;
}

int SimpleTableCell::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "SimpleTableCell/getAvailHeightOfChild",
                  "%p, %s", child, forceValue ? "true" : "false");

   int height = tablecell::correctAvailHeightOfChild
      (this, child, Textblock::getAvailHeightOfChild (child, forceValue),
       forceValue);

   DBG_OBJ_LEAVE ();
   return height;
}

void SimpleTableCell::correctRequisitionOfChild (Widget *child,
                                                 core::Requisition *requisition,
                                                 void (*splitHeightFun) (int,
                                                                         int*,
                                                                         int*))
{
   DBG_OBJ_ENTER ("resize", 0, "SimpleTableCell/correctRequisitionOfChild",
                  "%p, %d * (%d + %d), ...", child, requisition->width,
                  requisition->ascent, requisition->descent);

   Textblock::correctRequisitionOfChild (child, requisition, splitHeightFun);
   tablecell::correctCorrectedRequisitionOfChild (this, child, requisition,
                                                  splitHeightFun);

   DBG_OBJ_LEAVE ();
}

void SimpleTableCell::correctExtremesOfChild (Widget *child,
                                              core::Extremes *extremes)
{
   DBG_OBJ_ENTER ("resize", 0, "SimpleTableCell/correctExtremesOfChild",
                  "%p, %d (%d) / %d (%d)",
                  child, extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   Textblock::correctExtremesOfChild (child, extremes);
   tablecell::correctCorrectedExtremesOfChild (this, child, extremes);

   DBG_OBJ_LEAVE ();
}

int SimpleTableCell::applyPerWidth (int containerWidth,
                                    core::style::Length perWidth)
{
   return tablecell::applyPerWidth (this, containerWidth, perWidth);
}

int SimpleTableCell::applyPerHeight (int containerHeight,
                                     core::style::Length perHeight)
{
   return tablecell::applyPerHeight (this, containerHeight, perHeight);
}

} // namespace dw
