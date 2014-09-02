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
#include "textblock.hh"
#include "../lout/debug.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

OOFPositionedMgr::OOFPositionedMgr (Textblock *containingBlock)
{
   DBG_OBJ_CREATE ("dw::OOFPositionedMgr");

   this->containingBlock = (Textblock*)containingBlock;
   children = new Vector<Widget> (1, false);
   DBG_OBJ_SET_NUM ("children.size", children->size());
}

OOFPositionedMgr::~OOFPositionedMgr ()
{
   delete children;

   DBG_OBJ_DELETE ();
}

void OOFPositionedMgr::sizeAllocateStart (Textblock *caller,
                                          Allocation *allocation)
{
   containingBlockAllocation = *allocation;
}

void OOFPositionedMgr::sizeAllocateEnd (Textblock *caller)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateEnd", "%p", caller);

   if (caller == containingBlock) {
      sizeAllocateChildren ();

      bool sizeChanged = doChildrenExceedCB ();
      bool extremesChanged = haveExtremesChanged ();
      if (sizeChanged || extremesChanged)
         containingBlock->oofSizeChanged (extremesChanged);
   }

   DBG_OBJ_LEAVE ();
}


void OOFPositionedMgr::sizeAllocateChildren ()
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "sizeAllocateChildren");

   int refWidth = containingBlock->getAvailWidth (true) - cbBoxDiffWidth ();
   int refHeight = containingBlock->getAvailHeight (true) - cbBoxDiffHeight ();

   for (int i = 0; i < children->size(); i++) {
      Widget *child = children->get (i);

      int x, y, width, ascent, descent;
      calcPosAndSizeChildOfChild (child, refWidth, refHeight, &x, &y, &width,
                                  &ascent, &descent);

      Allocation childAllocation;
      childAllocation.x = containingBlockAllocation.x + x + cbBoxOffsetX ();
      childAllocation.y = containingBlockAllocation.y + y + cbBoxOffsetY ();
      childAllocation.width = width;
      childAllocation.ascent = ascent;
      childAllocation.descent = descent;

      child->sizeAllocate (&childAllocation);
   }
   
   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int i = 0; i < children->size(); i++)
      children->get(i)->containerSizeChanged ();

   DBG_OBJ_LEAVE ();
}

bool OOFPositionedMgr::doChildrenExceedCB ()
{
   // TODO Something similar like this for positioned elements.

#if 0
   DBG_OBJ_ENTER ("resize.oofm", 0, "doFloatsExceedCB", "%s",
                  side == LEFT ? "LEFT" : "RIGHT");

   // This method is called to determine whether the *requisition* of
   // the CB must be recalculated. So, we check the float allocations
   // against the *requisition* of the CB, which may (e. g. within
   // tables) differ from the new allocation. (Generally, a widget may
   // allocated at a different size.)
   Requisition cbReq;
   containingBlock->sizeRequest (&cbReq);

   for (int i = 0; i < children->size () && !exceeds; i++) {
      Widget *child = children->get (i);
      Allocation *childAlloc = child->getAllocation ();
      DBG_OBJ_MSGF ("resize.oofm", 2,
                    "Does childAlloc = (%d, %d, %d * %d) exceed CBA = "
                    "(%d, %d, %d * %d)?",
                    childAlloc->x, childAlloc->y, childAlloc->width,
                    childAlloc->ascent + childAlloc->descent,
                    containingBlockAllocation.x, containingBlockAllocation.y,
                    cbReq.width, cbReq.ascent + cbReq.descent);
      if (childAlloc->x + childAlloc->width
          > containingBlockAllocation.x + cbReq.width ||
          childAlloc->y + childAlloc->ascent + childAlloc->descent
          > containingBlockAllocation.y + cbReq.ascent + cbReq.descent) {
         exceeds = true;
         DBG_OBJ_MSG ("resize.oofm", 2, "Yes.");
      } else
         DBG_OBJ_MSG ("resize.oofm", 2, "No.");
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", exceeds ? "true" : "false");
   DBG_OBJ_LEAVE ();

   return exceeds;
#endif

   return false;
}

bool OOFPositionedMgr::haveExtremesChanged ()
{
   // TODO Something to do?
   return false;
}


void OOFPositionedMgr::draw (View *view, Rectangle *area)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   for (int i = 0; i < children->size(); i++) {
      Widget *child = children->get(i);
      Rectangle childArea;
      if (child->intersects (area, &childArea))
         child->draw (view, &childArea);
   }

   DBG_OBJ_LEAVE ();
}


