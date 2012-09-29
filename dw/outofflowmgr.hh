#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Represents additional data for containing boxes.
 */
class OutOfFlowMgr
{
public:
   class ContainingBlock
   {
   public:
      virtual void leftBorderChanged (int y) = 0;
      virtual void rightBorderChanged (int y) = 0;
   };

private:
   core::Widget *containingBlock;

   class Float: public lout::object::Object
   {
   public:
      core::Widget *generator, *widget;
      // width includes border of the containing box
      int y, width, ascent, descent;
   };

   //lout::container::typed::HashTable<lout::object::TypedPointer
   //                                <dw::core::Widget>, Float> *floatsByWidget;
   lout::container::typed::Vector<Float> *leftFloats, *rightFloats;

   Float *findFloatByWidget (core::Widget *widget);

   void markChange (int ref);

   void sizeAllocate(lout::container::typed::Vector<Float> *list,
                     core::Allocation *containingBoxAllocation);
   void draw (lout::container::typed::Vector<Float> *list,
              core::View *view, core::Rectangle *area);

   inline static bool isRefLeftFloat (int ref)
   { return ref != -1 && (ref & 3) == 1; }
   inline static bool isRefRightFloat (int ref)
   { return ref != -1 && (ref & 3) == 3; }

   inline static int createRefLeftFloat (int index)
   { return (index << 2) | 1; }
   inline static int createRefRightFloat (int index)
   { return (index << 2) | 3; }

   inline static int getFloatIndexFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 2); }

public:
   OutOfFlowMgr (core::Widget *containingBlock);
   ~OutOfFlowMgr ();

   void sizeAllocate(core::Allocation *containingBoxAllocation);
   void draw (core::View *view, core::Rectangle *area);
   void queueResize(int ref);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);

   static bool isWidgetOutOfFlow (core::style::Style *style);
   void addWidget (core::Widget *widget, core::Widget *generator);

   void tellNoPosition (core::Widget *widget);
   void tellPosition (core::Widget *widget, int y);

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

   //inline static bool isRefOutOfFlow (int ref) { return false; }
   //inline static int createRefNormalFlow (int lineNo) { return lineNo; }
   //inline static int getLineNoFromRef (int ref) { return ref; }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
