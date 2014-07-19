/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2014 Sebastian Geerken <sgeerken@dillo.org>
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

//#define DBG

#include "table.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"
#include "../lout/debug.hh"

using namespace lout;

namespace dw {

int Table::CLASS_ID = -1;

Table::Table(bool limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::Table");
   registerName ("dw::Table", &CLASS_ID);
   setButtonSensitive(false);

   this->limitTextWidth = limitTextWidth;

   rowClosed = false;

   numRows = 0;
   numCols = 0;
   curRow = -1;
   curCol = 0;

   DBG_OBJ_SET_NUM ("numCols", numCols);
   DBG_OBJ_SET_NUM ("numRows", numCols);

   children = new misc::SimpleVector <Child*> (16);
   colExtremes = new misc::SimpleVector<core::Extremes> (8);
   colWidths = new misc::SimpleVector <int> (8);
   cumHeight = new misc::SimpleVector <int> (8);
   rowSpanCells = new misc::SimpleVector <int> (8);
   baseline = new misc::SimpleVector <int> (8);
   rowStyle = new misc::SimpleVector <core::style::Style*> (8);

   colWidthsUpToDateWidthColExtremes = true;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   redrawX = 0;
   redrawY = 0;
}

Table::~Table()
{
   for (int i = 0; i < children->size (); i++) {
      if (children->get(i)) {
         switch (children->get(i)->type) {
         case Child::CELL:
            delete children->get(i)->cell.widget;
            break;
         case Child::SPAN_SPACE:
            break;
         }

         delete children->get(i);
      }
   }

   for (int i = 0; i < rowStyle->size (); i++)
      if (rowStyle->get (i))
         rowStyle->get(i)->unref ();

   delete children;
   delete colExtremes;
   delete colWidths;
   delete cumHeight;
   delete rowSpanCells;
   delete baseline;
   delete rowStyle;

   DBG_OBJ_DELETE ();
}

void Table::sizeRequestImpl (core::Requisition *requisition)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequestImpl");

   forceCalcCellSizes (true);

   /**
    * \bug Baselines are not regarded here.
    */
   requisition->width = getStyle()->boxDiffWidth ()
      + (numCols + 1) * getStyle()->hBorderSpacing;
   for (int col = 0; col < numCols; col++)
      requisition->width += colWidths->get (col);

   requisition->ascent =
      getStyle()->boxDiffHeight () + cumHeight->get (numRows)
      + getStyle()->vBorderSpacing;
   requisition->descent = 0;

   correctRequisition (requisition, core::splitHeightPreserveDescent);

   DBG_OBJ_LEAVE ();
}

void Table::getExtremesImpl (core::Extremes *extremes)
{
   DBG_OBJ_ENTER0 ("resize", 0, "getExtremesImpl");

   if (numCols == 0)
      extremes->minWidth = extremes->minWidthIntrinsic = extremes->maxWidth =
         extremes->maxWidthIntrinsic = boxDiffWidth ();
   else {
      forceCalcColumnExtremes ();
      
      extremes->minWidth = extremes->minWidthIntrinsic = extremes->maxWidth =
         extremes->maxWidthIntrinsic = 
         (numCols + 1) * getStyle()->hBorderSpacing + boxDiffWidth ();
      for (int col = 0; col < numCols; col++) {
         extremes->minWidth += colExtremes->getRef(col)->minWidth;
         extremes->minWidthIntrinsic +=
            colExtremes->getRef(col)->minWidthIntrinsic;
         extremes->maxWidth += colExtremes->getRef(col)->maxWidth;
         extremes->maxWidthIntrinsic +=
            colExtremes->getRef(col)->maxWidthIntrinsic;
      }     
   }

   correctExtremes (extremes);

   DBG_OBJ_LEAVE ();
}

void Table::sizeAllocateImpl (core::Allocation *allocation)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeAllocateImpl");

   calcCellSizes (true);

   /**
    * \bug Baselines are not regarded here.
    */

   int offy =
      allocation->y + getStyle()->boxOffsetY () + getStyle()->vBorderSpacing;
   int x =
      allocation->x + getStyle()->boxOffsetX () + getStyle()->hBorderSpacing;

   for (int col = 0; col < numCols; col++) {
      for (int row = 0; row < numRows; row++) {
         int n = row * numCols + col;
         if (childDefined (n)) {
            int width = (children->get(n)->cell.colspanEff - 1)
               * getStyle()->hBorderSpacing;
            for (int i = 0; i < children->get(n)->cell.colspanEff; i++)
               width += colWidths->get (col + i);

            core::Allocation childAllocation;
            core::Requisition childRequisition;

            children->get(n)->cell.widget->sizeRequest (&childRequisition);

            childAllocation.x = x;
            childAllocation.y = cumHeight->get (row) + offy;
            childAllocation.width = width;
            childAllocation.ascent = childRequisition.ascent;
            childAllocation.descent =
               cumHeight->get (row + children->get(n)->cell.rowspan)
               - cumHeight->get (row) - getStyle()->vBorderSpacing
               - childRequisition.ascent;
            children->get(n)->cell.widget->sizeAllocate (&childAllocation);
         }
      }

      x += colWidths->get (col) + getStyle()->hBorderSpacing;
   }

   DBG_OBJ_LEAVE ();
}

