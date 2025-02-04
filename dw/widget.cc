/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2023-2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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

//#define DEBUG_LEVEL 1
#include "core.hh"

#include "../lout/msg.h"
#include "../lout/debug.hh"

using namespace lout;
using namespace lout::object;
using namespace lout::misc;

namespace dw {
namespace core {

/* Used to determine which action to take when correcting the aspect ratio of a
 * requisition in Widget::correctReqAspectRatio(). */
enum {
   PASS_INCREASE = 0,
   PASS_DECREASE = 1,
   PASS_KEEP = 2
};


// ----------------------------------------------------------------------

bool Widget::WidgetImgRenderer::readyToDraw ()
{
   return widget->wasAllocated ();
}

void Widget::WidgetImgRenderer::getBgArea (int *x, int *y, int *width,
                                           int *height)
{
   widget->getPaddingArea (x, y, width, height);
}

void Widget::WidgetImgRenderer::getRefArea (int *xRef, int *yRef, int *widthRef,
                                            int *heightRef)
{
   widget->getPaddingArea (xRef, yRef, widthRef, heightRef);
}

style::Style *Widget::WidgetImgRenderer::getStyle ()
{
   return widget->getStyle ();
}

void Widget::WidgetImgRenderer::draw (int x, int y, int width, int height)
{
   widget->queueDrawArea (x - widget->allocation.x, y - widget->allocation.y,
                          width, height);
}

// ----------------------------------------------------------------------

bool Widget::adjustMinWidth = true;
int Widget::CLASS_ID = -1;

Widget::Widget ()
{
   DBG_OBJ_CREATE ("dw::core::Widget");
   registerName ("dw::core::Widget", &CLASS_ID);

   DBG_OBJ_ASSOC_CHILD (&requisitionParams);
   DBG_OBJ_ASSOC_CHILD (&extremesParams);

   flags = (Flags)(NEEDS_RESIZE | EXTREMES_CHANGED);
   parent = quasiParent = generator = container = NULL;
   setWidgetReference (NULL);
   DBG_OBJ_SET_PTR ("container", container);

   layout = NULL;

   allocation.x = -1;
   allocation.y = -1;
   allocation.width = 1;
   allocation.ascent = 1;
   allocation.descent = 0;

   extraSpace.top = extraSpace.right = extraSpace.bottom = extraSpace.left = 0;

   style = NULL;
   bgColor = NULL;
   buttonSensitive = true;
   buttonSensitiveSet = false;

   deleteCallbackData = NULL;
   deleteCallbackFunc = NULL;

   widgetImgRenderer = NULL;

   stackingContextMgr = NULL;

   ratio = 0.0;
}

Widget::~Widget ()
{
   if (deleteCallbackFunc)
      deleteCallbackFunc (deleteCallbackData);

   if (widgetImgRenderer) {
      if (style && style->backgroundImage)
         style->backgroundImage->removeExternalImgRenderer (widgetImgRenderer);
      delete widgetImgRenderer;
   }

   if (stackingContextMgr)
      delete stackingContextMgr;

   if (style)
      style->unref ();

   if (parent)
      parent->removeChild (this);
   else if (layout)
      layout->removeWidget ();

   DBG_OBJ_DELETE ();
}


/**
 * \brief Calculates the intersection of the visible allocation
 *    (i. e. the intersection with the visible parent allocation) and
 *    "area" (in widget coordinates referring to "refWidget"),
 *    returned in intersection (in widget coordinates).
 *
 * Typically used by containers when drawing their children (passing
 * "this" as "refWidget"). Returns whether intersection is not empty.
 */
bool Widget::intersects (Widget *refWidget, Rectangle *area,
                         Rectangle *intersection)
{
   DBG_OBJ_ENTER ("draw", 0, "intersects", "%p, [%d, %d, %d * %d]",
                  refWidget, area->x, area->y, area->width, area->height);
   bool r;

   if (wasAllocated ()) {
      *intersection = *area;
      intersection->x += refWidget->allocation.x;
      intersection->y += refWidget->allocation.y;
      
      r = true;
      // "RefWidget" is excluded; it is assumed that "area" its already within
      // its allocation.
      for (Widget *widget = this; r && widget != refWidget;
           widget = widget->parent) {
         assert (widget != NULL); // refWidget must be ancestor.

         Rectangle widgetArea, newIntersection;
         widgetArea.x = widget->allocation.x;
         widgetArea.y = widget->allocation.y;
         widgetArea.width = widget->allocation.width;
         widgetArea.height = widget->getHeight ();

         if (intersection->intersectsWith (&widgetArea, &newIntersection)) {
            DBG_OBJ_MSGF ("draw", 1, "new intersection: %d, %d, %d * %d",
                          newIntersection.x, newIntersection.y,
                          newIntersection.width, newIntersection.height);
            *intersection = newIntersection;
         } else {
            DBG_OBJ_MSG ("draw", 1, "no new intersection");
            r = false;
         }
      }

      if (r) {
         intersection->x -= allocation.x;
         intersection->y -= allocation.y;

         DBG_OBJ_MSGF ("draw", 1, "final intersection: %d, %d, %d * %d",
                       intersection->x, intersection->y,
                       intersection->width, intersection->height);
      }
   } else {
      r = false;
      DBG_OBJ_MSG ("draw", 1, "not allocated");
   }

   if (r)
      DBG_OBJ_LEAVE_VAL ("true: %d, %d, %d * %d",
                         intersection->x, intersection->y,
                         intersection->width, intersection->height);
   else
      DBG_OBJ_LEAVE_VAL0 ("false");

   return r;
}

/**
 * See \ref dw-interrupted-drawing for details.
 */
void Widget::drawInterruption (View *view, Rectangle *area,
                               DrawingContext *context)
{
   Rectangle thisArea;
   if (intersects (layout->topLevel, context->getToplevelArea (), &thisArea))
      draw (view, &thisArea, context);

   context->addWidgetProcessedAsInterruption (this);
}

Widget *Widget::getWidgetAtPoint (int x, int y,
                                  GettingWidgetAtPointContext *context)
{
   // Suitable for simple widgets, without children.

   if (inAllocation (x, y))
      return this;
   else
      return NULL;
}

Widget *Widget::getWidgetAtPointInterrupted (int x, int y,
                                             GettingWidgetAtPointContext
                                             *context)
{
   Widget *widgetAtPoint = getWidgetAtPoint (x, y, context);
   context->addWidgetProcessedAsInterruption (this);
   return widgetAtPoint;
}

void Widget::setParent (Widget *parent)
{
   DBG_OBJ_ENTER ("construct", 0, "setParent", "%p", parent);

   this->parent = parent;
   layout = parent->layout;

   if (!buttonSensitiveSet)
      buttonSensitive = parent->buttonSensitive;

   DBG_OBJ_ASSOC_PARENT (parent);
   //printf ("The %s %p becomes a child of the %s %p\n",
   //        getClassName(), this, parent->getClassName(), parent);

   // Determine the container. Currently rather simple; will become
   // more complicated when absolute and fixed positions are
   // supported.
   container = NULL;
   for (Widget *widget = getParent (); widget != NULL && container == NULL;
        widget = widget->getParent())
      if (widget->isPossibleContainer ())
         container = widget;
   // If there is no possible container widget, there is
   // (surprisingly!) also no container (i. e. the viewport is
   // used). Does not occur in dillo, where the toplevel widget is a
   // Textblock.
   DBG_OBJ_SET_PTR ("container", container);

   // If at all, stackingContextMgr should have set *before*, see also
   // Widget::setStyle() and Layout::addWidget().
   if (stackingContextMgr) {
      Widget *stackingContextWidget = parent;
      while (stackingContextWidget &&
             stackingContextWidget->stackingContextMgr == NULL)
         stackingContextWidget = stackingContextWidget->parent;
      assert (stackingContextWidget);
      stackingContextWidget->stackingContextMgr->addChildSCWidget (this);
   } else
      stackingContextWidget = parent->stackingContextWidget;

   notifySetParent();

   DBG_OBJ_LEAVE ();
}

void Widget::setQuasiParent (Widget *quasiParent)
{
   this->quasiParent = quasiParent;

   // More to do? Compare with setParent().

   DBG_OBJ_SET_PTR ("quasiParent", quasiParent);
}

void Widget::queueDrawArea (int x, int y, int width, int height)
{
   /** \todo Maybe only the intersection? */

   DBG_OBJ_ENTER ("draw", 0, "queueDrawArea", "%d, %d, %d, %d",
                  x, y, width, height);

   _MSG("Widget::queueDrawArea alloc(%d %d %d %d) wid(%d %d %d %d)\n",
       allocation.x, allocation.y,
       allocation.width, allocation.ascent + allocation.descent,
       x, y, width, height);
   if (layout)
      layout->queueDraw (x + allocation.x, y + allocation.y, width, height);

   DBG_OBJ_LEAVE ();
}

/**
 * \brief This method should be called, when a widget changes its size.
 *
 * A "fast" queueResize will ignore the ancestors, and furthermore
 * not trigger the idle function. Used only within
 * viewportSizeChanged, and not available outside Layout and Widget.
 */
void Widget::queueResize (int ref, bool extremesChanged, bool fast)
{
   DBG_OBJ_ENTER ("resize", 0, "queueResize", "%d, %s, %s",
                  ref, extremesChanged ? "true" : "false",
                  fast ? "true" : "false");

   enterQueueResize ();

   Widget *widget2, *child;

   Flags resizeFlag, extremesFlag, totalFlags;

   if (layout) {
      // If RESIZE_QUEUED is set, this widget is already in the list.
      if (!resizeQueued ())
         layout->queueResizeList->put (this);

      resizeFlag = RESIZE_QUEUED;
      extremesFlag = EXTREMES_QUEUED;
   } else {
      resizeFlag = NEEDS_RESIZE;
      extremesFlag = EXTREMES_CHANGED;
   }

   setFlags (resizeFlag);
   setFlags (ALLOCATE_QUEUED);
   markSizeChange (ref);

   totalFlags = resizeFlag;
   
   if (extremesChanged) {
      totalFlags = (Flags)(totalFlags | extremesFlag);
      
      setFlags (extremesFlag);
      markExtremesChange (ref);
   }

   if (fast) {
      if (parent) {
         // In this case, queueResize is called from top (may be a
         // random entry point) to bottom, so markSizeChange and
         // markExtremesChange have to be called explicitly for the
         // parent. The tests (needsResize etc.) are uses to check
         // whether queueResize has been called for the parent, or
         // whether this widget is the entry point.
         if (parent->needsResize () || parent->resizeQueued ())
            parent->markSizeChange (parentRef);
         if (parent->extremesChanged () || parent->extremesQueued ())
            parent->markExtremesChange (parentRef);
      }
   } else {
      for (widget2 = parent, child = this; widget2;
           child = widget2, widget2 = widget2->parent) {         
         if (layout && !widget2->resizeQueued ())
            layout->queueResizeList->put (widget2);

         DBG_OBJ_MSGF ("resize", 2, "setting %s and ALLOCATE_QUEUED for %p",
                       resizeFlag == RESIZE_QUEUED ?
                       "RESIZE_QUEUED" : "NEEDS_RESIZE",
                       widget2);

         widget2->setFlags (resizeFlag);
         widget2->markSizeChange (child->parentRef);
         widget2->setFlags (ALLOCATE_QUEUED);

         if (extremesChanged) {
            widget2->setFlags (extremesFlag);
            widget2->markExtremesChange (child->parentRef);
         }

         DBG_IF_RTFL {
            if (widget2->parent)
               DBG_OBJ_MSGF ("resize", 2,
                             "checking parent %p: (%d & %d) [= %d] == %d?",
                             widget2->parent, widget2->parent->flags,
                             totalFlags, widget2->parent->flags & totalFlags,
                             totalFlags);
         }
         
         if (widget2->parent &&
             (widget2->parent->flags & totalFlags) == totalFlags) {
            widget2->parent->markSizeChange (widget2->parentRef);
            if (extremesChanged) {
               widget2->parent->markExtremesChange (widget2->parentRef);
            }
            
            break;
         }
      }

      if (layout)
         layout->queueResize (extremesChanged);
   }

   leaveQueueResize ();

   DBG_OBJ_LEAVE ();
}

void Widget::containerSizeChanged ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChanged");

