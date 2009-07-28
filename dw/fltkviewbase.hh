#ifndef __DW_FLTKVIEWBASE_HH__
#define __DW_FLTKVIEWBASE_HH__

#include <time.h>         // for time_t
#include <sys/time.h>     // for time_t in FreeBSD

#include <fltk/Group.h>
#include <fltk/Image.h>
#include <fltk/Scrollbar.h>

#include "fltkcore.hh"

namespace dw {
namespace fltk {

class FltkViewBase: public FltkView, public ::fltk::Group
{
private:
   typedef enum { DRAW_PLAIN, DRAW_CLIPPED, DRAW_BUFFERED } DrawType;

   int bgColor;
   core::Region drawRegion;
   static ::fltk::Image *backBuffer;
   static bool backBufferInUse;

   void draw (const core::Rectangle *rect, DrawType type);
   void drawChildWidgets ();

protected:
   core::Layout *theLayout;
   int canvasWidth, canvasHeight;
   int mouse_x, mouse_y;

   virtual int translateViewXToCanvasX (int x) = 0;
   virtual int translateViewYToCanvasY (int y) = 0;
   virtual int translateCanvasXToViewX (int x) = 0;
   virtual int translateCanvasYToViewY (int y) = 0;

public:
   FltkViewBase (int x, int y, int w, int h, const char *label = 0);
   ~FltkViewBase ();

   void draw();
   int handle (int event);

   void setLayout (core::Layout *layout);
   void setCanvasSize (int width, int ascent, int descent);
   void setCursor (core::style::Cursor cursor);
   void setBgColor (core::style::Color *color);

   void startDrawing (core::Rectangle *area);
   void finishDrawing (core::Rectangle *area);
   void queueDraw (core::Rectangle *area);
   void queueDrawTotal ();
   void cancelQueueDraw ();
   void drawPoint (core::style::Color *color,
                   core::style::Color::Shading shading,
                   int x, int y);
   void drawLine (core::style::Color *color,
                  core::style::Color::Shading shading,
                  int x1, int y1, int x2, int y2);
   void drawRectangle (core::style::Color *color,
                       core::style::Color::Shading shading, bool filled,
                       int x, int y, int width, int height);
   void drawArc (core::style::Color *color,
                 core::style::Color::Shading shading, bool filled,
                 int x, int y, int width, int height,
                 int angle1, int angle2);
    void drawPolygon (core::style::Color *color,
                      core::style::Color::Shading shading,
                      bool filled, int points[][2], int npoints);

   core::View *getClippingView (int x, int y, int width, int height);
   void mergeClippingView (core::View *clippingView);
   void setBufferedDrawing (bool b);
};


class FltkWidgetView: public FltkViewBase
{
public:
   FltkWidgetView (int x, int y, int w, int h, const char *label = 0);
   ~FltkWidgetView ();

   void layout();

   void drawText (core::style::Font *font,
                  core::style::Color *color,
                  core::style::Color::Shading shading,
                  int x, int y, const char *text, int len);
   void drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                   int x, int y, int width, int height);

   bool usesFltkWidgets ();
   void addFltkWidget (::fltk::Widget *widget, core::Allocation *allocation);
   void removeFltkWidget (::fltk::Widget *widget);
   void allocateFltkWidget (::fltk::Widget *widget,
                            core::Allocation *allocation);
   void drawFltkWidget (::fltk::Widget *widget, core::Rectangle *area);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKVIEWBASE_HH__

