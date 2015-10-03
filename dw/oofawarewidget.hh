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
 * See \ref dw-out-of-flow for an overview.
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
 *   between widgets in flow and out of flow; general pattern:
 *
 *    \code
 * if (isWidgetOOF (child) && getWidgetOutOfFlowMgr(child) &&
 *     getWidgetOutOfFlowMgr(child)->dealingWithSizeOfChild (child))
 *    width =
 *      getWidgetOutOfFlowMgr(child)->getAvailWidthOfChild (child,forceValue);
 * else {
 *    // ... specific implementation ...
 *    \endcode
 *
 *   See also implementations of dw::Textblock and dw::Table. (Open
 *   issue: What about dw::core::Widget::correctRequisitionOfChild and
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
public:
   enum { IMPL_POS = false };
   
protected:
   enum { OOFM_FLOATS, OOFM_ABSOLUTE, OOFM_RELATIVE, OOFM_FIXED, NUM_OOFM };
   static const char *OOFM_NAME[NUM_OOFM];
   enum { PARENT_REF_OOFM_BITS = 3,
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
   core::Requisition requisitionWithoutOOF;

   inline OutOfFlowMgr *searchOutOfFlowMgr (int oofmIndex)
   { return oofContainer[oofmIndex] ?
         oofContainer[oofmIndex]->outOfFlowMgr[oofmIndex] : NULL; }

   static int getOOFMIndex (Widget *widget);

   void initOutOfFlowMgrs ();
   void correctRequisitionByOOF (core::Requisition *requisition,
                                 void (*splitHeightFun) (int, int*, int*));
   void correctExtremesByOOF (core::Extremes *extremes);
   void sizeAllocateStart (core::Allocation *allocation);
   void sizeAllocateEnd ();
   void containerSizeChangedForChildrenOOF ();

   virtual void drawLevel (core::View *view, core::Rectangle *area, int level,
                           core::DrawingContext *context);
   void drawOOF (core::View *view, core::Rectangle *area,
                 core::DrawingContext *context);

   Widget *getWidgetAtPoint (int x, int y,
                             core::GettingWidgetAtPointContext *context);
   virtual Widget *getWidgetAtPointLevel (int x, int y, int level,
                                          core::GettingWidgetAtPointContext
                                          *context);
   Widget *getWidgetOOFAtPoint (int x, int y,
                                core::GettingWidgetAtPointContext *context);

   static bool isOOFContainer (Widget *widget, int oofmIndex);

   void notifySetAsTopLevel();
   void notifySetParent();

   void removeChild (Widget *child);

   virtual bool adjustExtraSpaceWhenCorrectingRequisitionByOOF ();

public:
   enum {
      SL_START, SL_BACKGROUND, SL_SC_BOTTOM, SL_IN_FLOW, SL_OOF_REF,
      SL_OOF_CONT, SL_SC_TOP, SL_END };

   static int CLASS_ID;

   OOFAwareWidget ();
   ~OOFAwareWidget ();

   static const char *stackingLevelText (int level);

   static inline bool testStyleFloat (core::style::Style *style)
   { return style->vloat != core::style::FLOAT_NONE; }

   static inline bool testStyleAbsolutelyPositioned (core::style::Style *style)
   { return IMPL_POS && style->position == core::style::POSITION_ABSOLUTE; }
   static inline bool testStyleFixedlyPositioned (core::style::Style *style)
   { return IMPL_POS && style->position == core::style::POSITION_FIXED; }
   static inline bool testStyleRelativelyPositioned (core::style::Style *style)
   { return IMPL_POS && style->position == core::style::POSITION_RELATIVE; }

   static inline bool testStylePositioned (core::style::Style *style)
   { return testStyleAbsolutelyPositioned (style) ||
         testStyleRelativelyPositioned (style) ||
         testStyleFixedlyPositioned (style); }

   static inline bool testStyleOutOfFlow (core::style::Style *style)
   {  // Second part is equivalent to testStylePositioned(), but we still keep
      // the two seperately.
      return testStyleFloat (style) || testStyleAbsolutelyPositioned (style)
         || testStyleRelativelyPositioned (style)
         || testStyleFixedlyPositioned (style); }
   
   static inline bool testWidgetFloat (Widget *widget)
   { return testStyleFloat (widget->getStyle ()); }

   static inline bool testWidgetAbsolutelyPositioned (Widget *widget)
   { return testStyleAbsolutelyPositioned (widget->getStyle ()); }
   static inline bool testWidgetFixedlyPositioned (Widget *widget)
   { return testStyleFixedlyPositioned (widget->getStyle ()); }
   static inline bool testWidgetRelativelyPositioned (Widget *widget)
   { return testStyleRelativelyPositioned (widget->getStyle ()); }

   static inline bool testWidgetPositioned (Widget *widget)
   { return testStylePositioned (widget->getStyle ()); }

   static inline bool testWidgetOutOfFlow (Widget *widget)
   { return testStyleOutOfFlow (widget->getStyle ()); }

   inline core::Requisition *getRequisitionWithoutOOF ()
   { return &requisitionWithoutOOF; }

   bool doesWidgetOOFInterruptDrawing (Widget *widget);

   void draw (core::View *view, core::Rectangle *area,
              core::DrawingContext *context);

   virtual bool mustBeWidenedToAvailWidth ();

   /**
    * Called by an implementation of dw::oof::OutOfFlowMgr (actually only
    * OOFFloatsMgr) when the border has changed due to a widget out of flow, or
    * some widgets out of flow (actually floats).
    *
    * `y`, given relative to the container, denotes the minimal position (when
    * more than one float caused this), `widgetOOF` the respective widget out of
    * flow.
    */
   virtual void borderChanged (int oofmIndex, int y, core::Widget *widgetOOF);

   /**
    * Called by an implementation of dw::oof::OutOfFlowMgr (actually only
    * OOFPosRelMgr) for the generator of a widget out of flow, when the
    * reference size has changed. (The size of the reference is 0 * 0 for all
    * other implementations of dw::oof::OutOfFlowMgr.)
    */
   virtual void widgetRefSizeChanged (int externalIndex);
   
   /**
    * TODO Needed after SRDOP?
    */
   virtual void clearPositionChanged ();

   /**
    * Called by an implementation of dw::oof::OutOfFlowMgr when the size
    * of the container has changed, typically in sizeAllocateEnd.
    */
   virtual void oofSizeChanged (bool extremesChanged);

   /**
    * Called by OOFFloatsMgr to position floats. (Should perhaps be renamed.)
    */
   virtual int getLineBreakWidth ();
   
   virtual bool isPossibleContainer (int oofmIndex);

   virtual bool isPossibleContainerParent (int oofmIndex);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFAWAREWIDGET_HH__
