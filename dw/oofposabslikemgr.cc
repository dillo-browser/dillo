/*
 * Dillo Widget
 *
 * Copyright 2015 Sebastian Geerken <sgeerken@dillo.org>
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

#include "oofposabslikemgr.hh"

using namespace dw::core;
using namespace lout::misc;

namespace dw {

namespace oof {

OOFPosAbsLikeMgr::OOFPosAbsLikeMgr (OOFAwareWidget *container) :
   OOFPositionedMgr (container)
{
   DBG_OBJ_CREATE ("dw::OOFPosAbsLikeMgr");
}

OOFPosAbsLikeMgr::~OOFPosAbsLikeMgr ()
{
   DBG_OBJ_DELETE ();
}

void OOFPosAbsLikeMgr::calcWidgetRefSize (Widget *widget, Requisition *size)
{
   size->width = size->ascent = size->descent = 0;
}

void OOFPosAbsLikeMgr::sizeAllocateStart (OOFAwareWidget *caller,
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

void OOFPosAbsLikeMgr::sizeAllocateEnd (OOFAwareWidget *caller)
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

void OOFPosAbsLikeMgr::sizeAllocateChildren ()
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

bool OOFPosAbsLikeMgr::doChildrenExceedContainer ()
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

void OOFPosAbsLikeMgr::getSize (Requisition *containerReq, int *oofWidth,
                                int *oofHeight)
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "getSize");

   *oofWidth = *oofHeight = 0;

   int refWidth = container->getAvailWidth (true);
   int refHeight = container->getAvailHeight (true);

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      // Children whose position cannot be determined will be
      // considered later in sizeAllocateEnd.
      if (posXDefined (child) && posYDefined (child)) {
         int x, y, width, ascent, descent;
         calcPosAndSizeChildOfChild (child, refWidth, refHeight, &x, &y, &width,
                                     &ascent, &descent);
         *oofWidth = max (*oofWidth, x + width) + containerBoxDiffWidth ();
         *oofHeight =
            max (*oofHeight, y + ascent + descent) + containerBoxDiffHeight ();

         child->consideredForSize = true;
      } else
         child->consideredForSize = false;
   }      

   DBG_OBJ_MSGF ("resize.oofm", 0, "=> %d * %d", *oofWidth, *oofHeight);
   DBG_OBJ_LEAVE ();
}

void OOFPosAbsLikeMgr::getExtremes (Extremes *containerExtr, int *oofMinWidth,
                                    int *oofMaxWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getExtremes", "(%d / %d), ...",
                  containerExtr->minWidth, containerExtr->maxWidth);

   *oofMinWidth = *oofMaxWidth = 0;

   for (int i = 0; i < children->size(); i++) {
      Child *child = children->get(i);

      // If clause: see getSize().
      if (posXDefined (child)) {
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

         child->consideredForExtremes = true;
      } else
         child->consideredForExtremes = false;
   }      

   *oofMinWidth += containerBoxDiffWidth ();
   *oofMaxWidth += containerBoxDiffWidth ();

   DBG_OBJ_MSGF ("resize.oofm", 0, "=> %d / %d", *oofMinWidth, *oofMaxWidth);
   DBG_OBJ_LEAVE ();
}

int OOFPosAbsLikeMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
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
         int availWidth = container->getAvailWidth (true), left, right;

         // Regard undefined values as 0:
         if (!getPosLeft (child, availWidth, &left)) left = 0;
         if (!getPosRight (child, availWidth, &right)) right = 0;

         width = max (availWidth - containerBoxDiffWidth () - left - right, 0);
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

int OOFPosAbsLikeMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
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
         int availHeight = container->getAvailHeight (true), top, bottom;

         // Regard undefined values as 0:
         if (!getPosTop (child, availHeight, &top)) top = 0;
         if (!getPosBottom (child, availHeight, &bottom)) bottom = 0;

         height =
            max (availHeight - containerBoxDiffHeight () - top - bottom, 0);
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

bool OOFPosAbsLikeMgr::posXAbsolute (Child *child)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "posXAbsolute", "[%p]", child->widget);
   bool b =
      (style::isAbsLength (child->widget->getStyle()->left) ||
       style::isPerLength (child->widget->getStyle()->left)) &&
      (style::isAbsLength (child->widget->getStyle()->right) ||
       style::isPerLength (child->widget->getStyle()->right));
   DBG_OBJ_LEAVE_VAL ("%s", boolToStr (b));
   return b;
}

bool OOFPosAbsLikeMgr::posYAbsolute (Child *child)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "posYAbsolute", "[%p]", child->widget);
   bool b =
      (style::isAbsLength (child->widget->getStyle()->top) ||
       style::isPerLength (child->widget->getStyle()->top)) &&
      (style::isAbsLength (child->widget->getStyle()->bottom) ||
       style::isPerLength (child->widget->getStyle()->bottom));
   DBG_OBJ_LEAVE_VAL ("%s", boolToStr (b));
   return b;
}

void OOFPosAbsLikeMgr::calcPosAndSizeChildOfChild (Child *child, int refWidth,
                                                   int refHeight, int *xPtr,
                                                   int *yPtr, int *widthPtr,
                                                   int *ascentPtr,
                                                   int *descentPtr)
{
   // *xPtr and *yPtr refer to reference area; caller must adjust them.

   DBG_OBJ_ENTER ("resize.oofm", 0, "calcPosAndSizeChildOfChild",
                  "[%p], %d, %d, ...", child->widget, refWidth, refHeight);

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

void OOFPosAbsLikeMgr::calcHPosAndSizeChildOfChild (Child *child, int refWidth,
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

   int left, right;
   bool leftDefined = getPosLeft (child->widget, refWidth, &left),
      rightDefined = getPosRight (child->widget, refWidth, &right);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> left = %d, right = %d, width = %d (defined: %s)",
                 left, right, width, widthDefined ? "true" : "false");

   if (xPtr) {
      if (!leftDefined && !rightDefined)
         *xPtr = generatorPosX (child) + child->x;
      else {
         if (!leftDefined && rightDefined)
            *xPtr = refWidth - width - right;
         else if (leftDefined && !rightDefined)
            *xPtr = left;
         else {
            *xPtr = left;
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

void OOFPosAbsLikeMgr::calcVPosAndSizeChildOfChild (Child *child, int refHeight,
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

   int top, bottom;
   bool topDefined = getPosTop (child->widget, refHeight, &top),
      bottomDefined = getPosBottom (child->widget, refHeight, &bottom);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> top = %d, bottom = %d, height = %d + %d (defined: %s)",
                 top, bottom, ascent, descent,
                 heightDefined ? "true" : "false");

   if (yPtr) {
      if (!topDefined && !bottomDefined)
         *yPtr = generatorPosY (child) + child->y;
      else {
         if (!topDefined && bottomDefined)
            *yPtr = refHeight - (ascent + descent) - bottom;
         else if (topDefined && !bottomDefined)
            *yPtr = top;
         else {
            *yPtr = top;
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

} // namespace oof

} // namespace dw
