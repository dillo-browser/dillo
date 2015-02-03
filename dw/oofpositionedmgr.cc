/*
 * Dillo Widget
 *
 * Copyright 2013-2014 Sebastian Geerken <sgeerken@dillo.org>
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

#include "oofpositionedmgr.hh"
#include "../lout/debug.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

namespace oof {

OOFPositionedMgr::Child::Child (core::Widget *widget, OOFAwareWidget *generator,
                                int externalIndex)
{
   this->widget = widget;
   this->generator = generator;
   this->externalIndex = externalIndex;
   
   x = y = 0;

   // Initially, this child does not actually have been considered,
   // but since adding a new element will force a size/extremes
   // calculation, this is equivalent.
   consideredForSize = consideredForExtremes = true;
}

OOFPositionedMgr::OOFPositionedMgr (OOFAwareWidget *container)
{
   DBG_OBJ_CREATE ("dw::OOFPositionedMgr");

   this->container = (OOFAwareWidget*)container;
   children = new Vector<Child> (1, false);
   childrenByWidget = new HashTable<TypedPointer<Widget>, Child> (true, true);

   containerAllocation = *(container->getAllocation());

   DBG_OBJ_SET_NUM ("children.size", children->size());
}

OOFPositionedMgr::~OOFPositionedMgr ()
{
   delete children;
   delete childrenByWidget;

   DBG_OBJ_DELETE ();
}

void OOFPositionedMgr::sizeAllocateStart (OOFAwareWidget *caller,
                                          Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateStart",
                  "%p, (%d, %d, %d * (%d + %d))",
                  caller, allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   if (caller == container) {
      if (containerAllocationState == NOT_ALLOCATED)
         containerAllocationState = IN_ALLOCATION;
      containerAllocation = *allocation;
   }

   DBG_OBJ_LEAVE ();
}


void OOFPositionedMgr::sizeAllocateEnd (OOFAwareWidget *caller)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateEnd", "%p", caller);

   if (caller == container) {
      sizeAllocateChildren ();

      bool extremesChanged = !allChildrenConsideredForExtremes ();
      if (extremesChanged || doChildrenExceedContainer () ||
          !allChildrenConsideredForSize ())
         container->oofSizeChanged (extremesChanged);
      
      containerAllocationState = WAS_ALLOCATED;
   }

   DBG_OBJ_LEAVE ();
}

bool OOFPositionedMgr::doChildrenExceedContainer ()
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "doChildrenExceedContainer");

   // This method is called to determine whether the *requisition* of
   // the container must be recalculated. So, we check the allocations
   // of the children against the *requisition* of the container,
   // which may (e. g. within tables) differ from the new allocation.
   // (Generally, a widget may allocated at a different size.)

   Requisition containerReq;
   container->sizeRequest (&containerReq);
   bool exceeds = false;

   DBG_OBJ_MSG_START ();

   for (int i = 0; i < children->size () && !exceeds; i++) {
      Child *child = children->get (i);
      Allocation *childAlloc = child->widget->getAllocation ();
      DBG_OBJ_MSGF ("resize.oofm", 2,
                    "Does childAlloc = (%d, %d, %d * %d) exceed container "
                    "alloc+req = (%d, %d, %d * %d)?",
                    childAlloc->x, childAlloc->y, childAlloc->width,
                    childAlloc->ascent + childAlloc->descent,
                    containerAllocation.x, containerAllocation.y,
                    containerReq.width,
                    containerReq.ascent + containerReq.descent);
      if (childAlloc->x + childAlloc->width
          > containerAllocation.x + containerReq.width ||
          childAlloc->y + childAlloc->ascent + childAlloc->descent
          > containerAllocation.y +
            containerReq.ascent + containerReq.descent) {
         exceeds = true;
         DBG_OBJ_MSG ("resize.oofm", 2, "Yes.");
      } else
         DBG_OBJ_MSG ("resize.oofm", 2, "No.");
   }

   DBG_OBJ_MSG_END ();

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", exceeds ? "true" : "false");
   DBG_OBJ_LEAVE ();

   return exceeds;
}

void OOFPositionedMgr::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int i = 0; i < children->size(); i++)
      children->get(i)->widget->containerSizeChanged ();

   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::draw (View *view, Rectangle *area,
                             DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      Rectangle childArea;
      if (!context->hasWidgetBeenProcessedAsInterruption (child->widget) &&
          !StackingContextMgr::handledByStackingContextMgr (child->widget) &&
          child->widget->intersects (container, area, &childArea))
         child->widget->draw (view, &childArea, context);
   }

   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::addWidgetInFlow (OOFAwareWidget *widget,
                                        OOFAwareWidget *parent,
                                        int externalIndex)
{
}

int OOFPositionedMgr::addWidgetOOF (Widget *widget, OOFAwareWidget *generator,
                                    int externalIndex)
{
   DBG_OBJ_ENTER ("construct.oofm", 0, "addWidgetOOF", "%p, %p, %d",
                  widget, generator, externalIndex);

   Child *child = new Child (widget, generator, externalIndex);
   children->put (child);
   childrenByWidget->put (new TypedPointer<Widget> (widget), child);

   int subRef = children->size() - 1;
   DBG_OBJ_SET_NUM ("children.size", children->size());
   DBG_OBJ_ARRSET_PTR ("children", children->size() - 1, widget);

   DBG_OBJ_SET_PTR_O (widget, "<Positioned>.generator", generator);
   DBG_OBJ_SET_NUM_O (widget, "<Positioned>.externalIndex", externalIndex);

   DBG_OBJ_MSGF ("construct.oofm", 1, "=> %d", subRef);
   DBG_OBJ_LEAVE ();
   return subRef;
}

void OOFPositionedMgr::moveExternalIndices (OOFAwareWidget *generator,
                                            int oldStartIndex, int diff)
{
   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get (i);
      if (child->externalIndex >= oldStartIndex) {
         child->externalIndex += diff;
         DBG_OBJ_SET_NUM_O (child->widget, "<Positioned>.externalIndex",
                            child->externalIndex);
      }
   }
}

void OOFPositionedMgr::markSizeChange (int ref)
{
}


void OOFPositionedMgr::markExtremesChange (int ref)
{
}

Widget *OOFPositionedMgr::getWidgetAtPoint (int x, int y,
                                            GettingWidgetAtPointContext
                                            *context)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPoint", "%d, %d", x, y);

   Widget *widgetAtPoint = NULL;
   
   for (int i = children->size() - 1; widgetAtPoint == NULL && i >= 0; i--) {
      Widget *childWidget = children->get(i)->widget;
      if (!context->hasWidgetBeenProcessedAsInterruption (childWidget) &&
          !StackingContextMgr::handledByStackingContextMgr (childWidget))
         widgetAtPoint = childWidget->getWidgetAtPoint (x, y, context);
   }

   DBG_OBJ_MSGF ("events", 0, "=> %p", widgetAtPoint);
   DBG_OBJ_LEAVE ();

   return widgetAtPoint;
}

void OOFPositionedMgr::tellPosition1 (Widget *widget, int x, int y)
{
}

void OOFPositionedMgr::tellPosition2 (Widget *widget, int x, int y)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "tellPosition2", "%p, %d, %d",
                  widget, x, y);

   TypedPointer<Widget> key (widget);
   Child *child = childrenByWidget->get (&key);
   assert (child);

   child->x = x;
   child->y = y;

   DBG_OBJ_SET_NUM_O (child->widget, "<Positioned>.x", x);
   DBG_OBJ_SET_NUM_O (child->widget, "<Positioned>.y", y);

   DBG_OBJ_LEAVE ();
}

bool OOFPositionedMgr::containerMustAdjustExtraSpace ()
{
   return true;
}

int OOFPositionedMgr::getLeftBorder (OOFAwareWidget *widget, int y, int h,
                                     OOFAwareWidget *lastGen, int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getRightBorder (OOFAwareWidget *widget, int y, int h,
                                      OOFAwareWidget *lastGen, int lastExtIndex)
{
   return 0;
}

bool OOFPositionedMgr::hasFloatLeft (OOFAwareWidget *widget, int y, int h,
                                     OOFAwareWidget *lastGen, int lastExtIndex)
{
   return false;
}

bool OOFPositionedMgr::hasFloatRight (OOFAwareWidget *widget, int y, int h,
                                      OOFAwareWidget *lastGen, int lastExtIndex)
{
   return false;
}


int OOFPositionedMgr::getLeftFloatHeight (OOFAwareWidget *widget, int y, int h,
                                          OOFAwareWidget *lastGen,
                                          int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getRightFloatHeight (OOFAwareWidget *widget, int y, int h,
                                           OOFAwareWidget *lastGen,
                                           int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getClearPosition (OOFAwareWidget *widget)
{
   return 0;
}

bool OOFPositionedMgr::affectsLeftBorder (Widget *widget)
{
   return false;
}

bool OOFPositionedMgr::affectsRightBorder (Widget *widget)
{
   return false;
}

bool OOFPositionedMgr::mayAffectBordersAtAll ()
{
   return false;
}

bool OOFPositionedMgr::dealingWithSizeOfChild (Widget *child)
{
   return true;
}

int OOFPositionedMgr::getNumWidgets ()
{
   return children->size();
}

Widget *OOFPositionedMgr::getWidget (int i)
{
   return children->get(i)->widget;
}

bool OOFPositionedMgr::getPosBorder (style::Length cssValue, int refLength,
                                     int *result)
{
   if (style::isAbsLength (cssValue)) {
      *result = style::absLengthVal (cssValue);
      return true;
   }  else if (style::isPerLength (cssValue)) {
      *result = style::multiplyWithPerLength (refLength, cssValue);
      return true;
   } else
      // "false" means "undefined":
      return false;
}

bool OOFPositionedMgr::allChildrenConsideredForSize ()
{   
   for (int i = 0; i < children->size(); i++)
      if (!children->get(i)->consideredForSize)
         return false;
   return true;
}

bool OOFPositionedMgr::allChildrenConsideredForExtremes ()
{   
   for (int i = 0; i < children->size(); i++)
      if (!children->get(i)->consideredForExtremes)
         return false;
   return true;
}

} // namespace oof

} // namespace dw
