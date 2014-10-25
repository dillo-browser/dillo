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

   minZIndex = maxZIndex = 0; // Just to have some defined values.
}

StackingContextMgr::~StackingContextMgr ()
{
   delete childSCWidgets;
   DBG_OBJ_DELETE ();
}

void StackingContextMgr::addChildSCWidget (Widget *widget)
{
   if (childSCWidgets->size () == 0)
      minZIndex = maxZIndex = widget->getStyle()->zIndex;
   else {
      minZIndex = min (minZIndex, widget->getStyle()->zIndex);
      maxZIndex = max (maxZIndex, widget->getStyle()->zIndex);
   }

   childSCWidgets->put (widget);
   DBG_OBJ_SET_NUM ("childSCWidgets.size", childSCWidgets->size());
   DBG_OBJ_ARRSET_PTR ("childSCWidgets", childSCWidgets->size() - 1, widget);
}

void StackingContextMgr::drawBottom (View *view, Rectangle *area,
                                     StackingIteratorStack *iteratorStack,
                                     Widget **interruptedWidget,
                                     int *zIndexOffset, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "drawBottom", "(%d, %d, %d * %d), [%d], [%d]",
                  area->x, area->y, area->width, area->height, *zIndexOffset,
                  *index);
   draw (view, area, iteratorStack, interruptedWidget, index, INT_MIN, -1,
         zIndexOffset);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::drawTop (View *view, Rectangle *area,
                                  StackingIteratorStack *iteratorStack,
                                  Widget **interruptedWidget,
                                  int *zIndexOffset, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "drawTop", "(%d, %d, %d * %d), [%d], [%d]",
                  area->x, area->y, area->width, area->height, *zIndexOffset,
                  *index);
   draw (view, area, iteratorStack, interruptedWidget, index, 0, INT_MAX,
         zIndexOffset);
   DBG_OBJ_LEAVE ();
}

void StackingContextMgr::draw (View *view, Rectangle *area,
                               StackingIteratorStack *iteratorStack,
                               Widget **interruptedWidget, int *zIndexOffset,
                               int startZIndex, int endZIndex, int *index)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "(%d, %d, %d * %d), [%d], %d, %d, [%d]",
                  area->x, area->y, area->width, area->height,
                  *zIndexOffset, startZIndex, endZIndex, *index);

   DBG_OBJ_MSGF ("draw", 1, "initially: index = %d (of %d)",
                 *index, childSCWidgets->size ());

   int startZIndexEff = max (minZIndex, startZIndex),
      endZIndexEff = min (maxZIndex, endZIndex);
   // Make sure *zIndexOffset starts at 0, for zIndex = startZIndexEff,
   // so *zIndexOffset = zIndex - startZIndexEff, and
   // zIndex = *zIndexOffset + startZIndexEff.
   while (*interruptedWidget == NULL &&
          *zIndexOffset <= endZIndexEff - startZIndexEff) {
      DBG_OBJ_MSGF ("draw", 1, "drawing zIndex = %d + %d",
                    *zIndexOffset, startZIndexEff);
      DBG_OBJ_MSG_START ();

      while (*interruptedWidget == NULL && *index < childSCWidgets->size ()) {
         Widget *child = childSCWidgets->get (*index);
         DBG_OBJ_MSGF ("draw", 2, "widget %p has zIndex = %d",
                       child, child->getStyle()->zIndex);
         Rectangle childArea;
         if (child->getStyle()->zIndex == *zIndexOffset + startZIndexEff &&
             child->intersects (area, &childArea))
            child->drawTotal (view, &childArea, iteratorStack,
                              interruptedWidget);

         if (*interruptedWidget == NULL)
            (*index)++;
      }

      if (*interruptedWidget == NULL)
         (*zIndexOffset)++;

      DBG_OBJ_MSG_END ();
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
                                                 int *zIndexOffset, int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPointTop", "%d, %d", x, y);
   Widget *widget = getWidgetAtPoint (x, y, iteratorStack, interruptedWidget,
                                      zIndexOffset, 0, INT_MAX, index);
   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)", widget, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widget;
}

Widget *StackingContextMgr::getBottomWidgetAtPoint (int x, int y,
                                                    core::StackingIteratorStack
                                                    *iteratorStack,
                                                    Widget **interruptedWidget,
                                                    int *zIndexOffset,
                                                    int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPointBottom", "%d, %d", x, y);
   Widget *widget = getWidgetAtPoint (x, y, iteratorStack, interruptedWidget,
                                      zIndexOffset, INT_MIN, -1, index);
   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)", widget, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widget;
}

Widget *StackingContextMgr::getWidgetAtPoint (int x, int y,
                                              StackingIteratorStack
                                              *iteratorStack,
                                              Widget **interruptedWidget,
                                              int *zIndexOffset,
                                              int startZIndex, int endZIndex,
                                              int *index)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPointBottom", "%d, %d", x, y);

   Widget *widgetAtPoint = NULL;

   // For *zIndexOffset, see draw(). Actually, we start at the end and iterate
   // to the start (naming is somewhat confusing).
   int startZIndexEff = max (minZIndex, startZIndex);
   while (*interruptedWidget == NULL && widgetAtPoint == NULL &&
          *zIndexOffset >= 0) {
      DBG_OBJ_MSGF ("events", 1, "searching zIndex = %d + %d",
                    *zIndexOffset, startZIndexEff);
      DBG_OBJ_MSG_START ();

      while (*interruptedWidget == NULL && widgetAtPoint == NULL &&
             *index >= 0) {
         Widget *child = childSCWidgets->get (*index);
         DBG_OBJ_MSGF ("events", 2, "widget %p has zIndex = %d",
                       child, child->getStyle()->zIndex);
         if (child->getStyle()->zIndex == *zIndexOffset + startZIndexEff)
            widgetAtPoint =
               child->getWidgetAtPointTotal (x, y, iteratorStack,
                                             interruptedWidget);

         if (*interruptedWidget == NULL)
            (*index)--;
      }

      if (*interruptedWidget == NULL)
         (*zIndexOffset)--;

      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_MSGF ("events", 0, "=> %p (i: %p)",
                 widgetAtPoint, *interruptedWidget);
   DBG_OBJ_LEAVE ();
   return widgetAtPoint;
}

} // namespace core

} // namespace dw
