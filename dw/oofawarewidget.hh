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
 *
 * A sub class should at least take care to call these methods at the
 * respective points:
 *
 * - dw::oof::OOFAwareWidget::correctRequisitionByOOF (from
 *   dw::core::Widget::getExtremesImpl)
 * - dw::oof::OOFAwareWidget::correctExtremesByOOF (from
 *   dw::core::Widget::sizeRequestImpl)
 * - dw::oof::OOFAwareWidget::sizeAllocateStart
 * - dw::oof::OOFAwareWidget::sizeAllocateEnd (latter two from
 *   dw::core::Widget::sizeAllocateImpl)
 * - dw::oof::OOFAwareWidget::containerSizeChangedForChildrenOOF
 *   (from dw::core::Widget::containerSizeChangedForChildren)
 * - dw::oof::OOFAwareWidget::drawOOF (from dw::core::Widget::draw)
 * - dw::oof::OOFAwareWidget::getWidgetOOFAtPoint (from
 *   dw::core::Widget::getWidgetAtPoint)
 *
 * See dw::Textblock on how this is done best. For both generators and
 * containers of floats (which is only implemented by dw::Textblock)
 * it gets a bit more complicated.
 */
class OOFAwareWidget: public core::Widget
{
protected:
   enum { OOFM_FLOATS, OOFM_ABSOLUTE, OOFM_FIXED, NUM_OOFM };
   enum { PARENT_REF_OOFM_BITS = 2,
          PARENT_REF_OOFM_MASK = (1 << PARENT_REF_OOFM_BITS) - 1 };

   inline bool isParentRefOOF (int parentRef)
   { return parentRef != -1 && (parentRef & PARENT_REF_OOFM_MASK); }

   inline int makeParentRefInFlow (int inFlowSubRef)
   { return (inFlowSubRef << PARENT_REF_OOFM_BITS); }
   inline int getParentRefInFlowSubRef (int parentRef)
   { assert (!isParentRefOOF (parentRef));
      return parentRef >> PARENT_REF_OOFM_BITS; }

   inline int makeParentRefOOF (int oofmIndex, int oofmSubRef)
   { return (oofmSubRef << PARENT_REF_OOFM_BITS) | (oofmIndex + 1); }
   inline int getParentRefOOFSubRef (int parentRef)
   { assert (isParentRefOOF (parentRef));
      return parentRef >> PARENT_REF_OOFM_BITS; }
   inline int getParentRefOOFIndex (int parentRef)
   { assert (isParentRefOOF (parentRef));
      return (parentRef & PARENT_REF_OOFM_MASK) - 1; }
   inline oof::OutOfFlowMgr *getParentRefOutOfFlowMgr (int parentRef)
   { return outOfFlowMgr[getParentRefOOFIndex (parentRef)]; }

   inline bool isWidgetOOF (Widget *widget)
   { return isParentRefOOF (widget->parentRef); }

   inline int getWidgetInFlowSubRef (Widget *widget)
   { return getParentRefInFlowSubRef (widget->parentRef); }

   inline int getWidgetOOFSubRef (Widget *widget)
   { return getParentRefOOFSubRef (widget->parentRef); }
   inline int getWidgetOOFIndex (Widget *widget)
   { return getParentRefOOFIndex (widget->parentRef); }
   inline oof::OutOfFlowMgr *getWidgetOutOfFlowMgr (Widget *widget)
   { return getParentRefOutOfFlowMgr (widget->parentRef); }

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
   void correctRequisitionByOOF (core::Requisition *requisition);
   void correctExtremesByOOF (core::Extremes *extremes);
   void sizeAllocateStart (core::Allocation *allocation);
   void sizeAllocateEnd ();
   void containerSizeChangedForChildrenOOF ();
   void drawOOF (core::View *view, core::Rectangle *area);
   core::Widget *getWidgetOOFAtPoint (int x, int y, int level);

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