void OOFPositionedMgr::addWidgetInFlow (Textblock *textblock,
                                    Textblock *parentBlock, int externalIndex)
{
}

int OOFPositionedMgr::addWidgetOOF (Widget *widget, Textblock *generatingBlock,
                                 int externalIndex)
{
   DBG_OBJ_ENTER ("construct.oofm", 0, "addWidgetOOF", "%p, %p, %d",
                  widget, generatingBlock, externalIndex);

   children->put (widget);
   int subRef = children->size() - 1;
   DBG_OBJ_SET_NUM ("children.size", children->size());
   DBG_OBJ_ARRSET_PTR ("children", children->size() - 1, widget);

   DBG_OBJ_MSGF ("construct.oofm", 1, "=> %d", subRef);
   DBG_OBJ_LEAVE ();
   return subRef;
}

void OOFPositionedMgr::moveExternalIndices (Textblock *generatingBlock,
                                            int oldStartIndex, int diff)
{
}

void OOFPositionedMgr::markSizeChange (int ref)
{
}


void OOFPositionedMgr::markExtremesChange (int ref)
{
}

Widget *OOFPositionedMgr::getWidgetAtPoint (int x, int y, int level)
{
   for (int i = 0; i < children->size(); i++) {
      Widget *child = children->get(i);
      if (child->wasAllocated ()) {
         Widget *childAtPoint = child->getWidgetAtPoint (x, y, level + 1);
         if (childAtPoint)
            return childAtPoint;
      }
   }

   return NULL;
}

void OOFPositionedMgr::tellPosition (Widget *widget, int yReq)
{
}

void OOFPositionedMgr::getSize (Requisition *cbReq, int *oofWidth,
                                int *oofHeight)
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "getSize");

   *oofWidth = *oofHeight = 0;

   int refWidth = containingBlock->getAvailWidth (true);
   int refHeight = containingBlock->getAvailHeight (true);

   for (int i = 0; i < children->size(); i++) {
      Widget *child = children->get(i);
      int x, y, width, ascent, descent;
      calcPosAndSizeChildOfChild (child, refWidth, refHeight, &x, &y, &width,
                                  &ascent, &descent);
      *oofWidth = max (*oofWidth, x + width) + cbBoxDiffWidth ();
      *oofHeight = max (*oofHeight, y + ascent + descent) + cbBoxDiffHeight ();
   }      

   DBG_OBJ_LEAVE ();
}

void OOFPositionedMgr::getExtremes (Extremes *cbExtr, int *oofMinWidth,
                                    int *oofMaxWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getExtremes", "(%d / %d), ...",
                  cbExtr->minWidth, cbExtr->maxWidth);

   // TODO Something to do?
   *oofMinWidth = *oofMaxWidth = 0;

   DBG_OBJ_LEAVE ();
}


