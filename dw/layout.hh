#ifndef __DW_LAYOUT_HH__
#define __DW_LAYOUT_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief The central class for managing and drawing a widget tree.
 *
 * \sa\ref dw-overview, \ref dw-layout-widgets, \ref dw-layout-views
 */
class Layout: public object::Object
{
   friend class Widget;

public:
   /**
    * \brief Receiver interface different signals.
    *
    * May be extended
    */
   class Receiver: public lout::signal::Receiver
   {
   public:
      virtual void canvasSizeChanged (int width, int ascent, int descent);
   };

private:
   class Emitter: public lout::signal::Emitter
   {
   private:
      enum { CANVAS_SIZE_CHANGED };

   protected:
      bool emitToReceiver (lout::signal::Receiver *receiver, int signalNo,
                           int argc, Object **argv);

   public:
      inline void connectLayout (Receiver *receiver) { connect (receiver); }

      void emitCanvasSizeChanged (int width, int ascent, int descent);
   };

   Emitter emitter;

   class Anchor: public object::Object
   {
   public:
      char *name;
      Widget *widget;
      int y;

      ~Anchor ();
   };

   Platform *platform;
   container::typed::List <View> *views;
   Widget *topLevel, *widgetAtPoint;

   /* The state, which must be projected into the views. */
   style::Color *bgColor;
   style::Cursor cursor;
   int canvasWidth, canvasAscent, canvasDescent;

   bool usesViewport;
   int scrollX, scrollY, viewportWidth, viewportHeight;
   bool canvasHeightGreater;
   int hScrollbarThickness, vScrollbarThickness;
   
   HPosition scrollTargetHpos;
   VPosition scrollTargetVpos;
   int scrollTargetX, scrollTargetY, scrollTargetWidth, scrollTargetHeight;

   char *requestedAnchor;
   int scrollIdleId, resizeIdleId;
   bool scrollIdleNotInterrupted;

   /* Anchors of the widget tree */
   container::typed::HashTable <object::String, Anchor> *anchorsTable;

   SelectionState selectionState;
   FindtextState findtextState;

   enum ButtonEventType { BUTTON_PRESS, BUTTON_RELEASE, MOTION_NOTIFY };

   Widget *getWidgetAtPoint (int x, int y);
   void moveToWidget (Widget *newWidgetAtPoint, ButtonState state);

   /**
    * \brief Emit the necessary crossing events, when the mouse pointer has
    *    moved to position (\em x, \em );
    */
   void moveToWidgetAtPoint (int x, int y, ButtonState state)
   { moveToWidget (getWidgetAtPoint (x, y), state); }

   /**
    * \brief Emit the necessary crossing events, when the mouse pointer
    * has moved out of the view.
    */
   void moveOutOfView (ButtonState state) { moveToWidget (NULL, state); }

   bool processMouseEvent (MousePositionEvent *event, ButtonEventType type,
                           bool mayBeSuppressed);
   bool buttonEvent (ButtonEventType type, View *view,
                     int numPressed, int x, int y, ButtonState state,
                     int button);
   void resizeIdle ();
   void setSizeHints ();
   void draw (View *view, Rectangle *area);

   void scrollTo0(HPosition hpos, VPosition vpos,
                  int x, int y, int width, int height,
                  bool scrollingInterrupted);
   void scrollIdle ();
   void adjustScrollPos ();
   static bool calcScrollInto (int targetValue, int requestedSize,
                               int *value, int viewportSize);

   void updateAnchor ();

   /* Widget */

   char *addAnchor (Widget *widget, const char* name);
   char *addAnchor (Widget *widget, const char* name, int y);
   void changeAnchor (Widget *widget, char* name, int y);
   void removeAnchor (Widget *widget, char* name);
   void setCursor (style::Cursor cursor);
   void updateCursor ();
   void updateBgColor ();
   void queueDraw (int x, int y, int width, int height);
   void queueDrawExcept (int x, int y, int width, int height,
      int ex, int ey, int ewidth, int eheight);
   void queueResize ();
   void removeWidget ();
  
public:
   Layout (Platform *platform);
   ~Layout ();

   misc::ZoneAllocator *textZone;
   
   void addWidget (Widget *widget);
   void setWidget (Widget *widget);

   void attachView (View *view);
   void detachView (View *view);

   inline bool getUsesViewport () { return usesViewport; }
   inline int getWidthViewport () { return viewportWidth; }
   inline int getHeightViewport ()  { return viewportHeight; }
   inline int getScrollPosX ()  { return scrollX; }
   inline int getScrollPosY ()  { return scrollY; }

   /* public */

   void scrollTo (HPosition hpos, VPosition vpos,
                  int x, int y, int width, int height);
   void setAnchor (const char *anchor);

   /* View */

   inline void expose (View *view, Rectangle *area) { draw (view, area); }

   /**
    * \brief This function is called by a view, to delegate a button press
    * event.
    * 
    * \em numPressed is 1 for simple presses, 2 for double presses etc. (more
    * that 2 is never needed), \em x and \em y the world coordinates, and
    * \em button the number of the button pressed.
    */
   inline bool buttonPress (View *view, int numPressed, int x, int y,
                            ButtonState state, int button)
   {
      return buttonEvent (BUTTON_PRESS, view, numPressed, x, y, state, button);
   }

   /**
    * \brief This function is called by a view, to delegate a button press
    * event.
    *
    * Arguments are similar to dw::core::Layout::buttonPress.
    */
   inline bool buttonRelease (View *view, int numPressed, int x, int y,
                              ButtonState state, int button)
   {
      return buttonEvent (BUTTON_RELEASE, view, numPressed, x, y, state,
                          button);
   }

   bool motionNotify (View *view,  int x, int y, ButtonState state);
   void enterNotify (View *view, int x, int y, ButtonState state);
   void leaveNotify (View *view, ButtonState state);

   void scrollPosChanged (View *view, int x, int y);
   void viewportSizeChanged (View *view, int width, int height);

   /* delegated */

   inline int textWidth (style::Font *font, const char *text, int len)
   {
      return platform->textWidth (font, text, len);
   }

   inline int nextGlyph (const char *text, int idx)
   {
      return platform->nextGlyph (text, idx);
   }

   inline int prevGlyph (const char *text, int idx)
   {
      return platform->prevGlyph (text, idx);
   }

   inline style::Font *createFont (style::FontAttrs *attrs, bool tryEverything)
   {
      return  platform->createFont (attrs, tryEverything);
   }

   inline style::Color *createSimpleColor (int color)
   {
      return platform->createSimpleColor (color);
   }

   inline style::Color *createShadedColor (int color)
   {
      return platform->createShadedColor (color);
   }

   inline Imgbuf *createImgbuf (Imgbuf::Type type, int width, int height)
   {
      return platform->createImgbuf (type, width, height);
   }

   inline void copySelection(const char *text)
   {
      platform->copySelection(text);
   }

   inline ui::ResourceFactory *getResourceFactory ()
   {
      return platform->getResourceFactory ();
   }

   inline void connect (Receiver *receiver) {
      emitter.connectLayout (receiver); }

   /** \brief See dw::core::FindtextState::search. */
   inline FindtextState::Result search (const char *str, bool caseSens)
      { return findtextState.search (str, caseSens); }

   /** \brief See dw::core::FindtextState::resetSearch. */
   inline void resetSearch () { findtextState.resetSearch (); }
};

} // namespace dw
} // namespace core

#endif // __DW_LAYOUT_HH__

