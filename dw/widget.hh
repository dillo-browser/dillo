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

#ifndef __DW_WIDGET_HH__
#define __DW_WIDGET_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include "../lout/identity.hh"

typedef struct {
   int total;
   int marginLeft;
   int marginRight;
} BoxWidth;

/**
 * \brief The type for callback functions.
 */
typedef void (*DW_Callback_t)(void *data);

namespace dw {
namespace core {

/**
 * \brief The base class of all dillo widgets.
 *
 * \sa\ref dw-overview, \ref dw-layout-widgets
 */
class Widget: public lout::identity::IdentifiableObject
{
   friend class Layout;

protected:
   enum Flags {
      /**
       * \todo Comment this.
       */
      RESIZE_QUEUED    = 1 << 0,

      /**
       * \todo Comment this.
       */
      EXTREMES_QUEUED  = 1 << 1,

      /**
       * \brief Set, when dw::core::Widget::requisition is not up to date
       *    anymore.
       *
       * \todo Update, see RESIZE_QUEUED.
       */
      NEEDS_RESIZE     = 1 << 2,

      /**
       * \brief Only used internally, set to enforce size allocation.
       *
       * In some cases, the size of a widget remains the same, but the
       * children are allocated at different positions and in
       * different sizes, so that a simple comparison of old and new
       * allocation is insufficient. Therefore, this flag is set
       * (indirectly, as ALLOCATE_QUEUED) in queueResize.
       */
      NEEDS_ALLOCATE   = 1 << 3,

      /**
       * \todo Comment this.
       */
      ALLOCATE_QUEUED  = 1 << 4,

      /**
       * \brief Set, when dw::core::Widget::extremes is not up to date
       *    anymore.
       *
       * \todo Update, see RESIZE_QUEUED.
       */
      EXTREMES_CHANGED = 1 << 5,

      /**
       * \brief Set, when a widget was already once allocated,
       *
       * The dw::Image widget uses this flag, see dw::Image::setBuffer.
       */
      WAS_ALLOCATED    = 1 << 6
   };

   /**
    * \brief Implementation which represents the whole widget.
    *
    * The only instance is set created needed.
    */
   class WidgetImgRenderer: public style::StyleImage::ExternalWidgetImgRenderer
   {
   private:
      Widget *widget;

   public:
      inline WidgetImgRenderer (Widget *widget) { this->widget = widget; }

      bool readyToDraw ();
      void getBgArea (int *x, int *y, int *width, int *height);
      void getRefArea (int *xRef, int *yRef, int *widthRef, int *heightRef);
      style::Style *getStyle ();
      void draw (int x, int y, int width, int height);
   };

   WidgetImgRenderer *widgetImgRenderer;

private:
   static bool adjustMinWidth;

   /**
    * \brief The parent widget, NULL for top-level widgets.
    */
   Widget *parent;

   /**
    * \brief ...
    */
   Widget *quasiParent;

   /**
    * \brief The generating widget, NULL for top-level widgets, or if
    *    not set; in the latter case, the effective generator (see
    *    getGenerator) is the parent.
    */
   Widget *generator;

   /**
    * \brief The containing widget, equivalent to the "containing
    *    block" defined by CSS. May be NULL, in this case the viewport
    *    is used.
    */
   Widget *container;

   WidgetReference *widgetReference;
   
   style::Style *style;

   Flags flags;

   /**
    * \brief Size_request() stores the result of the last call of
    *    size_request_impl().
    *
    * Do not read this directly, but call size_request().
    */
   Requisition requisition;
   SizeParams requisitionParams;

   /**
    * \brief Analogue to dw::core::Widget::requisition.
    */
   Extremes extremes;
   SizeParams extremesParams;

   /**
    * \brief See dw::core::Widget::setBgColor().
    */
   style::Color *bgColor;

   /**
    * \brief See dw::core::Widget::setButtonSensitive().
    */
   bool buttonSensitive;

   /**
    * \brief See dw::core::Widget::setButtonSensitive().
    */
   bool buttonSensitiveSet;

   void queueResize (int ref, bool extremesChanged, bool fast);
   inline void queueResizeFast (int ref, bool extremesChanged)
   { queueResize (ref, extremesChanged, true); }

public:
   /**
    * \brief This value is defined by the parent widget, and used for
    *    incremential resizing.
    *
    * See documentation for an explanation.
    */
   int parentRef;

protected:

   /**
    * \brief The current allocation: size and position, always relative to the
    * canvas. The allocation is the outermost box for the widget, as in the CSS
    * box model. It also includes the extraSpace.
    */
   Allocation allocation;

