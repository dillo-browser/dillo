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

   containerAllocationState =
      container->wasAllocated () ? WAS_ALLOCATED : NOT_ALLOCATED;
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

      bool sizeChanged = doChildrenExceedContainer ();
      bool extremesChanged = haveExtremesChanged ();

      // Consider children which were ignored in getSize (see comment
      // there). The respective widgets were not allocated before if
      // and only if containerAllocationState == IN_ALLOCATION. For
      // the generator (and reference widget), this is the case as
      // long as sizeRequest is immediately followed by sizeAllocate
      // (as in resizeIdle).

      bool notAllChildrenConsideredBefore = false;
      for (int i = 0;
           !notAllChildrenConsideredBefore && i < children->size(); i++)
         if (!isPosCalculable (children->get(i),
                               containerAllocationState != IN_ALLOCATION))
            notAllChildrenConsideredBefore = true;

      if (sizeChanged || notAllChildrenConsideredBefore || extremesChanged)
         container->oofSizeChanged (extremesChanged);

      containerAllocationState = WAS_ALLOCATED;
   }

   DBG_OBJ_LEAVE ();
}


void OOFPositionedMgr::sizeAllocateChildren ()
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "sizeAllocateChildren");

   int refWidth = container->getAvailWidth (true) - containerBoxDiffWidth ();
   int refHeight = container->getAvailHeight (true) - containerBoxDiffHeight ();

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get (i);

      int x, y, width, ascent, descent;
      calcPosAndSizeChildOfChild (child, refWidth, refHeight, &x, &y, &width,
                                  &ascent, &descent);

      Allocation childAllocation;
      childAllocation.x = containerAllocation.x + x + containerBoxOffsetX ();
      childAllocation.y = containerAllocation.y + y + containerBoxOffsetY ();
      childAllocation.width = width;
      childAllocation.ascent = ascent;
      childAllocation.descent = descent;

      child->widget->sizeAllocate (&childAllocation);
   }
   
   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int i = 0; i < children->size(); i++)
      children->get(i)->widget->containerSizeChanged ();

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

bool OOFPositionedMgr::haveExtremesChanged ()
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "haveExtremesChanged");

   Extremes containerExtr;
   container->getExtremes (&containerExtr);
   bool changed = false;

   DBG_OBJ_MSG_START ();

   // Search for children which have not yet been considered by
   // getExtremes(). Cf. sizeAllocateEnd().

   for (int i = 0; i < children->size () && !changed; i++)
      if (!isHPosCalculable (children->get (i),
                             containerAllocationState == IN_ALLOCATION))
         changed = true;

   DBG_OBJ_MSG_END ();

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", changed ? "true" : "false");
   DBG_OBJ_LEAVE ();

   return changed;
}


void OOFPositionedMgr::draw (View *view, Rectangle *area,
                             DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      Rectangle childArea;
      if (!context->hasWidgetBeenDrawnAsInterruption (child->widget) &&
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

   Child *child = new Child (widget, generator, reference);
   children->put (child);
   childrenByWidget->put (new TypedPointer<Widget> (widget), child);

   int subRef = children->size() - 1;
   DBG_OBJ_SET_NUM ("children.size", children->size());
   DBG_OBJ_ARRSET_PTR ("children", children->size() - 1, widget);

   DBG_OBJ_SET_PTR_O (widget, "<Positioned>.generator", generator);
   DBG_OBJ_SET_PTR_O (widget, "<Positioned>.reference", reference);

   DBG_OBJ_MSGF ("construct.oofm", 1, "=> %d", subRef);
   DBG_OBJ_LEAVE ();
   return subRef;
}

void OOFPositionedMgr::moveExternalIndices (OOFAwareWidget *generator,
                                            int oldStartIndex, int diff)
{
}

void OOFPositionedMgr::markSizeChange (int ref)
{
}


void OOFPositionedMgr::markExtremesChange (int ref)
{
}

Widget *OOFPositionedMgr::getWidgetAtPoint (int x, int y,
                                            StackingIteratorStack
                                            *iteratorStack,
                                            Widget **interruptedWidget,
                                            int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPoint", "%d, %d", x, y);

   Widget *widgetAtPoint = NULL;
   OOFAwareWidget::OOFStackingIterator *osi =
      (OOFAwareWidget::OOFStackingIterator*)iteratorStack->getTop ();
   
   while (widgetAtPoint == NULL && *interruptedWidget == NULL && *index >= 0) {
      Widget *childWidget = children->get(*index)->widget;
      if (!osi->hasWidgetBeenDrawnAfterInterruption (childWidget) &&
          !StackingContextMgr::handledByStackingContextMgr (childWidget))
         widgetAtPoint =
            childWidget->getWidgetAtPointTotal (x, y, iteratorStack,
                                                interruptedWidget);
      
      if (*interruptedWidget == NULL)
         (*index)--;
   }

   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)",
                 widgetAtPoint, *interruptedWidget);
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

