#ifndef __DW_FLTKFLATVIEW_HH__
#define __DW_FLTKFLATVIEW_HH__

#include "core.hh"
#include "fltkcore.hh"
#include "fltkviewbase.hh"

namespace dw {
namespace fltk {

class FltkFlatView: public FltkWidgetView
{
protected:
   int translateViewXToCanvasX (int x);
   int translateViewYToCanvasY (int y);
   int translateCanvasXToViewX (int x);
   int translateCanvasYToViewY (int y);

public:
   FltkFlatView (int x, int y, int w, int h, const char *label = 0);
   ~FltkFlatView ();

   void setCanvasSize (int width, int ascent, int descent);

   bool usesViewport ();
   int getHScrollbarThickness ();
   int getVScrollbarThickness ();
   void scrollTo (int x, int y);
   void setViewportSize (int width, int height,
                         int hScrollbarThickness, int vScrollbarThickness);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKFLATVIEW_HH__

