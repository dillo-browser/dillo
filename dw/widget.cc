/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
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

#include "../lout/msg.h"
#include "../lout/debug.hh"

using namespace lout;
using namespace lout::object;

namespace dw {
namespace core {

// ----------------------------------------------------------------------

int Widget::CLASS_ID = -1;

Widget::Widget ()
{
   registerName ("dw::core::Widget", &CLASS_ID);

   flags = (Flags)(NEEDS_RESIZE | EXTREMES_CHANGED | HAS_CONTENTS);
   parent = NULL;
   layout = NULL;

   allocation.x = -1;
   allocation.y = -1;
   allocation.width = 1;
   allocation.ascent = 1;
   allocation.descent = 0;

   style = NULL;
   bgColor = NULL;
   buttonSensitive = true;
   buttonSensitiveSet = false;

   deleteCallbackData = NULL;
   deleteCallbackFunc = NULL;
}

Widget::~Widget ()
{
   if (deleteCallbackFunc)
      deleteCallbackFunc (deleteCallbackData);

   if (style)
      style->unref ();

   if (parent)
      parent->removeChild (this);
   else
      layout->removeWidget ();
}


/**
 * \brief Calculates the intersection of widget->allocation and area, returned
 *    in intersection (in widget coordinates!).
 *
 * Typically used by containers when
 * drawing their children. Returns whether intersection is not empty.
 */
bool Widget::intersects (Rectangle *area, Rectangle *intersection)
{
   Rectangle parentArea, childArea;

   parentArea = *area;
   parentArea.x += parent->allocation.x;
   parentArea.y += parent->allocation.y;

   childArea.x = allocation.x;
   childArea.y = allocation.y;
   childArea.width = allocation.width;
   childArea.height = getHeight ();

   if (parentArea.intersectsWith (&childArea, intersection)) {
      intersection->x -= allocation.x;
      intersection->y -= allocation.y;
      return true;
   } else
      return false;
}

void Widget::setParent (Widget *parent)
{
   this->parent = parent;
   layout = parent->layout;

   if (!buttonSensitiveSet)
      buttonSensitive = parent->buttonSensitive;

   //DBG_OBJ_ASSOC (widget, parent);
}

void Widget::queueDrawArea (int x, int y, int width, int height)
{
   /** \todo Maybe only the intersection? */
   layout->queueDraw (x + allocation.x, y + allocation.y, width, height);
   _MSG("Widget::queueDrawArea x=%d y=%d w=%d h=%d\n", x, y, width, height);
}

/**
 * \brief This method should be called, when a widget changes its size.
 */
void Widget::queueResize (int ref, bool extremesChanged)
{
   Widget *widget2, *child;

   //DEBUG_MSG (DEBUG_SIZE,
   //           "a %stop-level %s with parent_ref = %d has changed its size\n",
   //           widget->parent ? "non-" : "",
   //           gtk_type_name (GTK_OBJECT_TYPE (widget)), widget->parent_ref);

   setFlags (NEEDS_RESIZE);
   setFlags (NEEDS_ALLOCATE);
   markSizeChange (ref);

   if (extremesChanged) {
      setFlags (EXTREMES_CHANGED);
      markExtremesChange (ref);
   }

   for (widget2 = parent, child = this;
        widget2;
        child = widget2, widget2 = widget2->parent) {
      widget2->setFlags (NEEDS_RESIZE);
      widget2->markSizeChange (child->parentRef);
      widget2->setFlags (NEEDS_ALLOCATE);

      //DEBUG_MSG (DEBUG_ALLOC,
      //           "setting DW_NEEDS_ALLOCATE for a %stop-level %s "
      //           "with parent_ref = %d\n",
      //           widget2->parent ? "non-" : "",
      //           gtk_type_name (GTK_OBJECT_TYPE (widget2)),
      //           widget2->parent_ref);

      if (extremesChanged) {
         widget2->setFlags (EXTREMES_CHANGED);
         widget2->markExtremesChange (child->parentRef);
      }
   }

   if (layout)
      layout->queueResize ();
}


/**
 *  \brief This method is a wrapper for Widget::sizeRequestImpl(); it calls
 *     the latter only when needed.
 */
void Widget::sizeRequest (Requisition *requisition)
{
   if (needsResize ()) {
      /** \todo Check requisition == &(this->requisition) and do what? */
      sizeRequestImpl (requisition);
      this->requisition = *requisition;
      unsetFlags (NEEDS_RESIZE);

      DBG_OBJ_SET_NUM (this, "requisition->width", requisition->width);
      DBG_OBJ_SET_NUM (this, "requisition->ascent", requisition->ascent);
      DBG_OBJ_SET_NUM (this, "requisition->descent", requisition->descent);
   } else
      *requisition = this->requisition;
}

/**
 * \brief Wrapper for Widget::getExtremesImpl().
 */
void Widget::getExtremes (Extremes *extremes)
{
   if (extremesChanged ()) {
      getExtremesImpl (extremes);
      this->extremes = *extremes;
      unsetFlags (EXTREMES_CHANGED);

      DBG_OBJ_SET_NUM (this, "extremes->minWidth", extremes->minWidth);
      DBG_OBJ_SET_NUM (this, "extremes->maxWidth", extremes->maxWidth);
   } else
      *extremes = this->extremes;
}

/**
 * \brief Wrapper for Widget::sizeAllocateImpl, calls the latter only when
 *    needed.
 */
void Widget::sizeAllocate (Allocation *allocation)
{
   if (needsAllocate () ||
       allocation->x != this->allocation.x ||
       allocation->y != this->allocation.y ||
       allocation->width != this->allocation.width ||
       allocation->ascent != this->allocation.ascent ||
       allocation->descent != this->allocation.descent) {

      //DEBUG_MSG (DEBUG_ALLOC,
      //           "a %stop-level %s with parent_ref = %d is newly allocated "
      //           "from %d, %d, %d x %d x %d ...\n",
      //           widget->parent ? "non-" : "",
      //           (GTK_OBJECT_TYPE_NAME (widget), widget->parent_ref,
      //           widget->allocation.x, widget->allocation.y,
      //           widget->allocation.width, widget->allocation.ascent,
      //           widget->allocation.descent);

      if (wasAllocated ()) {
         layout->queueDrawExcept (
            this->allocation.x,
            this->allocation.y,
            this->allocation.width,
            this->allocation.ascent + this->allocation.descent,
            allocation->x,
            allocation->y,
            allocation->width,
            allocation->ascent + allocation->descent);
      }

      sizeAllocateImpl (allocation);

      //DEBUG_MSG (DEBUG_ALLOC, "... to %d, %d, %d x %d x %d\n",
      //           widget->allocation.x, widget->allocation.y,
      //           widget->allocation.width, widget->allocation.ascent,
      //           widget->allocation.descent);

      this->allocation = *allocation;
      unsetFlags (NEEDS_ALLOCATE);
      setFlags (WAS_ALLOCATED);

      resizeDrawImpl ();

      DBG_OBJ_SET_NUM (this, "allocation.x", this->allocation.x);
      DBG_OBJ_SET_NUM (this, "allocation.y", this->allocation.y);
      DBG_OBJ_SET_NUM (this, "allocation.width", this->allocation.width);
      DBG_OBJ_SET_NUM (this, "allocation.ascent", this->allocation.ascent);
      DBG_OBJ_SET_NUM (this, "allocation.descent", this->allocation.descent);
   }

   /*unsetFlags (NEEDS_RESIZE);*/
}

bool Widget::buttonPress (EventButton *event)
{
   return buttonPressImpl (event);
}

bool Widget::buttonRelease (EventButton *event)
{
   return buttonReleaseImpl (event);
}

bool Widget::motionNotify (EventMotion *event)
{
   return motionNotifyImpl (event);
}

void Widget::enterNotify (EventCrossing *event)
{
   enterNotifyImpl (event);
}

void Widget::leaveNotify (EventCrossing *event)
{
   leaveNotifyImpl (event);
}

/**
 *  \brief Change the style of a widget.
 *
 * The old style is automatically unreferred, the new is referred. If this
 * call causes the widget to change its size, dw::core::Widget::queueResize
 * is called.
 */
void Widget::setStyle (style::Style *style)
{
   bool sizeChanged;

   style->ref ();

   if (this->style) {
      sizeChanged = this->style->sizeDiffs (style);
      this->style->unref ();
   } else
      sizeChanged = true;

   this->style = style;

   if (layout != NULL) {
      layout->updateCursor ();
   }

   if (sizeChanged)
      queueResize (0, true);
   else
      queueDraw ();
}

/**
 * \brief Set the background "behind" the widget, if it is not the
 *    background of the parent widget, e.g. the background of a table
 *    row.
 */
void Widget::setBgColor (style::Color *bgColor)
{
   this->bgColor = bgColor;
}

/**
 * \brief Get the actual background of a widget.
 */
style::Color *Widget::getBgColor ()
{
   Widget *widget = this;

   while (widget != NULL) {
      if (widget->style->backgroundColor)
         return widget->style->backgroundColor;
      if (widget->bgColor)
         return widget->bgColor;

      widget = widget->parent;
   }

   return layout->getBgColor ();
}


/**
 * \brief Draw borders and background of a widget part, which allocation is
 *    given by (x, y, width, height) (widget coordinates).
 *
 * area is given in widget coordinates.
 */
void Widget::drawBox (View *view, style::Style *style, Rectangle *area,
                      int x, int y, int width, int height, bool inverse)
{
   Rectangle viewArea;
   viewArea.x = area->x + allocation.x;
   viewArea.y = area->y + allocation.y;
   viewArea.width = area->width;
   viewArea.height = area->height;

   style::drawBorder (view, &viewArea, allocation.x + x, allocation.y + y,
                      width, height, style, inverse);

   /** \todo Background images? */
   if (style->backgroundColor)
      style::drawBackground (view, &viewArea,
                             allocation.x + x, allocation.y + y, width, height,
                             style, inverse);
}

/**
 * \brief Draw borders and background of a widget.
 *
 * area is given in widget coordinates.
 *
 */
void Widget::drawWidgetBox (View *view, Rectangle *area, bool inverse)
{
   Rectangle viewArea;
   viewArea.x = area->x + allocation.x;
   viewArea.y = area->y + allocation.y;
   viewArea.width = area->width;
   viewArea.height = area->height;

   style::drawBorder (view, &viewArea, allocation.x, allocation.y,
                      allocation.width, getHeight (), style, inverse);

   /** \todo Adjust following comment from the old dw sources. */
   /*
    * - Toplevel widget background colors are set as viewport
    *   background color. This is not crucial for the rendering, but
    *   looks a bit nicer when scrolling. Furthermore, the viewport
    *   does anything else in this case.
    *
    * - Since widgets are always drawn from top to bottom, it is
    *   *not* necessary to draw the background if
    *   widget->style->background_color is NULL (shining through).
    */
   /** \todo Background images? */

   if (style->backgroundColor &&
       (parent || layout->getBgColor () != style->backgroundColor))
      style::drawBackground (view, &viewArea, allocation.x, allocation.y,
                             allocation.width, getHeight (), style, inverse);
}

/*
 * This function is used by some widgets, when they are selected (as a whole).
 *
 * \todo This could be accelerated by using clipping bitmaps. Two important
 * issues:
 *
 *     (i) There should always been a pixel in the upper-left corner of the
 *         *widget*, so probably two different clipping bitmaps have to be
 *         used (10/01 and 01/10).
 *
 *    (ii) Should a new GC always be created?
 *
 * \bug Not implemented.
 */
void Widget::drawSelected (View *view, Rectangle *area)
{
}


void Widget::setButtonSensitive (bool buttonSensitive)
{
   this->buttonSensitive = buttonSensitive;
   buttonSensitiveSet = true;
}


/**
 * \brief Get the widget at the root of the tree, this widget is part from.
 */
Widget *Widget::getTopLevel ()
{
   Widget *widget = this;

   while (widget->parent)
      widget = widget->parent;

   return widget;
}

/**
 * \brief Get the level of the widget within the tree.
 *
 * The root widget has the level 0.
 */
int Widget::getLevel ()
{
   Widget *widget = this;
   int level = 0;

   while (widget->parent) {
      level++;
      widget = widget->parent;
   }

   return level;
}

/**
 * \brief Get the widget with the highest level, which is a direct ancestor of
 *    widget1 and widget2.
 */
Widget *Widget::getNearestCommonAncestor (Widget *otherWidget)
{
   Widget *widget1 = this, *widget2 = otherWidget;
   int level1 = widget1->getLevel (), level2 = widget2->getLevel();

   /* Get both widgets onto the same level.*/
   while (level1 > level2) {
      widget1 = widget1->parent;
      level1--;
   }

   while (level2 > level1) {
      widget2 = widget2->parent;
      level2--;
   }

   /* Search upwards. */
   while (widget1 != widget2) {
      if (widget1->parent == NULL) {
         MSG_WARN("widgets in different trees\n");
         return NULL;
      }

      widget1 = widget1->parent;
      widget2 = widget2->parent;
   }

   return widget1;
}


/**
 * \brief Search recursively through widget.
 *
 * Used by dw::core::Layout:getWidgetAtPoint.
 */
Widget *Widget::getWidgetAtPoint (int x, int y, int level)
{
   Iterator *it;
   Widget *childAtPoint;

   //_MSG ("%*s-> examining the %s %p (%d, %d, %d x (%d + %d))\n",
   //      3 * level, "", gtk_type_name (GTK_OBJECT_TYPE (widget)), widget,
   //      allocation.x, allocation.y,
   //      allocation.width, allocation.ascent,
   //      allocation.descent);

   if (x >= allocation.x &&
       y >= allocation.y &&
       x <= allocation.x + allocation.width &&
       y <= allocation.y + getHeight ()) {
      //_MSG ("%*s   -> inside\n", 3 * level, "");
      /*
       * Iterate over the children of this widget. Test recursively, whether
       * the point is within the child (or one of its children...). If there
       * is such a child, it is returned. Otherwise, this widget is returned.
       */
      childAtPoint = NULL;
      it = iterator (Content::WIDGET, false);

      while (childAtPoint == NULL && it->next ())
         childAtPoint = it->getContent()->widget->getWidgetAtPoint (x, y,
                                                                    level + 1);

      it->unref ();

      if (childAtPoint)
         return childAtPoint;
      else
         return this;
   } else
      return NULL;
}


void Widget::scrollTo (HPosition hpos, VPosition vpos,
               int x, int y, int width, int height)
{
   layout->scrollTo (hpos, vpos,
                     x + allocation.x, y + allocation.y, width, height);
}

void Widget::getExtremesImpl (Extremes *extremes)
{
   /* Simply return the requisition width */
   Requisition requisition;
   sizeRequest (&requisition);
   extremes->minWidth = extremes->maxWidth = requisition.width;
}

void Widget::sizeAllocateImpl (Allocation *allocation)
{
}

void Widget::markSizeChange (int ref)
{
}

void Widget::markExtremesChange (int ref)
{
}

void Widget::setWidth (int width)
{
}

void Widget::setAscent (int ascent)
{
}

void Widget::setDescent (int descent)
{
}

bool Widget::buttonPressImpl (EventButton *event)
{
   return false;
}

bool Widget::buttonReleaseImpl (EventButton *event)
{
   return false;
}

bool Widget::motionNotifyImpl (EventMotion *event)
{
   return false;
}

void Widget::enterNotifyImpl (EventCrossing *)
{
   core::style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onEnter();
}

void Widget::leaveNotifyImpl (EventCrossing *)
{
   core::style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onLeave();
}

void Widget::removeChild (Widget *child)
{
   // Should be implemented.
   misc::assertNotReached ();
}



} // namespace dw
} // namespace core
