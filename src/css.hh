#ifndef __CSS_HH__
#define __CSS_HH__

#include "dw/core.hh"

class StyleEngine {
   private:
      dw::core::style::Style *currentStyle;

   public:
      StyleEngine ();
      ~StyleEngine ();

      void startElement (int tag, const char *id, const char *style);
      void endElement (int tag);
      inline dw::core::style::Style *style () { return currentStyle; };
};

#endif
