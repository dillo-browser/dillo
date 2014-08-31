#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

class Textblock;

/**
 * \brief Represents additional data for containing blocks.
 */
class OutOfFlowMgr
{
public:
   OutOfFlowMgr ();
   virtual ~OutOfFlowMgr ();

   virtual void sizeAllocateStart (Textblock *caller,
                                   core::Allocation *allocation) = 0;
   virtual void sizeAllocateEnd (Textblock *caller) = 0;
   virtual void containerSizeChangedForChildren () = 0;
   virtual void draw (core::View *view, core::Rectangle *area) = 0;

   virtual void markSizeChange (int ref) = 0;
   virtual void markExtremesChange (int ref) = 0;
   virtual core::Widget *getWidgetAtPoint (int x, int y, int level) = 0;

   virtual void addWidgetInFlow (Textblock *textblock, Textblock *parentBlock,
                                 int externalIndex) = 0;
   virtual int addWidgetOOF (core::Widget *widget, Textblock *generatingBlock,
                             int externalIndex) = 0;
   virtual void moveExternalIndices (Textblock *generatingBlock,
                                     int oldStartIndex, int diff) = 0;
   
   virtual void tellPosition (core::Widget *widget, int yReq) = 0;

   virtual void getSize (core::Requisition *cbReq, int *oofWidth,
                         int *oofHeight) = 0;
   virtual void getExtremes (core::Extremes *cbExtr, int *oofMinWidth,
                             int *oofMaxWidth) = 0;


   virtual int getLeftBorder (Textblock *textblock, int y, int h,
                              Textblock *lastGB, int lastExtIndex) = 0;
   virtual int getRightBorder (Textblock *textblock, int y, int h,
                               Textblock *lastGB, int lastExtIndex) = 0;

   virtual bool hasFloatLeft (Textblock *textblock, int y, int h,
                              Textblock *lastGB, int lastExtIndex) = 0;
   virtual bool hasFloatRight (Textblock *textblock, int y, int h,
                               Textblock *lastGB, int lastExtIndex) = 0;

   virtual int getLeftFloatHeight (Textblock *textblock, int y, int h,
                                   Textblock *lastGB, int lastExtIndex) = 0;
   virtual int getRightFloatHeight (Textblock *textblock, int y, int h,
                                    Textblock *lastGB, int lastExtIndex) = 0;

   virtual bool affectsLeftBorder (core::Widget *widget) = 0;
   virtual bool affectsRightBorder (core::Widget *widget) = 0;

   virtual int getClearPosition (Textblock *textblock) = 0;

   virtual bool dealingWithSizeOfChild (core::Widget *child) = 0;
   virtual int getAvailWidthOfChild (core::Widget *child, bool forceValue) = 0;
   virtual int getAvailHeightOfChild (core::Widget *child, bool forceValue) = 0;

   // for iterators
   virtual int getNumWidgets () = 0;
   virtual core::Widget *getWidget (int i) = 0;
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