void Table::resizeDrawImpl ()
{
   queueDrawArea (redrawX, 0, allocation.width - redrawX, getHeight ());
   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   redrawX = allocation.width;
   redrawY = getHeight ();
}

int Table::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;

   // Unlike other containers, the table widget sometimes narrows
   // columns to a width less than specified by CSS (see
   // forceCalcCellSizes). For this reason, the column widths have to
   // be calculated in all cases.
   if (forceValue) {
      calcCellSizes (false);
      width = calcAvailWidthForDescendant (child);
   } else
      width = -1;

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();
   return width;
}

void Table::correctRequisitionOfChild (core::Widget *child,
                                       core::Requisition *requisition,
                                       void (*splitHeightFun) (int, int*, int*))
{
   // TODO Same comments as in Widget::correctRequisitionOfChild may
   // apply here.

   DBG_OBJ_ENTER ("resize", 0, "correctRequisitionOfChild",
                  "%p, %d * (%d + %d), ...)",
                  child, requisition->width, requisition->ascent,
                  requisition->descent);

   if (child->getStyle()->width != core::style::LENGTH_AUTO) {
      calcCellSizes (false);
      // The requisition is always the width of the column; the table
      // widget is special in inforcing this, based on extremes, which
      // may again be corrected again by CSS attributes.
      requisition->width = calcAvailWidthForDescendant (child);
   }

   correctReqHeightOfChild (child, requisition, splitHeightFun);

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent,
                 requisition->descent);
   DBG_OBJ_LEAVE ();
}

int Table::calcAvailWidthForDescendant (Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "calcAvailWidthForDescendant", "%p", child);

   // "child" is not a direct child, but a direct descendant. Search
   // for the actual childs.
   Widget *actualChild = child;
   while (actualChild != NULL && actualChild->getParent () != this)
      actualChild = actualChild->getParent ();
   
   assert (actualChild != NULL);
  
   // TODO This is inefficient. (Use parentRef?)
   int width = -1;
   for (int row = numRows - 1; width == -1 && row >= 0; row--) {
      for (int col = 0; width == -1 && col < numCols; col++) {
         int n = row * numCols + col;
         if (childDefined (n) &&
             children->get(n)->cell.widget == actualChild) {
            DBG_OBJ_MSGF ("resize", 1, "calculated from column %d", col);
            width = (children->get(n)->cell.colspanEff - 1)
               * getStyle()->hBorderSpacing;
            for (int i = 0; i < children->get(n)->cell.colspanEff; i++)
               width += colWidths->get (col + i);
            width = misc::max (width, 0);
         }
      }
   }
   
   assert (width != -1);

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();
   return width;
}

int Table::applyPerWidth (int containerWidth, core::style::Length perWidth)
{
   return core::style::multiplyWithPerLength (containerWidth, perWidth);
}

int Table::applyPerHeight (int containerHeight, core::style::Length perHeight)
{
   return core::style::multiplyWithPerLength (containerHeight, perHeight);
}

void Table::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int col = 0; col < numCols; col++) {
      for (int row = 0; row < numRows; row++) {
         int n = row * numCols + col;
         if (childDefined (n))
            children->get(n)->cell.widget->containerSizeChanged ();
      }
   }

   DBG_OBJ_LEAVE ();
}

bool Table::affectsSizeChangeContainerChild (core::Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "affectsSizeChangeContainerChild", "%p", child);

   bool ret;

   // This is a bit more complicated, as compared to the standard
   // implementation (Widget::affectsSizeChangeContainerChild).
   // Height would handled the same way, but width is more
   // complicated: we would have to track numerous values here. Always
   // returning true is correct in all cases, but generally
   // inefficient.

   // TODO Better solution?

   ret = true;

   DBG_OBJ_MSGF ("resize", 1, "=> %s", ret ? "true" : "false");
   DBG_OBJ_LEAVE ();
   return ret;  
}

bool Table::usesAvailWidth ()
{
   return true;
}

bool Table::isBlockLevel ()
{
   return true;
}

