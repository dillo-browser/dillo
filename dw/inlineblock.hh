#ifndef __DW_INLINEBLOCK_HH__
#define __DW_INLINEBLOCK_HH__

#include "core.hh"
#include "textblock.hh"

namespace dw {

class InlineBlock: public Textblock
{
public:
   static int CLASS_ID;

   InlineBlock(bool limitTextWidth);
   ~InlineBlock();

   void sizeRequestImpl (core::Requisition *requisition);
};

} // namespace dw

#endif // __DW_INLINEBLOCK_HH__