   // If there is a container widget (not the viewport), which has not
   // changed its size (which can be determined by the respective
   // flags: this method is called recursively), this widget will
   // neither change its size. Also, the recursive iteration can be
   // stopped, since the children of this widget will
   if (container == NULL ||
       container->needsResize () || container->resizeQueued () ||
       container->extremesChanged () || container->extremesQueued ()) {
      // Viewport (container == NULL) or container widget has changed
      // its size.
      if (affectedByContainerSizeChange ())
         queueResizeFast (0, true);

      // Even if *this* widget is not affected, children may be, so
      // iterate over children.
      containerSizeChangedForChildren ();
   }

   DBG_OBJ_LEAVE ();
}

bool Widget::affectedByContainerSizeChange ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "affectedByContainerSizeChange");

   bool ret;

   // This standard implementation is suitable for all widgets which
   // call correctRequisition() and correctExtremes(), even in the way
   // how Textblock and Image do (see comments there). Has to be kept
   // in sync.

   if (container == NULL) {
      if (style::isAbsLength (getStyle()->width) &&
          style::isAbsLength (getStyle()->height))
         // Both absolute, i. e. fixed: no dependency.
         ret = false;
      else if (style::isPerLength (getStyle()->width) ||
               style::isPerLength (getStyle()->height)) {
         // Any percentage: certainly dependenant.
         ret = true;
      } else
         // One or both is "auto": depends ...
         ret =
            (getStyle()->width == style::LENGTH_AUTO ?
             usesAvailWidth () : false) ||
            (getStyle()->height == style::LENGTH_AUTO ?
             usesAvailHeight () : false);
   } else
      ret = container->affectsSizeChangeContainerChild (this);

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(ret));
   return ret;
}

bool Widget::affectsSizeChangeContainerChild (Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "affectsSizeChangeContainerChild", "%p", child);

   bool ret;

   // From the point of view of the container. This standard
   // implementation should be suitable for most (if not all)
   // containers.

   if (style::isAbsLength (child->getStyle()->width) &&
       style::isAbsLength (child->getStyle()->height))
      // Both absolute, i. e. fixed: no dependency.
      ret = false;
   else if (style::isPerLength (child->getStyle()->width) ||
            style::isPerLength (child->getStyle()->height)) {
      // Any percentage: certainly dependenant.
      ret = true;
   } else
      // One or both is "auto": depends ...
      ret =
         (child->getStyle()->width == style::LENGTH_AUTO ?
          child->usesAvailWidth () : false) ||
         (child->getStyle()->height == style::LENGTH_AUTO ?
          child->usesAvailHeight () : false);

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(ret));
   return ret;
}

void Widget::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   // Working, but inefficient standard implementation.
   Iterator *it = iterator ((Content::Type)(Content::WIDGET_IN_FLOW |
                                            Content::WIDGET_OOF_CONT),
                            false);
   while (it->next ())
      it->getContent()->widget->containerSizeChanged ();
   it->unref ();

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Must be implemengted by a method returning true, when
 *    getAvailWidth() is called.
 */
bool Widget::usesAvailWidth ()
{
   return false;
}

/**
 * \brief Must be implemengted by a method returning true, when
 *    getAvailHeight() is called.
 */
bool Widget::usesAvailHeight ()
{
   return false;
}

/**
 * \brief This method is a wrapper for Widget::sizeRequestImpl(); it calls
 * the latter only when needed.
 *
 * Computes the size (Requisition) that the current widget wants. The output
 * \param requisition has the final values which will be used to compute the
 * widget allocation.
 */
