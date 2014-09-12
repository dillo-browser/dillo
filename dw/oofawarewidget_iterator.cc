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

#include "oofawarewidget.hh"
#include "ooffloatsmgr.hh"
#include "oofposabsmgr.hh"
#include "oofposfixedmgr.hh"

using namespace dw;
using namespace dw::core;
using namespace lout::misc;
using namespace lout::object;

namespace dw {

namespace oof {

int OOFAwareWidget::OOFAwareWidgetIterator::numParts (int sectionIndex)
{
   OOFAwareWidget *widget = (OOFAwareWidget*)getWidget();

   if (sectionIndex == 0)
      return numContentsInFlow ();
   else
      return widget->outOfFlowMgr[sectionIndex - 1] ?
         widget->outOfFlowMgr[sectionIndex - 1]->getNumWidgets () : 0;
}

void OOFAwareWidget::OOFAwareWidgetIterator::getPart (int sectionIndex,
                                                      int index,
                                                      core::Content *content)
{
   OOFAwareWidget *widget = (OOFAwareWidget*)getWidget();

   if (sectionIndex == 0)
      getContentInFlow (index, content);
   else {
      content->type = core::Content::WIDGET_OOF_CONT;
      content->widget =
         widget->outOfFlowMgr[sectionIndex - 1]->getWidget (index);
   }
}

int OOFAwareWidget::OOFAwareWidgetIterator::compareTo (Comparable *other)
{
   OOFAwareWidgetIterator *otherTI = (OOFAwareWidgetIterator*)other;

   if (sectionIndex != otherTI->sectionIndex)
      return sectionIndex - otherTI->sectionIndex;
   else
      return index - otherTI->index;
}

bool OOFAwareWidget::OOFAwareWidgetIterator::next ()
{
   if (content.type == core::Content::END)
      return false;

   do {
      index++;

      if (index >= numParts(sectionIndex)) {
         sectionIndex++;
         while (sectionIndex < NUM_SECTIONS && numParts(sectionIndex) == 0)
            sectionIndex++;
            
         if (sectionIndex == NUM_SECTIONS) {
            content.type = core::Content::END;
            return false;
         } else
            index = 0;
      }

      getPart (sectionIndex, index, &content);
   } while ((content.type & getMask()) == 0);

   return true;
}

bool OOFAwareWidget::OOFAwareWidgetIterator::prev ()
{
   if (content.type == core::Content::START)
      return false;

   do {
      index--;

      if (index < 0) {
         sectionIndex--;
         while (sectionIndex >= 0 && numParts(sectionIndex) == 0)
            sectionIndex--;
            
         if (sectionIndex < 0) {
            content.type = core::Content::START;
            return false;
         } else
            index = numParts(sectionIndex) - 1;
      }

      getPart (sectionIndex, index, &content);
   } while ((content.type & getMask()) == 0);

   return true;
}

void OOFAwareWidget::OOFAwareWidgetIterator::highlightOOF (int start, int end,
                                                           core::HighlightLayer
                                                           layer)
{
   // TODO What about OOF widgets?
}

void OOFAwareWidget::OOFAwareWidgetIterator::unhighlightOOF
   (int direction, core::HighlightLayer layer)
{
   // TODO What about OOF widgets?
}

void OOFAwareWidget::OOFAwareWidgetIterator::getAllocationOOF (int start,
                                                               int end,
                                                               core::Allocation
                                                               *allocation)
{
   // TODO Consider start and end?
   OOFAwareWidget *widget = (OOFAwareWidget*)getWidget();
   *allocation = *(widget->outOfFlowMgr[sectionIndex - 1]
                   ->getWidget(index)->getAllocation());
}

void OOFAwareWidget::OOFAwareWidgetIterator::print ()
{
   Iterator::print ();
   printf (", sectionIndex = %d, index = %d", sectionIndex, index);
}

} // namespace oof

} // namespace dw
