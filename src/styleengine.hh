#ifndef __STYLEENGINE_HH__
#define __STYLEENGINE_HH__

#include "dw/core.hh"
#include "doctree.hh"

class StyleEngine {
   private:
      dw::core::style::Style *currentStyle;

   public:
      StyleEngine ();
      ~StyleEngine ();

      void startElement (int tag, const char *id, const char *klass, const char *style);
      void endElement (int tag);
      inline dw::core::style::Style *style () { return currentStyle; };
};

#endif
