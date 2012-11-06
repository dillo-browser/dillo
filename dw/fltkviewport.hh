#ifndef __DW_FLTKVIEWPORT_HH__
#define __DW_FLTKVIEWPORT_HH__

#include <FL/Fl_Group.H>
#include <FL/Fl_Scrollbar.H>

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
   int hasDragScroll, dragScrolling, dragX, dragY;
   int horScrolling, verScrolling;

   Fl_Scrollbar *vscrollbar, *hscrollbar;

   GadgetOrientation gadgetOrientation[4];
   lout::container::typed::List <lout::object::TypedPointer < Fl_Widget> >
      *gadgets;

   void adjustScrollbarsAndGadgetsAllocation ();
   void adjustScrollbarValues ();
   void hscrollbarChanged ();
   void vscrollbarChanged ();
   void positionChanged ();

   static void hscrollbarCallback (Fl_Widget *hscrollbar, void *viewportPtr);
   static void vscrollbarCallback (Fl_Widget *vscrollbar, void *viewportPtr);

   void selectionScroll();
   static void selectionScroll(void *vport);

   void updateCanvasWidgets (int oldScrollX, int oldScrollY);
   static void draw_area (void *data, int x, int y, int w, int h);

protected:
   int translateViewXToCanvasX (int x);
   int translateViewYToCanvasY (int y);
   int translateCanvasXToViewX (int x);
   int translateCanvasYToViewY (int y);

public:
   FltkViewport (int x, int y, int w, int h, const char *label = 0);
   ~FltkViewport ();

   void resize(int x, int y, int w, int h);
   void draw ();
   int handle (int event);

   void setCanvasSize (int width, int ascent, int descent);

   bool usesViewport ();
   int getHScrollbarThickness ();
   int getVScrollbarThickness ();
   void scroll(int dx, int dy);
   void scroll(dw::core::ScrollCommand cmd);
   void scrollTo (int x, int y);
   void setViewportSize (int width, int height,
                         int hScrollbarThickness, int vScrollbarThickness);
   void setScrollStep(int step);

   void setGadgetOrientation (bool hscrollbarVisible, bool vscrollbarVisible,
                              GadgetOrientation gadgetOrientation);
   void setDragScroll (bool enable) { hasDragScroll = enable ? 1 : 0; }
   void addGadget (Fl_Widget *gadget);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKVIEWPORT_HH__

