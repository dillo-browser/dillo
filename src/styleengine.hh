#ifndef __STYLEENGINE_HH__
#define __STYLEENGINE_HH__

class StyleEngine;

#include "dw/core.hh"
#include "doctree.hh"
#include "css.hh"
#include "cssparser.hh"

/**
 * \brief This class provides the glue between HTML parser and CSS subsystem.
 *
 * It maintains a document tree and creates and caches style objects for use
 * by the HTML parser.
 * The HTML parser in turn informs StyleEngine about opened or closed
 * HTML elements and their attributes via the startElement() / endElement()
 * methods.
 */
class StyleEngine : public Doctree {
   private:
      class Node : public DoctreeNode {
         public:
            dw::core::style::Style *style;
            dw::core::style::Style *wordStyle;
            const char *styleAttribute;
            bool inheritBackgroundColor;
      };

      dw::core::Layout *layout;
      lout::misc::SimpleVector <Node> *stack;
      CssContext *cssContext;
      int num;
      int importDepth;

      dw::core::style::Style *style0 (CssPropertyList *nonCssHints = NULL);
      dw::core::style::Style *wordStyle0 (CssPropertyList *nonCssHints = NULL);
      void apply (dw::core::style::StyleAttrs *attrs, CssPropertyList *props);
      bool computeValue (int *dest, CssLength value,
                         dw::core::style::Font *font);
      bool computeValue (int *dest, CssLength value,
                         dw::core::style::Font *font, int percentageBase);
      bool computeLength (dw::core::style::Length *dest, CssLength value,
                          dw::core::style::Font *font);

   public:
      StyleEngine (dw::core::Layout *layout);
      ~StyleEngine ();

      /* Doctree interface */
      inline const DoctreeNode *top () {
         return stack->getRef (stack->size () - 1);
      };

      inline const DoctreeNode *parent (const DoctreeNode *n) {
         if (n->depth > 1)
            return stack->getRef (n->depth - 1);
         else
            return NULL;
      };

      void parse (DilloHtml *html, DilloUrl *url, const char *buf, int buflen,
                  CssOrigin origin);
      void startElement (int tag);
      void startElement (const char *tagname);
      void setId (const char *id);
      const char * getId () { return top ()->id; };
      void setClass (const char *klass);
      void setStyle (const char *style);
      void endElement (int tag);
      void setPseudoLink ();
      void setPseudoVisited ();
      void setNonCssHints (CssPropertyList *nonCssHints);
      void inheritBackgroundColor (); /* \todo get rid of this somehow */
      dw::core::style::Style *backgroundStyle ();

      inline dw::core::style::Style *style () {
         dw::core::style::Style *s = stack->getRef (stack->size () - 1)->style;
         if (s)
            return s;
         else
            return style0 ();
      };

      inline dw::core::style::Style *wordStyle () {
         dw::core::style::Style *s = stack->getRef(stack->size()-1)->wordStyle;
         if (s)
            return s;
         else
            return wordStyle0 ();
      };
};

#endif
