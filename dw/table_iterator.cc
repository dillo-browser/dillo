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
      content.type = core::Content::WIDGET_IN_FLOW;
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

   // tables only contain widgets (in flow):
   if ((getMask() & core::Content::WIDGET_IN_FLOW) == 0) {
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

   content.type = core::Content::WIDGET_IN_FLOW;
   content.widget = table->children->get(index)->cell.widget;
   return true;
}

bool Table::TableIterator::prev ()
{
   Table *table = (Table*)getWidget();

   if (content.type == core::Content::START)
      return false;

   // tables only contain widgets (in flow):
   if ((getMask() & core::Content::WIDGET_IN_FLOW) == 0) {
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

   content.type = core::Content::WIDGET_IN_FLOW;
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