void Table::draw (core::View *view, core::Rectangle *area)
{
   // Can be optimized, by iterating on the lines in area.
   drawWidgetBox (view, area, false);

#if 0
   int offx = getStyle()->boxOffsetX () + getStyle()->hBorderSpacing;
   int offy = getStyle()->boxOffsetY () + getStyle()->vBorderSpacing;
   int width = getContentWidth ();

   // This part seems unnecessary. It also segfaulted sometimes when
   // cumHeight size was less than numRows. --jcid
   for (int row = 0; row < numRows; row++) {
      if (rowStyle->get (row))
         drawBox (view, rowStyle->get (row), area,
                  offx, offy + cumHeight->get (row),
                  width - 2*getStyle()->hBorderSpacing,
                  cumHeight->get (row + 1) - cumHeight->get (row)
                  - getStyle()->vBorderSpacing, false);
   }
#endif

   for (int i = 0; i < children->size (); i++) {
      if (childDefined (i)) {
         Widget *child = children->get(i)->cell.widget;
         core::Rectangle childArea;
         if (child->intersects (area, &childArea))
            child->draw (view, &childArea);
      }
   }
}

void Table::removeChild (Widget *child)
{
   /** \bug Not implemented. */
}

core::Iterator *Table::iterator (core::Content::Type mask, bool atEnd)
{
   return new TableIterator (this, mask, atEnd);
}

void Table::addCell (Widget *widget, int colspan, int rowspan)
{
   DBG_OBJ_ENTER ("resize", 0, "addCell", "%p, %d, %d",
                  widget, colspan, rowspan);

   const int maxspan = 100;
   Child *child;
   int colspanEff;

   // We limit the values for colspan and rowspan to avoid
   // attacks by malicious web pages.
   if (colspan > maxspan || colspan < 0) {
      MSG_WARN("colspan = %d is set to %d.\n", colspan, maxspan);
      colspan = maxspan;
   }
   if (rowspan > maxspan || rowspan <= 0) {
      MSG_WARN("rowspan = %d is set to %d.\n", rowspan, maxspan);
      rowspan = maxspan;
   }

   if (numRows == 0) {
      // to prevent a crash
      MSG("addCell: cell without row.\n");
      addRow (NULL);
   }

   if (rowClosed) {
      MSG_WARN("Last cell had colspan=0.\n");
      addRow (NULL);
   }

   if (colspan == 0) {
      colspanEff = misc::max (numCols - curCol, 1);
      rowClosed = true;
   } else
      colspanEff = colspan;

   // Find next free cell-
   while (curCol < numCols &&
          (child = children->get(curRow * numCols + curCol)) != NULL &&
          child->type == Child::SPAN_SPACE)
      curCol++;

   _MSG("Table::addCell numCols=%d,curCol=%d,colspan=%d,colspanEff=%d\n",
       numCols, curCol, colspan, colspanEff);

   // Increase children array, when necessary.
   if (curRow + rowspan > numRows)
      reallocChildren (numCols, curRow + rowspan);
   if (curCol + colspanEff > numCols)
      reallocChildren (curCol + colspanEff, numRows);

   // Fill span space.
   for (int col = 0; col < colspanEff; col++)
      for (int row = 0; row < rowspan; row++)
         if (!(col == 0 && row == 0)) {
            int i = (curRow + row) * numCols + curCol + col;

            child = children->get(i);
            if (child) {
               MSG("Overlapping spans in table.\n");
               assert(child->type == Child::SPAN_SPACE);
               delete child;
            }
            child = new Child ();
            child->type = Child::SPAN_SPACE;
            child->spanSpace.startCol = curCol;
            child->spanSpace.startRow = curRow;
            children->set (i, child);
         }

   // Set the "root" cell.
   child = new Child ();
   child->type = Child::CELL;
   child->cell.widget = widget;
   child->cell.colspanOrig = colspan;
   child->cell.colspanEff = colspanEff;
   child->cell.rowspan = rowspan;
   children->set (curRow * numCols + curCol, child);

   curCol += colspanEff;

   widget->setParent (this);
   if (rowStyle->get (curRow))
      widget->setBgColor (rowStyle->get(curRow)->backgroundColor);
   queueResize (0, true);

#if 0
   // show table structure in stdout
   for (int row = 0; row < numRows; row++) {
      for (int col = 0; col < numCols; col++) {
         int n = row * numCols + col;
         if (!(child = children->get (n))) {
            MSG("[null     ] ");
         } else if (children->get(n)->type == Child::CELL) {
            MSG("[CELL rs=%d] ", child->cell.rowspan);
         } else if (children->get(n)->type == Child::SPAN_SPACE) {
            MSG("[SPAN rs=%d] ", child->cell.rowspan);
         } else {
            MSG("[Unk.     ] ");
         }
      }
      MSG("\n");
   }
   MSG("\n");
#endif

   DBG_OBJ_LEAVE ();
}