int OOFPositionedMgr::getLeftBorder (Textblock *textblock, int y, int h,
                                     Textblock *lastGB, int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getRightBorder (Textblock *textblock, int y, int h,
                                      Textblock *lastGB, int lastExtIndex)
{
   return 0;
}

bool OOFPositionedMgr::hasFloatLeft (Textblock *textblock, int y, int h,
                                 Textblock *lastGB, int lastExtIndex)
{
   return false;
}

bool OOFPositionedMgr::hasFloatRight (Textblock *textblock, int y, int h,
                                  Textblock *lastGB, int lastExtIndex)
{
   return false;
}


int OOFPositionedMgr::getLeftFloatHeight (Textblock *textblock, int y, int h,
                                      Textblock *lastGB, int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getRightFloatHeight (Textblock *textblock, int y, int h,
                                       Textblock *lastGB, int lastExtIndex)
{
   return 0;
}

int OOFPositionedMgr::getClearPosition (Textblock *textblock)
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
         int availWidth = containingBlock->getAvailWidth (true);
         width = max (availWidth - cbBoxDiffWidth ()
                      // Regard an undefined value as 0:
                      - max (getPosLeft (child, availWidth), 0),
                      - max (getPosRight (child, availWidth), 0),
                      0);
      } else
         width = -1;
   } else {
      if (forceValue) {
         int availWidth = containingBlock->getAvailWidth (true);
         child->calcFinalWidth (child->getStyle(),
                                availWidth - cbBoxDiffWidth (), NULL, 0, true,
                                &width);
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
         int availHeight = containingBlock->getAvailHeight (true);
         height = max (availHeight - cbBoxDiffHeight ()
                       // Regard an undefined value as 0:
                       - max (getPosTop (child, availHeight), 0),
                       - max (getPosBottom (child, availHeight), 0),
                       0);
      } else
         height = -1;
   } else {
      if (forceValue) {
         int availHeight = containingBlock->getAvailHeight (true);
         height = child->calcHeight (child->getStyle()->height, true,
                                     availHeight - - cbBoxDiffHeight (), NULL,
                                     true);
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

void OOFPositionedMgr::calcPosAndSizeChildOfChild (Widget *child, int refWidth,
                                                   int refHeight, int *x,
                                                   int *y, int *width,
                                                   int *ascent, int *descent)
{
   // *x and *y refer to reference area; caller must adjust them.

   DBG_OBJ_ENTER ("resize.oofm", 0, "calcPosAndSizeChildOfChild",
                  "%p, %d, %d, ...", child, refWidth, refHeight);

   // TODO (i) Consider {min|max}-{width|heigt}. (ii) Height is always
   // apportioned to descent (ascent is preserved), which makes sense
   // when the children are textblocks. (iii) Consider minimal width
   // (getMinWidth)?

   Requisition childRequisition;
   child->sizeRequest (&childRequisition);

   bool widthDefined;
   if (style::isAbsLength (child->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "absolute width: %dpx",
                    style::absLengthVal (child->getStyle()->width));
      *width = style::absLengthVal (child->getStyle()->width)
         + child->boxDiffWidth ();
      widthDefined = true;
   } else if (style::isPerLength (child->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "percentage width: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                             (child->getStyle()->width));
      *width = style::multiplyWithPerLength (refWidth,
                                             child->getStyle()->width)
         + child->boxDiffWidth ();
      widthDefined = true;
   } else {
      DBG_OBJ_MSG ("resize.oofm", 1, "width not specified");
      *width = childRequisition.width;
      widthDefined = false;
   }

   int left = getPosLeft (child, refWidth),
      right = getPosRight (child, refWidth);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "left = %d, right = %d, width = %d (defined: %s)",
                 left, right, *width, widthDefined ? "true" : "false");

   if (left == -1 && right == -1)
      *x = 0;
   else if (left == -1 && right != -1)
      *x = refWidth - *width - right;
   else if (left != -1 && right == -1)
      *x = left;
   else {
      *x = left;
      if (!widthDefined)
         *width = refWidth - (left + right);
   }

   bool heightDefined;
   *ascent = childRequisition.ascent;
   *descent = childRequisition.descent;
   if (style::isAbsLength (child->getStyle()->height)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "absolute height: %dpx",
                    style::absLengthVal (child->getStyle()->height));
      int height = style::absLengthVal (child->getStyle()->height)
         + child->boxDiffHeight ();
      splitHeightPreserveAscent (height, ascent, descent);
      heightDefined = true;
   } else if (style::isPerLength (child->getStyle()->height)) {
      DBG_OBJ_MSGF ("resize.oofm", 1, "percentage height: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                             (child->getStyle()->height));
      int height = style::multiplyWithPerLength (refHeight,
                                                 child->getStyle()->height)
         + child->boxDiffHeight ();
      splitHeightPreserveAscent (height, ascent, descent);
      heightDefined = true;
   } else {
      DBG_OBJ_MSG ("resize.oofm", 1, "height not specified");
      heightDefined = false;
   }

   int top = getPosTop (child, refHeight),
      bottom = getPosBottom (child, refHeight);
   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "top = %d, bottom = %d, height = %d + %d (defined: %s)",
                 top, bottom, *ascent, *descent,
                 heightDefined ? "true" : "false");

   if (top == -1 && bottom == -1)
      *y = 0;
   else if (top == -1 && bottom != -1)
      *y = refHeight - (*ascent + *descent) - bottom;
   else if (top != -1 && bottom == -1)
      *y = top;
   else {
      *y = top;
      if (!heightDefined) {
         int height = refHeight - (top + bottom);
         splitHeightPreserveAscent (height, ascent, descent);
      }
   }

   DBG_OBJ_MSGF ("resize.oofm", 0, "=> %d, %d, %d * (%d + %d)",
                 *x, *y, *width, *ascent, *descent);
   DBG_OBJ_LEAVE ();
}

int OOFPositionedMgr::getNumWidgets ()
{
   return children->size();
}

Widget *OOFPositionedMgr::getWidget (int i)
{
   return children->get (i);
}

} // namespace dw
