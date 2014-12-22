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

#include "oofposfixedmgr.hh"

namespace dw {

namespace oof {

OOFPosFixedMgr::OOFPosFixedMgr (OOFAwareWidget *container) :
   OOFPositionedMgr (container)
{
   DBG_OBJ_CREATE ("dw::OOFPosFixedMgr");
}

OOFPosFixedMgr::~OOFPosFixedMgr ()
{
   DBG_OBJ_DELETE ();
}

bool OOFPosFixedMgr::isReference (core::Widget *widget)
{
   return widget->getParent () == NULL;
}

int OOFPosFixedMgr::containerBoxOffsetX ()
{
   return 0;
}

int OOFPosFixedMgr::containerBoxOffsetY ()
{
   return 0;
}

int OOFPosFixedMgr::containerBoxRestWidth ()
{
   return 0;
}

int OOFPosFixedMgr::containerBoxRestHeight ()
{
   return 0;
}

} // namespace oof

} // namespace dw
