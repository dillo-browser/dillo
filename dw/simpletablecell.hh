#ifndef __DW_SIMPLETABLECELL_HH__
#define __DW_SIMPLETABLECELL_HH__

#include "textblock.hh"

namespace dw {

class SimpleTableCell: public Textblock
{
public:
   static int CLASS_ID;

   SimpleTableCell (bool limitTextWidth);
   ~SimpleTableCell ();

   bool isBlockLevel ();
};

} // namespace dw

#endif // __DW_SIMPLETABLECELL_HH__
