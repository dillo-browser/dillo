#ifndef __CSSPARSER_HH__
#define __CSSPARSER_HH__

/* The last three ones are never parsed. */
#define CSS_NUM_INTERNAL_PROPERTIES 3
#define CSS_NUM_PARSED_PROPERTIES \
   (CssProperty::CSS_PROPERTY_LAST - CSS_NUM_INTERNAL_PROPERTIES)


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

/* Origin and weight. Used only internally.*/
typedef enum {
   CSS_PRIMARY_USER_IMPORTANT,
   CSS_PRIMARY_AUTHOR_IMPORTANT,
   CSS_PRIMARY_AUTHOR,
   CSS_PRIMARY_USER,
   CSS_PRIMARY_USER_AGENT,
   CSS_PRIMARY_LAST,
} CssPrimaryOrder;

typedef struct {
   char *symbol;
   CssValueType type;
   char **enum_symbols;
} CssPropertyInfo;


/*
 * Lengths are represented as int in the following way:
 * 
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *    | integer part  | decimal fraction  |   type    |
 *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
 *     n-1          19  18              3   2  1   0
 *
 *    | <------ fixed point value ------> |
 *
 * where type is one of the CSS_LENGTH_TYPE_* values.
 */

typedef int CssLength;

enum {
   CSS_LENGTH_TYPE_PX,
   CSS_LENGTH_TYPE_MM,         /* "cm", "in", "pt" and "pc" are converted into
                                  millimeters. */
   CSS_LENGTH_TYPE_EM,
   CSS_LENGTH_TYPE_EX,
   CSS_LENGTH_TYPE_PERCENTAGE,
   CSS_LENGTH_TYPE_AUTO        /* This can be used as a simple value. */
};

#define CSS_CREATE_LENGTH(v, t) ( ( (int)((v) * (1 << 19)) & ~7 ) | (t) )
#define CSS_LENGTH_VALUE(l)     ( ( (float)((l) & ~7) ) / (1 << 19) )
#define CSS_LENGTH_TYPE(l)      ((l) & 7)

/*
 * Font weights are either absolute (number between 100 and 900), or have
 * the following relative values:
 */
#define CSS_FONT_WEIGHT_LIGHTER -1
#define CSS_FONT_WEIGHT_BOLDER  -2

/* This is the difference used. */
#define CSS_FONT_WEIGHT_STEP 300

/* Some special font weights. */
#define CSS_FONT_WEIGHT_LIGHT  100
#define CSS_FONT_WEIGHT_NORMAL 400
#define CSS_FONT_WEIGHT_BOLD   700

#define CSS_FONT_WEIGHT_MIN    100
#define CSS_FONT_WEIGHT_MAX    900


void        a_Css_init                        (void);
void        a_Css_freeall                     (void);

void        a_Css_parse                       (CssContext *context,
                                               const char *buf,
                                               int buflen,
                                               int order_count,
                                               CssOrigin origin);

void        p_Css_parse_element_style         (CssContext *context,
                                               char *id,
                                               char *klass,
                                               char *pseudo,
                                               char *element,
                                               const char *buf,
                                               int buflen,
                                               int order_count,
                                               CssOrigin origin);

extern CssPropertyInfo Css_property_info[CssProperty::CSS_PROPERTY_LAST];

#endif // __CSS_H__
