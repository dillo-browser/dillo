#ifndef __DW_OOFPOSRELMGR_HH__
#define __DW_OOFPOSRELMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosRelMgr: public OOFPositionedMgr
{
protected:
   bool isReference (core::Widget *widget);

public:
   OOFPosRelMgr (OOFAwareWidget *container);
   ~OOFPosRelMgr ();

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   void calcWidgetRefSize (core::Widget *widget, core::Requisition *size);

   void sizeAllocateStart (OOFAwareWidget *caller,
                           core::Allocation *allocation);
   void sizeAllocateEnd (OOFAwareWidget *caller);
   void getSize (core::Requisition *containerReq, int *oofWidth,
                 int *oofHeight);
   void getExtremes (core::Extremes *containerExtr, int *oofMinWidth,
                     int *oofMaxWidth);
   
   int getAvailWidthOfChild (core::Widget *child, bool forceValue);
   int getAvailHeightOfChild (core::Widget *child, bool forceValue);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSRELMGR_HH__
