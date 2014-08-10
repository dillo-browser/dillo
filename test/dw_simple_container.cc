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

#include <math.h>

#include "dw_simple_container.hh"

using namespace dw::core;
using namespace dw::core::style;
using namespace lout::misc;

namespace dw {

int SimpleContainer::CLASS_ID = -1;

// ----------------------------------------------------------------------

SimpleContainer::SimpleContainerIterator::SimpleContainerIterator
   (SimpleContainer *simpleContainer, Content::Type mask, bool atEnd) :
   Iterator (simpleContainer, mask, atEnd)
{
   content.type = atEnd ? Content::END : Content::START;
}

lout::object::Object *SimpleContainer::SimpleContainerIterator::clone ()
{
   SimpleContainerIterator *sci =
      new SimpleContainerIterator ((SimpleContainer*)getWidget(),
                                   getMask(), false);
   sci->content = content;
   return sci;
}

int SimpleContainer::SimpleContainerIterator::index ()
{
   switch (content.type) {
   case Content::START:
      return 0;
   case Content::WIDGET_IN_FLOW:
      return 1;
   case Content::END:
      return 2;
   default:
      assertNotReached ();
      return 0;
   }
}

int SimpleContainer::SimpleContainerIterator::compareTo
   (lout::object::Comparable *other)
{
   return index () - ((SimpleContainerIterator*)other)->index ();
}

bool SimpleContainer::SimpleContainerIterator::next ()
{
   SimpleContainer *simpleContainer = (SimpleContainer*)getWidget();

   if (content.type == Content::END)
      return false;

   // simple containers only contain widgets:
   if ((getMask() & Content::WIDGET_IN_FLOW) == 0) {
      content.type = Content::END;
      return false;
   }

   if (content.type == Content::START) {
      if (simpleContainer->child != NULL) {
         content.type = Content::WIDGET_IN_FLOW;
         content.widget = simpleContainer->child;
         return true;
      } else {
         content.type = Content::END;
         return false;
      }
   } else /* if (content.type == Content::WIDGET) */ {
      content.type = Content::END;
      return false;
   }
}

bool SimpleContainer::SimpleContainerIterator::prev ()
{
   SimpleContainer *simpleContainer = (SimpleContainer*)getWidget();

   if (content.type == Content::START)
      return false;

   // simple containers only contain widgets:
   if ((getMask() & Content::WIDGET_IN_FLOW) == 0) {
      content.type = Content::START;
      return false;
   }

   if (content.type == Content::END) {
      if (simpleContainer->child != NULL) {
         content.type = Content::WIDGET_IN_FLOW;
         content.widget = simpleContainer->child;
         return true;
      } else {
         content.type = Content::START;
         return false;
      }
   } else /* if (content.type == Content::WIDGET) */ {
      content.type = Content::START;
      return false;
   }
}

void SimpleContainer::SimpleContainerIterator::highlight (int start,
                                                          int end,
                                                          HighlightLayer layer)
{
   /** todo Needs this an implementation? */
}

void SimpleContainer::SimpleContainerIterator::unhighlight (int direction,
                                                            HighlightLayer
                                                            layer)
{
   /** todo Needs this an implementation? */
}

void SimpleContainer::SimpleContainerIterator::getAllocation (int start,
                                                              int end,
                                                              Allocation
                                                              *allocation)
{
   /** \bug Not implemented. */
}

// ----------------------------------------------------------------------

SimpleContainer::SimpleContainer ()
{
   registerName ("dw::SimpleContainer", &CLASS_ID);
   child = NULL;
}

SimpleContainer::~SimpleContainer ()
{
   if (child)
      delete child;
}

void SimpleContainer::sizeRequestImpl (Requisition *requisition)
{
   Requisition childReq;
   if (child)
      child->sizeRequest (&childReq);
   else
      childReq.width = childReq.ascent = childReq.descent = 0;

   requisition->width = childReq.width + boxDiffWidth ();
   requisition->ascent = childReq.ascent + boxOffsetY ();
   requisition->descent = childReq.descent + boxRestHeight ();

   correctRequisition (requisition, splitHeightPreserveAscent);
}


void SimpleContainer::getExtremesImpl (Extremes *extremes)
{
   Extremes childExtr;
   if (child)
      child->getExtremes (&childExtr);
   else
      childExtr.minWidth = childExtr.maxWidth = 0;

   extremes->minWidth = childExtr.minWidth + boxDiffWidth ();
   extremes->maxWidth = childExtr.maxWidth + boxDiffWidth ();

   correctExtremes (extremes);
}


void SimpleContainer::sizeAllocateImpl (Allocation *allocation)
{
   Allocation childAlloc;

   if (child) {
      childAlloc.x = allocation->x + boxOffsetX ();
      childAlloc.y = allocation->y + boxOffsetY ();
      childAlloc.width = allocation->width - boxDiffWidth ();
      childAlloc.ascent = allocation->ascent - boxOffsetY ();
      childAlloc.descent = allocation->descent - boxRestHeight ();
      child->sizeAllocate (&childAlloc);
   }
}

void SimpleContainer::draw (View *view, Rectangle *area)
{
   drawWidgetBox (view, area, false);
   Rectangle childArea;
   if (child && child->intersects (area, &childArea))
      child->draw (view, &childArea);
}

Iterator *SimpleContainer::iterator (Content::Type mask, bool atEnd)
{
   return new SimpleContainerIterator (this, mask, atEnd);
}

void SimpleContainer::removeChild (Widget *child)
{
   assert (child == this->child);
   this->child = NULL;

   queueResize (0, true);
}

void SimpleContainer::setChild (Widget *child)
{
   if (this->child)
      delete this->child;

   this->child = child;
   if (this->child)
      this->child->setParent (this);

   queueResize (0, true);
}

} // namespace dw