void Table::addRow (core::style::Style *style)
{
   curRow++;

   if (curRow >= numRows)
      reallocChildren (numCols, curRow + 1);

   if (rowStyle->get (curRow))
      rowStyle->get(curRow)->unref ();

   rowStyle->set (curRow, style);
   if (style)
      style->ref ();

   curCol = 0;
   rowClosed = false;
}

AlignedTableCell *Table::getCellRef ()
{
   core::Widget *child;

   for (int row = 0; row <= numRows; row++) {
      int n = curCol + row * numCols;
      if (childDefined (n)) {
         child = children->get(n)->cell.widget;
         if (child->instanceOf (AlignedTableCell::CLASS_ID))
            return (AlignedTableCell*)child;
      }
   }

   return NULL;
}

const char *Table::getExtrModName (ExtrMod mod)
{
   switch (mod) {
   case MIN:
      return "MIN";

   case MIN_INTR:
      return "MIN_INTR";

   case MIN_MIN:
      return "MIN_MIN";

   case MAX_MIN:
      return "MAX_MIN";

   case MAX:
      return "MAX";

   case MAX_INTR:
      return "MAX_INTR";

   default:
      misc::assertNotReached ();
      return NULL;
   }
}

int Table::getExtreme (core::Extremes *extremes, ExtrMod mod)
{
   switch (mod) {
   case MIN:
      return extremes->minWidth;

   case MIN_INTR:
      return extremes->minWidthIntrinsic;

   case MIN_MIN:
      return misc::min (extremes->minWidth, extremes->minWidthIntrinsic);

   case MAX_MIN:
      return misc::max (extremes->minWidth, extremes->minWidthIntrinsic);

   case MAX:
      return extremes->maxWidth;

   case MAX_INTR:
      return extremes->maxWidthIntrinsic;

   default:
      misc::assertNotReached (); return 0;
   }
}

void Table::setExtreme (core::Extremes *extremes, ExtrMod mod, int value)
{
   switch (mod) {
   case MIN:
      extremes->minWidth = value;
      break;

   case MIN_INTR:
      extremes->minWidthIntrinsic = value;
      break;

   // MIN_MIN and MAX_MIN not supported here.

   case MAX:
      extremes->maxWidth = value;
      break;

   case MAX_INTR:
      extremes->maxWidthIntrinsic = value;
      break;

   default:
      misc::assertNotReached ();
   }
}

void Table::reallocChildren (int newNumCols, int newNumRows)
{
   assert (newNumCols >= numCols);
   assert (newNumRows >= numRows);

   children->setSize (newNumCols * newNumRows);

   if (newNumCols > numCols) {
      // Complicated case, array got also wider.
      for (int row = newNumRows - 1; row >= 0; row--) {
         int colspan0Col = -1, colspan0Row = -1;

         // Copy old part.
         for (int col = numCols - 1; col >= 0; col--) {
            int n = row * newNumCols + col;
            children->set (n, children->get (row * numCols + col));
            if (children->get (n)) {
               switch (children->get(n)->type) {
               case Child::CELL:
                  if (children->get(n)->cell.colspanOrig == 0) {
                     colspan0Col = col;
                     colspan0Row = row;
                     children->get(n)->cell.colspanEff = newNumCols - col;
                  }
                  break;
               case Child::SPAN_SPACE:
                  if (children->get(children->get(n)->spanSpace.startRow
                                    * numCols +
                                    children->get(n)->spanSpace.startCol)
                      ->cell.colspanOrig == 0) {
                     colspan0Col = children->get(n)->spanSpace.startCol;
                     colspan0Row = children->get(n)->spanSpace.startRow;
                  }
                  break;
               }
            }
         }

         // Fill rest of the column.
         if (colspan0Col == -1) {
            for (int col = numCols; col < newNumCols; col++)
               children->set (row * newNumCols + col, NULL);
         } else {
            for (int col = numCols; col < newNumCols; col++) {
               Child *child = new Child ();
               child->type = Child::SPAN_SPACE;
               child->spanSpace.startCol = colspan0Col;
               child->spanSpace.startRow = colspan0Row;
               children->set (row * newNumCols + col, child);
            }
         }
      }
   }

   // Bottom part of the children array.
   for (int row = numRows; row < newNumRows; row++)
      for (int col = 0; col < newNumCols; col++)
         children->set (row * newNumCols + col, NULL);

   // Simple arrays.
   rowStyle->setSize (newNumRows);
   for (int row = numRows; row < newNumRows; row++)
      rowStyle->set (row, NULL);
   // Rest is increased, when needed.

   numCols = newNumCols;
   numRows = newNumRows;

   DBG_OBJ_SET_NUM ("numCols", numCols);
   DBG_OBJ_SET_NUM ("numRows", numCols);
}

