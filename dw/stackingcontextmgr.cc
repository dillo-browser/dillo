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


#include "core.hh"
#include "../lout/debug.hh"

using namespace lout::misc;
using namespace lout::container::typed;

namespace dw {

namespace core {

StackingContextMgr::StackingContextMgr (Widget *widget)
{
   DBG_OBJ_CREATE ("dw::core::StackingContextMgr");
   scWidgets = new Vector<Widget> (1, false);
   DBG_OBJ_SET_NUM ("scWidgets.size", scWidgets->size());

   minZIndex = maxZIndex = 0; // Just to have some defined values.
}

StackingContextMgr::~StackingContextMgr ()
{
   delete scWidgets;
   DBG_OBJ_DELETE ();
}

void StackingContextMgr::addChildSCWidget (Widget *widget)
{
   if (scWidgets->size () == 0)
      minZIndex = maxZIndex = widget->getStyle()->zIndex;
   else {
      minZIndex = min (minZIndex, widget->getStyle()->zIndex);
      maxZIndex = max (maxZIndex, widget->getStyle()->zIndex);
   }

   scWidgets->put (widget);
   DBG_OBJ_SET_NUM ("scWidgets.size", scWidgets->size());
   DBG_OBJ_ARRSET_PTR ("scWidgets", scWidgets->size() - 1, widget);
}

void StackingContextMgr::drawBottom (View *view, Rectangle *area)
{
   DBG_OBJ_ENTER ("draw", 0, "drawBottom", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);
   draw (view, area, INT_MIN, -1);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::drawTop (View *view, Rectangle *area)
{
   DBG_OBJ_ENTER ("draw", 0, "drawTop", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);
   draw (view, area, 0, INT_MAX);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::draw (View *view, Rectangle *area, int startZIndex,
                               int endZIndex)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "[%d, %d, %d * %d], %d, %d",
                  area->x, area->y, area->width, area->height, startZIndex,
                  endZIndex);

   for (int zIndex = max (minZIndex, startZIndex);
        zIndex <= min (maxZIndex, endZIndex); zIndex++) {
      DBG_OBJ_MSGF ("draw", 1, "drawing zIndex = %d", zIndex);
      DBG_OBJ_MSG_START ();

      for (int i = 0; i < scWidgets->size (); i++) {
         Widget *child = scWidgets->get (i);
         DBG_OBJ_MSGF ("draw", 2, "widget %p has zIndex = %d",
                       child, child->getStyle()->zIndex);
         Rectangle childArea;
         if (child->getStyle()->zIndex == zIndex &&
             child->intersects (area, &childArea))
            child->draw (view, &childArea);
      }

      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_LEAVE ();
}


} // namespace core

} // namespace dw
