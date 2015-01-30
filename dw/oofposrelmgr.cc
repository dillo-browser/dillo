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

void OOFPosRelMgr::calcWidgetRefSize (core::Widget *widget,
                                      core::Requisition *size)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "calcWidgetRefSize", "%p", widget);
   widget->sizeRequest (size);
   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)",
                      size->width, size->ascent, size->descent);
}

bool OOFPosRelMgr::isReference (core::Widget *widget)
{
   // TODO Remove soon. This implementation will imply reference = container.
   return false;
}

// TODO: Check all containerBox* implementations.

int OOFPosRelMgr::containerBoxOffsetX ()
{
   return container->getParent () ?
      container->boxOffsetX () - container->getStyle()->padding.left : 0;
}

int OOFPosRelMgr::containerBoxOffsetY ()
{
   return container->getParent () ?
      container->boxOffsetY () - container->getStyle()->padding.top : 0;
}

int OOFPosRelMgr::containerBoxRestWidth ()
{
   return container->getParent () ?
      container->boxRestWidth () - container->getStyle()->padding.right : 0;
}

int OOFPosRelMgr::containerBoxRestHeight ()
{
   return container->getParent () ?
      container->boxRestHeight () - container->getStyle()->padding.bottom : 0;
}

} // namespace oof

} // namespace dw
