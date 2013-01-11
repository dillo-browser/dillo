#ifndef __DW_VIEW_HH__
#define __DW_VIEW_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief An interface to encapsulate platform dependent drawing.
 *
 * \sa\ref dw-overview, \ref dw-layout-views
 */
class View: public lout::object::Object
{
public:
   /*
    * ----------------------------
    *    Operations on the view
    * ----------------------------
    */

   /**
    * \brief This methods notifies the view, that it has been attached to a
    *    layout.
    */
   virtual void setLayout (Layout *layout) = 0;

   /**
    * \brief Set the canvas size.
    */
   virtual void setCanvasSize (int width, int ascent, int descent) = 0;

   /**
    * \brief Set the cursor appearance.
    */
   virtual void setCursor (style::Cursor cursor) = 0;

   /**
    * \brief Set the background of the view.
    */
   virtual void setBgColor (style::Color *color) = 0;

   /*
    * ---------------------------------------------------------
    *    Scrolling and Related. Only usesViewport must be
    *    implemented, if it returns false, the other methods
    *    are never called.
    * ---------------­-----------­-----------------------------
    */

   /**
    * \brief Return, whether this view uses a viewport.
    */
   virtual bool usesViewport () = 0;

   /**
    * \brief Get the thickness of the horizontal scrollbar, when it is
    *    visible.
    *
    * Does not have to be implemented, when usesViewport returns false.
    */
   virtual int getHScrollbarThickness () = 0;

   /**
    * \brief Get the thickness of the vertical scrollbar, when it is
    *    visible.
    *
    * Does not have to be implemented, when usesViewport returns false.
    */
   virtual int getVScrollbarThickness () = 0;

   /**
    * \brief Scroll the vieport to the given position.
    *
    * Does not have to be implemented, when usesViewport returns false.
    */
   virtual void scrollTo (int x, int y) = 0;

   /**
    * \brief Scroll the viewport as commanded.
    */
   virtual void scroll (ScrollCommand) { };

   /**
    * \brief Set the viewport size.
    *
    * Does not have to be implemented, when usesViewport returns false.
    *
    * This will normally imply a resize of the UI widget. Width and height are
    * the dimensions of the new size, \em including the scrollbar thicknesses.
    *
    */
   virtual void setViewportSize (int width, int height,
                                 int hScrollbarThickness,
                                 int vScrollbarThickness) = 0;

   /*
    * -----------------------
    *    Drawing functions
    * -----------------------
    */

   /**
    * \brief Called before drawing.
    *
    * All actual drawing operations will be enclosed into calls of
    * dw::core:View::startDrawing and dw::core:View::finishDrawing. They
    * may be implemented, e.g. when a backing
    * pixmap is used, to prevent flickering. StartDrawing() will then
    * initialize the backing pixmap, all other drawing operations will draw
    * into it, and finishDrawing() will merge it into the window.
    */
   virtual void startDrawing (Rectangle *area) = 0;

   /**
    * \brief Called after drawing.
    *
    * \sa dw::core:View::startDrawing
    */
   virtual void finishDrawing (Rectangle *area) = 0;

   /**
    * \brief Queue a region, which is given in \em canvas coordinates, for
    *    drawing.
    *
    * The view implementation is responsible, that this region is drawn, either
    * immediately, or (which is more typical, since more efficient) the areas
    * are collected, combined (as far as possible), and the drawing is later
    * done in an idle function.
    */
   virtual void queueDraw (Rectangle *area) = 0;

   /**
    * \brief Queue the total viewport for drawing.
    *
    * \sa dw::core::View::queueDraw
    */
   virtual void queueDrawTotal () = 0;

   /**
    * \brief Cancel a draw queue request.
    *
    * If dw::core::View::queueDraw or dw::core::View::queueDrawTotal have been
    * called before, and the actual drawing was not processed yet, the actual
    * drawing should be cancelled. Otherwise, the cancellation should be
    * ignored.
    */
   virtual void cancelQueueDraw () = 0;

   /*
    * The following methods should be self-explaining.
    */

   virtual void drawPoint     (style::Color *color,
                               style::Color::Shading shading,
                               int x, int y) = 0;
   virtual void drawLine      (style::Color *color,
                               style::Color::Shading shading,
                               int x1, int y1, int x2, int y2) = 0;
   virtual void drawTypedLine (style::Color *color,
                               style::Color::Shading shading,
                               style::LineType type, int width,
                               int x1, int y1, int x2, int y2) = 0;
   virtual void drawRectangle (style::Color *color,
                               style::Color::Shading shading, bool filled,
                               int x, int y, int width, int height) = 0;
   virtual void drawArc       (style::Color *color,
                               style::Color::Shading shading, bool filled,
                               int centerX, int centerY, int width, int height,
                               int angle1, int angle2) = 0;
   virtual void drawPolygon    (style::Color *color,
                                style::Color::Shading shading,
                                bool filled, bool convex, Point *points,
                                int npoints) = 0;
   virtual void drawText       (style::Font *font,
                                style::Color *color,
                                style::Color::Shading shading,
                                int x, int y, const char *text, int len) = 0;
   virtual void drawSimpleWrappedText (style::Font *font, style::Color *color,
                                       style::Color::Shading shading,
                                       int x, int y, int w, int h,
                                       const char *text) = 0;
   virtual void drawImage (Imgbuf *imgbuf, int xRoot, int yRoot,
                           int x, int y, int width, int height) = 0;

   /*
    * --------------
    *    Clipping
    * --------------
    */

   /*
    * To prevent drawing outside of a given area, a clipping view may be
    * requested, which also implements this interface. The clipping view is
    * related to the parent view (clipping views may be nested!), anything
    * which is drawn into this clipping view, is later merged again into the
    * parent view. An implementation will typically use additional pixmaps,
    * which are later merged into the parent view pixmap/window.
    */

   virtual View *getClippingView (int x, int y, int width, int height) = 0;
   virtual void mergeClippingView (View *clippingView) = 0;
};

} // namespace core
} // namespace dw

#endif // __DW_VIEW_HH__
