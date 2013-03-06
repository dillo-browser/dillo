#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

class Textblock;

/**
 * \brief Represents additional data for containing blocks.
 */
class OutOfFlowMgr
{
private:
   Textblock *containingBlock;
   core::Allocation containingBlockAllocation; /* Set by sizeAllocateStart(),
                                                  and accessable also before
                                                  sizeAllocateEnd(), as opposed
                                                  by containingBlock->
                                                     getAllocation(). */

   class Float: public lout::object::Object
   {
   public:
      core::Widget *widget;
      Textblock *generatingBlock;
      bool positioned;
      int yReq, yReal; // relative to generator, not container
      core::Requisition size;
      bool dirty;

      int yForContainer (OutOfFlowMgr *oofm, int y);

      inline int yForContainer (OutOfFlowMgr *oofm) {
         return yForContainer (oofm, yReal);
      }
   };

   class TBInfo: public lout::object::Object
   {
   public:
      bool wasAllocated;
      int xCB, yCB; // relative to the containing block
      int width, height;
   };

   //lout::container::typed::HashTable<lout::object::TypedPointer
   //                                <dw::core::Widget>, Float> *floatsByWidget;
   lout::container::typed::Vector<Float> *leftFloats, *rightFloats;
   lout::container::typed::HashTable<lout::object::TypedPointer <Textblock>,
                                     TBInfo> *tbInfos;
   
   Float *findFloatByWidget (core::Widget *widget);
   lout::container::typed::Vector<Float> *getFloatList (core::Widget *widget);
   lout::container::typed::Vector<Float> *getOppositeFloatList (core::Widget
                                                                *widget);

   void sizeAllocateFloats (lout::container::typed::Vector<Float> *list,
                            bool right);
   bool isTextblockCoveredByFloats (Textblock *tb, int tbx, int tby,
                                    int tbWidth, int tbHeight, int *floatPos);
   bool isTextblockCoveredByFloats (lout::container::typed::Vector<Float> *list,
                                    Textblock *tb, int tbx, int tby,
                                    int tbWidth, int tbHeight, int *floatPos);

   void draw (lout::container::typed::Vector<Float> *list,
              core::View *view, core::Rectangle *area);
   core::Widget *getWidgetAtPoint (lout::container::typed::Vector<Float> *list,
                                   int x, int y, int level);
   void tellPositionOrNot (core::Widget *widget, int y, bool positioned);
   int getFloatsSize (lout::container::typed::Vector<Float> *list);
   void accumExtremes (lout::container::typed::Vector<Float> *list,
                       int *oofMinWidth, int *oofMaxWidth);
   void registerCaller (Textblock *textblock);
   int getBorder (Textblock *textblock,
                  lout::container::typed::Vector<Float> *list, bool right,
                  int y, int h);
   bool hasFloat (Textblock *textblock,
                  lout::container::typed::Vector<Float> *list, bool right,
                  int y, int h);

   void ensureFloatSize (Float *vloat);
   bool getYWidget (Textblock *textblock, Float *vloat, int *yWidget);
   int getBorderDiff (Textblock *textblock, Float *vloat, bool right);


   inline static bool isRefFloat (int ref)
   { return ref != -1 && (ref & 1) == 1; }
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
   OutOfFlowMgr (Textblock *containingBlock);
   ~OutOfFlowMgr ();

   void sizeAllocateStart (core::Allocation *containingBlockAllocation);
   void sizeAllocateEnd ();
   void draw (core::View *view, core::Rectangle *area);
   void queueResize(int ref);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   core::Widget *getWidgetAtPoint (int x, int y, int level);

   static bool isWidgetOutOfFlow (core::Widget *widget);
   void addWidget (core::Widget *widget, Textblock *generatingBlock);

   void tellNoPosition (core::Widget *widget);
   void tellPosition (core::Widget *widget, int y);

   void getSize (int cbWidth, int cbHeight, int *oofWidth, int *oofHeight);
   void getExtremes (int cbMinWidth, int cbMaxWidth, int *oofMinWidth,
                     int *oofMaxWidth);

   int getLeftBorder (Textblock *textblock, int y, int h);
   int getRightBorder (Textblock *textblock, int y, int h);

   bool hasFloatLeft (Textblock *textblock, int y, int h);
   bool hasFloatRight (Textblock *textblock, int y, int h);

   inline static bool isRefOutOfFlow (int ref)
   { return ref != -1 && (ref & 1) != 0; }
   inline static int createRefNormalFlow (int lineNo) { return lineNo << 1; }
   inline static int getLineNoFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 1); }

   // for iterators
   inline int getNumWidgets () {
      return leftFloats->size() + rightFloats->size(); }
   inline core::Widget *getWidget (int i) {
      return i < leftFloats->size() ? leftFloats->get(i)->widget :
         rightFloats->get(i - leftFloats->size())->widget; }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
