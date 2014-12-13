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
   void draw (View *view, Rectangle *area,
              StackingIteratorStack *iteratorStack, Widget **interruptedWidget,
              int *zIndexIndex, int startZIndex, int endZIndex, int *index);
   Widget *getWidgetAtPoint (int x, int y, StackingIteratorStack *iteratorStack,
                             Widget **interruptedWidget, int *zIndexIndex,
                             int startZIndex, int endZIndex, int *index);

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

   void drawBottom (View *view, Rectangle *area,
                    StackingIteratorStack *iteratorStack,
                    Widget **interruptedWidget, int *zIndexIndex, int *index);
   void drawTop (View *view, Rectangle *area,
                 StackingIteratorStack *iteratorStack,
                 Widget **interruptedWidget, int *zIndexIndex, int *index);

   Widget *getTopWidgetAtPoint (int x, int y,
                                core::StackingIteratorStack *iteratorStack,
                                Widget **interruptedWidget,
                                int *zIndexIndex, int *index);
   Widget *getBottomWidgetAtPoint (int x, int y,
                                   core::StackingIteratorStack *iteratorStack,
                                   Widget **interruptedWidget,
                                   int *zIndexIndex, int *index);
};

} // namespace core

} // namespace dw

#endif // __DW_STACKINGCONTEXTMGR_HH__