void Widget::sizeRequest (Requisition *requisition, int numPos,
                          Widget **references, int *x, int *y)
{
   assert (!queueResizeEntered ());

   DBG_OBJ_ENTER ("resize", 0, "sizeRequest", "%d, ...", numPos);

   DBG_IF_RTFL {
      DBG_OBJ_MSG_START();
      for(int i = 0; i < numPos; i++)
         DBG_OBJ_MSGF ("resize", 1, "ref #%d: %p, %d, %d",
                       i, references[i], x[i], y[i]);
      DBG_OBJ_MSG_END();
   }

   enterSizeRequest ();

   if (resizeQueued ()) {
      // This method is called outside of Layout::resizeIdle.
      setFlags (NEEDS_RESIZE);
      unsetFlags (RESIZE_QUEUED);
      // The widget is not taken out of Layout::queueResizeList, since
      // other *_QUEUED flags may still be set and processed in
      // Layout::resizeIdle.
   }

   SizeParams newRequisitionParams (numPos, references, x, y);
   DBG_OBJ_ASSOC_CHILD (&newRequisitionParams);

   bool callImpl;
   if (needsResize ())
      callImpl = true;
   else {
      // Even if RESIZE_QUEUED / NEEDS_RESIZE is not set, calling
      // sizeRequestImpl is necessary when the relavive positions passed here
      // have changed.
      callImpl = !newRequisitionParams.isEquivalent (&requisitionParams);
   }

   DBG_OBJ_MSGF ("resize", 1, "callImpl = %s", boolToStr (callImpl));

   requisitionParams = newRequisitionParams;

   if (callImpl) {
      calcExtraSpace (numPos, references, x, y);
      /** \todo Check requisition == &(this->requisition) and do what? */
      sizeRequestImpl (requisition, numPos, references, x, y);
      this->requisition = *requisition;
      unsetFlags (NEEDS_RESIZE);

      DBG_OBJ_SET_NUM ("requisition.width", requisition->width);
      DBG_OBJ_SET_NUM ("requisition.ascent", requisition->ascent);
      DBG_OBJ_SET_NUM ("requisition.descent", requisition->descent);
   } else
      *requisition = this->requisition;

   leaveSizeRequest ();

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Used to evaluate Widget::adjustMinWidth.
 *
 * If extremes == NULL, getExtremes is called. ForceValue is the same
 * value passed to getAvailWidth etc.; if false, getExtremes is not
 * called. A value of "false" is passed for "useCorrected" in the
 * context of correctExtemes etc., to avoid cyclic dependencies.
 * 
 */
int Widget::getMinWidth (Extremes *extremes, bool forceValue)
{
   DBG_IF_RTFL {
      if (extremes)
         DBG_OBJ_ENTER ("resize", 0, "getMinWidth", "[%d (%d) / %d (%d)], %s",
                        extremes->minWidth, extremes->minWidthIntrinsic,
                        extremes->maxWidth, extremes->maxWidthIntrinsic,
                        forceValue ? "true" : "false");
      else
         DBG_OBJ_ENTER ("resize", 0, "getMinWidth", "(nil), %s",
                        forceValue ? "true" : "false");
   }

   int minWidth;

   if (getAdjustMinWidth ()) {
      Extremes extremes2;
      if (extremes == NULL) {
         if (forceValue) {
            getExtremes (&extremes2);
            extremes = &extremes2;
         }
      }

      // TODO Not completely clear whether this is feasible: Within
      // the context of getAvailWidth(false) etc., getExtremes may not
      // be called. We ignore the minimal width then.
      if (extremes)
         minWidth = extremes->adjustmentWidth;
      else
         minWidth = 0;
   } else
      minWidth = 0;

   DBG_OBJ_LEAVE_VAL ("%d", minWidth);
   return minWidth;
}

/**
 * Return available width including margin/border/padding
 * (extraSpace?), not only the content width.
 *
 * If the widget has a parent or a quasiParent, the width computation is
 * delegated to the parent first, or the quasiParent later.
 */
int Widget::getAvailWidth (bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "getAvailWidth", "%s",
                  forceValue ? "true" : "false");

   int width;

   if (parent == NULL && quasiParent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      // TODO Consider nested layouts (e. g. <button>).

      int viewportWidth =
         layout->viewportWidth - (layout->canvasHeightGreater ?
                                  layout->vScrollbarThickness : 0);
      width = viewportWidth;
      calcFinalWidth (getStyle (), viewportWidth, NULL, 0, forceValue, &width);
      assert(width != -1);
      DBG_OBJ_MSG_END ();
   } else if (parent) {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      width = parent->getAvailWidthOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   } else /* if (quasiParent) */ {
      DBG_OBJ_MSG ("resize", 1, "delegated to quasiParent");
      DBG_OBJ_MSG_START ();
      width = quasiParent->getAvailWidthOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_LEAVE_VAL ("%d", width);
   return width;
}

/**
 * Return available height including margin/border/padding
 * (extraSpace?), not only the content height.
 */
int Widget::getAvailHeight (bool forceValue)
{
   // TODO Correct by ... not extremes, but ...? (Height extremes?)

   // TODO Consider 'min-height' and 'max-height'. (Minor priority, as long as
   //      "getAvailHeight (true)" is not used.

   DBG_OBJ_ENTER ("resize", 0, "getAvailHeight", "%s",
                  forceValue ? "true" : "false");

   int height;

   if (parent == NULL && quasiParent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      // TODO Consider nested layouts (e. g. <button>).
      if (style::isAbsLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                       style::absLengthVal (getStyle()->height));
         height = style::absLengthVal (getStyle()->height) + boxDiffHeight ();
      } else if (style::isPerLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                       100 * style::perLengthVal_useThisOnlyForDebugging
                                (getStyle()->height));
         // Notice that here -- unlike getAvailWidth() --
         // layout->hScrollbarThickness is not considered here;
         // something like canvasWidthGreater (analogue to
         // canvasHeightGreater) would be complicated and lead to
         // possibly contradictory self-references.
         height = applyPerHeight (layout->viewportHeight, getStyle()->height);
      } else {
         DBG_OBJ_MSG ("resize", 1, "no specification");
         if (forceValue)
            height = layout->viewportHeight;
         else
            height = -1;
      }

      DBG_OBJ_MSG_END ();
   } else if (parent) {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      height = parent->getAvailHeightOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   } else /* if (quasiParent) */ {
      DBG_OBJ_MSG ("resize", 1, "delegated to quasiParent");
      DBG_OBJ_MSG_START ();
      height = quasiParent->getAvailHeightOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_LEAVE_VAL ("%d", height);
   return height;
}

/**
 * Corrects a requisition to fit in the viewport.
 *
 * Instead of asking the parent widget, it uses the viewport dimensions to
 * correct the requisition if needed. This is used for the top level widget (the
 * body) which doesn't have any parent.
 */
