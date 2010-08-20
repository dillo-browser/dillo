#ifndef __FlTKPREVIEW_HH__
#define __FlTKPREVIEW_HH__

#include <fltk/Button.h>
#include <fltk/MenuWindow.h>
#include "fltkviewbase.hh"

namespace dw {
namespace fltk {

class FltkPreview: public FltkViewBase
{
   friend class FltkPreviewWindow;

private:
   int scrollX, scrollY, scrollWidth, scrollHeight;

protected:
   int translateViewXToCanvasX (int x);
   int translateViewYToCanvasY (int y);
   int translateCanvasXToViewX (int x);
   int translateCanvasYToViewY (int y);

public:
   FltkPreview (int x, int y, int w, int h, dw::core::Layout *layout,
                const char *label = 0);
   ~FltkPreview ();

   int handle (int event);

   void setCanvasSize (int width, int ascent, int descent);

   bool usesViewport ();
   int getHScrollbarThickness ();
   int getVScrollbarThickness ();
   void scrollTo (int x, int y);
   void scroll (dw::core::ScrollCommand cmd);
   void setViewportSize (int width, int height,
                         int hScrollbarThickness, int vScrollbarThickness);

   void drawText (core::style::Font *font,
                  core::style::Color *color,
                  core::style::Color::Shading shading,
                  int x, int y, const char *text, int len);
   void drawImage (core::Imgbuf *imgbuf, int xRoot, int yRoot,
                   int x, int y, int width, int height);

   bool usesFltkWidgets ();
   void drawFltkWidget (::fltk::Widget *widget, core::Rectangle *area);
};


class FltkPreviewWindow: public ::fltk::MenuWindow
{
private:
   enum { BORDER_WIDTH = 2 };

   FltkPreview *preview;
   int posX, posY;

public:
   FltkPreviewWindow (dw::core::Layout *layout);
   ~FltkPreviewWindow ();

   void reallocate ();

   void showWindow ();
   void hideWindow ();

   void scrollTo (int mouseX, int mouseY);
};


class FltkPreviewButton: public  ::fltk::Button
{
private:
   FltkPreviewWindow *window;

public:
   FltkPreviewButton (int x, int y, int w, int h,
                      dw::core::Layout *layout, const char *label = 0);
   ~FltkPreviewButton ();

   int handle (int event);
};

} // namespace fltk
} // namespace dw

#endif // __FlTKPREVIEW_HH__
