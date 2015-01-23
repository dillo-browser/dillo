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

   this->widget = widget;

   childSCWidgets = new Vector<Widget> (1, false);
   DBG_OBJ_SET_NUM ("childSCWidgets.size", childSCWidgets->size());

   numZIndices = 0;
   zIndices = NULL;
   DBG_OBJ_SET_NUM ("numZIndices", numZIndices);
}

StackingContextMgr::~StackingContextMgr ()
{
   delete childSCWidgets;
   if (zIndices)
      free (zIndices);
   DBG_OBJ_DELETE ();
}

void StackingContextMgr::addChildSCWidget (Widget *widget)
{
   DBG_OBJ_ENTER ("common.scm", 0, "addChildSCWidget", "%p [z-index = %d]",
                  widget, widget->getStyle()->zIndex);

   int pos = findZIndex (widget->getStyle()->zIndex, true);
   DBG_OBJ_MSGF ("common.scm", 1, "pos = %d", pos);
   if (pos == -1) {
      pos = findZIndex (widget->getStyle()->zIndex, false);
      DBG_OBJ_MSGF ("common.scm", 1, "pos = %d", pos);

      numZIndices++;
      DBG_OBJ_SET_NUM ("numZIndices", numZIndices);
      zIndices = (int*)(zIndices ?
                        realloc (zIndices, numZIndices * sizeof (int)) :
                        malloc (numZIndices * sizeof (int)));

      for (int i = numZIndices - 1; i >= pos + 1; i--) {
         zIndices[i] = zIndices[i - 1];
         DBG_OBJ_ARRSET_NUM ("zIndex", i, zIndices[i]);
      }

      zIndices[pos] = widget->getStyle()->zIndex;
      DBG_OBJ_ARRSET_NUM ("zIndex", pos, zIndices[pos]);
   }      

   childSCWidgets->put (widget);
   DBG_OBJ_SET_NUM ("childSCWidgets.size", childSCWidgets->size());
   DBG_OBJ_ARRSET_PTR ("childSCWidgets", childSCWidgets->size() - 1, widget);

   DBG_OBJ_LEAVE ();
}

int StackingContextMgr::findZIndex (int zIndex, bool mustExist)
{
   int result = -123; // Compiler happiness: GCC 4.7 does not handle this?

   if (numZIndices == 0)
      result = mustExist ? -1 : 0;
   else {
      int low = 0, high = numZIndices - 1;
      bool found = false;
   
      while (!found) {
         int index = (low + high) / 2;
         if (zIndex == zIndices[index]) {
            found = true;
            result = index;
         } else {
            if (low >= high) {
               if (mustExist) {
                  found = true;
                  result = -1;
               } else {
                  found = true;
                  result = zIndex > zIndices[index] ? index + 1 : index;
               }
            }
            
            if (zIndex < zIndices[index])
               high = index - 1;
            else
               low = index + 1;
         }
      }
   }

   return result;
}

void StackingContextMgr::draw (View *view, Rectangle *area, int startZIndex,
                               int endZIndex, DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "[%d, %d, %d * %d], %d, %d",
                  area->x, area->y, area->width, area->height, startZIndex,
                  endZIndex);

   for (int zIndexIndex = 0; zIndexIndex < numZIndices; zIndexIndex++) {
      // Wrong region of z-indices (top or bottom) is simply ignored
      // (as well as non-defined zIndices).
      if (zIndices != NULL && zIndices[zIndexIndex] >= startZIndex &&
          zIndices[zIndexIndex] <= endZIndex) {
         DBG_OBJ_MSGF ("draw", 1, "drawing zIndex = %d", zIndices[zIndexIndex]);
         DBG_OBJ_MSG_START ();

         for (int i = 0; i < childSCWidgets->size (); i++) {
            Widget *child = childSCWidgets->get (i);
            DBG_OBJ_MSGF ("draw", 2, "widget %p has zIndex = %d",
                          child, child->getStyle()->zIndex);

            Rectangle childArea;
            if (child->getStyle()->zIndex == zIndices[zIndexIndex] &&
                child->intersects (widget, area, &childArea))
               child->draw (view, &childArea, context);
         }

         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_LEAVE ();
}

Widget *StackingContextMgr::getWidgetAtPoint (int x, int y,
                                              GettingWidgetAtPointContext
                                              *context,
                                              int startZIndex, int endZIndex)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPoint", "%d, %d", x, y);

   Widget *widgetAtPoint = NULL;

   for (int zIndexIndex = numZIndices - 1;
        widgetAtPoint == NULL && zIndexIndex >= 0; zIndexIndex--) {
      // Wrong region of z-indices (top or bottom) is simply ignored
      // (as well as non-defined zIndices).
      if (zIndices != NULL && zIndices[zIndexIndex] >= startZIndex &&
          zIndices[zIndexIndex] <= endZIndex) {
         DBG_OBJ_MSGF ("events", 1, "searching zIndex = %d",
                       zIndices[zIndexIndex]);
         DBG_OBJ_MSG_START ();

         for (int i = childSCWidgets->size () - 1;
              widgetAtPoint == NULL && i >= 0; i--) {
            Widget *child = childSCWidgets->get (i);
            DBG_OBJ_MSGF ("events", 2, "widget %p has zIndex = %d",
                          child, child->getStyle()->zIndex);
            if (child->getStyle()->zIndex == zIndices[zIndexIndex])
               widgetAtPoint = child->getWidgetAtPoint (x, y, context);
         }
         
         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_MSGF ("events", 0, "=> %p", widgetAtPoint);
   DBG_OBJ_LEAVE ();
   return widgetAtPoint;
}

} // namespace core

} // namespace dw
