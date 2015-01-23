#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

/**
 *  \brief Out Of Flow. See \ref dw-out-of-flow.
 */
namespace oof {

class OOFAwareWidget;

/**
 * \brief Represents additional data for OOF containers.
 */
class OutOfFlowMgr
{
public:
   OutOfFlowMgr ();
   virtual ~OutOfFlowMgr ();

   virtual void sizeAllocateStart (OOFAwareWidget *caller,
                                   core::Allocation *allocation) = 0;
   virtual void sizeAllocateEnd (OOFAwareWidget *caller) = 0;
   virtual void containerSizeChangedForChildren () = 0;
   virtual void draw (core::View *view, core::Rectangle *area,
                      core::DrawingContext *context) = 0;

   virtual void markSizeChange (int ref) = 0;
   virtual void markExtremesChange (int ref) = 0;
   virtual core::Widget *getWidgetAtPoint (int x, int y,
                                           core::GettingWidgetAtPointContext
                                           *context) = 0;

   virtual void addWidgetInFlow (OOFAwareWidget *widget,
                                 OOFAwareWidget *parent, int externalIndex) = 0;
   virtual int addWidgetOOF (core::Widget *widget, OOFAwareWidget *generator,
                             int externalIndex) = 0;
   virtual void moveExternalIndices (OOFAwareWidget *generator,
                                     int oldStartIndex, int diff) = 0;
   
   virtual void tellPosition (core::Widget *widget, int x, int y) = 0;
   
   virtual void getSize (core::Requisition *containerReq, int *oofWidth,
                         int *oofHeight) = 0;
   virtual bool containerMustAdjustExtraSpace ()= 0;
   virtual void getExtremes (core::Extremes *containerExtr, int *oofMinWidth,
                             int *oofMaxWidth) = 0;


   virtual int getLeftBorder (OOFAwareWidget *widget, int y, int h,
                              OOFAwareWidget *lastGen, int lastExtIndex) = 0;
   virtual int getRightBorder (OOFAwareWidget *widget, int y, int h,
                               OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   virtual bool hasFloatLeft (OOFAwareWidget *widget, int y, int h,
                              OOFAwareWidget *lastGen, int lastExtIndex) = 0;
   virtual bool hasFloatRight (OOFAwareWidget *widget, int y, int h,
                               OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   virtual int getLeftFloatHeight (OOFAwareWidget *widget, int y, int h,
                                   OOFAwareWidget *lastGen, int lastExtIndex)
      = 0;
   virtual int getRightFloatHeight (OOFAwareWidget *widget, int y, int h,
                                    OOFAwareWidget *lastGen, int lastExtIndex)
      = 0;
   
   virtual bool affectsLeftBorder (core::Widget *widget) = 0;
   virtual bool affectsRightBorder (core::Widget *widget) = 0;
   virtual bool mayAffectBordersAtAll () = 0;

   virtual int getClearPosition (OOFAwareWidget *widget) = 0;

   virtual bool dealingWithSizeOfChild (core::Widget *child) = 0;
   virtual int getAvailWidthOfChild (core::Widget *child, bool forceValue) = 0;
   virtual int getAvailHeightOfChild (core::Widget *child, bool forceValue) = 0;
   
   // for iterators
   virtual int getNumWidgets () = 0;
   virtual core::Widget *getWidget (int i) = 0;
};

} // namespace oof

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
