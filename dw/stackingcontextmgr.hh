#ifndef __DW_STACKINGCONTEXTMGR_HH__
#define __DW_STACKINGCONTEXTMGR_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include "../lout/container.hh"

#include <limits.h>

namespace dw {

namespace core {

/**
 * \brief See \ref dw-stacking-context.
 */
class StackingContextMgr
{
private:
   Widget *widget;
   lout::container::typed::Vector<Widget> *childSCWidgets;
   int *zIndices, numZIndices;

   int findZIndex (int zIndex, bool mustExist);
   void draw (View *view, Rectangle *area, int startZIndex, int endZIndex,
              DrawingContext *context);
   Widget *getWidgetAtPoint (int x, int y,
                             core::GettingWidgetAtPointContext *context,
                             int startZIndex, int endZIndex);

public:
   StackingContextMgr (Widget *widget);
   ~StackingContextMgr ();

   inline static bool isEstablishingStackingContext (Widget *widget) {
      return widget->getStyle()->position != style::POSITION_STATIC &&
         widget->getStyle()->zIndex != style::Z_INDEX_AUTO;
   }

   inline static bool handledByStackingContextMgr  (Widget *widget) {
      // Each widget establishing a stacking context is child of another
      // stacking context, so drawn by StackingContextMgr::drawTop or
      // StackingContextMgr::drawBottom etc.
      return widget->getParent () != NULL
         && isEstablishingStackingContext (widget);
   }

   void addChildSCWidget (Widget *widget);

   inline int getNumZIndices () { return numZIndices; }
   inline int getNumChildSCWidgets () { return childSCWidgets->size (); }

   inline void drawBottom (View *view, Rectangle *area, DrawingContext *context)
   { draw (view, area, INT_MIN, -1, context); }
   void drawTop (View *view, Rectangle *area, DrawingContext *context)
   { draw (view, area, 0, INT_MAX, context); }

   inline Widget *getTopWidgetAtPoint (int x, int y,
                                       core::GettingWidgetAtPointContext
                                       *context)
   { return getWidgetAtPoint (x, y, context, 0, INT_MAX); }

   inline Widget *getBottomWidgetAtPoint (int x, int y,
                                          core::GettingWidgetAtPointContext
                                          *context)
   { return getWidgetAtPoint (x, y, context, INT_MIN, -1); }
};

} // namespace core

} // namespace dw

#endif // __DW_STACKINGCONTEXTMGR_HH__
