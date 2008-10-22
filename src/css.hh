#ifndef __CSS_HH__
#define __CSS_HH__

#include "dw/core.hh"
#include "html_common.hh"

class StyleEngine {
   private:
      DilloHtml *html;
      dw::core::style::Style *currentStyle;

   public:
      StyleEngine (DilloHtml *html);
      ~StyleEngine ();

      void startElement (const char *name);
      void endElement (const char *name);
      inline dw::core::style::Style *style () { return currentStyle; };
};

#endif