// ----------------------------------------------------------------------

void Table::calcCellSizes (bool calcHeights)
{
   DBG_OBJ_ENTER ("resize", 0, "calcCellSizes", "%s",
                  calcHeights ? "true" : "false");

   bool sizeChanged = needsResize () || resizeQueued ();
   bool extremesChanges = extremesChanged () || extremesQueued ();

   if (calcHeights ? (extremesChanges || sizeChanged) :
       (extremesChanges || !colWidthsUpToDateWidthColExtremes))
      forceCalcCellSizes (calcHeights);

   DBG_OBJ_LEAVE ();
}


void Table::forceCalcCellSizes (bool calcHeights)
{
   DBG_OBJ_ENTER0 ("resize", 0, "forceCalcCellSizes");

   int childHeight;
   core::Extremes extremes;

   // Will also call calcColumnExtremes(), when needed.
   getExtremes (&extremes);

   int availWidth = getAvailWidth (true);
   int totalWidth = availWidth -
      ((numCols + 1) * getStyle()->hBorderSpacing + boxDiffWidth ());

   DBG_OBJ_MSGF ("resize", 1,
                 "totalWidth = %d - ((%d - 1) * %d + %d) = <b>%d</b>",
                 availWidth, numCols, getStyle()->hBorderSpacing,
                 boxDiffWidth (), totalWidth);

   colWidths->setSize (numCols, 0);
   cumHeight->setSize (numRows + 1, 0);
   rowSpanCells->setSize (0);
   baseline->setSize (numRows);

   misc::SimpleVector<int> *oldColWidths = colWidths;
   colWidths = new misc::SimpleVector <int> (8);

   int minWidth = 0;
   for (int col = 0; col < colExtremes->size(); col++)
      minWidth += getColExtreme (col, MIN);

   DBG_OBJ_MSGF ("resize", 1, "minWidth (= %d) > totalWidth (= %d)?",
                 minWidth, totalWidth);
      
   if (minWidth > totalWidth)
      // The sum of all column minima is larger than the available
      // width, so we narrow the columns (see also CSS2 spec,
      // section 17.5, #6). We use a similar apportioning, but not
      // bases on minimal and maximal widths, but on intrinsic minimal
      // widths and corrected minimal widths. This way, intrinsic
      // extremes are preferred (so avoiding columns too narrow for
      // the actual contents), at the expenses of corrected ones
      // (which means that sometimes CSS values are handled
      // incorrectly).

      apportion2 (totalWidth, 0, colExtremes->size() - 1, MIN_MIN, MAX_MIN,
                  colWidths, 0);
   else {
      // Normal apportioning.
      int width;
      if (getStyle()->width == core::style::LENGTH_AUTO) {
         // Do not force width, when maximal width is smaller.
         int maxWidth = 0;
         for (int col = 0; col < colExtremes->size(); col++)
            maxWidth += getColExtreme (col, MAX);
         width = misc::min (totalWidth, maxWidth);
         DBG_OBJ_MSGF ("resize", 1, "width = min (%d, %d) = %d",
                       totalWidth, maxWidth, width);
      } else
         // CSS 'width' defined: force this width.
         width = totalWidth;

      apportion2 (width, 0, colExtremes->size() - 1, MIN, MAX, colWidths, 0);
   }

   // TODO: Adapted from old inline function "setColWidth". But (i) is
   // this anyway correct (col width is is not x)? And does the
   // performance gain actually play a role?
   for (int col = 0; col < colExtremes->size(); col++) {
      if (colWidths->get (col) != oldColWidths->get (col))
         redrawX = lout::misc::min (redrawX, colWidths->get (col));
   }
   
   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colWidths.size", colWidths->size ());
      for (int i = 0; i < colWidths->size (); i++)
         DBG_OBJ_ARRSET_NUM ("colWidths", i, colWidths->get (i));
   }

   colWidthsUpToDateWidthColExtremes = true;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   for (int col = 0; col < numCols; col++) {
      if (col >= oldColWidths->size () || col >= colWidths->size () ||
          oldColWidths->get (col) != colWidths->get (col)) {
         // Column width has changed, tell children about this.
         for (int row = 0; row < numRows; row++) {
            int n = row * numCols + col;
            // TODO: Columns spanning several rows are only regarded
            // when the first column is affected.
            if (childDefined (n))
               children->get(n)->cell.widget->containerSizeChanged ();
         }
      }
   }

   delete oldColWidths;

   if (calcHeights) {
      setCumHeight (0, 0);
      for (int row = 0; row < numRows; row++) {
         /**
          * \bug dw::Table::baseline is not filled.
          */
         int rowHeight = 0;
         
         for (int col = 0; col < numCols; col++) {
            int n = row * numCols + col;
            if (childDefined (n)) {
               int width = (children->get(n)->cell.colspanEff - 1)
                  * getStyle()->hBorderSpacing;
               for (int i = 0; i < children->get(n)->cell.colspanEff; i++)
                  width += colWidths->get (col + i);
               
               core::Requisition childRequisition;
               //children->get(n)->cell.widget->setWidth (width);
               children->get(n)->cell.widget->sizeRequest (&childRequisition);
               childHeight = childRequisition.ascent + childRequisition.descent;
               if (children->get(n)->cell.rowspan == 1) {
                  rowHeight = misc::max (rowHeight, childHeight);
               } else {
                  rowSpanCells->increase();
                  rowSpanCells->set(rowSpanCells->size()-1, n);
               }
            }
         } // for col
         
         setCumHeight (row + 1,
            cumHeight->get (row) + rowHeight + getStyle()->vBorderSpacing);
      } // for row

      apportionRowSpan ();
   }

   DBG_OBJ_LEAVE ();
}

