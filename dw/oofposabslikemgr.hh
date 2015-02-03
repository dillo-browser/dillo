#ifndef __DW_OOFPOSABSLIKEMGR_HH__
#define __DW_OOFPOSABSLIKEMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

namespace oof {

class OOFPosAbsLikeMgr: public OOFPositionedMgr
{
protected:
   virtual int containerBoxOffsetX () = 0;
   virtual int containerBoxOffsetY () = 0;
   virtual int containerBoxRestWidth () = 0;
   virtual int containerBoxRestHeight () = 0;

   inline int containerBoxDiffWidth ()
   { return containerBoxOffsetX () + containerBoxRestWidth (); }
   inline int containerBoxDiffHeight ()
   { return containerBoxOffsetY () + containerBoxRestHeight (); }

   bool haveExtremesChanged ();

   void sizeAllocateChildren ();

   bool posXAbsolute (Child *child);
   bool posYAbsolute (Child *child);

   void calcPosAndSizeChildOfChild (Child *child, int refWidth, int refHeight,
                                    int *xPtr, int *yPtr, int *widthPtr,
                                    int *ascentPtr, int *descentPtr);
   void calcHPosAndSizeChildOfChild (Child *child, int refWidth,
                                     int origChildWidth, int *xPtr,
                                     int *widthPtr);
   void calcVPosAndSizeChildOfChild (Child *child, int refHeight,
                                     int origChildAscent, int origChildDescent,
                                     int *yPtr, int *ascentPtr,
                                     int *descentPtr);


public:
   OOFPosAbsLikeMgr (OOFAwareWidget *container);
   ~OOFPosAbsLikeMgr ();

   void calcWidgetRefSize (core::Widget *widget, core::Requisition *size);

   void getSize (core::Requisition *containerReq, int *oofWidth,
                 int *oofHeight);
   void getExtremes (core::Extremes *containerExtr,
                     int *oofMinWidth, int *oofMaxWidth);

   int getAvailWidthOfChild (core::Widget *child, bool forceValue);
   int getAvailHeightOfChild (core::Widget *child, bool forceValue);
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFPOSABSLIKEMGR_HH__
