#ifndef __DW_OOFPOSFIXEDMGR_HH__
#define __DW_OOFPOSFIXEDMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

class OOFPosFixedMgr: public OOFPositionedMgr
{
public:
   OOFPosFixedMgr (Textblock *containingBlock);
};

} // namespace dw

#endif // __DW_OOFPOSFIXEDMGR_HH__