void Table::apportionRowSpan ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "apportionRowSpan");

   int *rowHeight = NULL;

   for (int c = 0; c < rowSpanCells->size(); ++c) {
      int n = rowSpanCells->get(c);
      int row = n / numCols;
      int rs = children->get(n)->cell.rowspan;
      int sumRows = cumHeight->get(row+rs) - cumHeight->get(row);
      core::Requisition childRequisition;
      children->get(n)->cell.widget->sizeRequest (&childRequisition);
      int spanHeight = childRequisition.ascent + childRequisition.descent
                       + getStyle()->vBorderSpacing;
      if (sumRows >= spanHeight)
         continue;

      // Cell size is too small.
      _MSG("Short cell %d, sumRows=%d spanHeight=%d\n",
          n,sumRows,spanHeight);

      // Fill height array
      if (!rowHeight) {
         rowHeight = new int[numRows];
         for (int i = 0; i < numRows; i++)
            rowHeight[i] = cumHeight->get(i+1) - cumHeight->get(i);
      }
#ifdef DBG
      MSG(" rowHeight { ");
      for (int i = 0; i < numRows; i++)
         MSG("%d ", rowHeight[i]);
      MSG("}\n");
#endif

      // Calc new row sizes for this span.
      int cumHnew_i = 0, cumh_i = 0, hnew_i;
      for (int i = row; i < row + rs; ++i) {
         hnew_i =
            sumRows == 0 ? (int)((float)(spanHeight-cumHnew_i)/(row+rs-i)) :
            (sumRows-cumh_i) <= 0 ? 0 :
            (int)((float)(spanHeight-cumHnew_i)*rowHeight[i]/(sumRows-cumh_i));

         _MSG(" i=%-3d h=%d hnew_i=%d =%d*%d/%d   cumh_i=%d cumHnew_i=%d\n",
             i,rowHeight[i],hnew_i,
             spanHeight-cumHnew_i,rowHeight[i],sumRows-cumh_i,
             cumh_i, cumHnew_i);

         cumHnew_i += hnew_i;
         cumh_i += rowHeight[i];
         rowHeight[i] = hnew_i;
      }
      // Update cumHeight
      for (int i = 0; i < numRows; ++i)
         setCumHeight (i+1, cumHeight->get(i) + rowHeight[i]);
   }
   delete[] rowHeight;

   DBG_OBJ_LEAVE ();
}


/**
 * \brief Fills dw::Table::colExtremes, only if recalculation is necessary.
 *
 * \bug Some parts are missing.
 */
void Table::calcColumnExtremes ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "calcColumnExtremes");

   if (extremesChanged () || extremesQueued ())
      forceCalcColumnExtremes ();

   DBG_OBJ_LEAVE ();
}


/**
 * \brief Fills dw::Table::colExtremes in all cases.
 */
