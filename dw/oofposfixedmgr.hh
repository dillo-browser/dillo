#ifndef __DW_OOFPOSFIXEDMGR_HH__
#define __DW_OOFPOSFIXEDMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosFixedMgr: public OOFPositionedMgr
{
protected:
   bool isReference (core::Widget *widget);
   int containerBoxOffsetX ();
   int containerBoxOffsetY ();
   int containerBoxRestWidth ();
   int containerBoxRestHeight ();

public:
   OOFPosFixedMgr (OOFAwareWidget *container);
   ~OOFPosFixedMgr ();
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSFIXEDMGR_HH__
