#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Represents additional data for containing blocks.
 */
class OutOfFlowMgr
{
public:
   class ContainingBlock
   {
   public:
      virtual void borderChanged (int y) = 0;

      // An additional "CB" to resolve the ambiguity to the methods in Widget.
      virtual core::style::Style *getCBStyle () = 0;
      virtual core::Allocation *getCBAllocation () = 0;
   };

private:
   ContainingBlock *containingBlock;

   class Float: public lout::object::Object
   {
   public:
      core::Widget *widget;
      // width includes border of the containing block
      int y, width, ascent, descent;
      bool dirty;
   };

   //lout::container::typed::HashTable<lout::object::TypedPointer
   //                                <dw::core::Widget>, Float> *floatsByWidget;
   lout::container::typed::Vector<Float> *leftFloats, *rightFloats;

   Float *findFloatByWidget (core::Widget *widget);
   void ensureFloatSize (Float *vloat);

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
   OutOfFlowMgr (ContainingBlock *containingBlock);
   ~OutOfFlowMgr ();

   void sizeAllocate(core::Allocation *containingBlockAllocation);
   void draw (core::View *view, core::Rectangle *area);
   void queueResize(int ref);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);

   static bool isWidgetOutOfFlow (core::Widget *widget);
   void addWidget (core::Widget *widget);

   void tellNoPosition (core::Widget *widget);
   void tellPosition (core::Widget *widget, int y);

   /**
    * Get the left border for the vertical position of y, based on
    * floats. The border includes marging/border/padding of the
    * containging block, but is 0 if there is no float, so a caller
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

   // for iterators
   inline int getNumWidgets () {
      return leftFloats->size() + rightFloats->size(); }
   inline core::Widget *getWidget (int i) {
      return i < leftFloats->size() ? leftFloats->get(i)->widget :
         rightFloats->get(leftFloats->size())->widget; }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
