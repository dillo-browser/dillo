#ifndef __DW_OOFPOSABSMGR_HH__
#define __DW_OOFPOSABSMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

class OOFPosAbsMgr: public OOFPositionedMgr
{
public:
   OOFPosAbsMgr (Textblock *containingBlock);
};

} // namespace dw

#endif // __DW_OOFPOSABSMGR_HH__
