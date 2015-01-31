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

   OOFAwareWidget *container;
   core::Allocation containerAllocation;

   lout::container::typed::Vector<Child> *children;
   lout::container::typed::HashTable<lout::object::TypedPointer
                                        <dw::core::Widget>,
                                     Child> *childrenByWidget;

   inline bool getPosLeft (core::Widget *child, int availWidth, int *result)
   { return getPosBorder (child->getStyle()->left, availWidth, result); }
   inline bool getPosRight (core::Widget *child, int availWidth, int *result)
   { return getPosBorder (child->getStyle()->right, availWidth, result); }
   inline bool getPosTop (core::Widget *child, int availHeight, int *result)
   { return getPosBorder (child->getStyle()->top, availHeight, result); }
   inline bool getPosBottom (core::Widget *child, int availHeight, int *result)
   { return getPosBorder (child->getStyle()->bottom, availHeight, result); }

   bool getPosBorder (core::style::Length cssValue, int refLength, int *result);

public:
   OOFPositionedMgr (OOFAwareWidget *container);
   ~OOFPositionedMgr ();

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
   void moveExternalIndices (OOFAwareWidget *generator, int oldStartIndex,
                             int diff);

   void tellPosition1 (core::Widget *widget, int x, int y);
   void tellPosition2 (core::Widget *widget, int x, int y);

   bool containerMustAdjustExtraSpace ();

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

   int getNumWidgets ();
   core::Widget *getWidget (int i);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSITIONEDMGR_HH__
