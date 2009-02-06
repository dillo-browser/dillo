#ifndef __CSSPARSER_HH__
#define __CSSPARSER_HH__

#include "css.hh"

/* The last three ones are never parsed. */
#define CSS_NUM_INTERNAL_PROPERTIES 3
#define CSS_NUM_PARSED_PROPERTIES \
   (CSS_PROPERTY_LAST - CSS_NUM_INTERNAL_PROPERTIES)


typedef enum {
   CSS_TYPE_INTEGER,            /* This type is only used internally, for x-*
                                   properties. */
   CSS_TYPE_ENUM,               /* Value is i, if represented by
                                   enum_symbols[i]. */
   CSS_TYPE_MULTI_ENUM,         /* For all enum_symbols[i], 1 << i are
                                   combined. */
   CSS_TYPE_LENGTH_PERCENTAGE,  /* <length> or <percentage>. Represented by
                                   CssLength. */
   CSS_TYPE_LENGTH,             /* <length>, represented as CssLength.
                                   Note: In some cases, CSS_TYPE_LENGTH is used
                                   instead of CSS_TYPE_LENGTH_PERCENTAGE,
                                   only because Dw cannot handle percentages
                                   in this particular case (e.g.
                                   'margin-*-width'). */
   CSS_TYPE_COLOR,              /* Represented as integer. */
   CSS_TYPE_FONT_WEIGHT,        /* this very special and only used by
                                   'font-weight' */
   CSS_TYPE_STRING,             /* <string> */
   CSS_TYPE_SYMBOL,             /* Symbols, which are directly copied (as
                                   opposed to CSS_TYPE_ENUM and 
                                   CSS_TYPE_MULTI_ENUM). Used for
                                   'font-family'. */
   CSS_TYPE_UNUSED              /* Not yet used. Will itself get unused some
                                   day. */
} CssValueType;

typedef enum {
   CSS_ORIGIN_USER_AGENT,
   CSS_ORIGIN_USER,
   CSS_ORIGIN_AUTHOR,
} CssOrigin;

typedef struct {
   const char *symbol;
   const CssValueType type[2];
   const char *const *enum_symbols;
} CssPropertyInfo;

void        a_Css_parse                       (CssContext *context,
                                               const char *buf,
                                               int buflen,
                                               CssOrigin origin);

CssPropertyList *a_Css_parse_declaration(const char *buf, int buflen);

extern const CssPropertyInfo Css_property_info[CSS_PROPERTY_LAST];

#endif // __CSS_H__
