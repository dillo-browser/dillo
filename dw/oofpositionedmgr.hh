#ifndef __DW_OOFPOSITIONEDMGR_HH__
#define __DW_OOFPOSITIONEDMGR_HH__

#include "outofflowmgr.hh"

namespace dw {

namespace oof {

class OOFPositionedMgr: public OutOfFlowMgr
{
protected:
   class Child: public lout::object::Object
   {
   public:
      core::Widget *widget, *reference;
      OOFAwareWidget *generator;
      int externalIndex, x, y;

      inline Child (core::Widget *widget, OOFAwareWidget *generator,
                    core::Widget *reference, int externalIndex)
      { this->widget = widget; this->generator = generator;
         this->reference = reference; this->externalIndex = externalIndex;
         x = y = 0; }  
   };

   virtual bool isReference (core::Widget *widget) = 0;
   virtual int containerBoxOffsetX () = 0;
   virtual int containerBoxOffsetY () = 0;
   virtual int containerBoxRestWidth () = 0;
   virtual int containerBoxRestHeight () = 0;

   inline int containerBoxDiffWidth ()
   { return containerBoxOffsetX () + containerBoxRestWidth (); }
   inline int containerBoxDiffHeight ()
   { return containerBoxOffsetY () + containerBoxRestHeight (); }

   OOFAwareWidget *container;
   enum { NOT_ALLOCATED, IN_ALLOCATION, WAS_ALLOCATED }
      containerAllocationState;
   core::Allocation containerAllocation;

   lout::container::typed::Vector<Child> *children;
   lout::container::typed::HashTable<lout::object::TypedPointer
                                        <dw::core::Widget>,
                                     Child> *childrenByWidget;
   
   bool doChildrenExceedContainer ();
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

   bool isHPosComplete (Child *child);
   bool isVPosComplete (Child *child);

   bool isHPosCalculable (Child *child, bool allocated);
   bool isVPosCalculable (Child *child, bool allocated);

   bool isPosCalculable (Child *child, bool allocated);

   void calcPosAndSizeChildOfChild (Child *child, int refWidth, int refHeight,
                                    int *xPtr, int *yPtr, int *widthPtr,
                                    int *ascentPtr, int *descentPtr);
   void calcHPosAndSizeChildOfChild (Child *child, int refWidth,
                                     int origChildWidth, int *xPtr,
                                     int *widthPtr);
   void calcVPosAndSizeChildOfChild (Child *child, int refHeight,
                                     int origChildAscent, int origChildDescent,
                                     int *yPtr, int *ascentPtr,
                                     int *descentPtr);

public:
   OOFPositionedMgr (OOFAwareWidget *container);
   ~OOFPositionedMgr ();

   void sizeAllocateStart (OOFAwareWidget *caller,
                           core::Allocation *allocation);
   void sizeAllocateEnd (OOFAwareWidget *caller);
   void containerSizeChangedForChildren ();
   void draw (core::View *view, core::Rectangle *area,
              core::DrawingContext *context);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   core::Widget *getWidgetAtPoint (int x, int y,
                                   core::GettingWidgetAtPointContext *context);

   void addWidgetInFlow (OOFAwareWidget *widget, OOFAwareWidget *parent,
                         int externalIndex);
   int addWidgetOOF (core::Widget *widget, OOFAwareWidget *generator,
                     int externalIndex);
   void calcWidgetRefSize (core::Widget *widget, core::Requisition *size);
   void moveExternalIndices (OOFAwareWidget *generator, int oldStartIndex,
                             int diff);

   void tellPosition (core::Widget *widget, int x, int y);

   void getSize (core::Requisition *containerReq, int *oofWidth,
                 int *oofHeight);
   bool containerMustAdjustExtraSpace ();
   void getExtremes (core::Extremes *containerExtr,
                     int *oofMinWidth, int *oofMaxWidth);

   int getLeftBorder (OOFAwareWidget *widget, int y, int h,
                      OOFAwareWidget *lastGen, int lastExtIndex);
   int getRightBorder (OOFAwareWidget *widget, int y, int h,
                       OOFAwareWidget *lastGen, int lastExtIndex);

   bool hasFloatLeft (OOFAwareWidget *widget, int y, int h,
                      OOFAwareWidget *lastGen, int lastExtIndex);
   bool hasFloatRight (OOFAwareWidget *widget, int y, int h,
                       OOFAwareWidget *lastGen, int lastExtIndex);

   int getLeftFloatHeight (OOFAwareWidget *widget, int y, int h,
                           OOFAwareWidget *lastGen, int lastExtIndex);
   int getRightFloatHeight (OOFAwareWidget *widget, int y, int h,
                            OOFAwareWidget *lastGen, int lastExtIndex);

   int getClearPosition (OOFAwareWidget *widget);

   bool affectsLeftBorder (core::Widget *widget);
   bool affectsRightBorder (core::Widget *widget);
   bool mayAffectBordersAtAll ();

   bool dealingWithSizeOfChild (core::Widget *child);
   int getAvailWidthOfChild (core::Widget *child, bool forceValue);
   int getAvailHeightOfChild (core::Widget *child, bool forceValue);

   int getNumWidgets ();
   core::Widget *getWidget (int i);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSITIONEDMGR_HH__