   inline int getHeight () { return allocation.ascent + allocation.descent; }
   inline int getContentWidth() { return allocation.width - boxDiffWidth (); }
   inline int getContentHeight() { return getHeight () - boxDiffHeight (); }

   /**
    * Preferred aspect ratio of the widget. Set to 0 when there is none. It is
    * computed as width / height.
    */
   float ratio;

   Layout *layout;

   /**
    * \brief Space around the margin box. Allocation is extraSpace +
    *    margin + border + padding + contents.
    *
    * See also dw::core::Widget::calcExtraSpace and
    * dw::core::Widget::calcExtraSpaceImpl. Also, it is feasible to
    * correct this value within dw::core::Widget::sizeRequestImpl.
    */
   style::Box extraSpace;

   style::Box margin;

   /**
    * \brief Set iff this widget constitutes a stacking context, as defined by
    *    CSS.
    */
   StackingContextMgr *stackingContextMgr;

   /**
    * \brief The bottom-most ancestor (or this) for which stackingContextMgr is
    *    set.
    */
   Widget *stackingContextWidget;

   inline StackingContextMgr *getNextStackingContextMgr ()
   { return stackingContextWidget->stackingContextMgr; }

   /*inline void printFlags () {
      DBG_IF_RTFL {
         char buf[10 * 3 - 1 + 1];
         snprintf (buf, sizeof (buf), "%s:%s:%s:%s:%s:%s:%s",
                   (flags & RESIZE_QUEUED)    ? "Rq" : "--",
                   (flags & EXTREMES_QUEUED)  ? "Eq" : "--",
                   (flags & NEEDS_RESIZE)     ? "nR" : "--",
                   (flags & NEEDS_ALLOCATE)   ? "nA" : "--",
                   (flags & ALLOCATE_QUEUED)  ? "Aq" : "--",
                   (flags & EXTREMES_CHANGED) ? "Ec" : "--",
                   (flags & WAS_ALLOCATED)    ? "wA" : "--");
         DBG_OBJ_SET_SYM ("flags", buf);
      }
   }*/

   inline void printFlag (Flags f) {
      DBG_IF_RTFL {
         switch (f) {
         case RESIZE_QUEUED:
               DBG_OBJ_SET_SYM ("flags.RESIZE_QUEUED",
                                (flags & RESIZE_QUEUED) ? "true" : "false");
               break;

         case EXTREMES_QUEUED:
            DBG_OBJ_SET_SYM ("flags.EXTREMES_QUEUED",
                             (flags & EXTREMES_QUEUED) ? "true" : "false");
            break;

         case NEEDS_RESIZE:
            DBG_OBJ_SET_SYM ("flags.NEEDS_RESIZE",
                             (flags & NEEDS_RESIZE) ? "true" : "false");
            break;

         case NEEDS_ALLOCATE:
            DBG_OBJ_SET_SYM ("flags.NEEDS_ALLOCATE",
                             (flags & NEEDS_ALLOCATE) ? "true" : "false");
            break;

         case ALLOCATE_QUEUED:
            DBG_OBJ_SET_SYM ("flags.ALLOCATE_QUEUED",
                             (flags & ALLOCATE_QUEUED) ? "true" : "false");
            break;

         case EXTREMES_CHANGED:
            DBG_OBJ_SET_SYM ("flags.EXTREMES_CHANGED",
                             (flags & EXTREMES_CHANGED) ? "true" : "false");
            break;

         case WAS_ALLOCATED:
            DBG_OBJ_SET_SYM ("flags.WAS_ALLOCATED",
                             (flags & WAS_ALLOCATED) ? "true" : "false");
            break;
         }
      }
   }

   inline void setFlags (Flags f)
   { flags = (Flags)(flags | f); printFlag (f); }
   inline void unsetFlags (Flags f)
   { flags = (Flags)(flags & ~f); printFlag (f); }

   inline void queueDraw ()
   { queueDrawArea (0, 0, allocation.width, getHeight()); }
   void queueDrawArea (int x, int y, int width, int height);
   inline void queueResize (int ref, bool extremesChanged)
   { queueResize (ref, extremesChanged, false); }

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void sizeRequestImpl (Requisition *requisition, int numPos,
                                 Widget **references, int *x, int *y);

   /**
    * \brief Simple variant, to be implemented by widgets with sizes
    *    not depending on positions.
    */
   virtual void sizeRequestSimpl (Requisition *requisition);

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void getExtremesImpl (Extremes *extremes, int numPos,
                                 Widget **references, int *x, int *y);

