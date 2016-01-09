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
   DBG_OBJ_CREATE ("dw::oof::OOFPosRelMgr");
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

   // In some cases, the widget has been enlarged for widgets out of
   // flow. Partly, this is done by adding "extra space"; however, at
   // this point, the extra space is not relevant here. See
   // "oofawarewidget.cc" for a calculation of RequisitionWithoutOOF.
   // (Notice also that Widget::sizeRequest has to be called in all
   // cases.)

   if (widget->instanceOf (OOFAwareWidget::CLASS_ID))
      *size = *((OOFAwareWidget*)widget)->getRequisitionWithoutOOF ();

   
   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)",
                      size->width, size->ascent, size->descent);
}


void OOFPosRelMgr::sizeAllocateChildren ()
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "sizeAllocateChildren");

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);
         
      Requisition childReq;      
      child->widget->sizeRequest (&childReq);
         
      Allocation childAlloc;
      childAlloc.x = containerAllocation.x + getChildPosX (child);
      childAlloc.y = containerAllocation.y + getChildPosY (child);
      childAlloc.width = childReq.width;
      childAlloc.ascent = childReq.ascent;
      childAlloc.descent = childReq.descent;
      child->widget->sizeAllocate (&childAlloc);
   }

   DBG_OBJ_LEAVE ();
}

void OOFPosRelMgr::getSize (Requisition *containerReq, int *oofWidth,
                            int *oofHeight)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getSize", "%d * (%d + %d)",
                  containerReq->width, containerReq->ascent,
                  containerReq->descent);

   *oofWidth = *oofHeight = 0;

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);

      // Children whose position cannot be determined will be
      // considered later in sizeAllocateEnd.
      if (posXDefined (child) && posYDefined (child)) {
         Requisition childReq;
         child->widget->sizeRequest (&childReq);
         *oofWidth = max (*oofWidth, getChildPosX (child) + childReq.width);
         *oofHeight = max (*oofHeight,
                           getChildPosY (child) + childReq.ascent
                           + childReq.descent);
         
         child->consideredForSize = true;
      } else
         child->consideredForSize = false;
   }

   DBG_OBJ_LEAVE_VAL ("%d * %d", *oofWidth, *oofHeight);
}

void OOFPosRelMgr::getExtremes (Extremes *containerExtr, int *oofMinWidth,
                                int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);

      // Children whose position cannot be determined will be
      // considered later in sizeAllocateEnd.
      if (posXDefined (child)) {
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

         child->consideredForExtremes = true;
      } else
         child->consideredForExtremes = false;
   }
}

bool OOFPosRelMgr::posXAbsolute (Child *child)
{
   return false;
}

bool OOFPosRelMgr::posYAbsolute (Child *child)
{
   return false;
}

int OOFPosRelMgr::getChildPosX (Child *child, int refWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getChildPosX", "[%p], %d",
                  child->widget, refWidth);

   int gx = generatorPosX (child);
   int dim = getChildPosDim (child->widget->getStyle()->left,
                             child->widget->getStyle()->right,
                             child->x,
                             refWidth
                             - child->widget->getStyle()->boxDiffWidth ());

   DBG_OBJ_LEAVE_VAL ("%d + %d = %d", gx, dim, gx + dim);
   return gx + dim;
}


int OOFPosRelMgr::getChildPosY (Child *child, int refHeight)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getChildPosY", "[%p], %d",
                  child->widget, refHeight);

   int gy = generatorPosY (child);
   int dim = getChildPosDim (child->widget->getStyle()->top,
                             child->widget->getStyle()->bottom,
                             child->y,
                             refHeight
                             - child->widget->getStyle()->boxDiffHeight ());

   DBG_OBJ_LEAVE_VAL ("%d + %d = %d", gy, dim, gy + dim);
   return gy + dim;
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

bool OOFPosRelMgr::dealingWithSizeOfChild (Widget *child)
{
   return false;
}

int OOFPosRelMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   notImplemented("OOFPosRelMgr::getAvailWidthOfChild");
   return 0;
}

int OOFPosRelMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   notImplemented ("OOFPosRelMgr::getAvailHeightOfChild");
   return 0;
}

} // namespace oof

} // namespace dw
