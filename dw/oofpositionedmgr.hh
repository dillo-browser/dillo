#ifndef __DW_OOFPOSITIONEDMGR_HH__
#define __DW_OOFPOSITIONEDMGR_HH__

#include "outofflowmgr.hh"

namespace dw {

class OOFPositionedMgr: public OutOfFlowMgr
{
private:
   Textblock *containingBlock;
   core::Allocation containingBlockAllocation;

   lout::container::typed::Vector<core::Widget> *children;

   bool doChildrenExceedCB ();
   bool haveExtremesChanged ();
   void sizeAllocateChildren ();

   inline int getAbsPosLeft (core::Widget *child, int availWidth)
   { return getAbsPosBorder (child->getStyle()->left, availWidth); }
   inline int getAbsPosRight (core::Widget *child, int availWidth)
   { return getAbsPosBorder (child->getStyle()->right, availWidth); }
   inline int getAbsPosTop (core::Widget *child, int availHeight)
   { return getAbsPosBorder (child->getStyle()->top, availHeight); }
   inline int getAbsPosBottom (core::Widget *child, int availHeight)
   { return getAbsPosBorder (child->getStyle()->bottom, availHeight); }

   int getAbsPosBorder (core::style::Length cssValue, int refLength);

public:
   OOFPositionedMgr (Textblock *containingBlock);
   ~OOFPositionedMgr ();

   void sizeAllocateStart (Textblock *caller, core::Allocation *allocation);
   void sizeAllocateEnd (Textblock *caller);
   void containerSizeChangedForChildren ();
   void draw (core::View *view, core::Rectangle *area);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   core::Widget *getWidgetAtPoint (int x, int y, int level);

   void addWidgetInFlow (Textblock *textblock, Textblock *parentBlock,
                         int externalIndex);
   int addWidgetOOF (core::Widget *widget, Textblock *generatingBlock,
                     int externalIndex);
   void moveExternalIndices (Textblock *generatingBlock, int oldStartIndex,
                             int diff);

   void tellPosition (core::Widget *widget, int yReq);

   void getSize (core::Requisition *cbReq, int *oofWidth, int *oofHeight);
   void getExtremes (core::Extremes *cbExtr,
                     int *oofMinWidth, int *oofMaxWidth);

   int getLeftBorder (Textblock *textblock, int y, int h, Textblock *lastGB,
                      int lastExtIndex);
   int getRightBorder (Textblock *textblock, int y, int h, Textblock *lastGB,
                       int lastExtIndex);

   bool hasFloatLeft (Textblock *textblock, int y, int h, Textblock *lastGB,
                      int lastExtIndex);
   bool hasFloatRight (Textblock *textblock, int y, int h, Textblock *lastGB,
                       int lastExtIndex);

   int getLeftFloatHeight (Textblock *textblock, int y, int h,
                           Textblock *lastGB, int lastExtIndex);
   int getRightFloatHeight (Textblock *textblock, int y, int h,
                            Textblock *lastGB, int lastExtIndex);

   int getClearPosition (Textblock *textblock);

   bool affectsLeftBorder (core::Widget *widget);
   bool affectsRightBorder (core::Widget *widget);

   bool dealingWithSizeOfChild (core::Widget *child);
   int getAvailWidthOfChild (core::Widget *child, bool forceValue);
   int getAvailHeightOfChild (core::Widget *child, bool forceValue);

   int getNumWidgets ();
   core::Widget *getWidget (int i);
};

} // namespace dw

#endif // __DW_OOFPOSITIONEDMGR_HH__
