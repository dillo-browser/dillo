#ifndef __DW_OOFPOSITIONEDMGR_HH__
#define __DW_OOFPOSITIONEDMGR_HH__

#include "outofflowmgr.hh"

namespace dw {

namespace oof {

class OOFPositionedMgr: public OutOfFlowMgr
{
protected:
   virtual int cbBoxOffsetX () = 0;
   virtual int cbBoxOffsetY () = 0;
   virtual int cbBoxRestWidth () = 0;
   virtual int cbBoxRestHeight () = 0;

   inline int cbBoxDiffWidth () { return cbBoxOffsetX () + cbBoxRestWidth (); }
   inline int cbBoxDiffHeight ()
   { return cbBoxOffsetY () + cbBoxRestHeight (); }

   Textblock *containingBlock;
   core::Allocation containingBlockAllocation;

   lout::container::typed::Vector<core::Widget> *children;

   bool doChildrenExceedCB ();
   bool haveExtremesChanged ();
   void sizeAllocateChildren ();

   inline int getPosLeft (core::Widget *child, int availWidth)
   { return getPosBorder (child->getStyle()->left, availWidth); }
   inline int getPosRight (core::Widget *child, int availWidth)
   { return getPosBorder (child->getStyle()->right, availWidth); }
   inline int getPosTop (core::Widget *child, int availHeight)
   { return getPosBorder (child->getStyle()->top, availHeight); }
   inline int getPosBottom (core::Widget *child, int availHeight)
   { return getPosBorder (child->getStyle()->bottom, availHeight); }

   int getPosBorder (core::style::Length cssValue, int refLength);
   void calcPosAndSizeChildOfChild (core::Widget *child, int refWidth,
                                    int refHeight, int *x, int *y, int *width,
                                    int *ascent, int *descent);

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

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSITIONEDMGR_HH__
