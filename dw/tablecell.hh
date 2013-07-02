#ifndef __DW_TABLECELL_HH__
#define __DW_TABLECELL_HH__

#include "core.hh"
#include "alignedtextblock.hh"

namespace dw {

class TableCell: public AlignedTextblock
{
private:
   int charWordIndex, charWordPos;

protected:
   bool wordWrap (int wordIndex, bool wrapAll);

   int getValue ();
   void setMaxValue (int maxValue, int value);

public:
   static int CLASS_ID;

   TableCell(TableCell *ref, bool limitTextWidth);
   ~TableCell();
};

} // namespace dw

#endif // __DW_TABLECELL_HH__
