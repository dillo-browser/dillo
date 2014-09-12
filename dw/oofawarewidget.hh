#ifndef __DW_OOFAWAREWIDGET_HH__
#define __DW_OOFAWAREWIDGET_HH__

#include "core.hh"
#include "outofflowmgr.hh"

namespace dw {

namespace oof {

/**
 * \brief Base class for widgets which can act as container and
 *     generator for widgets out of flow.
 *
 * (Perhaps it should be diffenciated between the two roles, container
 * and generator, but this would make multiple inheritance necessary.)
 */
class OOFAwareWidget: public core::Widget
{
protected:
   enum { OOFM_FLOATS, OOFM_ABSOLUTE, OOFM_FIXED, NUM_OOFM };
   enum { PARENT_REF_OOFM_BITS = 2,
          PARENT_REF_OOFM_MASK = (1 << PARENT_REF_OOFM_BITS) - 1 };

   OOFAwareWidget *oofContainer[NUM_OOFM];
   OutOfFlowMgr *outOfFlowMgr[NUM_OOFM];

   inline OutOfFlowMgr *searchOutOfFlowMgr (int oofmIndex)
   { return oofContainer[oofmIndex] ?
         oofContainer[oofmIndex]->outOfFlowMgr[oofmIndex] : NULL; }

   static inline bool testWidgetFloat (Widget *widget)
   { return widget->getStyle()->vloat != core::style::FLOAT_NONE; }
   static inline bool testWidgetAbsolutelyPositioned (Widget *widget)
   { return widget->getStyle()->position == core::style::POSITION_ABSOLUTE; }
   static inline bool testWidgetFixedlyPositioned (Widget *widget)
   { return widget->getStyle()->position == core::style::POSITION_FIXED; }
   static inline bool testWidgetOutOfFlow (Widget *widget)
   { return testWidgetFloat (widget) || testWidgetAbsolutelyPositioned (widget)
         || testWidgetFixedlyPositioned (widget); }

   static inline bool testWidgetRelativelyPositioned (Widget *widget)
   { return widget->getStyle()->position == core::style::POSITION_RELATIVE; }

   void initOutOfFlowMgrs ();
   void sizeAllocateStart (core::Allocation *allocation);
   void sizeAllocateEnd ();

   void notifySetAsTopLevel();
   void notifySetParent();

   static bool isOOFContainer (Widget *widget, int oofmIndex);

public:
   static int CLASS_ID;

   OOFAwareWidget ();
   ~OOFAwareWidget ();

   virtual void borderChanged (int y, core::Widget *vloat);
   virtual void oofSizeChanged (bool extremesChanged);
   virtual int getLineBreakWidth (); // Should perhaps be renamed.
   virtual bool isPossibleContainer (int oofmIndex);
   virtual bool isPossibleContainerParent (int oofmIndex);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFAWAREWIDGET_HH__
