#ifndef __DW_ALIGNEDTABLECELL_HH__
#define __DW_ALIGNEDTABLECELL_HH__

#include "core.hh"
#include "alignedtextblock.hh"

namespace dw {

class AlignedTableCell: public AlignedTextblock
{
private:
   int charWordIndex, charWordPos;

protected:
   int getAvailWidthOfChild (Widget *child, bool forceValue);
   int getAvailHeightOfChild (Widget *child, bool forceValue);

   void correctRequisitionOfChild (Widget *child,
                                   core::Requisition *requisition,
                                   void (*splitHeightFun) (int, int*, int*));
   void correctExtremesOfChild (Widget *child, core::Extremes *extremes,
                                bool useAdjustmentWidth);

   bool getAdjustMinWidth ();

   int wordWrap (int wordIndex, bool wrapAll);

   int getValue ();
   void setMaxValue (int maxValue, int value);

public:
   static int CLASS_ID;

   AlignedTableCell(AlignedTableCell *ref, bool limitTextWidth);
   ~AlignedTableCell();

   int applyPerWidth (int containerWidth, core::style::Length perWidth);
   int applyPerHeight (int containerHeight, core::style::Length perHeight);

   bool isBlockLevel ();

   bool mustBeWidenedToAvailWidth ();
};

} // namespace dw

#endif // __DW_ALIGNEDTABLECELL_HH__
