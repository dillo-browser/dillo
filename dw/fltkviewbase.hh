#ifndef __DW_FLTKVIEWBASE_HH__
#define __DW_FLTKVIEWBASE_HH__

#include <time.h>         // for time_t
#include <sys/time.h>     // for time_t in FreeBSD

#include <FL/Fl_Group.H>
#include <FL/x.H>

#include "fltkcore.hh"

namespace dw {
namespace fltk {

class FltkViewBase: public FltkView, public Fl_Group
{
private:
   class BackBuffer {
      private:
         int w;
         int h;
         bool created;

      public:
         Fl_Offscreen offscreen;

         BackBuffer ();
         ~BackBuffer ();
         void setSize(int w, int h);
   };

   typedef enum { DRAW_PLAIN, DRAW_CLIPPED, DRAW_BUFFERED } DrawType;

   int bgColor;
   core::Region drawRegion;
   core::Rectangle *exposeArea;
   static BackBuffer *backBuffer;
   static bool backBufferInUse;

   void draw (const core::Rectangle *rect, DrawType type);
   void drawChildWidgets ();
   int manageTabToFocus();
   inline void clipPoint (int *x, int *y, int border) {
      if (exposeArea) {
         if (*x < exposeArea->x - border)
            *x = exposeArea->x - border;
         if (*x > exposeArea->x + exposeArea->width + border)
            *x = exposeArea->x + exposeArea->width + border;
         if (*y < exposeArea->y - border)
            *y = exposeArea->y - border;
         if (*y > exposeArea->y + exposeArea->height + border)
            *y = exposeArea->y + exposeArea->height + border;
      }
   }
protected:
   core::Layout *theLayout;
   int canvasWidth, canvasHeight;
   int mouse_x, mouse_y;
   Fl_Widget *focused_child;

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
   void drawTypedLine (core::style::Color *color,
                       core::style::Color::Shading shading,
                       core::style::LineType type, int width,
                       int x1, int y1, int x2, int y2);
   void drawRectangle (core::style::Color *color,
                       core::style::Color::Shading shading, bool filled,
                       int x, int y, int width, int height);
   void drawArc (core::style::Color *color,
                 core::style::Color::Shading shading, bool filled,
                 int centerX, int centerY, int width, int height,
                 int angle1, int angle2);
    void drawPolygon (core::style::Color *color,
                      core::style::Color::Shading shading,
                      bool filled, bool convex,
                      core::Point *points, int npoints);

   core::View *getClippingView (int x, int y, int width, int height);
   void mergeClippingView (core::View *clippingView);
   void setBufferedDrawing (bool b);
};


class FltkWidgetView: public FltkViewBase
{
public:
   FltkWidgetView (int x, int y, int w, int h, const char *label = 0);
   ~FltkWidgetView ();

   void drawText (core::style::Font *font,
                  core::style::Color *color,
                  core::style::Color::Shading shading,
                  int x, int y, const char *text, int len);
   void drawSimpleWrappedText (core::style::Font *font,
                               core::style::Color *color,
                               core::style::Color::Shading shading,
                               int x, int y, int w, int h,
                               const char *text);
   void drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                   int x, int y, int width, int height);

   bool usesFltkWidgets ();
   void addFltkWidget (Fl_Widget *widget, core::Allocation *allocation);
   void removeFltkWidget (Fl_Widget *widget);
   void allocateFltkWidget (Fl_Widget *widget,
                            core::Allocation *allocation);
   void drawFltkWidget (Fl_Widget *widget, core::Rectangle *area);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKVIEWBASE_HH__

