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
#include "oofawarewidget.hh"
#include "../lout/debug.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

namespace oof {

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

   Widget *reference = NULL;

   for (Widget *widget2 = generator; reference == NULL && widget2 != container;
        widget2 = widget2->getParent ())
      if (isReference (widget2))
         reference = widget2;

   if (reference == NULL)
      reference = container;

   Child *child = new Child (widget, generator, reference, externalIndex);
   children->put (child);
   childrenByWidget->put (new TypedPointer<Widget> (widget), child);

   int subRef = children->size() - 1;
   DBG_OBJ_SET_NUM ("children.size", children->size());
   DBG_OBJ_ARRSET_PTR ("children", children->size() - 1, widget);

   DBG_OBJ_SET_PTR_O (widget, "<Positioned>.generator", generator);
   DBG_OBJ_SET_PTR_O (widget, "<Positioned>.reference", reference);
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

void OOFPositionedMgr::tellPosition (Widget *widget, int x, int y)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "tellPosition", "%p, %d, %d",
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

int OOFPositionedMgr::getPosBorder (style::Length cssValue, int refLength)
{
   if (style::isAbsLength (cssValue))
      return style::absLengthVal (cssValue);
   else if (style::isPerLength (cssValue))
      return style::multiplyWithPerLength (refLength, cssValue);
   else
      // -1 means "undefined":
      return -1;
}

} // namespace oof

} // namespace dw
