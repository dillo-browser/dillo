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
            const char *styleAttribute;
      };

      dw::core::Layout *layout;
      lout::misc::SimpleVector <Node> *stack;
      CssContext *cssContext;

      dw::core::style::Style *style0 (CssPropertyList *nonCssHints = NULL);
      void apply (dw::core::style::StyleAttrs *attrs, CssPropertyList *props);
      int computeValue (CssLength value, dw::core::style::Font *font);

   public:
      StyleEngine (dw::core::Layout *layout);
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

      void startElement (int tag, const char *id = NULL, const char *klass = NULL,
         const char *style = NULL);
      void endElement (int tag);
      void setNonCssHints (CssPropertyList *nonCssHints);
      void setPseudoClass (const char *pseudoClass);

      inline dw::core::style::Style *style () {
         dw::core::style::Style *s = stack->getRef (stack->size () - 1)->style;
         if (s)
            return s;
         else
            return style0 ();
      };
};

#endif
