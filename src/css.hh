#ifndef __CSS_HH__
#define __CSS_HH__

#include "dw/core.hh"

class CssProperties {
   private:
      dw::core::style::StyleAttrs attrs;
      bool fontValid;
      bool textDecorationValid;
      bool colorValid;
      bool backgroundColorValid;
      bool textAlignValid;
      bool valignValid;
      bool textAlignCharValid;
      bool hBorderSpacingValid;
      bool vBorderSpacingValid;
      bool widthValid;
      bool heightValid;
      bool marginValid;
      bool borderWidthValid;
      bool paddingValid;
      bool borderColorValid;   
      bool borderStyleValid;
      bool displayValid;
      bool whiteSpaceValid;
      bool listStyleTypeValid;
      bool cursorValid;

   public:
      CssProperties ();
      ~CssProperties ();

     void apply (const CssProperties *p); 

};

class CssSelector {
};

class CssRule {
   private:
      CssProperties props;

   public:
      CssRule ();
      ~CssRule ();

};

#endif
