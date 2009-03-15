#ifndef __CSSPARSER_HH__
#define __CSSPARSER_HH__

#include "css.hh"

typedef struct {
   const char *symbol;
   const CssValueType type[3];
   const char *const *enum_symbols;
} CssPropertyInfo;

extern const CssPropertyInfo Css_property_info[CSS_PROPERTY_LAST];

class CssParser {
   private:
      typedef enum {
         CSS_TK_DECINT, CSS_TK_FLOAT, CSS_TK_COLOR, CSS_TK_SYMBOL, CSS_TK_STRING,
         CSS_TK_CHAR, CSS_TK_END
      } CssTokenType;

      static const int maxStrLen = 256;
      CssContext *context;
      CssOrigin origin;

      const char *buf;
      int buflen, bufptr;

      CssTokenType ttype;
      char tval[maxStrLen];
      bool withinBlock;
      bool spaceSeparated; /* used when parsing CSS selectors */

      CssParser(CssContext *context, CssOrigin origin,
                const char *buf, int buflen);
      int getc();
      void ungetc();
      void nextToken();
      bool tokenMatchesProperty(CssPropertyName prop, CssValueType * type);
      bool parseValue(CssPropertyName prop, CssValueType type,
                      CssPropertyValue * val);
      bool parseWeight();
      void parseDeclaration(CssPropertyList * props,
                            CssPropertyList * importantProps);
      bool parseSimpleSelector(CssSimpleSelector *selector);
      CssSelector *parseSelector();
      void parseRuleset();

   public:
      static CssPropertyList *parseDeclarationBlock(const char *buf, int buflen);
      static void parse(CssContext *context, const char *buf, int buflen,
                        CssOrigin origin);
};

#endif
