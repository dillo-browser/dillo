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
   setFlags (USES_HINTS);
   setButtonSensitive(false);

   this->limitTextWidth = limitTextWidth;

   rowClosed = false;

   numRows = 0;
   numCols = 0;
   curRow = -1;
   curCol = 0;

   children = new misc::SimpleVector <Child*> (16);
   colExtremes = new misc::SimpleVector<core::Extremes> (8);
   colWidths = new misc::SimpleVector <int> (8);
   cumHeight = new misc::SimpleVector <int> (8);
   rowSpanCells = new misc::SimpleVector <int> (8);
   baseline = new misc::SimpleVector <int> (8);
   rowStyle = new misc::SimpleVector <core::style::Style*> (8);

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
   DBG_OBJ_MSG ("resize", 0, "<b>sizeRequestImpl</b>");
   DBG_OBJ_MSG_START ();

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

   DBG_OBJ_MSG_END ();
}

void Table::getExtremesImpl (core::Extremes *extremes)
{
   DBG_OBJ_MSG ("resize", 0, "<b>getExtremesImpl</b>");
   DBG_OBJ_MSG_START ();

   if (numCols == 0)
      extremes->minWidth = extremes->maxWidth = 0;
   else {
      forceCalcColumnExtremes ();
      
      extremes->minWidth = extremes->maxWidth =
         (numCols + 1) * getStyle()->hBorderSpacing
         + getStyle()->boxDiffWidth ();
      for (int col = 0; col < numCols; col++) {
         extremes->minWidth += colExtremes->getRef(col)->minWidth;
         extremes->maxWidth += colExtremes->getRef(col)->maxWidth;
      }
      
      correctExtremes (extremes);
   }

   DBG_OBJ_MSG_END ();
}

void Table::sizeAllocateImpl (core::Allocation *allocation)
{
   DBG_OBJ_MSG ("resize", 0, "<b>sizeAllocateImpl</b>");
   DBG_OBJ_MSG_START ();

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

   DBG_OBJ_MSG_END ();
}

void Table::resizeDrawImpl ()
{
   queueDrawArea (redrawX, 0, allocation.width - redrawX, getHeight ());
   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   redrawX = allocation.width;
   redrawY = getHeight ();
}

int Table::getAvailWidthOfChild (Widget *child)
{
   DBG_OBJ_MSGF ("resize", 0, "<b>getAvailWidthOfChild</b> (%p)", child);
   DBG_OBJ_MSG_START ();

   calcCellSizes (false);

   int width;

   if (core::style::isAbsLength (child->getStyle()->width)) {
      DBG_OBJ_MSG ("resize", 1, "absolute length");
      width = core::style::absLengthVal (child->getStyle()->width)
         + child->boxDiffWidth ();
   } else if (core::style::isPerLength (child->getStyle()->width)) {
      DBG_OBJ_MSG ("resize", 1, "percentage length");
      int containerContentWidth = getAvailWidth () - boxDiffWidth ();
      width = core::style::multiplyWithPerLength (containerContentWidth,
                                                  child->getStyle()->width)
         + child->boxDiffWidth ();
   } else {
      width = -1;
      DBG_OBJ_MSG ("resize", 1, "no length specified");

      // "child" is not a direct child, but a direct descendant. Search
      // for the actual childs.
      Widget *actualChild = child;
      while (actualChild != NULL && actualChild->getParent () != this)
         actualChild = actualChild->getParent ();

      assert (actualChild != NULL);

      // TODO This is inefficient. (Use parentRef?)
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
            }
         }
      }
      
      assert (width != -1);
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_MSG_END ();
   return width;
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
   DBG_OBJ_MSGF ("resize", 0, "<b>addCell</b> (%p, %d, %d)",
                 widget, colspan, rowspan);
   DBG_OBJ_MSG_START ();

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

   DBG_OBJ_MSG_END ();
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
}

// ----------------------------------------------------------------------

