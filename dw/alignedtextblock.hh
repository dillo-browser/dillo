#ifndef __DW_ALIGNEDTEXTBLOCK_HH__
#define __DW_ALIGNEDTEXTBLOCK_HH__

#include "core.hh"
#include "textblock.hh"

namespace dw {

/**
 * \brief Base widget for all textblocks (sub classes of dw::Textblock), which
 *    are positioned vertically and aligned horizontally.
 */
class AlignedTextblock: public Textblock
{
private:
   class List
   {
   private:
      lout::misc::SimpleVector <AlignedTextblock*> *textblocks;
      lout::misc::SimpleVector <int> *values;
      int maxValue, refCount;

      ~List ();

   public:
      List ();
      inline int add (AlignedTextblock *textblock);
      void unref (int pos);

      inline int getMaxValue () { return maxValue; }
      inline void setMaxValue (int maxValue) { this->maxValue = maxValue; }

      inline int size () { return textblocks->size (); }
      inline AlignedTextblock *getTextblock (int pos) {
         return textblocks->get (pos); }
      inline int getValue (int pos) {return values->get (pos); }
      inline void setValue (int pos, int value) {
         return values->set (pos, value); }
   };

   List *list;
   int listPos;

protected:
   AlignedTextblock(bool limitTextWidth);

   virtual int getValue () = 0;
   virtual void setMaxValue (int maxValue, int value) = 0;

   void setRefTextblock (AlignedTextblock *ref);
   void updateValue ();

public:
   static int CLASS_ID;

   ~AlignedTextblock();
};

} // namespace dw

#endif // __DW_ALIGNEDTEXTBLOCK_HH__
