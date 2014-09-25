#ifndef __DW_STACKINGCONTEXTMGR_HH__
#define __DW_STACKINGCONTEXTMGR_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include "../lout/container.hh"

#include <limits.h>

namespace dw {

namespace core {

class OOFAwareWidget;

/**
 * \brief See \ref dw-stacking-context.
 */
class StackingContextMgr
{
private:
   lout::container::typed::Vector<Widget> *scWidgets;
   int minZIndex, maxZIndex;

   void draw (View *view, Rectangle *area, int startZIndex, int endZIndex);

   Widget *getWidgetAtPoint (int x, int y, int startZIndex, int endZIndex);
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

   void drawBottom (View *view, Rectangle *area);
   void drawTop (View *view, Rectangle *area);

   Widget *getTopWidgetAtPoint (int x, int y);
   Widget *getBottomWidgetAtPoint (int x, int y);
};

} // namespace core

} // namespace dw

#endif // __DW_STACKINGCONTEXTMGR_HH__