   /**
    * \brief Simple variant, to be implemented by widgets with
    *    extremes not depending on positions.
    */
   virtual void getExtremesSimpl (Extremes *extremes);

   virtual void calcExtraSpaceImpl (int numPos, Widget **references, int *x,
                                    int *y);

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void sizeAllocateImpl (Allocation *allocation);

   /**
    * \brief Called after sizeAllocateImpl() to redraw necessary areas.
    * By default the whole widget is redrawn.
    */
   virtual void resizeDrawImpl () { queueDraw (); };

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void markSizeChange (int ref);

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void markExtremesChange (int ref);

   virtual int getAvailWidthOfChild (Widget *child, bool forceValue);
   virtual int getAvailHeightOfChild (Widget *child, bool forceValue);

   virtual void correctRequisitionOfChild (Widget *child,
                                           Requisition *requisition,
                                           void (*splitHeightFun) (int, int*,
                                                                   int*),
                                           bool allowDecreaseWidth,
                                           bool allowDecreaseHeight);
   void correctRequisitionViewport (Requisition *requisition,
                            void (*splitHeightFun) (int, int*, int*),
                            bool allowDecreaseWidth, bool allowDecreaseHeight);
   void correctReqWidthOfChild (Widget *child, Requisition *requisition,
                                bool allowDecreaseWidth);
   void correctReqHeightOfChild (Widget *child, Requisition *requisition,
                                 void (*splitHeightFun) (int, int*, int*),
                                 bool allowDecreaseHeight);
   bool correctReqAspectRatio (int pass, Widget *child, Requisition *requisition,
                                      bool allowDecreaseWidth, bool allowDecreaseHeight,
                                      void (*splitHeightFun) (int, int*, int*));
   virtual void correctExtremesOfChild (Widget *child, Extremes *extremes,
                                        bool useAdjustmentWidth);

   virtual void containerSizeChangedForChildren ();

   virtual bool affectedByContainerSizeChange ();
   virtual bool affectsSizeChangeContainerChild (Widget *child);
   virtual bool usesAvailWidth ();
   virtual bool usesAvailHeight ();

   virtual void notifySetAsTopLevel();
   virtual void notifySetParent();

   virtual bool buttonPressImpl (EventButton *event);
   virtual bool buttonReleaseImpl (EventButton *event);
   virtual bool motionNotifyImpl (EventMotion *event);
   virtual void enterNotifyImpl (EventCrossing *event);
   virtual void leaveNotifyImpl (EventCrossing *event);

   inline char *addAnchor (const char* name)
   { return layout->addAnchor (this, name); }

   inline char *addAnchor (const char* name, int y)
   { return layout->addAnchor (this, name, y); }

   inline void changeAnchor (char* name, int y)
   { layout->changeAnchor (this, name, y); }

   inline void removeAnchor (char* name)
   { if (layout) layout->removeAnchor (this, name); }

   //inline void updateBgColor () { layout->updateBgColor (); }

   inline void setCursor (style::Cursor cursor)
   { layout->setCursor (cursor);  }
#if 0
   inline bool selectionButtonPress (Iterator *it, int charPos, int linkNo,
                                     EventButton *event, bool withinContent)
   { return layout->selectionState.buttonPress (it, charPos, linkNo, event); }

   inline bool selectionButtonRelease (Iterator *it, int charPos, int linkNo,
                                       EventButton *event, bool withinContent)
   { return layout->selectionState.buttonRelease (it, charPos, linkNo, event);}

   inline bool selectionButtonMotion (Iterator *it, int charPos, int linkNo,
                                      EventMotion *event, bool withinContent)
   { return layout->selectionState.buttonMotion (it, charPos, linkNo, event); }
#endif
   inline bool selectionHandleEvent (SelectionState::EventType eventType,
                                     Iterator *it, int charPos, int linkNo,
                                     MousePositionEvent *event)
   { return layout->selectionState.handleEvent (eventType, it, charPos, linkNo,
                                                event); }

private:
   void *deleteCallbackData;
   DW_Callback_t deleteCallbackFunc;

public:
   inline void setDeleteCallback(DW_Callback_t func, void *data)
   { deleteCallbackFunc = func; deleteCallbackData = data; }

private:
   bool resizeIdleEntered () { return layout && layout->resizeIdleCounter; }

   void enterQueueResize () { if (layout) layout->queueResizeCounter++; }
   void leaveQueueResize () { if (layout) layout->queueResizeCounter--; }
   bool queueResizeEntered () { return layout && layout->queueResizeCounter; }

