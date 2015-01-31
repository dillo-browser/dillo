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
#include "oofawarewidget.hh"

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
   containerAllocation = *allocation;
}

void OOFPosRelMgr::sizeAllocateEnd (OOFAwareWidget *caller)
{
   if (caller == container) {
      for (int i = 0; i < children->size (); i++) {
         Child *child = children->get(i);
         
         Requisition childReq;      
         child->widget->sizeRequest (&childReq);
         
         Allocation childAlloc;
         childAlloc.x = containerAllocation.x +
            container->getStyle()->boxOffsetX () + child->x;
         childAlloc.y = containerAllocation.y +
            container->getStyle()->boxOffsetY () + child->y;
         childAlloc.width = childReq.width;
         childAlloc.ascent = childReq.ascent;
         childAlloc.descent = childReq.descent;
         child->widget->sizeAllocate (&childAlloc);
      }
   }
}

void OOFPosRelMgr::getSize (Requisition *containerReq, int *oofWidth,
                            int *oofHeight)
{
   *oofWidth = *oofHeight = 0;

   for (int i = 0; i < children->size (); i++) {
      Child *child = children->get(i);
      Requisition childReq;      
      child->widget->sizeRequest (&childReq);
      *oofWidth = max (*oofWidth, child->x + childReq.width);
      *oofHeight = max (*oofHeight,
                        child->y + childReq.ascent + childReq.descent);
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
      *oofMinWidth = max (*oofMinWidth, child->x + childExtr.minWidth);
      *oofMaxWidth = max (*oofMaxWidth, child->x + childExtr.maxWidth);
   }
}

int OOFPosRelMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   TypedPointer<Widget> key (child);
   Child *tshild = childrenByWidget->get (&key);
   assert (tshild);
   return tshild->generator->getAvailWidthOfChild (child, forceValue);
}

int OOFPosRelMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   TypedPointer<Widget> key (child);
   Child *tshild = childrenByWidget->get (&key);
   assert (tshild);
   return tshild->generator->getAvailHeightOfChild (child, forceValue);
}

bool OOFPosRelMgr::isReference (Widget *widget)
{
   // TODO Remove soon. This implementation will imply reference = container.
   return false;
}

} // namespace oof

} // namespace dw
