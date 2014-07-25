#ifndef __DW_SIMPLETABLECELL_HH__
#define __DW_SIMPLETABLECELL_HH__

#include "textblock.hh"

namespace dw {

class SimpleTableCell: public Textblock
{
protected:
   int getAvailWidthOfChild (Widget *child, bool forceValue);
   int getAvailHeightOfChild (Widget *child, bool forceValue);

   void correctRequisitionOfChild (Widget *child,
                                   core::Requisition *requisition,
                                   void (*splitHeightFun) (int, int*, int*));
   void correctExtremesOfChild (Widget *child, core::Extremes *extremes);

   bool getAdjustMinWidth ();

public:
   static int CLASS_ID;

   SimpleTableCell (bool limitTextWidth);
   ~SimpleTableCell ();

   int applyPerWidth (int containerWidth, core::style::Length perWidth);
   int applyPerHeight (int containerHeight, core::style::Length perHeight);

   bool isBlockLevel ();
};

} // namespace dw

#endif // __DW_SIMPLETABLECELL_HH__
