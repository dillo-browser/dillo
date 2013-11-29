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
       * \brief Set, when dw::core::Widget::requisition is not up to date
       *    anymore.
       */
      NEEDS_RESIZE     = 1 << 0,

      /**
       * \brief Only used internally, set to enforce size allocation.
       *
       * (I've forgotten the case, for which this is necessary.)
       */
      NEEDS_ALLOCATE   = 1 << 1,

      /**
       * \brief Set, when dw::core::Widget::extremes is not up to date
       *    anymore.
       */
      EXTREMES_CHANGED = 1 << 2,

      /**
       * \brief Set by the widget itself (in the constructor), when set...
       *    methods are implemented.
       *
       * Will hopefully be removed, after redesigning the size model.
       */
      USES_HINTS       = 1 << 3,

      /**
       * \brief Set by the widget itself (in the constructor), when it contains
       *    some contents, e.g. an image, as opposed to a horizontal ruler.
       *
       * Will hopefully be removed, after redesigning the size model.
       */
      HAS_CONTENTS     = 1 << 4,

      /**
       * \brief Set, when a widget was already once allocated,
       *
       * The dw::Image widget uses this flag, see dw::Image::setBuffer.
       */
      WAS_ALLOCATED    = 1 << 5,

      /**
       * \brief Set for block-level widgets (as opposed to inline widgets)
       */
      BLOCK_LEVEL      = 1 << 6,
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

   inline void setFlags (Flags f)   { flags = (Flags)(flags | f); }
   inline void unsetFlags (Flags f) { flags = (Flags)(flags & ~f); }


   inline void queueDraw ()
   {
      queueDrawArea (0, 0, allocation.width, getHeight());
   }
   void queueDrawArea (int x, int y, int width, int height);
   void queueResize (int ref, bool extremesChanged);

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void sizeRequestImpl (Requisition *requisition) = 0;

   /**
    * \brief See \ref dw-widget-sizes.
    */
   virtual void getExtremesImpl (Extremes *extremes);

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
   { layout->removeAnchor (this, name); }

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

public:
   static int CLASS_ID;

   Widget ();
   ~Widget ();

   inline bool needsResize ()     { return flags & NEEDS_RESIZE; }
   inline bool needsAllocate ()   { return flags & NEEDS_ALLOCATE; }
   inline bool extremesChanged () { return flags & EXTREMES_CHANGED; }
   inline bool wasAllocated ()    { return flags & WAS_ALLOCATED; }
   inline bool usesHints ()       { return flags & USES_HINTS; }
   inline bool hasContents ()     { return flags & HAS_CONTENTS; }
   inline bool blockLevel ()      { return flags & BLOCK_LEVEL; }

   void setParent (Widget *parent);

   inline style::Style *getStyle () { return style; }
   /** \todo I do not like this. */
   inline Allocation *getAllocation () { return &allocation; }

   void sizeRequest (Requisition *requisition);
   void getExtremes (Extremes *extremes);
   void sizeAllocate (Allocation *allocation);
   virtual void setWidth (int width);
   virtual void setAscent (int ascent);
   virtual void setDescent (int descent);

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
   Widget *getNearestCommonAncestor (Widget *otherWidget);

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

} // namespace core
} // namespace dw

#endif // __DW_WIDGET_HH__
