#ifndef __DW_WIDGET_HH__
#define __DW_WIDGET_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include "../lout/identity.hh"

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
      WAS_ALLOCATED    = 1 << 6,
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
   /**
    * \brief The parent widget, NULL for top-level widgets.
    */
   Widget *parent;

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

   style::Style *style;

   Flags flags;

   /**
    * \brief Size_request() stores the result of the last call of
    *    size_request_impl().
    *
    * Do not read this directly, but call size_request().
    */
   Requisition requisition;

   /**
    * \brief Analogue to dw::core::Widget::requisition.
    */
   Extremes extremes;

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
   void actualQueueResize (int ref, bool extremesChanged, bool fast);

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
    *    canvas.
    */
   Allocation allocation;

   inline int getHeight () { return allocation.ascent + allocation.descent; }
   inline int getContentWidth() { return allocation.width
                                     - style->boxDiffWidth (); }
   inline int getContentHeight() { return getHeight ()
                                      - style->boxDiffHeight (); }

   Layout *layout;

   /**
    * \brief Space around the margin box. Allocation is extraSpace +
    *    margin + border + padding + contents;
    */
   style::Box extraSpace;

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
   virtual void sizeRequestImpl (Requisition *requisition) = 0;

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void getExtremesImpl (Extremes *extremes) = 0;

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
                                                                   int*));
   void correctReqWidthOfChild (Widget *child, Requisition *requisition);
   void correctReqHeightOfChild (Widget *child, Requisition *requisition,
                                 void (*splitHeightFun) (int, int*, int*));
   virtual void correctExtremesOfChild (Widget *child, Extremes *extremes);

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

   void setGenerator (Widget *generator) { this->generator = generator; }

   inline style::Style *getStyle () { return style; }
   /** \todo I do not like this. */
   inline Allocation *getAllocation () { return &allocation; }

   inline int boxOffsetX ()
   { return extraSpace.left + getStyle()->boxOffsetX (); }
   inline int boxRestWidth ()
   { return extraSpace.right + getStyle()->boxRestWidth (); }
   inline int boxDiffWidth () { return boxOffsetX () + boxRestWidth (); }
   inline int boxOffsetY ()
   { return extraSpace.top + getStyle()->boxOffsetY (); }
   inline int boxRestHeight ()
   { return extraSpace.bottom + getStyle()->boxRestHeight (); }
   inline int boxDiffHeight () { return boxOffsetY () + boxRestHeight (); }

   void sizeRequest (Requisition *requisition);
   void getExtremes (Extremes *extremes);
   void sizeAllocate (Allocation *allocation);

   int getAvailWidth (bool forceValue);
   int getAvailHeight (bool forceValue);
   void correctRequisition (Requisition *requisition,
                            void (*splitHeightFun) (int, int*, int*));
   void correctExtremes (Extremes *extremes);

   virtual int applyPerWidth (int containerWidth, style::Length perWidth);
   virtual int applyPerHeight (int containerHeight, style::Length perHeight);
   
   virtual bool isBlockLevel ();
   virtual bool isPossibleContainer ();

   void containerSizeChanged ();

   bool intersects (Rectangle *area, Rectangle *intersection);

   /** Area is given in widget coordinates. */
   virtual void draw (View *view, Rectangle *area) = 0;

   bool buttonPress (EventButton *event);
   bool buttonRelease (EventButton *event);
   bool motionNotify (EventMotion *event);
   void enterNotify (EventCrossing *event);
   void leaveNotify (EventCrossing *event);

   virtual void setStyle (style::Style *style);
   void setBgColor (style::Color *bgColor);
   style::Color *getBgColor ();

   void drawBox (View *view, style::Style *style, Rectangle *area,
                 int x, int y, int width, int height, bool inverse);
   void drawWidgetBox (View *view, Rectangle *area, bool inverse);
   void drawSelected (View *view, Rectangle *area);

   void setButtonSensitive (bool buttonSensitive);
   inline bool isButtonSensitive () { return buttonSensitive; }

   inline Widget *getParent () { return parent; }
   Widget *getTopLevel ();
   int getLevel ();
   int getGeneratorLevel ();
   Widget *getNearestCommonAncestor (Widget *otherWidget);

   inline Widget *getGenerator () { return generator ? generator : parent; }

   inline Layout *getLayout () { return layout; }

   virtual Widget *getWidgetAtPoint (int x, int y, int level);

   void scrollTo (HPosition hpos, VPosition vpos,
                  int x, int y, int width, int height);

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
