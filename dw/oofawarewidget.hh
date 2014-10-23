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
 * Requirements for sub classes (in most cases refer to dw::Textblock
 * as a good example):
 *
 * - A sub class should at least take care to call these methods at the
 *   respective points:
 *
 *   - dw::oof::OOFAwareWidget::correctRequisitionByOOF (from
 *     dw::core::Widget::getExtremesImpl)
 *   - dw::oof::OOFAwareWidget::correctExtremesByOOF (from
 *     dw::core::Widget::sizeRequestImpl)
 *   - dw::oof::OOFAwareWidget::sizeAllocateStart
 *   - dw::oof::OOFAwareWidget::sizeAllocateEnd (latter two from
 *     dw::core::Widget::sizeAllocateImpl)
 *   - dw::oof::OOFAwareWidget::containerSizeChangedForChildrenOOF
 *     (from dw::core::Widget::containerSizeChangedForChildren)
 *   - dw::oof::OOFAwareWidget::drawOOF (from dw::core::Widget::draw)
 *   - dw::oof::OOFAwareWidget::getWidgetOOFAtPoint (from
 *     dw::core::Widget::getWidgetAtPoint)
 *
 * - Implementations of dw::core::Widget::getAvailWidthOfChild and
 *   dw::core::Widget::getAvailHeightOfChild have to distinguish
 *   between widgets in flow and out of flow; see implementations of
 *   dw::oof::OOFAwareWidget. (Open issue: What about
 *   dw::core::Widget::correctRequisitionOfChild and
 *   dw::core::Widget::correctExtremesOfChild? Currently, all widgets
 *   are used the default implementation.)
 *
 * - Iterators have to consider widgets out of flow;
 *   dw::oof::OOFAwareWidget::OOFAwareWidgetIterator is recommended as
 *   base class.
 *
 * - dw::core::Widget::parentRef has to be set for widgets in flow; if
 *   not used further, a simple *makeParentRefInFlow(0)* is sufficient
 *   (as dw::Table::addCell does). Widgets which are only containers,
 *   but not generators, do not have to care about widgets out of
 *   flow in this regard.
 *
 * For both generators and containers of floats (which is only
 * implemented by dw::Textblock) it gets a bit more complicated.
 *
 * \todo Currently, on the level of dw::oof::OOFAwareWidget, nothing
 * is done about dw::core::Widget::markSizeChange and
 * dw::core::Widget::markExtremesChange. This does not matter, though:
 * dw::Textblock takes care of these, and dw::Table is only connected
 * to subclasses of dw::oof::OOFPositionedMgr, which do care about
 * these. However, this should be considered for completeness.
 */
class OOFAwareWidget: public core::Widget
{
protected:
   enum { OOFM_FLOATS, OOFM_ABSOLUTE, OOFM_FIXED, NUM_OOFM };
   enum { PARENT_REF_OOFM_BITS = 2,
          PARENT_REF_OOFM_MASK = (1 << PARENT_REF_OOFM_BITS) - 1 };

   class OOFAwareWidgetIterator: public core::Iterator
   {
   private:
      enum { NUM_SECTIONS = NUM_OOFM + 1 };
      int sectionIndex; // 0 means in flow, otherwise OOFM index + 1
      int index;

      int numParts (int sectionIndex, int numContentsInFlow = -1);
      void getPart (int sectionIndex, int index, core::Content *content);

   protected:
      virtual int numContentsInFlow () = 0;
      virtual void getContentInFlow (int index, core::Content *content) = 0;

      void setValues (int sectionIndex, int index);
      inline void cloneValues (OOFAwareWidgetIterator *other)
      { other->setValues (sectionIndex, index); }         

      inline bool inFlow () { return sectionIndex == 0; }
      inline int getInFlowIndex () { assert (inFlow ()); return index; }
      void highlightOOF (int start, int end, core::HighlightLayer layer);
      void unhighlightOOF (int direction, core::HighlightLayer layer);
      void getAllocationOOF (int start, int end, core::Allocation *allocation);

   public:
      OOFAwareWidgetIterator (OOFAwareWidget *widget, core::Content::Type mask,
                              bool atEnd, int numContentsInFlow);

      void intoStringBuffer(lout::misc::StringBuffer *sb);
      int compareTo(lout::object::Comparable *other);

      bool next ();
      bool prev ();
   };

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
   static inline bool testWidgetPositioned (Widget *widget)
   { return testWidgetAbsolutelyPositioned (widget) ||
         testWidgetRelativelyPositioned (widget) ||
         testWidgetFixedlyPositioned (widget); }
   static inline bool testWidgetOutOfFlow (Widget *widget)
   { return testWidgetFloat (widget) || testWidgetAbsolutelyPositioned (widget)
         || testWidgetFixedlyPositioned (widget); }

   static inline bool testWidgetRelativelyPositioned (Widget *widget)
   { return widget->getStyle()->position == core::style::POSITION_RELATIVE; }

   static bool getOOFMIndex (Widget *widget);

   void initOutOfFlowMgrs ();
   void correctRequisitionByOOF (core::Requisition *requisition,
                                 void (*splitHeightFun) (int, int*, int*));
   void correctExtremesByOOF (core::Extremes *extremes);
   void sizeAllocateStart (core::Allocation *allocation);
   void sizeAllocateEnd ();
   void containerSizeChangedForChildrenOOF ();

   virtual Widget *drawLevel (core::View *view, core::Rectangle *area,
                              core::StackingIteratorStack *iteratorStack,
                              int majorLevel);
   Widget *drawOOF (core::View *view, core::Rectangle *area,
                    core::StackingIteratorStack *iteratorStack);
   Widget *getWidgetOOFAtPoint (int x, int y);

   static bool isOOFContainer (Widget *widget, int oofmIndex);

   void notifySetAsTopLevel();
   void notifySetParent();

   int getAvailWidthOfChild (Widget *child, bool forceValue);
   int getAvailHeightOfChild (Widget *child, bool forceValue);

   void removeChild (Widget *child);

   core::Widget *getWidgetAtPoint (int x, int y);

public:
   static int CLASS_ID;

   class OOFStackingIterator: public lout::object::Object
   {
   private:
      lout::container::typed::HashSet<lout::object::TypedPointer<Widget> >
         *widgetsDrawnAfterInterruption;

   public:
      enum { START, BACKGROUND, SC_BOTTOM, IN_FLOW, OOF_REF, OOF_CONT, SC_TOP,
             END } ;
      int majorLevel, minorLevel, index;

      static const char *majorLevelText (int majorLevel);

      OOFStackingIterator (bool atEnd);
      ~OOFStackingIterator ();

      void intoStringBuffer(lout::misc::StringBuffer *sb);

      void registerWidgetDrawnAfterInterruption (Widget *widget);
      bool hasWidgetBeenDrawnAfterInterruption (Widget *widget);
   };

   OOFAwareWidget ();
   ~OOFAwareWidget ();

   bool doesWidgetOOFInterruptDrawing (Widget *widget);

   Widget *draw (core::View *view, core::Rectangle *area,
                 core::StackingIteratorStack *iteratorStack);
   lout::object::Object *stackingIterator (bool atEnd);

   virtual void borderChanged (int y, core::Widget *vloat);
   virtual void clearPositionChanged ();
   virtual void oofSizeChanged (bool extremesChanged);
   virtual int getLineBreakWidth (); // Should perhaps be renamed.
   virtual bool isPossibleContainer (int oofmIndex);
   virtual bool isPossibleContainerParent (int oofmIndex);
   virtual bool adjustExtraSpaceWhenCorrectingRequisitionByOOF ();
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFAWAREWIDGET_HH__