void OOFPositionedMgr::getSize (Requisition *containerReq, int *oofWidth,
                                int *oofHeight)
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "getSize");

   *oofWidth = *oofHeight = 0;

   int refWidth = container->getAvailWidth (true);
   int refHeight = container->getAvailHeight (true);

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      // The position of a child (which goes into the return value of
      // this method) can only be determined when the following
      // condition (if clause) is is fulfilled. Other children will be
      // considered later in sizeAllocateEnd.
      if (isPosCalculable (child,
                           containerAllocationState != NOT_ALLOCATED &&
                           child->generator->wasAllocated ())) {
         int x, y, width, ascent, descent;
         calcPosAndSizeChildOfChild (child, refWidth, refHeight, &x, &y, &width,
                                     &ascent, &descent);
         *oofWidth = max (*oofWidth, x + width) + containerBoxDiffWidth ();
         *oofHeight =
            max (*oofHeight, y + ascent + descent) + containerBoxDiffHeight ();
      }
   }      

   DBG_OBJ_MSGF ("resize.oofm", 0, "=> %d * %d", *oofWidth, *oofHeight);
   DBG_OBJ_LEAVE ();
}

bool OOFPositionedMgr::containerMustAdjustExtraSpace ()
{
   return true;
}

void OOFPositionedMgr::getExtremes (Extremes *containerExtr, int *oofMinWidth,
                                    int *oofMaxWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getExtremes", "(%d / %d), ...",
                  containerExtr->minWidth, containerExtr->maxWidth);

   *oofMinWidth = *oofMaxWidth = 0;

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      // If clause: see getSize().
      if (isHPosCalculable (child,
                            containerAllocationState != NOT_ALLOCATED
                            && child->generator->wasAllocated ())) {
         int x, width;
         Extremes childExtr;
         child->widget->getExtremes (&childExtr);
         
         // Here, we put the extremes of the container in relation to
         // the extremes of the child, as sizes are put in relation
         // for calculating the size. In one case, the allocation is
         // used: when neither "left" nor "right" is set, and so the
         // position told by the generator is used.
         //
         // If you look at the Textblock widget, you'll find that this
         // is always boxOffsetX(), and the horizontal position of a
         // textblock within its parent is also constant; so this is
         // not a problem.
         //
         // (TODO What about a table cell within a table?)

         calcHPosAndSizeChildOfChild (child, containerExtr->minWidth,
                                      childExtr.minWidth, &x, &width);
         *oofMinWidth = max (*oofMinWidth, x + width);

         calcHPosAndSizeChildOfChild (child, containerExtr->maxWidth,
                                      childExtr.maxWidth, &x, &width);
         *oofMaxWidth = max (*oofMaxWidth, x + width);
      }
   }      

   *oofMinWidth += containerBoxDiffWidth ();
   *oofMaxWidth += containerBoxDiffWidth ();

   DBG_OBJ_MSGF ("resize.oofm", 0, "=> %d / %d", *oofMinWidth, *oofMaxWidth);
   DBG_OBJ_LEAVE ();
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

int OOFPositionedMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize.oofm", 0,
                  "OOFPositionedMgr/getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;

   if (child->getStyle()->width == style::LENGTH_AUTO &&
       child->getStyle()->minWidth == style::LENGTH_AUTO &&
       child->getStyle()->maxWidth == style::LENGTH_AUTO) {
      // TODO This should (perhaps?) only used when 'width' is undefined.
      // TODO Is "boxDiffWidth()" correct here?
      DBG_OBJ_MSG ("resize.oofm", 1, "no specification");
      if (forceValue) {
         int availWidth = container->getAvailWidth (true);
         width = max (availWidth - containerBoxDiffWidth ()
                      // Regard an undefined value as 0:
                      - max (getPosLeft (child, availWidth), 0),
                      - max (getPosRight (child, availWidth), 0),
                      0);
      } else
         width = -1;
   } else {
      if (forceValue) {
         int availWidth = container->getAvailWidth (true);
         child->calcFinalWidth (child->getStyle(),
                                availWidth - containerBoxDiffWidth (), NULL,
                                0, true, &width);
      } else
         width = -1;
   }

   if (width != -1)
      width = max (width, child->getMinWidth (NULL, forceValue));

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();

   return width;  
}

int OOFPositionedMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize.oofm", 0,
                  "OOFPositionedMgr/getAvailHeightOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int height;

   if (child->getStyle()->height == style::LENGTH_AUTO &&
       child->getStyle()->minHeight == style::LENGTH_AUTO &&
       child->getStyle()->maxHeight == style::LENGTH_AUTO) {
      // TODO This should (perhaps?) only used when 'height' is undefined.
      // TODO Is "boxDiffHeight()" correct here?
      DBG_OBJ_MSG ("resize.oofm", 1, "no specification");
      if (forceValue) {
         int availHeight = container->getAvailHeight (true);
         height = max (availHeight - containerBoxDiffHeight ()
                       // Regard an undefined value as 0:
                       - max (getPosTop (child, availHeight), 0),
                       - max (getPosBottom (child, availHeight), 0),
                       0);
      } else
         height = -1;
   } else {
      if (forceValue) {
         int availHeight = container->getAvailHeight (true);
         height = child->calcHeight (child->getStyle()->height, true,
                                     availHeight - containerBoxDiffHeight (),
                                     NULL, true);
      } else
         height = -1;
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", height);
   DBG_OBJ_LEAVE ();

   return height;  
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

bool OOFPositionedMgr::isHPosComplete (Child *child)
{
   return (style::isAbsLength (child->widget->getStyle()->left) ||
           style::isPerLength (child->widget->getStyle()->left)) &&
      (style::isAbsLength (child->widget->getStyle()->right) ||
       style::isPerLength (child->widget->getStyle()->right));
}

bool OOFPositionedMgr::isVPosComplete (Child *child)
{
   return (style::isAbsLength (child->widget->getStyle()->top) ||
           style::isPerLength (child->widget->getStyle()->top)) &&
      (style::isAbsLength (child->widget->getStyle()->bottom) ||
       style::isPerLength (child->widget->getStyle()->bottom));
}

bool OOFPositionedMgr::isHPosCalculable (Child *child, bool allocated)
{
   return
      allocated || (isHPosComplete (child) && child->reference == container);
}

bool OOFPositionedMgr::isVPosCalculable (Child *child, bool allocated)
{
   return
      allocated || (isVPosComplete (child) && child->reference == container);
}

bool OOFPositionedMgr::isPosCalculable (Child *child, bool allocated)
{
   return isHPosCalculable (child, allocated) &&
      isVPosCalculable (child, allocated);
}

void OOFPositionedMgr::calcPosAndSizeChildOfChild (Child *child, int refWidth,
                                                   int refHeight, int *xPtr,
                                                   int *yPtr, int *widthPtr,
                                                   int *ascentPtr,
                                                   int *descentPtr)
{
   // *xPtr and *yPtr refer to reference area; caller must adjust them.

   DBG_OBJ_ENTER ("resize.oofm", 0, "calcPosAndSizeChildOfChild",
                  "%p, %d, %d, ...", child, refWidth, refHeight);

   // TODO (i) Consider {min|max}-{width|heigt}. (ii) Height is always
   // apportioned to descent (ascent is preserved), which makes sense
   // when the children are textblocks. (iii) Consider minimal width
   // (getMinWidth)?

   Requisition childRequisition;
   child->widget->sizeRequest (&childRequisition);

   calcHPosAndSizeChildOfChild (child, refWidth, childRequisition.width,
                                xPtr, widthPtr);
   calcVPosAndSizeChildOfChild (child, refHeight, childRequisition.ascent,
                                childRequisition.descent, yPtr, ascentPtr,
                                descentPtr);

   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::calcHPosAndSizeChildOfChild (Child *child, int refWidth,
                                                    int origChildWidth,
                                                    int *xPtr, int *widthPtr)
{
   assert (refWidth != -1 || (xPtr == NULL && widthPtr == NULL));

   int width;
   bool widthDefined;
   if (style::isAbsLength (child->widget->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "absolute width: %dpx",
                    style::absLengthVal (child->widget->getStyle()->width));
      width = style::absLengthVal (child->widget->getStyle()->width)
         + child->widget->boxDiffWidth ();
      widthDefined = true;
   } else if (style::isPerLength (child->widget->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "percentage width: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                             (child->widget->getStyle()->width));
      width = style::multiplyWithPerLength (refWidth,
                                             child->widget->getStyle()->width)
         + child->widget->boxDiffWidth ();
      widthDefined = true;
   } else {
      DBG_OBJ_MSG ("resize.oofm", 1, "width not specified");
      width = origChildWidth;
      widthDefined = false;
   }

   int left = getPosLeft (child->widget, refWidth),
      right = getPosRight (child->widget, refWidth);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> left = %d, right = %d, width = %d (defined: %s)",
                 left, right, width, widthDefined ? "true" : "false");

   if (xPtr) {
      if (left == -1 && right == -1) {
         assert (child->generator == container ||
                 (containerAllocationState != NOT_ALLOCATED
                  && child->generator->wasAllocated ()));
         *xPtr =
            child->x + (child->generator == container ? 0 :
                        child->generator->getAllocation()->x
                        - (containerAllocation.x + containerBoxOffsetX ()));
      } else {
         assert (child->reference == container ||
                 (containerAllocationState != NOT_ALLOCATED
                  && child->reference->wasAllocated ()));
         int xBase = child->reference == container ? 0 :
            child->generator->getAllocation()->x - containerAllocation.x;
         DBG_OBJ_MSGF ("resize.oofm", 0, "=> xBase = %d", xBase);

         if (left == -1 && right != -1)
            *xPtr = xBase + refWidth - width - right;
         else if (left != -1 && right == -1)
            *xPtr = xBase + left;
         else {
            *xPtr = xBase + left;
            if (!widthDefined) {
               width = refWidth - (left + right);
               DBG_OBJ_MSGF ("resize.oofm", 0, "=> width (corrected) = %d",
                             width);
            }
         }
      }

      DBG_OBJ_MSGF ("resize.oofm", 0, "=> x = %d", *xPtr);
   }

   if (widthPtr)
      *widthPtr = width;
}

void OOFPositionedMgr::calcVPosAndSizeChildOfChild (Child *child, int refHeight,
                                                    int origChildAscent,
                                                    int origChildDescent,
                                                    int *yPtr, int *ascentPtr,
                                                    int *descentPtr)
{
   assert (refHeight != -1 ||
           (yPtr == NULL && ascentPtr == NULL && descentPtr == NULL));

   int ascent = origChildAscent, descent = origChildDescent;
   bool heightDefined;

   if (style::isAbsLength (child->widget->getStyle()->height)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "absolute height: %dpx",
                    style::absLengthVal (child->widget->getStyle()->height));
      int height = style::absLengthVal (child->widget->getStyle()->height)
         + child->widget->boxDiffHeight ();
      splitHeightPreserveAscent (height, &ascent, &descent);
      heightDefined = true;
   } else if (style::isPerLength (child->widget->getStyle()->height)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "percentage height: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                             (child->widget->getStyle()->height));
      int height =
         style::multiplyWithPerLength (refHeight,
                                       child->widget->getStyle()->height)
         + child->widget->boxDiffHeight ();
      splitHeightPreserveAscent (height, &ascent, &descent);
      heightDefined = true;
   } else {
      DBG_OBJ_MSG ("resize.oofm", 1, "height not specified");
      heightDefined = false;
   }

   int top = getPosTop (child->widget, refHeight),
      bottom = getPosBottom (child->widget, refHeight);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> top = %d, bottom = %d, height = %d + %d (defined: %s)",
                 top, bottom, ascent, descent,
                 heightDefined ? "true" : "false");

   if (yPtr) {
      if (top == -1 && bottom == -1) {
         assert (child->generator == container ||
                 (containerAllocationState != NOT_ALLOCATED
                  && child->generator->wasAllocated ()));
         *yPtr =
            child->y + (child->generator == container ? 0 :
                        child->generator->getAllocation()->y
                        - (containerAllocation.y + containerBoxOffsetY ()));
      } else {
         assert (child->reference == container ||
                 (containerAllocationState != NOT_ALLOCATED
                  && child->reference->wasAllocated ()));
         int yBase = child->reference == container ? 0 :
            child->generator->getAllocation()->y - containerAllocation.y;
         DBG_OBJ_MSGF ("resize.oofm", 0, "=> yBase = %d", yBase);
            
         if (top == -1 && bottom != -1)
            *yPtr = yBase + refHeight - (ascent + descent) - bottom;
         else if (top != -1 && bottom == -1)
            *yPtr = yBase + top;
         else {
            *yPtr = yBase + top;
            if (!heightDefined) {
               int height = refHeight - (top + bottom);
               splitHeightPreserveAscent (height, &ascent, &descent);
               DBG_OBJ_MSGF ("resize.oofm", 0,
                             "=> ascent + descent (corrected) = %d + %d",
                             ascent, descent);
            }
         }
      }

      DBG_OBJ_MSGF ("resize.oofm", 0, "=> y = %d", *yPtr);
   }

   if (ascentPtr)
      *ascentPtr = ascent;
   if (descentPtr)
      *descentPtr = descent;
}

int OOFPositionedMgr::getNumWidgets ()
{
   return children->size();
}

Widget *OOFPositionedMgr::getWidget (int i)
{
   return children->get(i)->widget;
}

} // namespace oof

} // namespace dw
