#ifndef __DW_OOFPOSABSLIKEMGR_HH__
#define __DW_OOFPOSABSLIKEMGR_HH__

#include "oofpositionedmgr.hh"
#include "oofawarewidget.hh"

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

   inline bool generatorPosDefined (Child *child) {
      return child->generator == container ||
         (containerAllocationState != NOT_ALLOCATED
          && child->generator->wasAllocated ());
   }
   inline int generatorPosX (Child *child) {
      assert (generatorPosDefined (child));
      return child->generator == container ? 0 :
         child->generator->getAllocation()->x
         - (containerAllocation.x + containerBoxOffsetX ());
   }
   inline int generatorPosY (Child *child) {
      assert (generatorPosDefined (child));
      return child->generator == container ? 0 :
         child->generator->getAllocation()->y
         - (containerAllocation.y + containerBoxOffsetY ());
   }
   
   inline bool posXDefined (Child *child)
   { return posXAbsolute (child) || generatorPosDefined (child); }

   inline bool posYDefined (Child *child)
   { return posYAbsolute (child) || generatorPosDefined (child); }

   bool doChildrenExceedContainer ();
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
