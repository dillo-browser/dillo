#ifndef __DW_OOFPOSRELMGR_HH__
#define __DW_OOFPOSRELMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosRelMgr: public OOFPositionedMgr
{
protected:
   void sizeAllocateChildren ();
   bool posXAbsolute (Child *child);
   bool posYAbsolute (Child *child);

   int getChildPosX (Child *child, int refWidth);
   int getChildPosY (Child *child, int refHeight);
   int getChildPosDim (core::style::Length posCssValue,
                       core::style::Length negCssValue, int refPos,
                       int refLength);

   inline int getChildPosX (Child *child) 
   { return getChildPosX (child, container->getAvailWidth (true)); }
   inline int getChildPosY (Child *child) 
   { return getChildPosY (child, container->getAvailHeight (true)); }

public:
   OOFPosRelMgr (OOFAwareWidget *container);
   ~OOFPosRelMgr ();

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   void calcWidgetRefSize (core::Widget *widget, core::Requisition *size);

   void getSize (core::Requisition *containerReq, int *oofWidth,
                 int *oofHeight);
   void getExtremes (core::Extremes *containerExtr, int *oofMinWidth,
                     int *oofMaxWidth);

   bool dealingWithSizeOfChild (core::Widget *child);
   int getAvailWidthOfChild (core::Widget *child, bool forceValue);
   int getAvailHeightOfChild (core::Widget *child, bool forceValue);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSRELMGR_HH__
