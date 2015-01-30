#ifndef __DW_OOFPOSABSLIKEMGR_HH__
#define __DW_OOFPOSABSLIKEMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosAbsLikeMgr: public OOFPositionedMgr
{
public:
   OOFPosAbsLikeMgr (OOFAwareWidget *container);
   ~OOFPosAbsLikeMgr ();
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSABSLIKEMGR_HH__
