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

   enum { NOT_ALLOCATED, IN_ALLOCATION, WAS_ALLOCATED }
      containerAllocationState;

   
   bool doChildrenExceedContainer ();
   bool haveExtremesChanged ();
   void sizeAllocateChildren ();

   bool isHPosComplete (Child *child);
   bool isVPosComplete (Child *child);

   bool isHPosCalculable (Child *child, bool allocated);
   bool isVPosCalculable (Child *child, bool allocated);

   bool isPosCalculable (Child *child, bool allocated);

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

   void sizeAllocateStart (OOFAwareWidget *caller,
                           core::Allocation *allocation);
   void sizeAllocateEnd (OOFAwareWidget *caller);
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