   void enterSizeAllocate () { if (layout) layout->sizeAllocateCounter++; }
   void leaveSizeAllocate () { if (layout) layout->sizeAllocateCounter--; }
   bool sizeAllocateEntered () { return layout && layout->sizeAllocateCounter; }

   void enterSizeRequest () { if (layout) layout->sizeRequestCounter++; }
   void leaveSizeRequest () { if (layout) layout->sizeRequestCounter--; }
   bool sizeRequestEntered () { return layout && layout->sizeRequestCounter; }

   void enterGetExtremes () { if (layout) layout->getExtremesCounter++; }
   void leaveGetExtremes () { if (layout) layout->getExtremesCounter--; }
   bool getExtremesEntered () { return layout && layout->getExtremesCounter; }


public:
   static int CLASS_ID;

   inline static void setAdjustMinWidth (bool adjustMinWidth)
   { Widget::adjustMinWidth = adjustMinWidth; }

   Widget ();
   ~Widget ();

   inline bool resizeQueued ()    { return flags & RESIZE_QUEUED; }
   inline bool extremesQueued ()  { return flags & EXTREMES_QUEUED; }
   inline bool needsResize ()     { return flags & NEEDS_RESIZE; }
   inline bool needsAllocate ()   { return flags & NEEDS_ALLOCATE; }
   inline bool allocateQueued ()  { return flags & ALLOCATE_QUEUED; }
   inline bool extremesChanged () { return flags & EXTREMES_CHANGED; }
   inline bool wasAllocated ()    { return flags & WAS_ALLOCATED; }

   void setParent (Widget *parent);
   void setQuasiParent (Widget *quasiParent);

   void setGenerator (Widget *generator) { this->generator = generator; }

   inline style::Style *getStyle () { return style; }
   /** \todo I do not like this. */
   inline Allocation *getAllocation () { return &allocation; }
   inline bool inAllocation (int x, int y) {
      return wasAllocated () && x >= allocation.x && y >= allocation.y &&
         x <= allocation.x + allocation.width &&
         y <= allocation.y + getHeight ();
   }

   int marginBoxOffsetX ()
   { return getStyle()->borderWidth.left + getStyle()->padding.left + margin.left; }
   int marginBoxRestWidth ()
   { return getStyle()->borderWidth.right + getStyle()->padding.right + margin.right; }
   int marginBoxDiffWidth ()
   { return marginBoxOffsetX() + marginBoxRestWidth(); }
   // TODO: replace marginTop() with margin.top once it's populated
   int marginBoxOffsetY ()
   { return getStyle()->borderWidth.top + getStyle()->padding.top + getStyle()->marginTop(); }
   // TODO: replace marginBottom() with margin.bottom once it's populated
   int marginBoxRestHeight ()
   { return getStyle()->borderWidth.bottom + getStyle()->padding.bottom + getStyle()->marginBottom(); }
   int marginBoxDiffHeight ()
   { return marginBoxOffsetY() + marginBoxRestHeight(); }

   inline int boxOffsetX ()
   { return extraSpace.left + marginBoxOffsetX(); }
   inline int boxRestWidth ()
   { return extraSpace.right + marginBoxRestWidth(); }
   inline int boxDiffWidth () { return boxOffsetX () + boxRestWidth (); }
   inline int boxOffsetY ()
   { return extraSpace.top + marginBoxOffsetY(); }
   inline int boxRestHeight ()
   { return extraSpace.bottom + marginBoxRestHeight(); }
   inline int boxDiffHeight () { return boxOffsetY () + boxRestHeight (); }

   inline int contentWidth(bool forceValue)
   {
      int width = -1;
      int availWidth = getAvailWidth (forceValue);
      if (availWidth != -1) {
         width = availWidth - boxDiffWidth ();
      }
      return width;
   }

   /**
    * \brief See \ref dw-widget-sizes (or \ref dw-size-request-pos).
    */
   virtual int numSizeRequestReferences ();

   /**
    * \brief See \ref dw-widget-sizes (or \ref dw-size-request-pos).
    */
   virtual Widget *sizeRequestReference (int index);

   /**
    * \brief See \ref dw-widget-sizes (or \ref dw-size-request-pos).
    */
   virtual int numGetExtremesReferences ();

   /**
    * \brief See \ref dw-widget-sizes (or \ref dw-size-request-pos).
    */
   virtual Widget *getExtremesReference (int index);

   void sizeRequest (Requisition *requisition, int numPos = 0,
                     Widget **references = NULL, int *x = NULL, int *y = NULL);
   void getExtremes (Extremes *extremes, int numPos = 0,
                     Widget **references = NULL, int *x = NULL, int *y = NULL);
   void sizeAllocate (Allocation *allocation);

