#ifndef __STYLEENGINE_HH__
#define __STYLEENGINE_HH__

#include "dw/core.hh"
#include "doctree.hh"
#include "css.hh"

class StyleEngine : public Doctree {
   private:
      class Node : public DoctreeNode {
         public:
            dw::core::style::Style *style;
      };

      lout::misc::SimpleVector <Node> *stack;

      dw::core::style::Style *style0 ();

   public:
      StyleEngine ();
      ~StyleEngine ();
   
      /* Doctree interface */
      inline const DoctreeNode *top () {
         return stack->getRef (stack->size () - 1);
      };

      inline const DoctreeNode *parent (const DoctreeNode *n) {
         if (n->depth > 0)
            return stack->getRef (n->depth - 1);
         else
            return NULL;
      };

      void startElement (int tag, const char *id, const char *klass,
         const char *style);
      void endElement (int tag);
      void setNonCssProperties (CssPropertyList *props);

      inline dw::core::style::Style *style () {
         dw::core::style::Style *s = stack->getRef (stack->size () - 1)->style;
         if (s)
            return s;
         else
            return style0 ();
      };
};

#endif
