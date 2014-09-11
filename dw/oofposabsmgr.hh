#ifndef __DW_OOFPOSABSMGR_HH__
#define __DW_OOFPOSABSMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosAbsMgr: public OOFPositionedMgr
{
protected:
   int cbBoxOffsetX ();
   int cbBoxOffsetY ();
   int cbBoxRestWidth ();
   int cbBoxRestHeight ();

public:
   OOFPosAbsMgr (Textblock *containingBlock);
   ~OOFPosAbsMgr ();
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSABSMGR_HH__
