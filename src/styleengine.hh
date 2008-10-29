#ifndef __STYLEENGINE_HH__
#define __STYLEENGINE_HH__

#include "dw/core.hh"
#include "doctree.hh"

class StyleEngine : public Doctree {
   private:
      class Node : public DoctreeNode {
         public:
            dw::core::style::Style *style;


      };

      lout::misc::SimpleVector <Node> *stack;

   public:
      StyleEngine ();
      ~StyleEngine ();
   
      /* Doctree interface */
      const DoctreeNode *top () {
         return stack->getRef (stack->size () - 1);
      };
      const DoctreeNode *parent (const DoctreeNode *n) {
         if (n->depth > 0)
            return stack->getRef (n->depth - 1);
         else
            return NULL;
      };

      void startElement (int tag, const char *id, const char *klass, const char *style);
      void endElement (int tag);

      inline dw::core::style::Style *style () {
         return stack->getRef (stack->size () - 1)->style;
      };
};

#endif
