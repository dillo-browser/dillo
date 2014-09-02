#ifndef __DW_OOFPOSABSMGR_HH__
#define __DW_OOFPOSABSMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

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

} // namespace dw

#endif // __DW_OOFPOSABSMGR_HH__
