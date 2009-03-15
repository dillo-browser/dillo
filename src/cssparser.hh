#ifndef __CSSPARSER_HH__
#define __CSSPARSER_HH__

#include "css.hh"

typedef enum {
   CSS_ORIGIN_USER_AGENT,
   CSS_ORIGIN_USER,
   CSS_ORIGIN_AUTHOR,
} CssOrigin;

typedef struct {
   const char *symbol;
   const CssValueType type[3];
   const char *const *enum_symbols;
} CssPropertyInfo;

void        a_Css_parse                       (CssContext *context,
                                               const char *buf,
                                               int buflen,
                                               CssOrigin origin);

CssPropertyList *a_Css_parse_declaration(const char *buf, int buflen);

extern const CssPropertyInfo Css_property_info[CSS_PROPERTY_LAST];

#endif // __CSS_H__
