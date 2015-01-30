#ifndef __DW_OOFPOSRELMGR_HH__
#define __DW_OOFPOSRELMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosRelMgr: public OOFPositionedMgr
{
protected:
   bool isReference (core::Widget *widget);
   int containerBoxOffsetX ();
   int containerBoxOffsetY ();
   int containerBoxRestWidth ();
   int containerBoxRestHeight ();

public:
   OOFPosRelMgr (OOFAwareWidget *container);
   ~OOFPosRelMgr ();

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   void calcWidgetRefSize (core::Widget *widget, core::Requisition *size);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSRELMGR_HH__