void Widget::correctRequisitionViewport (Requisition *requisition,
                                 void (*splitHeightFun) (int, int *, int *),
                                 bool allowDecreaseWidth,
                                 bool allowDecreaseHeight)
{
   int limitMinWidth = getMinWidth (NULL, true);
   if (!allowDecreaseWidth && limitMinWidth < requisition->width)
      limitMinWidth = requisition->width;

   int viewportWidth =
      layout->viewportWidth - (layout->canvasHeightGreater ?
                               layout->vScrollbarThickness : 0);
   calcFinalWidth (getStyle (), viewportWidth, NULL, limitMinWidth, false,
                   &requisition->width);

   // For layout->viewportHeight, see comment in getAvailHeight().
   int height = calcHeight (getStyle()->height, false,
                            layout->viewportHeight, NULL, false);
   adjustHeight (&height, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   int minHeight = calcHeight (getStyle()->minHeight, false,
                               layout->viewportHeight, NULL, false);

   int maxHeight = calcHeight (getStyle()->maxHeight, false,
                               layout->viewportHeight, NULL, false);

   if (minHeight != -1 && maxHeight != -1) {
      /* Prefer the maximum size for pathological cases (min > max) */
      if (maxHeight < minHeight)
         maxHeight = minHeight;
   }

   adjustHeight (&minHeight, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   adjustHeight (&maxHeight, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   // TODO Perhaps split first, then add box ascent and descent.
   if (height != -1)
      splitHeightFun (height, &requisition->ascent, &requisition->descent);
   if (minHeight != -1 &&
       requisition->ascent + requisition->descent < minHeight)
      splitHeightFun (minHeight, &requisition->ascent,
                      &requisition->descent);
   if (maxHeight != -1 &&
       requisition->ascent + requisition->descent > maxHeight)
      splitHeightFun (maxHeight, &requisition->ascent,
                      &requisition->descent);
}

void Widget::correctRequisition (Requisition *requisition,
                                 void (*splitHeightFun) (int, int *, int *),
                                 bool allowDecreaseWidth,
                                 bool allowDecreaseHeight)
{
   // TODO Correct height by ... not extremes, but ...? (Height extremes?)

   DBG_OBJ_ENTER ("resize", 0, "correctRequisition",
                  "%d * (%d + %d), ..., %s, %s",
                  requisition->width, requisition->ascent,
                  requisition->descent, misc::boolToStr (allowDecreaseWidth),
                  misc::boolToStr (allowDecreaseHeight));

   if (parent == NULL && quasiParent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      for (int pass = 0; pass < 3; pass++) {
         correctRequisitionViewport (requisition, splitHeightFun,
               allowDecreaseWidth, allowDecreaseHeight);
         bool changed = correctReqAspectRatio (pass, this, requisition,
               allowDecreaseWidth, allowDecreaseHeight, splitHeightFun);

         /* No need to repeat more passes */
         if (!changed)
            break;
      }

      DBG_OBJ_MSG_END ();
   } else if (parent) {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      parent->correctRequisitionOfChild (this, requisition, splitHeightFun,
                                         allowDecreaseWidth,
                                         allowDecreaseHeight);
      DBG_OBJ_MSG_END ();
   } else /* if (quasiParent) */ {
      DBG_OBJ_MSG ("resize", 1, "delegated to quasiParent");
      DBG_OBJ_MSG_START ();
      quasiParent->correctRequisitionOfChild (this, requisition,
                                              splitHeightFun,
                                              allowDecreaseWidth,
                                              allowDecreaseHeight);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)", requisition->width, requisition->ascent,
                      requisition->descent);
}

void Widget::correctExtremes (Extremes *extremes, bool useAdjustmentWidth)
{
   DBG_OBJ_ENTER ("resize", 0, "correctExtremes", "%d (%d) / %d (%d)",
                  extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   if (container == NULL && quasiParent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      int limitMinWidth =
         useAdjustmentWidth ? getMinWidth (extremes, false) : 0;
      int viewportWidth =
         layout->viewportWidth - (layout->canvasHeightGreater ?
                                  layout->vScrollbarThickness : 0);

      int width = calcWidth (getStyle()->width, viewportWidth, NULL,
                             limitMinWidth, false);
      int minWidth = calcWidth (getStyle()->minWidth, viewportWidth, NULL,
                                limitMinWidth, false);
      int maxWidth = calcWidth (getStyle()->maxWidth, viewportWidth, NULL,
                                limitMinWidth, false);

      DBG_OBJ_MSGF ("resize", 1, "width = %d, minWidth = %d, maxWidth = %d",
                    width, minWidth, maxWidth);

      if (width != -1)
         extremes->minWidth = extremes->maxWidth = width;
      if (minWidth != -1)
         extremes->minWidth = minWidth;
      if (maxWidth != -1)
         extremes->maxWidth = maxWidth;

      DBG_OBJ_MSG_END ();
   } else if (parent) {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      parent->correctExtremesOfChild (this, extremes, useAdjustmentWidth);
      DBG_OBJ_MSG_END ();
   } else /* if (quasiParent) */ {
      DBG_OBJ_MSG ("resize", 1, "delegated to quasiParent");
      DBG_OBJ_MSG_START ();
      quasiParent->correctExtremesOfChild (this, extremes, useAdjustmentWidth);
      DBG_OBJ_MSG_END ();
   }

   if (extremes->maxWidth < extremes->minWidth)
      extremes->maxWidth = extremes->minWidth;

   DBG_OBJ_LEAVE_VAL ("%d / %d", extremes->minWidth, extremes->maxWidth);
}

/** Computes a width value in pixels from cssValue.
 *
 * If cssValue is absolute, the absolute value is used.
 * If cssValue is relative, then it is applied to refWidth.
 * Otherwise, -1 is used.
 *
 * In any case, the returned value is clamped so that is not smaller
 * than limitMinWidth.
 *
 */
int Widget::calcWidth (style::Length cssValue, int refWidth, Widget *refWidget,
                       int limitMinWidth, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "calcWidth", "0x%x, %d, %p, %d",
                  cssValue, refWidth, refWidget, limitMinWidth);

   assert (refWidth != -1 || refWidget != NULL);

   int width;

   if (style::isAbsLength (cssValue)) {
      DBG_OBJ_MSGF ("resize", 1, "absolute width: %dpx",
                    style::absLengthVal (cssValue));
      width = misc::max (style::absLengthVal (cssValue) + boxDiffWidth (),
                         limitMinWidth);
   } else if (style::isPerLength (cssValue)) {
      DBG_OBJ_MSGF ("resize", 1, "percentage width: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                             (cssValue));
      if (refWidth != -1)
         width = misc::max (applyPerWidth (refWidth, cssValue), limitMinWidth);
      else {
         int availWidth = refWidget->getAvailWidth (forceValue);
         if (availWidth != -1) {
            int containerWidth = availWidth - refWidget->boxDiffWidth ();
            width = misc::max (applyPerWidth (containerWidth, cssValue),
                               limitMinWidth);
         } else
            width = -1;
      }
   } else {
      DBG_OBJ_MSG ("resize", 1, "not specified");
      width = -1;
   }

   DBG_OBJ_LEAVE_VAL ("%d", width);
   return width;
}

/**
 * Computes the final width if possible and constraints it by min-width and
 * max-width.
 *
 * This function performs a very particular computation. It will try to find the
 * fixed width of the style provided by taking the refWidth or refWidget as the
 * reference to expand relative values.
 *
 * The value of *finalWidth is used to initialized the first value of width, so
 * it can be used to initialize a width when the style sets the width property
 * to auto.
 *
 * If both the initial *finalWidth and the style with are -1, the value will be
 * left as is, even if min-width and max-width could constraint the size. For the
 * width to be constrained, either the initial *finalWidth or the computed width
 * should return an absolute value.
 *
 * \post If *finalWidth != -1 the computed *finalWidth value is guarantee not to
 * be -1.
 */
void Widget::calcFinalWidth (style::Style *style, int refWidth,
                             Widget *refWidget, int limitMinWidth,
                             bool forceValue, int *finalWidth)
{
   DBG_OBJ_ENTER ("resize", 0, "calcFinalWidth", "..., %d, %p, %d, [%d]",
                  refWidth, refWidget, limitMinWidth, *finalWidth);

   int w = *finalWidth;
   int width = calcWidth (style->width, refWidth, refWidget, limitMinWidth,
                          forceValue);
   DBG_OBJ_MSGF ("resize", 1, "w = %d, width = %d", w, width);

   if (width != -1)
      w = width;

   /* Only correct w if not set to auto (-1) */
   if (w != -1) {
      int minWidth = calcWidth (style->minWidth, refWidth, refWidget,
                                limitMinWidth, forceValue);
      int maxWidth = calcWidth (style->maxWidth, refWidth, refWidget,
                                limitMinWidth, forceValue);

      DBG_OBJ_MSGF ("resize", 1, "minWidth = %d, maxWidth = %d",
                    minWidth, maxWidth);

      if (minWidth != -1 && maxWidth != -1) {
         /* Prefer the maximum size for pathological cases (min > max) */
         if (maxWidth < minWidth)
            maxWidth = minWidth;
      }

      if (minWidth != -1 && w < minWidth)
         w = minWidth;

      if (maxWidth != -1 && w > maxWidth)
         w = maxWidth;
   }

   /* Check postcondition: *finalWidth != -1 (implies) w != -1  */
   assert(!(*finalWidth != -1 && w == -1));

   *finalWidth = w;

   DBG_OBJ_LEAVE_VAL ("%d", *finalWidth);
}

int Widget::calcHeight (style::Length cssValue, bool usePercentage,
                        int refHeight, Widget *refWidget, bool forceValue)
{
   // TODO Search for usage of this method and check the value of
   // "usePercentage"; this has to be clarified.

   DBG_OBJ_ENTER ("resize", 0, "calcHeight", "0x%x, %s, %d, %p",
                  cssValue, usePercentage ? "true" : "false", refHeight,
                  refWidget);

   assert (refHeight != -1 || refWidget != NULL);

   int height;

   if (style::isAbsLength (cssValue)) {
      DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                    style::absLengthVal (cssValue));
      height =
         misc::max (style::absLengthVal (cssValue) + boxDiffHeight (), 0);
   } else if (style::isPerLength (cssValue)) {
      DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                    100 *
                    style::perLengthVal_useThisOnlyForDebugging (cssValue));
      if (usePercentage) {
         if (refHeight != -1)
            height = misc::max (applyPerHeight (refHeight, cssValue), 0);
         else {
            int availHeight = refWidget->getAvailHeight (forceValue);
            if (availHeight != -1) {
               int containerHeight = availHeight - refWidget->boxDiffHeight ();
               height =
                  misc::max (applyPerHeight (containerHeight, cssValue), 0);
            } else
               height = -1;
         }
      } else
         height = -1;
   } else {
      DBG_OBJ_MSG ("resize", 1, "not specified");
      height = -1;
   }

   DBG_OBJ_LEAVE_VAL ("%d", height);
   return height;
}

void Widget::adjustHeight (int *height, bool allowDecreaseHeight, int ascent,
                           int descent)
{
   if (!allowDecreaseHeight && *height != -1 && *height < ascent + descent)
      *height = ascent + descent;
}

/**
 * \brief Wrapper for Widget::getExtremesImpl().
 */
void Widget::getExtremes (Extremes *extremes, int numPos, Widget **references,
                          int *x, int *y)
{
   assert (!queueResizeEntered ());

   DBG_OBJ_ENTER ("resize", 0, "getExtremes", "%d, ...", numPos);

   enterGetExtremes ();

   if (extremesQueued ()) {
      // This method is called outside of Layout::resizeIdle.
      setFlags (EXTREMES_CHANGED);
      unsetFlags (EXTREMES_QUEUED);
      // The widget is not taken out of Layout::queueResizeList, since
      // other *_QUEUED flags may still be set and processed in
      // Layout::resizeIdle.
   }

   bool callImpl;
   if (extremesChanged ())
      callImpl = true;
   else {
      // Even if EXTREMES_QUEUED / EXTREMES_CHANGED is not set, calling
      // getExtremesImpl is necessary when the relavive positions passed here
      // have changed.
      SizeParams newParams (numPos, references, x, y);
      DBG_OBJ_ASSOC_CHILD (&newParams);
      if (newParams.isEquivalent (&extremesParams))
         callImpl = false;
      else {
         callImpl = true;
         extremesParams = newParams;
      }
   }
   
   if (callImpl) {
      // For backward compatibility (part 1/2):
      extremes->minWidthIntrinsic = extremes->maxWidthIntrinsic = -1;

      getExtremesImpl (extremes, numPos, references, x, y);

      // For backward compatibility (part 2/2):
      if (extremes->minWidthIntrinsic == -1)
         extremes->minWidthIntrinsic = extremes->minWidth;
      if (extremes->maxWidthIntrinsic == -1)
         extremes->maxWidthIntrinsic = extremes->maxWidth;

      this->extremes = *extremes;
      unsetFlags (EXTREMES_CHANGED);

      DBG_OBJ_SET_NUM ("extremes.minWidth", extremes->minWidth);
      DBG_OBJ_SET_NUM ("extremes.minWidthIntrinsic",
                       extremes->minWidthIntrinsic);
      DBG_OBJ_SET_NUM ("extremes.maxWidth", extremes->maxWidth);
      DBG_OBJ_SET_NUM ("extremes.maxWidthIntrinsic",
                       extremes->maxWidthIntrinsic);
      DBG_OBJ_SET_NUM ("extremes.adjustmentWidth", extremes->adjustmentWidth);
   } else
      *extremes = this->extremes;

   leaveGetExtremes ();

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Calculates dw::core::Widget::extraSpace.
 *
 * Delegated to dw::core::Widget::calcExtraSpaceImpl. Called both from
 * dw::core::Widget::sizeRequest and dw::core::Widget::getExtremes.
 */
void Widget::calcExtraSpace (int numPos, Widget **references, int *x, int *y)
{
   DBG_OBJ_ENTER0 ("resize", 0, "calcExtraSpace");

   extraSpace.top = extraSpace.right = extraSpace.bottom = extraSpace.left = 0;
   calcExtraSpaceImpl (numPos, references, x, y);

   DBG_OBJ_SET_NUM ("extraSpace.top", extraSpace.top);
   DBG_OBJ_SET_NUM ("extraSpace.bottom", extraSpace.bottom);
   DBG_OBJ_SET_NUM ("extraSpace.left", extraSpace.left);
   DBG_OBJ_SET_NUM ("extraSpace.right", extraSpace.right);

   DBG_OBJ_LEAVE ();
}

int Widget::numSizeRequestReferences ()
{
   return 0;
}

Widget *Widget::sizeRequestReference (int index)
{
   misc::notImplemented ("Widget::sizeRequestReference");
   return NULL;
}

int Widget::numGetExtremesReferences ()
{
   return 0;
}

Widget *Widget::getExtremesReference (int index)
{
   misc::notImplemented ("Widget::getExtremesReference");
   return NULL;
}

/**
 * \brief Wrapper for Widget::sizeAllocateImpl, calls the latter only when
 * needed.
 *
 * Sets the allocation of the widget to \param allocation, which is the final
 * size and position it will have on the canvas. This is usually called after
 * the requisition size is determined in Widget::sizeRequest().
 */
void Widget::sizeAllocate (Allocation *allocation)
{
   assert (!queueResizeEntered ());
   assert (!sizeRequestEntered ());
   assert (!getExtremesEntered ());
   assert (resizeIdleEntered ());

   DBG_OBJ_ENTER ("resize", 0, "sizeAllocate", "%d, %d; %d * (%d + %d)",
                  allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   DBG_OBJ_MSGF ("resize", 1,
                 "old allocation (%d, %d; %d * (%d + %d)); needsAllocate: %s",
                 this->allocation.x, this->allocation.y, this->allocation.width,
                 this->allocation.ascent, this->allocation.descent,
                 needsAllocate () ? "true" : "false");

   enterSizeAllocate ();

   /*printf ("The %stop-level %s %p is allocated:\n",
           parent ? "non-" : "", getClassName(), this);
   printf ("   old = (%d, %d, %d + (%d + %d))\n",
           this->allocation.x, this->allocation.y, this->allocation.width,
           this->allocation.ascent, this->allocation.descent);
   printf ("   new = (%d, %d, %d + (%d + %d))\n",
           allocation->x, allocation->y, allocation->width, allocation->ascent,
           allocation->descent);
   printf ("   NEEDS_ALLOCATE = %s\n", needsAllocate () ? "true" : "false");*/

   if (needsAllocate () ||
       allocation->x != this->allocation.x ||
       allocation->y != this->allocation.y ||
       allocation->width != this->allocation.width ||
       allocation->ascent != this->allocation.ascent ||
       allocation->descent != this->allocation.descent) {

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

      DBG_OBJ_SET_NUM ("allocation.x", this->allocation.x);
      DBG_OBJ_SET_NUM ("allocation.y", this->allocation.y);
      DBG_OBJ_SET_NUM ("allocation.width", this->allocation.width);
      DBG_OBJ_SET_NUM ("allocation.ascent", this->allocation.ascent);
      DBG_OBJ_SET_NUM ("allocation.descent", this->allocation.descent);
   }

   /*unsetFlags (NEEDS_RESIZE);*/

   leaveSizeAllocate ();

   DBG_OBJ_LEAVE ();
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

   if (widgetImgRenderer && this->style && this->style->backgroundImage)
      this->style->backgroundImage->removeExternalImgRenderer
         (widgetImgRenderer);

   style->ref ();

   if (this->style) {
      sizeChanged = this->style->sizeDiffs (style);
      this->style->unref ();
   } else
      sizeChanged = true;

   this->style = style;

   DBG_OBJ_ASSOC_CHILD (style);

   if (style && style->backgroundImage) {
      // Create instance of WidgetImgRenderer when needed. Until this
      // widget is deleted, "widgetImgRenderer" will be kept, since it
      // is not specific to the style, but only to this widget.
      if (widgetImgRenderer == NULL)
         widgetImgRenderer = new WidgetImgRenderer (this);
      style->backgroundImage->putExternalImgRenderer (widgetImgRenderer);
   }

   if (layout != NULL) {
      layout->updateCursor ();
   }

   // After Layout::addWidget() (as toplevel widget) or Widget::setParent()
   // (which also sets layout), changes of the style cannot be considered
   // anymore. (Should print a warning?)
   if (layout == NULL &&
       StackingContextMgr::isEstablishingStackingContext (this)) {
      stackingContextMgr = new StackingContextMgr (this);
      DBG_OBJ_ASSOC_CHILD (stackingContextMgr);
      stackingContextWidget = this;
   }

   if (sizeChanged)
      queueResize (0, true);
   else
      queueDraw ();

   // These should better be attributed to the style itself, and a
   // script processing RTFL messages could transfer it to something
   // equivalent:

   DBG_OBJ_SET_NUM ("style.margin.top", style->margin.top);
   DBG_OBJ_SET_NUM ("style.margin.bottom", style->margin.bottom);
   DBG_OBJ_SET_NUM ("style.margin.left", style->margin.left);
   DBG_OBJ_SET_NUM ("style.margin.right", style->margin.right);

   DBG_OBJ_SET_NUM ("style.border-width.top", style->borderWidth.top);
   DBG_OBJ_SET_NUM ("style.border-width.bottom", style->borderWidth.bottom);
   DBG_OBJ_SET_NUM ("style.border-width.left", style->borderWidth.left);
   DBG_OBJ_SET_NUM ("style.border-width.right", style->borderWidth.right);

   DBG_OBJ_SET_NUM ("style.padding.top", style->padding.top);
   DBG_OBJ_SET_NUM ("style.padding.bottom", style->padding.bottom);
   DBG_OBJ_SET_NUM ("style.padding.left", style->padding.left);
   DBG_OBJ_SET_NUM ("style.padding.right", style->padding.right);

   DBG_OBJ_SET_NUM ("style.border-spacing (h)", style->hBorderSpacing);
   DBG_OBJ_SET_NUM ("style.border-spacing (v)", style->vBorderSpacing);

   DBG_OBJ_SET_SYM ("style.display",
                    style->display == style::DISPLAY_BLOCK ? "block" :
                    style->display == style::DISPLAY_INLINE ? "inline" :
                    style->display == style::DISPLAY_INLINE_BLOCK ?
                       "inline-block" :
                    style->display == style::DISPLAY_LIST_ITEM ? "list-item" :
                    style->display == style::DISPLAY_NONE ? "none" :
                    style->display == style::DISPLAY_TABLE ? "table" :
                    style->display == style::DISPLAY_TABLE_ROW_GROUP ?
                       "table-row-group" :
                    style->display == style::DISPLAY_TABLE_HEADER_GROUP ?
                       "table-header-group" :
                    style->display == style::DISPLAY_TABLE_FOOTER_GROUP ?
                       "table-footer-group" :
                    style->display == style::DISPLAY_TABLE_ROW ? "table-row" :
                    style->display == style::DISPLAY_TABLE_CELL ? "table-cell" :
                    "???");

   DBG_OBJ_SET_NUM ("style.width (raw)", style->width);
   DBG_OBJ_SET_NUM ("style.min-width (raw)", style->minWidth);
   DBG_OBJ_SET_NUM ("style.max-width (raw)", style->maxWidth);
   DBG_OBJ_SET_NUM ("style.height (raw)", style->height);
   DBG_OBJ_SET_NUM ("style.min-height (raw)", style->minHeight);
   DBG_OBJ_SET_NUM ("style.max-height (raw)", style->maxHeight);

   if (style->backgroundColor)
      DBG_OBJ_SET_COL ("style.background-color",
                       style->backgroundColor->getColor ());
   else
      DBG_OBJ_SET_SYM ("style.background-color", "transparent");
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
 * \brief Get the actual foreground color of a widget.
 */
style::Color *Widget::getFgColor ()
{
   Widget *widget = this;

   while (widget != NULL) {
      if (widget->style->color)
         return widget->style->color;

      widget = widget->parent;
   }

   return NULL;
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
   Rectangle canvasArea;
   canvasArea.x = area->x + allocation.x;
   canvasArea.y = area->y + allocation.y;
   canvasArea.width = area->width;
   canvasArea.height = area->height;

   style::drawBorder (view, layout, &canvasArea,
                      allocation.x + x, allocation.y + y,
                      width, height, style, inverse);

   // This method is used for inline elements, where the CSS 2 specification
   // does not define what here is called "reference area". To make it look
   // smoothly, the widget padding box is used.

   // TODO Handle inverse drawing the same way as in drawWidgetBox?
   // Maybe this method (drawBox) is anyway obsolete when extraSpace
   // is fully supported (as here, in the "dillo_grows" repository).

   int xPad, yPad, widthPad, heightPad;
   getPaddingArea (&xPad, &yPad, &widthPad, &heightPad);
   style::drawBackground
      (view, layout, &canvasArea,
       allocation.x + x + style->margin.left + style->borderWidth.left,
       allocation.y + y + style->margin.top + style->borderWidth.top,
       width - style->margin.left - style->borderWidth.left
       - style->margin.right - style->borderWidth.right,
       height - style->margin.top - style->borderWidth.top
       - style->margin.bottom - style->borderWidth.bottom,
       xPad, yPad, widthPad, heightPad, style, style->backgroundColor,
       inverse, false);
}

/**
 * \brief Draw borders and background of a widget.
 *
 * area is given in widget coordinates.
 *
 */
void Widget::drawWidgetBox (View *view, Rectangle *area, bool inverse)
{
   Rectangle canvasArea;
   canvasArea.x = area->x + allocation.x;
   canvasArea.y = area->y + allocation.y;
   canvasArea.width = area->width;
   canvasArea.height = area->height;

   int xMar, yMar, widthMar, heightMar;
   getMarginArea (&xMar, &yMar, &widthMar, &heightMar);
   style::drawBorder (view, layout, &canvasArea, xMar, yMar, widthMar,
                      heightMar, style, inverse);

   int xPad, yPad, widthPad, heightPad;
   getPaddingArea (&xPad, &yPad, &widthPad, &heightPad);

   style::Color *bgColor;
   if (inverse && style->backgroundColor == NULL) {
      // See style::drawBackground: for inverse drawing, we need a
      // defined background color. Search through ancestors.
      Widget *w = this;
      while (w != NULL && w->style->backgroundColor == NULL)
         w = w->parent;
      
      if (w != NULL && w->style->backgroundColor != NULL)
         bgColor = w->style->backgroundColor;
      else
         bgColor = layout->getBgColor ();
   } else
      bgColor = style->backgroundColor;

   style::drawBackground (view, layout, &canvasArea,
                          xPad, yPad, widthPad, heightPad,
                          xPad, yPad, widthPad, heightPad,
                          style, bgColor, inverse, parent == NULL);
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
 * \brief Get the level of the widget within the tree, regarding the
 * generators, not the parents.
 *
 * The root widget has the level 0.
 */
int Widget::getGeneratorLevel ()
{
   Widget *widget = this;
   int level = 0;

   while (widget->getGenerator ()) {
      level++;
      widget = widget->getGenerator ();
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

void Widget::scrollTo (HPosition hpos, VPosition vpos,
               int x, int y, int width, int height)
{
   layout->scrollTo (hpos, vpos,
                     x + allocation.x, y + allocation.y, width, height);
}

void Widget::getMarginArea (int *xMar, int *yMar, int *widthMar, int *heightMar)
{
   *xMar = allocation.x + extraSpace.left;
   *yMar = allocation.y + extraSpace.top;
   *widthMar = allocation.width - (extraSpace.left + extraSpace.right);
   *heightMar = getHeight () - (extraSpace.top + extraSpace.bottom);
}

void Widget::getBorderArea (int *xBor, int *yBor, int *widthBor, int *heightBor)
{
   getMarginArea (xBor, yBor, widthBor, heightBor);

   *xBor += style->margin.left;
   *yBor += style->margin.top;
   *widthBor -= style->margin.left + style->margin.right;
   *heightBor -= style->margin.top + style->margin.bottom;
}

/**
 * \brief Return the padding area (content plus padding).
 *
 * Used as "reference area" (ee comment of "style::drawBackground")
 * for backgrounds.
 */
void Widget::getPaddingArea (int *xPad, int *yPad, int *widthPad,
                             int *heightPad)
{
   getBorderArea (xPad, yPad, widthPad, heightPad);

   *xPad += style->borderWidth.left;
   *yPad += style->borderWidth.top;
   *widthPad -= style->borderWidth.left + style->borderWidth.right;
   *heightPad -= style->borderWidth.top + style->borderWidth.bottom;
}

void Widget::sizeRequestImpl (Requisition *requisition, int numPos,
                              Widget **references, int *x, int *y)
{
   // Use the simple variant.
   DBG_OBJ_ENTER0 ("resize", 0, "Widget::sizeRequestImpl");
   sizeRequestSimpl (requisition);
   DBG_OBJ_LEAVE ();
}

void Widget::sizeRequestSimpl (Requisition *requisition)
{
   // Either variant should be implemented.
   misc::notImplemented ("Widget::sizeRequestSimpl");
}

void Widget::getExtremesImpl (Extremes *extremes, int numPos,
                              Widget **references, int *x, int *y)
{
   // Use the simple variant.
   DBG_OBJ_ENTER0 ("resize", 0, "Widget::getExtremesImpl");
   getExtremesSimpl (extremes);
   DBG_OBJ_LEAVE ();
}

void Widget::getExtremesSimpl (Extremes *extremes)
{
    // Either variant should be implemented.
   misc::notImplemented ("Widget::getExtremesSimpl");
}

void Widget::sizeAllocateImpl (Allocation *allocation)
{
}

/**
 * \brief The actual implementation for calculating
 *    dw::core::Widget::extraSpace.
 *
 * The implementation gets a clean value of
 * dw::core::Widget::extraSpace, which is only corrected. To make sure
 * all possible influences are considered, the implementation of the
 * base class should be called, too.
 */
void Widget::calcExtraSpaceImpl (int numPos, Widget **references, int *x,
                                 int *y)
{
}

void Widget::markSizeChange (int ref)
{
}

void Widget::markExtremesChange (int ref)
{
}

int Widget::applyPerWidth (int containerWidth, style::Length perWidth)
{
   return style::multiplyWithPerLength (containerWidth, perWidth)
      + boxDiffWidth ();
}

int Widget::applyPerHeight (int containerHeight, style::Length perHeight)
{
   return style::multiplyWithPerLength (containerHeight, perHeight)
      + boxDiffHeight ();
}

/**
 * Computes the content width available of a child widget.
 *
 * @param child      The child widget of which the available width will be
 *                   computed.
 * @param forceValue If true, computes the width of the child with value
 *                   "auto". Otherwise, it won't.
 *
 * @return The available width in pixels or -1.
 */
int Widget::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   // This is a halfway suitable implementation for all
   // containers. For simplification, this will be used during the
   // development; then, a differentiation could be possible.

   DBG_OBJ_ENTER ("resize", 0, "getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;

   if (child->getStyle()->width == style::LENGTH_AUTO) {
      DBG_OBJ_MSG ("resize", 1, "no specification");

      /* We only compute when forceValue is true */
      if (forceValue) {
         width = misc::max (getAvailWidth (true) - boxDiffWidth (), 0);

         if (width != -1) {
            /* Clamp to min-width and max-width if given */
            int maxWidth = child->calcWidth (child->getStyle()->maxWidth,
                  -1, this, -1, false);
            if (maxWidth != -1 && width > maxWidth)
               width = maxWidth;

            int minWidth = child->calcWidth (child->getStyle()->minWidth,
                  -1, this, -1, false);
            if (minWidth != -1 && width < minWidth)
               width = minWidth;
         }

      } else {
         width = -1;
      }
   } else {
      // In most cases, the toplevel widget should be a container, so
      // the container is non-NULL when the parent is non-NULL. Just
      // in case, regard also parent. And quasiParent.
      Widget *effContainer = child->quasiParent ? child->quasiParent :
         (child->container ? child->container : child->parent);

      if (effContainer == this) {
         width = -1;
         child->calcFinalWidth (child->getStyle(), -1, this, 0, forceValue,
                                &width);
      } else {
         DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
         DBG_OBJ_MSG_START ();
         width = effContainer->getAvailWidthOfChild (child, forceValue);
         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_LEAVE_VAL ("%d", width);
   return width;
}

int Widget::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   // Again, a suitable implementation for all widgets (perhaps).

   // TODO Consider 'min-height' and 'max-height'. (Minor priority, as long as
   //      "getAvailHeight (true)" is not used.

   DBG_OBJ_ENTER ("resize", 0, "getAvailHeightOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int height;

   if (child->getStyle()->height == style::LENGTH_AUTO) {
      DBG_OBJ_MSG ("resize", 1, "no specification");
      if (forceValue)
         height = misc::max (getAvailHeight (true) - boxDiffHeight (), 0);
      else
         height = -1;
   } else {
      // See comment in Widget::getAvailWidthOfChild.
      Widget *effContainer = child->quasiParent ? child->quasiParent :
         (child->container ? child->container : child->parent);

      if (effContainer == this) {
         if (style::isAbsLength (child->getStyle()->height)) {
            DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                          style::absLengthVal (child->getStyle()->height));
            height = misc::max (style::absLengthVal (child->getStyle()->height)
                               + child->boxDiffHeight (), 0);
         } else {
            assert (style::isPerLength (child->getStyle()->height));
            DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                          100 * style::perLengthVal_useThisOnlyForDebugging
                          (child->getStyle()->height));

            int availHeight = getAvailHeight (forceValue);
            if (availHeight == -1)
               height = -1;
            else
               height =
                  misc::max (child->applyPerHeight (availHeight -
                                                    boxDiffHeight (),
                                                    child->getStyle()->height),
                             0);
         }
      } else {
         DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
         DBG_OBJ_MSG_START ();
         height = effContainer->getAvailHeightOfChild (child, forceValue);
         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_LEAVE_VAL ("%d", height);
   return height;
}

void Widget::correctRequisitionOfChild (Widget *child, Requisition *requisition,
                                        void (*splitHeightFun) (int, int*,
                                                                int*),
                                        bool allowDecreaseWidth,
                                        bool allowDecreaseHeight)
{
   // Again, a suitable implementation for all widgets (perhaps).

   DBG_OBJ_ENTER ("resize", 0, "correctRequisitionOfChild",
                  "%p, %d * (%d + %d), ..., %s, %s", child, requisition->width,
                  requisition->ascent, requisition->descent,
                  misc::boolToStr (allowDecreaseWidth),
                  misc::boolToStr (allowDecreaseHeight));

   // See comment in Widget::getAvailWidthOfChild.
   Widget *effContainer = child->quasiParent ? child->quasiParent :
      (child->container ? child->container : child->parent);

   if (effContainer == this) {
      /* Try several passes before giving up */
      for (int pass = 0; pass < 3; pass++) {
         correctReqWidthOfChild (child, requisition, allowDecreaseWidth);
         correctReqHeightOfChild (child, requisition, splitHeightFun,
                                  allowDecreaseHeight);
         bool changed = correctReqAspectRatio (pass, child, requisition,
               allowDecreaseWidth, allowDecreaseHeight, splitHeightFun);

         /* No need to repeat more passes */
         if (!changed)
            break;
      }
   } else {
      DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
      DBG_OBJ_MSG_START ();
      effContainer->correctRequisitionOfChild (child, requisition,
                                               splitHeightFun,
                                               allowDecreaseWidth,
                                               allowDecreaseHeight);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)", requisition->width, requisition->ascent,
                      requisition->descent);
}

/**
 * Correct a child requisition aspect ratio if needed.
 *
 * After the parent widget corrects the requisition provided by the \param
 * child, the preferred aspect ratio may no longer be the current ratio. This
 * method tries to adjust the size of the requisition so the aspect ratio is the
 * preferred aspect ratio of the child.
 *
 * It implements three passes: increase (0), decrease (1) or keep (2). In the
 * increase pass, the size is increased to fill the aspect ratio. If after
 * correcting the size, it is still not the preferred aspect ratio (maybe it
 * breaks some other constraint), reducing the size will be attempted. If at the
 * end, reducing the size doesn't fix the preferred aspect ratio, the size is
 * kept as it is.
 *
 * It can be called from the parent or the child itself, as it doesn't read any
 * information from the current widget.
 *
 * \return true if the requisition has been modified, false otherwise.
 */
bool Widget::correctReqAspectRatio (int pass, Widget *child, Requisition *requisition,
                                     bool allowDecreaseWidth, bool allowDecreaseHeight,
                                     void (*splitHeightFun) (int, int*, int*))
{
   /* Only correct the requisition if both dimensions are set, otherwise is left
    * to the child how to proceed. */
   int wReq = requisition->width;
   int hReq = requisition->ascent + requisition->descent;

   /* Prevent division by 0 */
   bool sizeSet = wReq > 0 && hReq > 0;

   float ratio = child->ratio;
   bool changed = false;

   DEBUG_MSG(1, "Widget::correctReqAspectRatio() -- wReq=%d, hReq=%d, pass=%d\n",
         wReq, hReq, pass);

   DEBUG_MSG(1, "Widget::correctReqAspectRatio() -- border: w=%d, h=%d\n",
         child->boxDiffWidth(), child->boxDiffHeight());

   wReq -= child->boxDiffWidth();
   hReq -= child->boxDiffHeight();
   DEBUG_MSG(1, "Widget::correctReqAspectRatio() -- with border: wReq=%d, hReq=%d\n",
         wReq, hReq);
   DEBUG_MSG(1, "child=%s, preferred ratio=%f\n", child->getClassName(), ratio);

   if (pass != PASS_KEEP && ratio > 0.0 && sizeSet) {
      /* Compute the current ratio from the content box. */
      float curRatio = (float) wReq / (float) hReq;
      DEBUG_MSG(1, "curRatio=%f, preferred ratio=%f\n", curRatio, ratio);

      if (curRatio < ratio) {
         /* W is too small compared to H. Try to increase W or decrease H. */
         if (pass == PASS_INCREASE) {
            /* Increase w */
            int w = (float) hReq * ratio;
            DEBUG_MSG(1, "increase w: %d -> %d\n", wReq, w);
            w += child->boxDiffWidth();
            DEBUG_MSG(1, "increase w (with border): %d -> %d\n", wReq, w);
            requisition->width = w;
            changed = true;
         } else if (pass == PASS_DECREASE) {
            /* Decrease h */
            if (allowDecreaseHeight) {
               /* FIXME: This may lose cases where allowDecreaseHeight is false, and
                * the requisition has increased the height first, but we could still
                * reduce the corrected height above the original height, without
                * making the requisition height smaller. */
               int h = (float) wReq / ratio;
               DEBUG_MSG(1, "decrease h: %d -> %d\n", hReq, h);
               h += child->boxDiffHeight();
               DEBUG_MSG(1, "decrease h (with border): %d -> %d\n", hReq, h);
               splitHeightFun (h, &requisition->ascent, &requisition->descent);
               changed = true;
            }
         }
      } else if (curRatio > ratio) {
         /* W is too big compared to H. Try to decrease W or increase H. */
         if (pass == PASS_INCREASE) {
            /* Increase h */
            int h = (float) wReq / ratio;
            DEBUG_MSG(1, "increase h: %d -> %d\n", hReq, h);
            h += child->boxDiffHeight();
            DEBUG_MSG(1, "increase h (width border): %d -> %d\n", hReq, h);
            splitHeightFun (h, &requisition->ascent, &requisition->descent);
            changed = true;
         } else if (pass == PASS_DECREASE) {
            /* Decrease w */
            if (allowDecreaseWidth) {
               /* FIXME: This may lose cases where allowDecreaseWidth is false, and
                * the requisition has increased the width first, but we could still
                * reduce the corrected width above the original width, without
                * making the requisition width smaller. */
               int w = (float) hReq * ratio;
               DEBUG_MSG(1, "decrease w: %d -> %d\n", wReq, w);
               w += child->boxDiffWidth();
               DEBUG_MSG(1, "decrease w (width border): %d -> %d\n", wReq, w);
               requisition->width = w;
               changed = true;
            }
         }
      } else {
         /* Current ratio is the preferred one. */
      }
   }

   DEBUG_MSG(1, "Widget::correctReqAspectRatio() -- output: wReq=%d, hReq=%d, changed=%d\n",
         requisition->width, requisition->ascent + requisition->descent, changed);

   return changed;
}

/** Correct a child requisition to fit the parent.
 *
 * The \param requisition is adjusted in width so it fits in the current widget
 * (parent).
 */
void Widget::correctReqWidthOfChild (Widget *child, Requisition *requisition,
                                     bool allowDecreaseWidth)
{
   DBG_OBJ_ENTER ("resize", 0, "correctReqWidthOfChild",
                  "%p, %d * (%d + %d), %s",
                  child, requisition->width, requisition->ascent,
                  requisition->descent, misc::boolToStr (allowDecreaseWidth));

   assert (this == child->quasiParent || this == child->container);

   int limitMinWidth = child->getMinWidth (NULL, true);
   if (!allowDecreaseWidth && limitMinWidth < requisition->width)
      limitMinWidth = requisition->width;

   child->calcFinalWidth (child->getStyle(), -1, this, limitMinWidth, true,
                          &requisition->width);

   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)", requisition->width, requisition->ascent,
                      requisition->descent);
}

void Widget::correctReqHeightOfChild (Widget *child, Requisition *requisition,
                                      void (*splitHeightFun) (int, int*, int*),
                                      bool allowDecreaseHeight)
{
   // TODO Correct height by extremes? (Height extremes?)

   assert (this == child->quasiParent || this == child->container);

   DBG_OBJ_ENTER ("resize", 0, "correctReqHeightOfChild",
                  "%p, %d * (%d + %d), ..., %s", child, requisition->width,
                  requisition->ascent, requisition->descent,
                  misc::boolToStr (allowDecreaseHeight));

   int height = child->calcHeight (child->getStyle()->height, true, -1, this,
                                   false);
   adjustHeight (&height, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   int minHeight = child->calcHeight (child->getStyle()->minHeight, true, -1,
                                      this, false);
   int maxHeight = child->calcHeight (child->getStyle()->maxHeight, true, -1,
                                      this, false);

   if (minHeight != -1 && maxHeight != -1) {
      /* Prefer the maximum size for pathological cases (min > max) */
      if (maxHeight < minHeight)
         maxHeight = minHeight;
   }

   adjustHeight (&minHeight, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   adjustHeight (&maxHeight, allowDecreaseHeight, requisition->ascent,
                 requisition->descent);

   // TODO Perhaps split first, then add box ascent and descent.
   if (height != -1)
      splitHeightFun (height, &requisition->ascent, &requisition->descent);
   if (minHeight != -1 &&
       requisition->ascent + requisition->descent < minHeight)
      splitHeightFun (minHeight, &requisition->ascent,
                      &requisition->descent);
   if (maxHeight != -1 &&
       requisition->ascent + requisition->descent > maxHeight)
      splitHeightFun (maxHeight, &requisition->ascent,
                      &requisition->descent);

   DBG_OBJ_LEAVE_VAL ("%d * (%d + %d)", requisition->width, requisition->ascent,
                      requisition->descent);
}

void Widget::correctExtremesOfChild (Widget *child, Extremes *extremes,
                                     bool useAdjustmentWidth)
{
   // See comment in correctRequisitionOfChild.

   DBG_OBJ_ENTER ("resize", 0, "correctExtremesOfChild",
                  "%p, %d (%d) / %d (%d)",
                  child, extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   // See comment in Widget::getAvailWidthOfChild.
   Widget *effContainer = child->quasiParent ? child->quasiParent :
      (child->container ? child->container : child->parent);

   if (effContainer == this) {
      int limitMinWidth =
         useAdjustmentWidth ? child->getMinWidth (extremes, false) : 0;
      int width = child->calcWidth (child->getStyle()->width, -1, this,
                                    limitMinWidth, false);
      int minWidth = child->calcWidth (child->getStyle()->minWidth, -1, this,
                                       limitMinWidth, false);
      int maxWidth = child->calcWidth (child->getStyle()->maxWidth, -1, this,
                                       limitMinWidth, false);

      DBG_OBJ_MSGF ("resize", 1, "width = %d, minWidth = %d, maxWidth = %d",
                    width, minWidth, maxWidth);

      if (width != -1)
         extremes->minWidth = extremes->maxWidth = width;
      if (minWidth != -1)
         extremes->minWidth = minWidth;
      if (maxWidth != -1)
         extremes->maxWidth = maxWidth;
   } else {
      DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
      DBG_OBJ_MSG_START ();
      effContainer->correctExtremesOfChild (child, extremes,
                                            useAdjustmentWidth);
      DBG_OBJ_MSG_END ();
   }


   DBG_OBJ_LEAVE_VAL ("%d / %d", extremes->minWidth, extremes->maxWidth);
}

/**
 * \brief This method is called after a widget has been set as the top of a
 *    widget tree.
 *
 * A widget may override this method when it is necessary to be notified.
 */
void Widget::notifySetAsTopLevel()
{
}

/**
 * \brief This method is called after a widget has been added to a parent.
 *
 * A widget may override this method when it is necessary to be notified.
 */
void Widget::notifySetParent()
{
}

bool Widget::isBlockLevel ()
{
   // Most widgets are not block-level.
   return false;
}

bool Widget::isPossibleContainer ()
{
   // In most (all?) cases identical to:
   return isBlockLevel ();
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
   style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onEnter();
}

void Widget::leaveNotifyImpl (EventCrossing *)
{
   style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onLeave();
}


void Widget::removeChild (Widget *child)
{
   // Should be implemented.
   misc::notImplemented ("Widget::removeChild");
}

// ----------------------------------------------------------------------

void splitHeightPreserveAscent (int height, int *ascent, int *descent)
{
   DBG_OBJ_ENTER_S ("resize", 1, "splitHeightPreserveAscent", "%d, %d, %d",
                    height, *ascent, *descent);
   
   *descent = height - *ascent;
   if (*descent < 0) {
      *descent = 0;
      *ascent = height;
   }

   DBG_OBJ_LEAVE_VAL_S ("%d, %d", *ascent, *descent);
}

void splitHeightPreserveDescent (int height, int *ascent, int *descent)
{
   DBG_OBJ_ENTER_S ("resize", 1, "splitHeightPreserveDescent", "%d, %d, %d",
                    height, *ascent, *descent);

   *ascent = height - *descent;
   if (*ascent < 0) {
      *ascent = 0;
      *descent = height;
   }

   DBG_OBJ_LEAVE_VAL_S ("%d, %d", *ascent, *descent);
}

} // namespace core
} // namespace dw
