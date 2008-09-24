#ifndef __DW_FLTKVIEWPORT_HH__
#define __DW_FLTKVIEWPORT_HH__

#include <fltk/Group.h>
#include <fltk/Scrollbar.h>

#include "core.hh"
#include "fltkcore.hh"
#include "fltkviewbase.hh"

namespace dw {
namespace fltk {

class FltkViewport: public FltkWidgetView
{
public:
   enum GadgetOrientation { GADGET_VERTICAL, GADGET_HORIZONTAL };

private:
   enum { SCROLLBAR_THICKNESS = 15 };

   int scrollX, scrollY;
   int scrollDX, scrollDY;
   int dragScrolling, dragX, dragY;

   ::fltk::Scrollbar *vscrollbar, *hscrollbar;

   GadgetOrientation gadgetOrientation[4];
   container::typed::List <object::TypedPointer < ::fltk::Widget> > *gadgets; 

   void adjustScrollbarsAndGadgetsAllocation ();
   void adjustScrollbarValues ();
   void hscrollbarChanged ();
   void vscrollbarChanged ();
   void positionChanged ();

   static void hscrollbarCallback (Widget *hscrollbar, void *viewportPtr);
   static void vscrollbarCallback (Widget *vscrollbar, void *viewportPtr);

   void updateCanvasWidgets (int oldScrollX, int oldScrollY);
   static void draw_area (void *data, const Rectangle& cr);

protected:
   int translateViewXToCanvasX (int x);
   int translateViewYToCanvasY (int y);
   int translateCanvasXToViewX (int x);
   int translateCanvasYToViewY (int y);

public:
   FltkViewport (int x, int y, int w, int h, const char *label = 0);
   ~FltkViewport ();
 
   void layout();
   void draw ();
   int handle (int event);

   void setCanvasSize (int width, int ascent, int descent);

   bool usesViewport ();
   int getHScrollbarThickness ();
   int getVScrollbarThickness ();
   void scroll(int dx, int dy);
   void scrollTo (int x, int y);
   void setViewportSize (int width, int height,
                         int hScrollbarThickness, int vScrollbarThickness);
   void setScrollStep(int step);

   void setGadgetOrientation (bool hscrollbarVisible, bool vscrollbarVisible,
                              GadgetOrientation gadgetOrientation);
   void addGadget (::fltk::Widget *gadget);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKVIEWPORT_HH__

