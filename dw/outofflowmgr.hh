#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

class Textblock;

/**
 * \brief Represents additional data for containing boxes.
 */
class OutOfFlowMgr
{
private:
   void markChange (int ref);

public:
   void sizeAllocate(core::Allocation *containingBoxAllocation);
   void draw (core::View *view, core::Rectangle *area);
   void queueResize(int ref);

   inline void markSizeChange (int ref) { markChange (ref); }
   inline void markExtremesChange (int ref) { markChange (ref); }

   void addWidget (core::Widget *widget, core::style::Style *style);

   /**
    * Get the left border for the vertical position of y, based on
    * floats. The border includes marging/border/padding of the
    * containging box, but is 0 if there is no float, so a caller
    * should also consider other borders.
    */
   int getLeftBorder (int y);

   int getRightBorder (int y);

   inline static bool isRefOutOfFlow (int ref)
   { return ref != -1 && (ref & 1) != 0; }
   inline static int createRefNormalFlow (int lineNo) { return lineNo << 1; }
   inline static int getLineNoFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 1); }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
