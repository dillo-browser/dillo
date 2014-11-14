#ifndef __DW_SIMPLE_CONTAINER_HH__
#define __DW_SIMPLE_CONTAINER_HH__

#include "dw/core.hh"

namespace dw {

/**
 * Simple widget used for testing concepts.
 */
class SimpleContainer: public core::Widget
{
private:
   class SimpleContainerIterator: public core::Iterator
   {
   private:
      int index ();

   public:
      SimpleContainerIterator (SimpleContainer *simpleContainer,
                               core::Content::Type mask,
                      bool atEnd);

      lout::object::Object *clone ();
      int compareTo (lout::object::Comparable *other);

      bool next ();
      bool prev ();
      void highlight (int start, int end, core::HighlightLayer layer);
      void unhighlight (int direction, core::HighlightLayer layer);
      void getAllocation (int start, int end, core::Allocation *allocation);
   };

   Widget *child;

protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void getExtremesImpl (core::Extremes *extremes);
   void sizeAllocateImpl (core::Allocation *allocation);

public:
   static int CLASS_ID;

   SimpleContainer ();
   ~SimpleContainer ();

   void draw (core::View *view, core::Rectangle *area);
   core::Iterator *iterator (core::Content::Type mask, bool atEnd);
   void removeChild (Widget *child);

   void setChild (core::Widget *child);
};

} // namespace dw

#endif // __DW_SIMPLE_CONTAINER_HH__