void Table::forceCalcColumnExtremes ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "forceCalcColumnExtremes");

   if (numCols > 0) {
      lout::misc::SimpleVector<int> colSpanCells (8);
      colExtremes->setSize (numCols);

      // 1. cells with colspan = 1
      for (int col = 0; col < numCols; col++) {
         DBG_OBJ_MSGF ("resize", 1, "column %d", col);
         DBG_OBJ_MSG_START ();

         colExtremes->getRef(col)->minWidth = 0;
         colExtremes->getRef(col)->minWidthIntrinsic = 0;
         colExtremes->getRef(col)->maxWidth = 0;
         colExtremes->getRef(col)->maxWidthIntrinsic = 0;
         
         for (int row = 0; row < numRows; row++) {
            DBG_OBJ_MSGF ("resize", 1, "row %d", row);
            DBG_OBJ_MSG_START ();

            int n = row * numCols + col;

            if (childDefined (n)) {
               if (children->get(n)->cell.colspanEff == 1) {
                  core::Extremes cellExtremes;
                  children->get(n)->cell.widget->getExtremes (&cellExtremes);

                  DBG_OBJ_MSGF ("resize", 1, "child: %d / %d",
                                cellExtremes.minWidth, cellExtremes.maxWidth);

                  colExtremes->getRef(col)->minWidthIntrinsic =
                     misc::max (colExtremes->getRef(col)->minWidthIntrinsic,
                                cellExtremes.minWidthIntrinsic);
                  colExtremes->getRef(col)->maxWidthIntrinsic =
                     misc::max (colExtremes->getRef(col)->minWidthIntrinsic,
                                colExtremes->getRef(col)->maxWidthIntrinsic,
                                cellExtremes.maxWidthIntrinsic);
                  
                  colExtremes->getRef(col)->minWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                cellExtremes.minWidth);
                  colExtremes->getRef(col)->maxWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                colExtremes->getRef(col)->maxWidth,
                                cellExtremes.maxWidth);

                  DBG_OBJ_MSGF ("resize", 1, "column: %d / %d (%d / %d)",
                                colExtremes->getRef(col)->minWidth,
                                colExtremes->getRef(col)->maxWidth,
                                colExtremes->getRef(col)->minWidthIntrinsic,
                                colExtremes->getRef(col)->maxWidthIntrinsic);
               } else {
                  colSpanCells.increase ();
                  colSpanCells.setLast (n);
               }
            }

            DBG_OBJ_MSG_END ();
         }

         DBG_OBJ_MSG_END ();
      }

      // 2. cells with colspan > 1

      // TODO: Is this old comment still relevant? "If needed, here we
      // set proportionally apportioned col maximums."

      for (int i = 0; i < colSpanCells.size(); i++) {
         int n = colSpanCells.get (i);
         int col = n % numCols;
         int cs = children->get(n)->cell.colspanEff;

         core::Extremes cellExtremes;
         children->get(n)->cell.widget->getExtremes (&cellExtremes);

         calcExtremesSpanMulteCols (col, cs, &cellExtremes, MIN, MAX);
         calcExtremesSpanMulteCols (col, cs, &cellExtremes, MIN_INTR, MAX_INTR);
      }
   }

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colExtremes.size", colExtremes->size ());
      for (int i = 0; i < colExtremes->size (); i++) {
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "minWidth",
                                 colExtremes->get(i).minWidth);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "minWidthIntrinsic",
                                 colExtremes->get(i).minWidthIntrinsic);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "maxWidth",
                                 colExtremes->get(i).maxWidth);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "maxWidthIntrinsic",
                                 colExtremes->get(i).maxWidthIntrinsic);
      }
   }

   colWidthsUpToDateWidthColExtremes = false;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   DBG_OBJ_LEAVE ();
}