void Table::calcCellSizes (bool calcHeights)
{
   DBG_OBJ_MSG ("resize", 0, "<b>calcCellSizes</b>");
   DBG_OBJ_MSG_START ();

   if ((calcHeights && (needsResize () || resizeQueued () ||
                        extremesChanged () || extremesQueued ())) ||
       (extremesChanged () || extremesQueued ()))
      forceCalcCellSizes (calcHeights);

   DBG_OBJ_MSG_END ();
}


void Table::forceCalcCellSizes (bool calcHeights)
{
   DBG_OBJ_MSG ("resize", 0, "<b>forceCalcCellSizes</b>");
   DBG_OBJ_MSG_START ();

   int totalWidth = 0, childHeight;
   core::Extremes extremes;

   // Will also call calcColumnExtremes(), when needed.
   getExtremes (&extremes);

   totalWidth = getAvailWidth ();

   if (totalWidth < extremes.minWidth)
      totalWidth = extremes.minWidth;
   totalWidth -= ((numCols + 1) * getStyle()->hBorderSpacing
                  + getStyle()->boxDiffWidth ());

   colWidths->setSize (numCols, 0);
   cumHeight->setSize (numRows + 1, 0);
   rowSpanCells->setSize (0);
   baseline->setSize (numRows);

   apportion2 (totalWidth, getStyle()->width != core::style::LENGTH_AUTO,
               0, colExtremes->size() - 1, colWidths, 0, true);

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colWidths.size", colWidths->size ());
      for (int i = 0; i < colWidths->size (); i++)
         DBG_OBJ_ARRSET_NUM ("colWidths", i, colWidths->get (i));
   }

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

   DBG_OBJ_MSG_END ();
}

void Table::apportionRowSpan ()
{
   DBG_OBJ_MSG ("resize", 0, "<b>apportionRowSpan</b>");
   DBG_OBJ_MSG_START ();

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

   DBG_OBJ_MSG_END ();
}


/**
 * \brief Fills dw::Table::colExtremes, only if recalculation is necessary.
 *
 * \bug Some parts are missing.
 */
void Table::calcColumnExtremes ()
{
   DBG_OBJ_MSG ("resize", 0, "<b>calcColumnExtremes</b>");
   DBG_OBJ_MSG_START ();

   if (extremesChanged () || extremesQueued ())
      forceCalcColumnExtremes ();

   DBG_OBJ_MSG_END ();
}


/**
 * \brief Fills dw::Table::colExtremes in all cases.
 */
void Table::forceCalcColumnExtremes ()
{
   DBG_OBJ_MSG ("resize", 0, "<b>forceCalcColumnExtremes</b>");
   DBG_OBJ_MSG_START ();

   if (numCols > 0) {
      lout::misc::SimpleVector<int> colSpanCells (8);
      colExtremes->setSize (numCols);

      // 1. cells with colspan = 1
      for (int col = 0; col < numCols; col++) {
         colExtremes->getRef(col)->minWidth = 0;
         colExtremes->getRef(col)->maxWidth = 0;
         
         for (int row = 0; row < numRows; row++) {
            int n = row * numCols + col;
            if (childDefined (n)) {
               if (children->get(n)->cell.colspanEff == 1) {
                  core::Extremes cellExtremes;
                  children->get(n)->cell.widget->getExtremes (&cellExtremes);
                  
                  colExtremes->getRef(col)->minWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                cellExtremes.minWidth);
                  colExtremes->getRef(col)->maxWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                colExtremes->getRef(col)->maxWidth,
                                cellExtremes.minWidth);
               } else {
                  colSpanCells.increase ();
                  colSpanCells.setLast (n);
               }
            }
         }
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

         int minSumCols = 0, maxSumCols = 0;
         for (int j = 0; j < cs; j++) {
            minSumCols += colExtremes->getRef(col + j)->minWidth;
            maxSumCols += colExtremes->getRef(col + j)->maxWidth;
         }

         DBG_OBJ_MSGF ("resize", 1, "cs = %d cell: %d / %d, sum: %d / %d\n",
                       cs, cellExtremes.minWidth, cellExtremes.maxWidth,
                       minSumCols, maxSumCols);

         bool changeMin = cellExtremes.minWidth > minSumCols;
         bool changeMax = cellExtremes.maxWidth > maxSumCols;
         if (changeMin || changeMax) {
            // TODO This differs from the documentation? Should work, anyway.
            misc::SimpleVector<int> newMin, newMax;
            if (changeMin)
               apportion2 (cellExtremes.minWidth, true, col, col + cs - 1,
                           &newMin, 0, false);
            if (changeMax)
               apportion2 (cellExtremes.maxWidth, true, col, col + cs - 1,
                           &newMax, 0, false);
            
            for (int j = 0; j < cs; j++) {
               if (changeMin)
                  colExtremes->getRef(col + j)->minWidth = newMin.get (i);
               if (changeMax)
                  colExtremes->getRef(col + j)->maxWidth = newMax.get (i);
               // For cases where min and max are somewhat confused:
               colExtremes->getRef(col + j)->maxWidth =
                  misc::max (colExtremes->getRef(col + j)->minWidth,
                             colExtremes->getRef(col + j)->maxWidth);
            }
         }
      }
   }

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colExtremes.size", colExtremes->size ());
      for (int i = 0; i < colExtremes->size (); i++) {
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "minWidth",
                                 colExtremes->get(i).minWidth);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "maxWidth",
                                 colExtremes->get(i).maxWidth);
      }
   }

   DBG_OBJ_MSG_END ();
}

