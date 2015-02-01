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

#include "oofposrelmgr.hh"

using namespace dw::core;
using namespace lout::object;
using namespace lout::misc;

namespace dw {

namespace oof {

OOFPosRelMgr::OOFPosRelMgr (OOFAwareWidget *container) :
   OOFPositionedMgr (container)
{
   DBG_OBJ_CREATE ("dw::OOFPosRelMgr");
}

OOFPosRelMgr::~OOFPosRelMgr ()
{
   DBG_OBJ_DELETE ();
}


void OOFPosRelMgr::markSizeChange (int ref)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "markSizeChange", "%d", ref);
   Child *child = children->get(ref);
   DBG_OBJ_MSGF ("resize.oofm", 1, "generator = %p, externalIndex = %d",
                 child->generator, child->externalIndex);
   child->generator->widgetRefSizeChanged (child->externalIndex);
   DBG_OBJ_LEAVE ();
}

void OOFPosRelMgr::markExtremesChange (int ref)
{
}

void OOFPosRelMgr::calcWidgetRefSize (Widget *widget, Requisition *size)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "calcWidgetRefSize", "%p", widget);
   widget->sizeRequest (size);
   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)",
                      size->width, size->ascent, size->descent);
}


void OOFPosRelMgr::sizeAllocateStart (OOFAwareWidget *caller,
                                      Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateStart",
                  "%p, (%d, %d, %d * (%d + %d))",
                  caller, allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   if (caller == container)
      containerAllocation = *allocation;

   DBG_OBJ_LEAVE ();
}

void OOFPosRelMgr::sizeAllocateEnd (OOFAwareWidget *caller)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateEnd", "%p", caller);

   if (caller == container) {
      for (int i = 0; i < children->size (); i++) {
         Child *child = children->get(i);
         
         Requisition childReq;      
         child->widget->sizeRequest (&childReq);
         
         Allocation *genAlloc = child->generator == container ?
            &containerAllocation : child->generator->getAllocation (),
            childAlloc;
         childAlloc.x = genAlloc->x + getChildPosX (child);
         childAlloc.y = genAlloc->y + getChildPosY (child);
         childAlloc.width = childReq.width;
         childAlloc.ascent = childReq.ascent;
         childAlloc.descent = childReq.descent;
         child->widget->sizeAllocate (&childAlloc);
      }
   }

   DBG_OBJ_LEAVE ();
}

void OOFPosRelMgr::getSize (Requisition *containerReq, int *oofWidth,
                            int *oofHeight)
{
   *oofWidth = *oofHeight = 0;

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);
      Requisition childReq;
      child->widget->sizeRequest (&childReq);
      *oofWidth = max (*oofWidth, getChildPosX (child) + childReq.width);
      *oofHeight = max (*oofHeight,
                        getChildPosX (child) + childReq.ascent
                        + childReq.descent);
   }
}

void OOFPosRelMgr::getExtremes (Extremes *containerExtr, int *oofMinWidth,
                                int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);
      Extremes childExtr;      
      child->widget->getExtremes (&childExtr);

      // Put the extremes of the container in relation to the extremes
      // of the child, as in OOFPosAbsLikeMgr::getExtremes (see
      // comment there).
      *oofMinWidth = max (*oofMinWidth,
                          getChildPosX (child, containerExtr->minWidth)
                          + childExtr.minWidth);
      *oofMaxWidth = max (*oofMaxWidth,
                          getChildPosX (child, containerExtr->maxWidth)
                          + childExtr.maxWidth);
   }
}

int OOFPosRelMgr::getChildPosDim (style::Length posCssValue,
                                  style::Length negCssValue, int refPos,
                                  int refLength)
{
   // posCssValue refers to "left" or "top", negCssValue refers to "right" or
   // "bottom". The former values are preferred ("left" over "right" etc.),
   // which should later depend on the CSS value "direction".

   DBG_OBJ_ENTER ("resize.oofm", 0, "getChildPosDim",
                  "<i>%d</i>, <i>%d</i>, %d, %d",
                  posCssValue, negCssValue, refPos, refLength);
   
   int diff;
   if (getPosBorder (posCssValue, refLength, &diff))
      DBG_OBJ_MSGF ("resize.oofm", 1, "posCssValue: diff = %d", diff);
   else {
      if (getPosBorder (negCssValue, refLength, &diff)) {
         DBG_OBJ_MSGF ("resize.oofm", 1, "negCssValue: diff = %d", diff);
         diff *= -1;
      } else
         diff = 0;
   }

   DBG_OBJ_LEAVE_VAL ("%d + %d = %d", refPos, diff, refPos + diff);
   return refPos + diff;
}

int OOFPosRelMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getAvailWidthOfChild", "%p, %s",
                  child, boolToStr (forceValue));

   TypedPointer<Widget> key (child);
   Child *tshild = childrenByWidget->get (&key);
   assert (tshild);
   int width = tshild->generator->getAvailWidthOfChild (child, forceValue);

   DBG_OBJ_LEAVE_VAL ("%d", width);
   return width;
}

int OOFPosRelMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getAvailWidthOfChild", "%p, %s",
                  child, boolToStr (forceValue));

   TypedPointer<Widget> key (child);
   Child *tshild = childrenByWidget->get (&key);
   assert (tshild);
   int height = tshild->generator->getAvailHeightOfChild (child, forceValue);

   DBG_OBJ_LEAVE_VAL ("%d", height);
   return height;
}

bool OOFPosRelMgr::isReference (Widget *widget)
{
   // TODO Remove soon. This implementation will imply reference = container.
   return false;
}

} // namespace oof

} // namespace dw