void Table::calcExtremesSpanMulteCols (int col, int cs,
                                       core::Extremes *cellExtremes,
                                       ExtrMod minExtrMod, ExtrMod maxExtrMod)
{
   DBG_OBJ_ENTER ("resize", 0, "calcExtremesSpanMulteCols",
                  "%d, %d, ..., %s, %s",
                  col, cs, getExtrModName (minExtrMod),
                  getExtrModName (maxExtrMod));
   
   int cellMin = getExtreme (cellExtremes, minExtrMod);
   int cellMax = getExtreme (cellExtremes, maxExtrMod);

   int minSumCols = 0, maxSumCols = 0;

   for (int j = 0; j < cs; j++) {
      minSumCols += getColExtreme (col + j, minExtrMod);
      maxSumCols += getColExtreme (col + j, maxExtrMod);
   }

   DBG_OBJ_MSGF ("resize", 1, "cs = %d, cell: %d / %d, sum: %d / %d\n",
                 cs, cellMin, cellMax, minSumCols, maxSumCols);

   bool changeMin = cellMin > minSumCols;
   bool changeMax = cellMax > maxSumCols; 
   if (changeMin || changeMax) {
      // TODO This differs from the documentation? Should work, anyway.
      misc::SimpleVector<int> newMin, newMax;
      if (changeMin)
         apportion2 (cellMin, col, col + cs - 1, MIN, MAX, &newMin, 0);
      if (changeMax)
         apportion2 (cellMax, col, col + cs - 1, MIN, MAX, &newMax, 0);
      
      for (int j = 0; j < cs; j++) {
         if (changeMin)
            setColExtreme (col + j, minExtrMod, newMin.get (j));
         if (changeMax)
            setColExtreme (col + j, maxExtrMod, newMax.get (j));
         
         // For cases where min and max are somewhat confused:
         setColExtreme (col + j, maxExtrMod,
                        misc::max (getColExtreme (col + j, minExtrMod),
                                   getColExtreme (col + j, maxExtrMod)));
      }
   }

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Actual apportionment function.
 */
void Table::apportion2 (int totalWidth, int firstCol, int lastCol,
                        ExtrMod minExtrMod, ExtrMod maxExtrMod,
                        misc::SimpleVector<int> *dest, int destOffset)
{
   DBG_OBJ_ENTER ("resize", 0, "apportion2", "%d, %d, %d, %s, %s, ..., %d",
                  totalWidth, firstCol, lastCol, getExtrModName (minExtrMod),
                  getExtrModName (maxExtrMod), destOffset);

   if (lastCol >= firstCol) {
      dest->setSize (destOffset + lastCol - firstCol + 1, 0);

      int totalMin = 0, totalMax = 0;
      for (int col = firstCol; col <= lastCol; col++) {
         totalMin += getColExtreme (col, minExtrMod);
         totalMax += getColExtreme (col, maxExtrMod);
      }
   
      DBG_OBJ_MSGF ("resize", 1,
                    "totalWidth = %d, totalMin = %d, totalMax = %d",
                    totalWidth, totalMin, totalMax);

      // The actual calculation is rather simple, the ith value is:
      //
      //
      //                     (max[i] - min[i]) * (totalMax - totalMin)
      // width[i] = min[i] + -----------------------------------------
      //                              (totalWidth - totalMin)
      //
      // (Regard "total" as "sum".) With the following general
      // definitions (for both the list and sums):
      //
      //    diffExtr = max - min
      //    diffWidth = width - min
      //
      // it is simplified to:
      //
      //                   diffExtr[i] * totalDiffWidth
      //    diffWidth[i] = ----------------------------
      //                           totalDiffExtr
      //
      // Of course, if totalDiffExtr is 0, this is not defined;
      // instead, we apportion according to the minima:
      //
      //               min[i] * totalWidth
      //    width[i] = -------------------
      //                    totalMin
      //
      // Since min[i] <= max[i] for all i, totalMin == totalMax
      // implies that min[i] == max[i] for all i.
      //
      // Third, it totalMin == 0 (which also implies min[i] = max[i] = 0),
      // the result is
      //
      //    width[i] = totalWidth / n
      
      int totalDiffExtr = totalMax - totalMin;
      if (totalDiffExtr != 0) {
         // Normal case. The algorithm described in
         // "rounding-errors.doc" is used, with:
         // 
         //    x[i] = diffExtr[i]
         //    y[i] = diffWidth[i]
         //    a = totalDiffWidth
         //    b = totalDiffExtr

         DBG_OBJ_MSG ("resize", 1, "normal case");

         int totalDiffWidth = totalWidth - totalMin;
         int cumDiffExtr = 0, cumDiffWidth = 0;
         
         for (int col = firstCol; col <= lastCol; col++) {
            int min = getColExtreme (col, minExtrMod);
            int max = getColExtreme (col, maxExtrMod);
            int diffExtr = max - min;

            cumDiffExtr += diffExtr;
            int diffWidth =
               (cumDiffExtr * totalDiffWidth) / totalDiffExtr - cumDiffWidth;
            cumDiffWidth += diffWidth;
            
            dest->set (destOffset - firstCol + col, diffWidth + min);
         }
      } else if (totalMin != 0) {
         // Special case. Again, same algorithm, with
         // 
         //    x[i] = min[i]
         //    y[i] = width[i]
         //    a = totalWidth
         //    b = totalMin

         DBG_OBJ_MSG ("resize", 1, "special case 1");

         int cumMin = 0, cumWidth = 0;
         for (int col = firstCol; col <= lastCol; col++) {
            int min = getColExtreme (col, minExtrMod);
            cumMin += min;
            int width = (cumMin * totalWidth) / totalMin - cumWidth;
            cumWidth += width;

            dest->set (destOffset - firstCol + col, width);
         }
      } else { // if (totalMin == 0)
         // Last special case. Ssame algorithm, with
         // 
         //    x[i] = 1 (so cumX = i = col - firstCol + 1)
         //    y[i] = width[i]
         //    a = totalWidth
         //    b = n = lastCol - firstCol + 1

         DBG_OBJ_MSG ("resize", 1, "special case 2");

         int cumWidth = 0, n = (lastCol - firstCol + 1);
         for (int col = firstCol; col <= lastCol; col++) {
            int i = (col - firstCol + 1);
            int width = (i * totalWidth) / n - cumWidth;
            cumWidth += width;

            dest->set (destOffset - firstCol + col, width);
         }
      }
   }
   
   DBG_OBJ_LEAVE ();
}

} // namespace dw
