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
   int minZIndex, maxZIndex;

   Widget *draw (View *view, Rectangle *area,
                 StackingIteratorStack *iteratorStack, int *zIndexOffset,
                 int startZIndex, int endZIndex, int *index);
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

   Widget *drawBottom (View *view, Rectangle *area,
                       StackingIteratorStack *iteratorStack, int *zIndexOffset,
                       int *index);
   Widget *drawTop (View *view, Rectangle *area,
                    StackingIteratorStack *iteratorStack, int *zIndexOffset,
                    int *index);

   Widget *getTopWidgetAtPoint (int x, int y);
   Widget *getBottomWidgetAtPoint (int x, int y);
};

} // namespace core

} // namespace dw

#endif // __DW_STACKINGCONTEXTMGR_HH__
