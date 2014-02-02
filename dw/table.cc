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
   setFlags (BLOCK_LEVEL);
   setFlags (USES_HINTS);
   setButtonSensitive(false);

   this->limitTextWidth = limitTextWidth;

   rowClosed = false;

   // random values
   availWidth = 100;
   availAscent = 100;
   availDescent = 0;

   numRows = 0;
   numCols = 0;
   curRow = -1;
   curCol = 0;

   children = new misc::SimpleVector <Child*> (16);
   colExtremes = new misc::SimpleVector<core::Extremes> (8);
   colWidths = new misc::SimpleVector <int> (8);
   cumHeight = new misc::SimpleVector <int> (8);
   rowSpanCells = new misc::SimpleVector <int> (8);
   colSpanCells = new misc::SimpleVector <int> (8);
   baseline = new misc::SimpleVector <int> (8);
   rowStyle = new misc::SimpleVector <core::style::Style*> (8);

   hasColPercent = 0;
   colPercents = new misc::SimpleVector <core::style::Length> (8);

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
   delete colSpanCells;
   delete baseline;
   delete rowStyle;
   delete colPercents;

   DBG_OBJ_DELETE ();
}

void Table::sizeRequestImpl (core::Requisition *requisition)
{
   forceCalcCellSizes ();

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

}

void Table::getExtremesImpl (core::Extremes *extremes)
{
   if (numCols == 0) {
      extremes->minWidth = extremes->maxWidth = 0;
      return;
   }

   forceCalcColumnExtremes ();

   extremes->minWidth = extremes->maxWidth =
      (numCols + 1) * getStyle()->hBorderSpacing
      + getStyle()->boxDiffWidth ();
   for (int col = 0; col < numCols; col++) {
      extremes->minWidth += colExtremes->getRef(col)->minWidth;
      extremes->maxWidth += colExtremes->getRef(col)->maxWidth;
   }
   if (core::style::isAbsLength (getStyle()->width)) {
      extremes->minWidth =
         misc::max (extremes->minWidth,
                    core::style::absLengthVal(getStyle()->width));
      extremes->maxWidth =
         misc::max (extremes->maxWidth,
                    core::style::absLengthVal(getStyle()->width));
   }

   _MSG(" Table::getExtremesImpl, {%d, %d} numCols=%d\n",
       extremes->minWidth, extremes->maxWidth, numCols);
}

void Table::sizeAllocateImpl (core::Allocation *allocation)
{
   calcCellSizes ();

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
            int width =
               (children->get(n)->cell.colspanEff - 1)
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
}

void Table::resizeDrawImpl ()
{
   queueDrawArea (redrawX, 0, allocation.width - redrawX, getHeight ());
   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   redrawX = allocation.width;
   redrawY = getHeight ();
}

void Table::setWidth (int width)
{
   // If limitTextWidth is set, a queueResize may also be necessary.
   if (availWidth != width || limitTextWidth) {
      _MSG(" Table::setWidth %d\n", width);
      availWidth = width;
      queueResize (0, false);
   }
}

void Table::setAscent (int ascent)
{
   if (availAscent != ascent) {
      availAscent = ascent;
      queueResize (0, false);
   }
}

