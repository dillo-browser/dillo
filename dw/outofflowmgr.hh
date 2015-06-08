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
   virtual void calcWidgetRefSize (core::Widget *widget,
                                   core::Requisition *size) = 0;
   virtual void moveExternalIndices (OOFAwareWidget *generator,
                                     int oldStartIndex, int diff) = 0;
   
   /**
    * \brief Called before tellPosition2, see there for more.
    */
   virtual void tellPosition1 (core::Widget *widget, int x, int y) = 0;

   /**
    * \brief Called after tellPosition1.
    *
    * An implementation should only implement either tellPosition1 or
    * tellPosition2. Coordinates are relative to the *container*.
    */
   virtual void tellPosition2 (core::Widget *widget, int x, int y) = 0;

   virtual void tellIncompletePosition1 (core::Widget *generator,
                                         core::Widget *widget, int x, int y)
      = 0;
   virtual void tellIncompletePosition2 (core::Widget *generator,
                                         core::Widget *widget, int x, int y)
      = 0;

   virtual void getSize (core::Requisition *containerReq, int *oofWidth,
                         int *oofHeight) = 0;
   virtual bool containerMustAdjustExtraSpace ()= 0;
   virtual void getExtremes (core::Extremes *containerExtr, int *oofMinWidth,
                             int *oofMaxWidth) = 0;

   /**
    * Get the left border for the vertical position of *y*, for a height
    * of *h", based on floats; relative to the *container*.
    *
    * The border includes marging/border/padding of the calling textblock
    * but is 0 if there is no float, so a caller should also consider
    * other borders.
    */
   virtual int getLeftBorder (OOFAwareWidget *widget, int y, int h,
                              OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   /**
    * Get the right border for the vertical position of *y*, for a height
    * of *h*, based on floats; relative to the *container*.
    *
    * See also getLeftBorder().
    */
   virtual int getRightBorder (OOFAwareWidget *widget, int y, int h,
                               OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   /**
    * Return whether there is a float on the left side. *y* is
    * relative to the *container*.
    *
    * See also getLeftBorder().
    */
   virtual bool hasFloatLeft (OOFAwareWidget *widget, int y, int h,
                              OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   /**
    * Return whether there is a float on the right side. *y* is
    * relative to the *container*.
    *
    * See also hasFloatLeft(), getLeftBorder();
    */
   virtual bool hasFloatRight (OOFAwareWidget *widget, int y, int h,
                               OOFAwareWidget *lastGen, int lastExtIndex) = 0;

   /**
    * Assuming there is a float on the left side, return the rest
    * height of it. *y* is relative to the *container*.
    *
    * See also getLeftBorder().
    */
   virtual int getLeftFloatHeight (OOFAwareWidget *widget, int y, int h,
                                   OOFAwareWidget *lastGen, int lastExtIndex)
      = 0;

   /**
    * Assuming there is a float on the right side, return the rest
    * height of it. *y* is relative to the *container*.
    *
    * See also getLeftFloatHeight(), getLeftBorder().
    */
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
