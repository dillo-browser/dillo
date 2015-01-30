#ifndef __DW_OOFPOSABSMGR_HH__
#define __DW_OOFPOSABSMGR_HH__

#include "oofposabslikemgr.hh"

namespace dw {

namespace oof {

class OOFPosAbsMgr: public OOFPosAbsLikeMgr
{
protected:
   bool isReference (core::Widget *widget);
   int containerBoxOffsetX ();
   int containerBoxOffsetY ();
   int containerBoxRestWidth ();
   int containerBoxRestHeight ();

public:
   OOFPosAbsMgr (OOFAwareWidget *container);
   ~OOFPosAbsMgr ();
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSABSMGR_HH__
