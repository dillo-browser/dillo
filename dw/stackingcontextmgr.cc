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
      numZIndices++;
      zIndices = (int*)(zIndices ?
                        realloc (zIndices, numZIndices * sizeof (int)) :
                        malloc (numZIndices * sizeof (int)));
      
      DBG_OBJ_SET_NUM ("numZIndices", numZIndices);

      pos = findZIndex (widget->getStyle()->zIndex, false);
      DBG_OBJ_MSGF ("common.scm", 1, "pos = %d", pos);

      for (int i = numZIndices - 2; i >= pos; i--) {
         zIndices[i] = zIndices[i + 1];
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

void StackingContextMgr::drawBottom (View *view, Rectangle *area,
                                     StackingIteratorStack *iteratorStack,
                                     Widget **interruptedWidget,
                                     int *zIndexIndex, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "drawBottom", "(%d, %d, %d * %d), [%d], [%d]",
                  area->x, area->y, area->width, area->height, *zIndexIndex,
                  *index);
   draw (view, area, iteratorStack, interruptedWidget, index, INT_MIN, -1,
         zIndexIndex);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::drawTop (View *view, Rectangle *area,
                                  StackingIteratorStack *iteratorStack,
                                  Widget **interruptedWidget,
                                  int *zIndexIndex, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "drawTop", "(%d, %d, %d * %d), [%d], [%d]",
                  area->x, area->y, area->width, area->height, *zIndexIndex,
                  *index);
   draw (view, area, iteratorStack, interruptedWidget, index, 0, INT_MAX,
         zIndexIndex);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::draw (View *view, Rectangle *area,
                               StackingIteratorStack *iteratorStack,
                               Widget **interruptedWidget, int *zIndexIndex,
                               int startZIndex, int endZIndex, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "(%d, %d, %d * %d), [%d], %d, %d, [%d]",
                  area->x, area->y, area->width, area->height,
                  *zIndexIndex, startZIndex, endZIndex, *index);

   DBG_OBJ_MSGF ("draw", 1, "initially: index = %d (of %d)",
                 *index, childSCWidgets->size ());

   while (*interruptedWidget == NULL &&
          *zIndexIndex <= numZIndices) {
      // Wrong region of z-indices (top or bottom) is simply ignored
      // (as well as non-defined zIndices).
      if (zIndices != NULL && zIndices[*zIndexIndex] >= startZIndex &&
          zIndices[*zIndexIndex] <= endZIndex) {
         DBG_OBJ_MSGF ("draw", 1, "drawing zIndex = %d",
                       zIndices[*zIndexIndex]);
         DBG_OBJ_MSG_START ();

         while (*interruptedWidget == NULL &&
                *index < childSCWidgets->size ()) {
            Widget *child = childSCWidgets->get (*index);
            DBG_OBJ_MSGF ("draw", 2, "widget %p has zIndex = %d",
                          child, child->getStyle()->zIndex);
            Rectangle childArea;
            if (child->getStyle()->zIndex == zIndices[*zIndexIndex] &&
                child->intersects (area, &childArea))
               child->drawTotal (view, &childArea, iteratorStack,
                                 interruptedWidget);

            if (*interruptedWidget == NULL)
               (*index)++;
         }

         DBG_OBJ_MSG_END ();
      }

      if (*interruptedWidget == NULL) {
         (*zIndexIndex)++;
         *index = 0;
      }
   }

   DBG_OBJ_MSGF ("draw", 1, "finally: index = %d (of %d)",
                 *index, childSCWidgets->size ());
   DBG_OBJ_MSGF ("draw", 1, "=> %p", *interruptedWidget);
   DBG_OBJ_LEAVE ();
}

Widget *StackingContextMgr::getTopWidgetAtPoint (int x, int y,
                                                 core::StackingIteratorStack
                                                 *iteratorStack,
                                                 Widget **interruptedWidget,
                                                 int *zIndexIndex, int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPointTop", "%d, %d", x, y);
   Widget *widget = getWidgetAtPoint (x, y, iteratorStack, interruptedWidget,
                                      zIndexIndex, 0, INT_MAX, index);
   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)", widget, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widget;
}

Widget *StackingContextMgr::getBottomWidgetAtPoint (int x, int y,
                                                    core::StackingIteratorStack
                                                    *iteratorStack,
                                                    Widget **interruptedWidget,
                                                    int *zIndexIndex,
                                                    int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPointBottom", "%d, %d", x, y);
   Widget *widget = getWidgetAtPoint (x, y, iteratorStack, interruptedWidget,
                                      zIndexIndex, INT_MIN, -1, index);
   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)", widget, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widget;
}

Widget *StackingContextMgr::getWidgetAtPoint (int x, int y,
                                              StackingIteratorStack
                                              *iteratorStack,
                                              Widget **interruptedWidget,
                                              int *zIndexIndex,
                                              int startZIndex, int endZIndex,
                                              int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPoint", "%d, %d", x, y);

   Widget *widgetAtPoint = NULL;

   while (*interruptedWidget == NULL && widgetAtPoint == NULL &&
          *zIndexIndex >= 0) {
      // Wrong region of z-indices (top or bottom) is simply ignored
      // (as well as non-defined zIndices).
      if (zIndices != NULL && zIndices[*zIndexIndex] >= startZIndex &&
          zIndices[*zIndexIndex] <= endZIndex) {
         DBG_OBJ_MSGF ("events", 1, "searching zIndex = %d",
                       zIndices[*zIndexIndex]);
         DBG_OBJ_MSG_START ();

         while (*interruptedWidget == NULL && widgetAtPoint == NULL &&
                *index >= 0) {
            Widget *child = childSCWidgets->get (*index);
            DBG_OBJ_MSGF ("events", 2, "widget %p has zIndex = %d",
                          child, child->getStyle()->zIndex);
            if (child->getStyle()->zIndex == zIndices[*zIndexIndex])
               widgetAtPoint =
                  child->getWidgetAtPointTotal (x, y, iteratorStack,
                                                interruptedWidget);
            
            if (*interruptedWidget == NULL)
               (*index)--;
         }
         
         DBG_OBJ_MSG_END ();
      }

      if (*interruptedWidget == NULL) {
         (*zIndexIndex)--;
         *index = childSCWidgets->size () - 1;
      }
   }

   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)",
                 widgetAtPoint, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widgetAtPoint;
}

} // namespace core

} // namespace dw