   void calcExtraSpace (int numPos, Widget **references, int *x, int *y);

   int getAvailWidth (bool forceValue);
   int getAvailHeight (bool forceValue);
   virtual bool getAdjustMinWidth () { return Widget::adjustMinWidth; }
   void correctRequisition (Requisition *requisition,
                            void (*splitHeightFun) (int, int*, int*),
                            bool allowDecreaseWidth, bool allowDecreaseHeight);
   void correctExtremes (Extremes *extremes, bool useAdjustmentWidth);
   BoxWidth calcWidth (style::Length cssValue, int refWidth, Widget *refWidget,
                       int limitMinWidth, bool forceValue);
   void calcFinalWidth (style::Style *style, int refWidth, Widget *refWidget,
                        int limitMinWidth, bool forceValue, int *finalWidth);
   int calcHeight (style::Length cssValue, bool usePercentage, int refHeight,
                   Widget *refWidget, bool forceValue);
   static void adjustHeight (int *height, bool allowDecreaseHeight, int ascent,
                             int descent);

   virtual int applyPerWidth (int containerWidth, style::Length perWidth);
   virtual int applyPerHeight (int containerHeight, style::Length perHeight);

   int getMinWidth (Extremes *extremes, bool forceValue);

   virtual bool isBlockLevel ();
   virtual bool isPossibleContainer ();

   void containerSizeChanged ();

   bool intersects (Widget *refWidget, Rectangle *area,
                    Rectangle *intersection);

   /** Area is given in widget coordinates. */
   virtual void draw (View *view, Rectangle *area, DrawingContext *context) = 0;
   void drawInterruption (View *view, Rectangle *area, DrawingContext *context);

   virtual Widget *getWidgetAtPoint (int x, int y,
                                     GettingWidgetAtPointContext *context);
   Widget *getWidgetAtPointInterrupted (int x, int y,
                                        GettingWidgetAtPointContext *context);

   bool buttonPress (EventButton *event);
   bool buttonRelease (EventButton *event);
   bool motionNotify (EventMotion *event);
   void enterNotify (EventCrossing *event);
   void leaveNotify (EventCrossing *event);

   virtual void setStyle (style::Style *style);
   void setBgColor (style::Color *bgColor);
   style::Color *getBgColor ();
   style::Color *getFgColor ();

   void drawBox (View *view, style::Style *style, Rectangle *area,
                 int x, int y, int width, int height, bool inverse);
   void drawWidgetBox (View *view, Rectangle *area, bool inverse);
   void drawSelected (View *view, Rectangle *area);

   void setButtonSensitive (bool buttonSensitive);
   inline bool isButtonSensitive () { return buttonSensitive; }

   inline Widget *getParent () { return parent; }
   inline Widget *getContainer () { return container; }
   Widget *getTopLevel ();
   int getLevel ();
   int getGeneratorLevel ();
   Widget *getNearestCommonAncestor (Widget *otherWidget);

   inline WidgetReference *getWidgetReference () { return widgetReference; }
   inline void setWidgetReference (WidgetReference *widgetReference) {
      this->widgetReference = widgetReference;
      DBG_OBJ_SET_PTR ("widgetReference", widgetReference);
   }
   
   inline Widget *getGenerator () { return generator ? generator : parent; }

   inline Layout *getLayout () { return layout; }

   void scrollTo (HPosition hpos, VPosition vpos,
                  int x, int y, int width, int height);

   void getMarginArea (int *xMar, int *yMar, int *widthMar, int *heightMar);
   void getBorderArea (int *xBor, int *yBor, int *widthBor, int *heightBor);
   void getPaddingArea (int *xPad, int *yPad, int *widthPad, int *heightPad);

   /**
    * \brief Return an iterator for this widget.
    *
    * \em mask can narrow the types returned by the iterator, this can
    * enhance performance quite much, e.g. when only searching for child
    * widgets.
    *
    * With \em atEnd == false, the iterator starts \em before the beginning,
    * i.e. the first call of dw::core::Iterator::next will let the iterator
    * point on the first piece of contents. Likewise, With \em atEnd == true,
    * the iterator starts \em after the last piece of contents, call
    * dw::core::Iterator::prev in this case.
    */
   virtual Iterator *iterator (Content::Type mask, bool atEnd) = 0;

   virtual void removeChild (Widget *child);
};

void splitHeightPreserveAscent (int height, int *ascent, int *descent);
void splitHeightPreserveDescent (int height, int *ascent, int *descent);

} // namespace core
} // namespace dw

#endif // __DW_WIDGET_HH__
