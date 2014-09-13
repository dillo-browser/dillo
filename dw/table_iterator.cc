/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2014 Sebastian Geerken <sgeerken@dillo.org>
 *
 * (This file was originally part of textblock.cc.)
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


#include "table.hh"

using namespace lout;

namespace dw {

Table::TableIterator::TableIterator (Table *table,
                                     core::Content::Type mask, bool atEnd):
   OOFAwareWidgetIterator (table, mask, atEnd, table->children->size ())
{
}

object::Object *Table::TableIterator::clone()
{
   TableIterator *tIt =
      new TableIterator ((Table*)getWidget(), getMask(), false);
   cloneValues (tIt);
   return tIt;
}


void Table::TableIterator::highlight (int start, int end,
                                      core::HighlightLayer layer)
{
   if (inFlow ()) {
      /** todo Needs this an implementation? */
   } else
      highlightOOF (start, end, layer);
}

void Table::TableIterator::unhighlight (int direction,
                                        core::HighlightLayer layer)
{
   if (inFlow ()) {
      // ???
   } else
      unhighlightOOF (direction, layer);
}

void Table::TableIterator::getAllocation (int start, int end,
                                                  core::Allocation *allocation)
{
   if (inFlow ()) {
      /** \bug Not implemented. */
   } else
      getAllocationOOF (start, end, allocation);
}

int Table::TableIterator::numContentsInFlow ()
{
   return ((Table*)getWidget())->children->size ();
}

void Table::TableIterator::getContentInFlow (int index,
                                             core::Content *content)
{
   Table *table = (Table*)getWidget();

   if (table->children->get(index) != NULL &&
       table->children->get(index)->type == Child::CELL) {
      content->type = core::Content::WIDGET_IN_FLOW;
      content->widget = table->children->get(index)->cell.widget;
   } else
      content->type = core::Content::INVALID;       
}

} // namespace dw