void Table::setDescent (int descent)
{
   if (availDescent != descent) {
      availDescent = descent;
      queueResize (0, false);
   }
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

TableCell *Table::getCellRef ()
{
   core::Widget *child;

   for (int row = 0; row <= numRows; row++) {
      int n = curCol + row * numCols;
      if (childDefined (n)) {
         child = children->get(n)->cell.widget;
         if (child->instanceOf (TableCell::CLASS_ID))
            return (TableCell*)child;
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

void Table::calcCellSizes ()
{
   if (needsResize ())
      forceCalcCellSizes ();
}


void Table::forceCalcCellSizes ()
{
   int totalWidth = 0, childHeight, forceTotalWidth = 1;
   core::Extremes extremes;

   // Will also call calcColumnExtremes(), when needed.
   getExtremes (&extremes);

   if (core::style::isAbsLength (getStyle()->width)) {
      totalWidth = core::style::absLengthVal (getStyle()->width);
   } else if (core::style::isPerLength (getStyle()->width)) {
      /*
       * If the width is > 100%, we use 100%, this prevents ugly
       * results. (May be changed in future, when a more powerful
       * rendering is implemented, to handle fixed positions etc.,
       * as defined by CSS2.)
       */
      totalWidth =
         misc::min (core::style::multiplyWithPerLength (availWidth,
                                                        getStyle()->width),
                    availWidth);
   } else if (getStyle()->width == core::style::LENGTH_AUTO) {
      totalWidth = availWidth;
      forceTotalWidth = 0;
   }

   _MSG(" availWidth = %d\n", availWidth);
   _MSG(" totalWidth1 = %d\n", totalWidth);

   if (totalWidth < extremes.minWidth)
      totalWidth = extremes.minWidth;
   totalWidth = totalWidth
                - (numCols + 1) * getStyle()->hBorderSpacing
                - getStyle()->boxDiffWidth ();

   _MSG(" totalWidth2 = %d curCol=%d\n", totalWidth,curCol);


   colWidths->setSize (numCols, 0);
   cumHeight->setSize (numRows + 1, 0);
   rowSpanCells->setSize (0);
   baseline->setSize (numRows);

   _MSG(" extremes = %d,%d\n", extremes.minWidth, extremes.maxWidth);
   _MSG(" getStyle()->boxDiffWidth() = %d\n", getStyle()->boxDiffWidth());
   _MSG(" getStyle()->hBorderSpacing = %d\n", getStyle()->hBorderSpacing);


   apportion_percentages2 (totalWidth, forceTotalWidth);
   if (!hasColPercent)
      apportion2 (totalWidth, forceTotalWidth);

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
            children->get(n)->cell.widget->setWidth (width);
            children->get(n)->cell.widget->sizeRequest (&childRequisition);
            childHeight = childRequisition.ascent + childRequisition.descent;
            if (children->get(n)->cell.rowspan == 1) {
               rowHeight = misc::max (rowHeight, childHeight);
            } else {
               rowSpanCells->increase();
               rowSpanCells->set(rowSpanCells->size()-1, n);
            }
         }
      }/*for col*/

      setCumHeight (row + 1,
         cumHeight->get (row) + rowHeight + getStyle()->vBorderSpacing);

   }/*for row*/

   apportionRowSpan ();
}

void Table::apportionRowSpan ()
{
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
}


/**
 * \brief Fills dw::Table::colExtremes, only if recalculation is necessary.
 *
 * \bug Some parts are missing.
 */
void Table::calcColumnExtremes ()
{
   if (extremesChanged ())
      forceCalcColumnExtremes ();
}


/**
 * \brief Fills dw::Table::colExtremes in all cases.
 */
void Table::forceCalcColumnExtremes ()
{
   _MSG("  Table::forceCalcColumnExtremes  numCols=%d\n", numCols);

   if (numCols == 0)
      return;

   colExtremes->setSize (numCols);
   colPercents->setSize (numCols);
   colSpanCells->setSize (0);
   /* 1. cells with colspan = 1 */
   for (int col = 0; col < numCols; col++) {
      colExtremes->getRef(col)->minWidth = 0;
      colExtremes->getRef(col)->maxWidth = 0;
      colPercents->set(col, core::style::LENGTH_AUTO);

      for (int row = 0; row < numRows; row++) {
         int n = row * numCols + col;
         if (!childDefined (n))
            continue;
         if (children->get(n)->cell.colspanEff == 1) {
            core::Extremes cellExtremes;
            int cellMinW, cellMaxW, pbm;
            core::style::Length width =
               children->get(n)->cell.widget->getStyle()->width;
            pbm = (numCols + 1) * getStyle()->hBorderSpacing
                  + children->get(n)->cell.widget->getStyle()->boxDiffWidth ();
            children->get(n)->cell.widget->getExtremes (&cellExtremes);
            if (core::style::isAbsLength (width)) {
               // Fixed lengths include table padding, border and margin.
               cellMinW = cellExtremes.minWidth;
               cellMaxW = misc::max (cellMinW,
                                     core::style::absLengthVal(width) - pbm);
            } else {
               cellMinW = cellExtremes.minWidth;
               cellMaxW = cellExtremes.maxWidth;
            }

            _MSG("FCCE, col%d colMin,colMax,cellMin,cellMax = %d,%d,%d,%d\n",
                col,
                colExtremes->getRef(col)->minWidth,
                colExtremes->getRef(col)->maxWidth,
                cellMinW, cellMaxW);

            colExtremes->getRef(col)->minWidth =
               misc::max (colExtremes->getRef(col)->minWidth, cellMinW);
            colExtremes->getRef(col)->maxWidth =
               misc::max (colExtremes->getRef(col)->minWidth, misc::max (
                             colExtremes->getRef(col)->maxWidth,
                             cellMaxW));

            // Also fill the colPercents array in this pass
            if (core::style::isPerLength (width)) {
               hasColPercent = 1;
               if (colPercents->get(col) == core::style::LENGTH_AUTO)
                  colPercents->set(col, width);
            } else if (core::style::isAbsLength (width)) {
               // We treat LEN_ABS as a special case of LEN_AUTO.
               /*
                * if (colPercents->get(col) == LEN_AUTO)
                *   colPercents->set(col, LEN_ABS);
                *
                * (Hint: that's old code!)
                */
            }
         } else {
            colSpanCells->increase();
            colSpanCells->set(colSpanCells->size()-1, n);
         }
      }
   }

   /* 2. cells with colspan > 1 */
   /* If needed, here we set proportionally apportioned col maximums */
   for (int c = 0; c < colSpanCells->size(); ++c) {
      core::Extremes cellExtremes;
      int cellMinW, cellMaxW, pbm;
      int n = colSpanCells->get(c);
      int col = n % numCols;
      int cs = children->get(n)->cell.colspanEff;
      core::style::Length width =
         children->get(n)->cell.widget->getStyle()->width;
      pbm = (numCols + 1) * getStyle()->hBorderSpacing
            + children->get(n)->cell.widget->getStyle()->boxDiffWidth ();
      children->get(n)->cell.widget->getExtremes (&cellExtremes);
      if (core::style::isAbsLength (width)) {
         // Fixed lengths include table padding, border and margin.
         cellMinW = cellExtremes.minWidth;
         cellMaxW =
            misc::max (cellMinW, core::style::absLengthVal(width) - pbm);
      } else {
         cellMinW = cellExtremes.minWidth;
         cellMaxW = cellExtremes.maxWidth;
      }
      int minSumCols = 0, maxSumCols = 0;
      for (int i = 0; i < cs; ++i) {
         minSumCols += colExtremes->getRef(col+i)->minWidth;
         maxSumCols += colExtremes->getRef(col+i)->maxWidth;
      }

      _MSG("cs=%d spanWidth=%d,%d sumCols=%d,%d\n",
          cs,cellMinW,cellMaxW,minSumCols,maxSumCols);

      if (minSumCols >= cellMinW && maxSumCols >= cellMaxW)
         continue;

      // Cell size is too small; apportion {min,max} for this colspan.
      int spanMinW = misc::max (misc::max (cs, minSumCols),
                                cellMinW - (cs-1) * getStyle()->hBorderSpacing),
          spanMaxW = misc::max (misc::max (cs, maxSumCols),
                                cellMaxW - (cs-1) * getStyle()->hBorderSpacing);

      if (minSumCols == 0) {
         // No single cells defined for this span => pre-apportion equally
         minSumCols = spanMinW; maxSumCols = spanMaxW;
         int minW = spanMinW, maxW = spanMaxW;
         for (int i = 0; i < cs; ++i) {
            colExtremes->getRef(col+i)->minWidth = minW / (cs - i);
            colExtremes->getRef(col+i)->maxWidth = maxW / (cs - i);
            minW -= colExtremes->getRef(col+i)->minWidth;
            maxW -= colExtremes->getRef(col+i)->maxWidth;
         }
      }

      // These values will help if the span has percents.
      int spanHasColPercent = 0;
      int availSpanMinW = spanMinW;
      float cumSpanPercent = 0.0f;
      for (int i = col; i < col + cs; ++i) {
         if (core::style::isPerLength (colPercents->get(i))) {
            cumSpanPercent += core::style::perLengthVal (colPercents->get(i));
            ++spanHasColPercent;
         } else
            availSpanMinW -= colExtremes->getRef(i)->minWidth;
      }

      // Calculate weighted-apportion columns for this span.
      int wMin = 0, wMax;
      int cumMaxWnew = 0, cumMaxWold = 0, goalMaxW = spanMaxW;
      int curAppW = maxSumCols;
      int curExtraW = spanMinW - minSumCols;
      for (int i = col; i < col + cs; ++i) {

         if (!spanHasColPercent) {
            int d_a = colExtremes->getRef(i)->maxWidth;
            int d_w = curAppW > 0 ? (int)((float)curExtraW * d_a/curAppW) : 0;
            if (d_a < 0||d_w < 0) {
               MSG("d_a=%d d_w=%d\n",d_a,d_w);
               exit(1);
            }
            wMin = colExtremes->getRef(i)->minWidth + d_w;
            colExtremes->getRef(i)->minWidth = wMin;
            curExtraW -= d_w;
            curAppW -= d_a;
         } else {
            if (core::style::isPerLength (colPercents->get(i))) {
               // multiplyWithPerLength would cause rounding errors,
               // therefore the deprecated way, using perLengthVal:
               wMin = misc::max (colExtremes->getRef(i)->minWidth,
                                 (int)(availSpanMinW *
                                       core::style::perLengthVal
                                          (colPercents->get (i))
                                       / cumSpanPercent));
               colExtremes->getRef(i)->minWidth = wMin;
            }
         }

         wMax = (goalMaxW-cumMaxWnew <= 0) ? 0 :
                   (int)((float)(goalMaxW-cumMaxWnew)
                         * colExtremes->getRef(i)->maxWidth
                           / (maxSumCols-cumMaxWold));
         wMax = misc::max (wMin, wMax);
         cumMaxWnew += wMax;
         cumMaxWold += colExtremes->getRef(i)->maxWidth;
         colExtremes->getRef(i)->maxWidth = wMax;

         _MSG("i=%d, wMin=%d wMax=%d cumMaxWold=%d\n",
             i,wMin,wMax,cumMaxWold);

      }
#ifdef DBG
      MSG("col min,max: [");
      for (int i = 0; i < numCols; i++)
         MSG("%d,%d ",
             colExtremes->getRef(i)->minWidth,
             colExtremes->getRef(i)->maxWidth);
      MSG("]\n");
      MSG("getStyle()->hBorderSpacing = %d\n", getStyle()->hBorderSpacing);
#endif
   }
}

/**
 * \brief Apportionment function for AUTO-length columns.
 * 'extremes' comes filled, 'result' comes defined for percentage columns.
 */
void Table::apportion2 (int totalWidth, int forceTotalWidth)
{
   if (colExtremes->size() == 0)
      return;
#ifdef DBG
   MSG("app2, availWidth=%d, totalWidth=%d, forceTotalWidth=%d\n",
       availWidth, totalWidth, forceTotalWidth);
   MSG("app2, extremes: ( ");
   for (int i = 0; i < colExtremes->size (); i++)
      MSG("%d,%d ",
          colExtremes->get(i).minWidth, colExtremes->get(i).maxWidth);
   MSG(")\n");
#endif
   int minAutoWidth = 0, maxAutoWidth = 0, availAutoWidth = totalWidth;
   for (int col = 0; col < numCols; col++) {
      if (core::style::isAbsLength (colPercents->get(col))) {
         // set absolute lengths
         setColWidth (col, colExtremes->get(col).minWidth);
      }
      if (colPercents->get(col) == core::style::LENGTH_AUTO) {
         maxAutoWidth += colExtremes->get(col).maxWidth;
         minAutoWidth += colExtremes->get(col).minWidth;
      } else
         availAutoWidth -= colWidths->get(col);
   }

   if (!maxAutoWidth) // no core::style::LENGTH_AUTO cols!
      return;

   colWidths->setSize (colExtremes->size (), 0);

   if (!forceTotalWidth && maxAutoWidth < availAutoWidth) {
      // Enough space for the maximum table, don't widen past max.
      availAutoWidth = maxAutoWidth;
   }

   // General case.
   int curTargetWidth = misc::max (availAutoWidth, minAutoWidth);
   int curExtraWidth = curTargetWidth - minAutoWidth;
   int curMaxWidth = maxAutoWidth;
   int curNewWidth = minAutoWidth;
   for (int col = 0; col < numCols; col++) {
      _MSG("app2, col %d, minWidth=%d maxWidth=%d\n",
           col, colExtremes->getRef(col)->minWidth,
           colExtremes->get(col).maxWidth);

      if (colPercents->get(col) != core::style::LENGTH_AUTO)
         continue;

      int colMinWidth = colExtremes->getRef(col)->minWidth;
      int colMaxWidth = colExtremes->getRef(col)->maxWidth;
      int w = (curMaxWidth <= 0) ? 0 :
                 (int)((float)curTargetWidth * colMaxWidth/curMaxWidth);

      _MSG("app2, curTargetWidth=%d colMaxWidth=%d curMaxWidth=%d "
          "curNewWidth=%d  ",
          curTargetWidth, colMaxWidth,curMaxWidth,curNewWidth);
      _MSG("w = %d, ", w);

      if (w <= colMinWidth)
         w = colMinWidth;
      else if (curNewWidth - colMinWidth + w > curTargetWidth)
         w = colMinWidth + curExtraWidth;

      _MSG("w = %d\n", w);

      curNewWidth -= colMinWidth;
      curMaxWidth -= colMaxWidth;
      curExtraWidth -= (w - colMinWidth);
      curTargetWidth -= w;
      setColWidth (col, w);
   }
#ifdef DBG
   MSG("app2, result: ( ");
   for (int i = 0; i < colWidths->size (); i++)
      MSG("%d ", colWidths->get (i));
   MSG(")\n");
#endif
}

void Table::apportion_percentages2(int totalWidth, int forceTotalWidth)
{
   int hasTablePercent = core::style::isPerLength (getStyle()->width) ? 1 : 0;

   if (colExtremes->size() == 0 || (!hasTablePercent && !hasColPercent))
      return;

   // If there's a table-wide percentage, totalWidth comes already scaled.
   _MSG("APP_P, availWidth=%d, totalWidth=%d, forceTotalWidth=%d\n",
       availWidth, totalWidth, forceTotalWidth);

   if (!hasColPercent) {
#ifdef DBG
      MSG("APP_P, only a table-wide percentage\n");
      MSG("APP_P, extremes = { ");
      for (int col = 0; col < numCols; col++)
         MSG("%d,%d ", colExtremes->getRef(col)->minWidth,
                       colExtremes->getRef(col)->maxWidth);
      MSG("}\n");
#endif
      // It has only a table-wide percentage. Apportion non-absolute widths.
      int sumMaxWidth = 0, perAvailWidth = totalWidth;
      for (int col = 0; col < numCols; col++) {
         if (core::style::isAbsLength (colPercents->get(col)))
            perAvailWidth -= colExtremes->getRef(col)->maxWidth;
         else
            sumMaxWidth += colExtremes->getRef(col)->maxWidth;
      }

      _MSG("APP_P, perAvailWidth=%d, sumMaxWidth=%d\n",
          perAvailWidth, sumMaxWidth);

      for (int col = 0; col < numCols; col++) {
         int max_wi = colExtremes->getRef(col)->maxWidth, new_wi;
         if (!core::style::isAbsLength (colPercents->get(col))) {
            new_wi =
               misc::max (colExtremes->getRef(col)->minWidth,
                          (int)((float)max_wi * perAvailWidth/sumMaxWidth));
            setColWidth (col, new_wi);
            perAvailWidth -= new_wi;
            sumMaxWidth -= max_wi;
         }
      }
#ifdef DBG
      MSG("APP_P, result = { ");
      for (int col = 0; col < numCols; col++)
         MSG("%d ", colWidths->get(col));
      MSG("}\n");
#endif

   } else {
      // we'll have to apportion...
      _MSG("APP_P, we'll have to apportion...\n");

      // Calculate cumPercent and available space
      float cumPercent = 0.0f;
      int hasAutoCol = 0;
      int sumMinWidth = 0, sumMaxWidth = 0, sumMinNonPer = 0, sumMaxNonPer = 0;
      for (int col = 0; col < numCols; col++) {
         if (core::style::isPerLength (colPercents->get(col))) {
            cumPercent += core::style::perLengthVal (colPercents->get(col));
         } else {
            sumMinNonPer += colExtremes->getRef(col)->minWidth;
            sumMaxNonPer += colExtremes->getRef(col)->maxWidth;
            if (colPercents->get(col) == core::style::LENGTH_AUTO)
               hasAutoCol++;
         }
         sumMinWidth += colExtremes->getRef(col)->minWidth;
         sumMaxWidth += colExtremes->getRef(col)->maxWidth;

         _MSG("APP_P, col %d minWidth=%d maxWidth=%d\n", col,
             colExtremes->getRef(col)->minWidth,
             colExtremes->getRef(col)->maxWidth);
      }
      int oldTotalWidth = totalWidth;
      if (!forceTotalWidth) {
         if (sumMaxNonPer == 0 || cumPercent < 0.99f) {
            // only percentage columns, or cumPercent < 100% => restrict width
            int totW = (int)(sumMaxNonPer / (1.0f - cumPercent));
            for (int col = 0; col < numCols; col++) {
               totW = misc::max
                  (totW,
                   (int)(colExtremes->getRef(col)->maxWidth
                         / core::style::perLengthVal (colPercents->get(col))));
            }
            totalWidth = misc::min (totW, totalWidth);
         }
      }

      // make sure there's enough space
      totalWidth = misc::max (totalWidth, sumMinWidth);
      // extraWidth is always >= 0
      int extraWidth = totalWidth - sumMinWidth;
      int sumMinWidthPer = sumMinWidth - sumMinNonPer;
      int curPerWidth = sumMinWidthPer;
      // percentages refer to workingWidth
      int workingWidth = totalWidth - sumMinNonPer;
      if (cumPercent < 0.99f) {
         // In this case, use the whole table width
         workingWidth = totalWidth;
         curPerWidth = sumMinWidth;
      }

      _MSG("APP_P, oldTotalWidth=%d totalWidth=%d"
          " workingWidth=%d extraWidth=%d sumMinNonPer=%d\n",
          oldTotalWidth,totalWidth,workingWidth,extraWidth,sumMinNonPer);

      for (int col = 0; col < numCols; col++) {
         int colMinWidth = colExtremes->getRef(col)->minWidth;
         if (core::style::isPerLength (colPercents->get(col))) {
            int w = core::style::multiplyWithPerLength (workingWidth,
                                                        colPercents->get(col));
            if (w < colMinWidth)
               w = colMinWidth;
            else if (curPerWidth - colMinWidth + w > workingWidth)
               w = colMinWidth + extraWidth;
            extraWidth -= (w - colMinWidth);
            curPerWidth += (w - colMinWidth);
            setColWidth (col, w);
         } else {
            setColWidth (col, colMinWidth);
         }
      }

      if (cumPercent < 0.99f) {
         // Will have to apportion the other columns
#ifdef DBG
         MSG("APP_P, extremes: ( ");
         for (int i = 0; i < colExtremes->size (); i++)
            MSG("%d,%d ",
                colExtremes->get(i).minWidth, colExtremes->get(i).maxWidth);
         MSG(")\n");
#endif
         curPerWidth -= sumMinNonPer;
         int perWidth = (int)(curPerWidth/cumPercent);
         totalWidth = misc::max (totalWidth, perWidth);
         totalWidth = misc::min (totalWidth, oldTotalWidth);

         _MSG("APP_P, curPerWidth=%d perWidth=%d, totalWidth=%d\n",
             curPerWidth, perWidth, totalWidth);

         if (hasAutoCol == 0) {
            // Special case, cumPercent < 100% and no other columns to expand.
            // We'll honor totalWidth by expanding the percentage cols.
            int extraWidth = totalWidth - curPerWidth - sumMinNonPer;
            for (int col = 0; col < numCols; col++) {
               if (core::style::isPerLength (colPercents->get(col))) {
                  // This could cause rounding errors:
                  //
                  // int d =
                  //    core::dw::multiplyWithPerLength (extraWidth,
                  //                                     colPercents->get(col))
                  //    / cumPercent;
                  //
                  // Thus the "old" way:
                  int d =
                     (int)(extraWidth *
                           core::style::perLengthVal (colPercents->get(col))
                           / cumPercent);
                  setColWidth (col, colWidths->get(col) + d);
               }
            }
         }
      }
#ifdef DBG
      MSG("APP_P, result ={ ");
      for (int col = 0; col < numCols; col++)
         MSG("%d ", colWidths->get(col));
      MSG("}\n");
#endif
      apportion2 (totalWidth, 2);

#ifdef DBG
      MSG("APP_P, percent={");
      for (int col = 0; col < numCols; col++)
         MSG("%f ", core::dw::perLengthVal (colPercents->get(col)));
      MSG("}\n");
      MSG("APP_P, result ={ ");
      for (int col = 0; col < numCols; col++)
         MSG("%d ", colWidths->get(col));
      MSG("}\n");
#endif
   }
}

// ----------------------------------------------------------------------

Table::TableIterator::TableIterator (Table *table,
                                     core::Content::Type mask, bool atEnd):
   core::Iterator (table, mask, atEnd)
{
   index = atEnd ? table->children->size () : -1;
   content.type = atEnd ? core::Content::END : core::Content::START;
}

Table::TableIterator::TableIterator (Table *table,
                                     core::Content::Type mask, int index):
   core::Iterator (table, mask, false)
{
   this->index = index;

   if (index < 0)
      content.type = core::Content::START;
   else if (index >= table->children->size ())
      content.type = core::Content::END;
   else {
      content.type = core::Content::WIDGET;
      content.widget = table->children->get(index)->cell.widget;
   }
}

object::Object *Table::TableIterator::clone()
{
   return new TableIterator ((Table*)getWidget(), getMask(), index);
}

int Table::TableIterator::compareTo(object::Comparable *other)
{
   return index - ((TableIterator*)other)->index;
}

bool Table::TableIterator::next ()
{
   Table *table = (Table*)getWidget();

   if (content.type == core::Content::END)
      return false;

   // tables only contain widgets:
   if ((getMask() & core::Content::WIDGET) == 0) {
      content.type = core::Content::END;
      return false;
   }

   do {
      index++;
      if (index >= table->children->size ()) {
         content.type = core::Content::END;
         return false;
      }
   } while (table->children->get(index) == NULL ||
            table->children->get(index)->type != Child::CELL);

   content.type = core::Content::WIDGET;
   content.widget = table->children->get(index)->cell.widget;
   return true;
}

bool Table::TableIterator::prev ()
{
   Table *table = (Table*)getWidget();

   if (content.type == core::Content::START)
      return false;

   // tables only contain widgets:
   if ((getMask() & core::Content::WIDGET) == 0) {
      content.type = core::Content::START;
      return false;
   }

   do {
      index--;
      if (index < 0) {
         content.type = core::Content::START;
         return false;
      }
   } while (table->children->get(index) == NULL ||
            table->children->get(index)->type != Child::CELL);

   content.type = core::Content::WIDGET;
   content.widget = table->children->get(index)->cell.widget;
   return true;
}

void Table::TableIterator::highlight (int start, int end,
                                      core::HighlightLayer layer)
{
   /** todo Needs this an implementation? */
}

void Table::TableIterator::unhighlight (int direction,
                                        core::HighlightLayer layer)
{
}

void Table::TableIterator::getAllocation (int start, int end,
                                                  core::Allocation *allocation)
{
   /** \bug Not implemented. */
}

} // namespace dw