/**
 * \brief Actual apportionment function.
 */
void Table::apportion2 (int totalWidth, bool forceTotalWidth,
                        int firstCol, int lastCol,
                        misc::SimpleVector<int> *dest, int destOffset,
                        bool setRedrawX)
{
   DBG_OBJ_MSGF ("resize", 0, "<b>apportion2</b> (%d, %s, %d, %d, ..., %d, %s)",
                 totalWidth, forceTotalWidth ? "true" : "false", firstCol,
                 lastCol, destOffset, setRedrawX ? "true" : "false");
   DBG_OBJ_MSG_START ();

   if (lastCol >= firstCol) {
      int minWidth = 0, maxWidth = 0, availWidth;
      
      for (int col = firstCol; col <= lastCol; col++) {
         maxWidth += colExtremes->get(col).maxWidth;
         minWidth += colExtremes->get(col).minWidth;
      }
      
      dest->setSize (destOffset + lastCol - firstCol + 11, 0);
      
      if (!forceTotalWidth && maxWidth < totalWidth) {
         // Enough space for the maximum table, don't widen past max.
         availWidth = maxWidth;
      } else
         availWidth = totalWidth;

      DBG_OBJ_MSGF ("resize", 1,
                    "maxWidth = %d, minWidth = %d, availWidth = %d",
                    maxWidth, minWidth, availWidth);

      // General case.
      int curTargetWidth = misc::max (availWidth, minWidth);
      int curExtraWidth = curTargetWidth - minWidth;
      int curMaxWidth = maxWidth;
      int curNewWidth = minWidth;

      for (int col = firstCol; col <= lastCol; col++) {
         int colMinWidth = colExtremes->getRef(col)->minWidth;
         int colMaxWidth = colExtremes->getRef(col)->maxWidth;
         int w = (curMaxWidth <= 0) ? 0 :
            (int)((float)curTargetWidth * colMaxWidth/curMaxWidth);

         if (w <= colMinWidth)
            w = colMinWidth;
         else if (curNewWidth - colMinWidth + w > curTargetWidth)
            w = colMinWidth + curExtraWidth;

         _MSG("w = %d\n", w);

         curNewWidth -= colMinWidth;
         curMaxWidth -= colMaxWidth;
         curExtraWidth -= (w - colMinWidth);
         curTargetWidth -= w;

         // TODO Adapted from old inline function "setColWidth". But
         // (i) is this anyway correct (w is not x)? And does the
         // performance gain actually play a role?
         if (setRedrawX && w != dest->get (destOffset + col))
            redrawX = lout::misc::min (redrawX, w);

         dest->set (destOffset + col, w);
      }
   }
   
   DBG_OBJ_MSG_END ();
}

} // namespace dw
