/*
 * File: cssparser.cc
 *
 * Copyright 2004 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2008-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * This file is heavily based on the CSS parser of dillo-0.8.0-css-3 -
 * a dillo1 based CSS prototype written by Sebastian Geerken.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "lout/debug.hh"
#include "msg.h"
#include "colors.h"
#include "html_common.hh"
#include "css.hh"
#include "cssparser.hh"

using namespace dw::core::style;

#define MSG_CSS(A, ...) MSG(A, __VA_ARGS__)
#define DEBUG_TOKEN_LEVEL   0
#define DEBUG_PARSE_LEVEL   0
#define DEBUG_CREATE_LEVEL  0

#define DEBUG_LEVEL 10

/* The last three ones are never parsed. */
#define CSS_NUM_INTERNAL_PROPERTIES 3
#define CSS_NUM_PARSED_PROPERTIES \
   (CSS_PROPERTY_LAST - CSS_NUM_INTERNAL_PROPERTIES)


typedef struct {
   const char *symbol;
   const CssValueType type[3];
   const char *const *enum_symbols;
} CssPropertyInfo;

static const char *const Css_background_attachment_enum_vals[] = {
   "scroll", "fixed", NULL
};

static const char *const Css_background_repeat_enum_vals[] = {
   "repeat", "repeat-x", "repeat-y", "no-repeat", NULL
};

static const char *const Css_border_collapse_enum_vals[] = {
   "separate", "collapse", NULL
};

static const char *const Css_border_color_enum_vals[] = {
   "transparent", NULL
};

static const char *const Css_border_style_enum_vals[] = {
   "none", "hidden", "dotted", "dashed", "solid", "double", "groove",
   "ridge", "inset", "outset", NULL
};

static const char *const Css_border_width_enum_vals[] = {
   "thin", "medium", "thick", NULL
};

static const char *const Css_cursor_enum_vals[] = {
   "crosshair", "default", "pointer", "move", "e-resize", "ne-resize",
   "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize",
   "w-resize", "text", "wait", "help", NULL
};

static const char *const Css_display_enum_vals[] = {
   "block", "inline", "inline-block", "list-item", "none", "table",
   "table-row-group", "table-header-group", "table-footer-group", "table-row",
   "table-cell", NULL
};

static const char *const Css_font_size_enum_vals[] = {
   "large", "larger", "medium", "small", "smaller", "xx-large", "xx-small",
   "x-large", "x-small", NULL
};

static const char *const Css_font_style_enum_vals[] = {
   "normal", "italic", "oblique", NULL
};

static const char *const Css_font_variant_enum_vals[] = {
   "normal", "small-caps", NULL
};

static const char *const Css_font_weight_enum_vals[] = {
   "bold", "bolder", "light", "lighter", "normal", NULL
};

static const char *const Css_letter_spacing_enum_vals[] = {
   "normal", NULL
};

static const char *const Css_list_style_position_enum_vals[] = {
   "inside", "outside", NULL
};

static const char *const Css_line_height_enum_vals[] = {
   "normal", NULL
};

static const char *const Css_list_style_type_enum_vals[] = {
   "disc", "circle", "square", "decimal", "decimal-leading-zero",
   "lower-roman", "upper-roman", "lower-greek", "lower-alpha",
   "lower-latin", "upper-alpha", "upper-latin", "hebrew", "armenian",
   "georgian", "cjk-ideographic", "hiragana", "katakana", "hiragana-iroha",
   "katakana-iroha", "none", NULL
};

static const char *const Css_text_align_enum_vals[] = {
   "left", "right", "center", "justify", "string", NULL
};

static const char *const Css_text_decoration_enum_vals[] = {
   "underline", "overline", "line-through", "blink", NULL
};

static const char *const Css_text_transform_enum_vals[] = {
   "none", "capitalize", "uppercase", "lowercase", NULL
};

static const char *const Css_vertical_align_vals[] = {
   "top", "bottom", "middle", "baseline", "sub", "super", "text-top",
   "text-bottom", NULL
};

static const char *const Css_white_space_vals[] = {
   "normal", "pre", "nowrap", "pre-wrap", "pre-line", NULL
};

static const char *const Css_word_spacing_enum_vals[] = {
   "normal", NULL
};

const CssPropertyInfo Css_property_info[CSS_PROPERTY_LAST] = {
   {"background-attachment", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_background_attachment_enum_vals},
   {"background-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"background-image", {CSS_TYPE_URI, CSS_TYPE_UNUSED}, NULL},
   {"background-position", {CSS_TYPE_BACKGROUND_POSITION, CSS_TYPE_UNUSED},
    NULL},
   {"background-repeat", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_background_repeat_enum_vals},
   {"border-bottom-color", {CSS_TYPE_ENUM, CSS_TYPE_COLOR, CSS_TYPE_UNUSED},
    Css_border_color_enum_vals},
   {"border-bottom-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_border_style_enum_vals},
   {"border-bottom-width", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH, CSS_TYPE_UNUSED},
    Css_border_width_enum_vals},
   {"border-collapse", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_border_collapse_enum_vals},
   {"border-left-color", {CSS_TYPE_ENUM, CSS_TYPE_COLOR, CSS_TYPE_UNUSED},
    Css_border_color_enum_vals},
   {"border-left-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_border_style_enum_vals},
   {"border-left-width", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH, CSS_TYPE_UNUSED},
    Css_border_width_enum_vals},
   {"border-right-color", {CSS_TYPE_ENUM, CSS_TYPE_COLOR, CSS_TYPE_UNUSED},
    Css_border_color_enum_vals},
   {"border-right-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_border_style_enum_vals},
   {"border-rigth-width", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH, CSS_TYPE_UNUSED},
    Css_border_width_enum_vals},
   {"border-spacing", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"border-top-color", {CSS_TYPE_ENUM, CSS_TYPE_COLOR, CSS_TYPE_UNUSED},
    Css_border_color_enum_vals},
   {"border-top-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_border_style_enum_vals},
   {"border-top-width", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH, CSS_TYPE_UNUSED},
    Css_border_width_enum_vals},
   {"bottom", {CSS_TYPE_UNUSED}, NULL},
   {"caption-side", {CSS_TYPE_UNUSED}, NULL},
   {"clear", {CSS_TYPE_UNUSED}, NULL},
   {"clip", {CSS_TYPE_UNUSED}, NULL},
   {"color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"content", {CSS_TYPE_STRING, CSS_TYPE_UNUSED}, NULL},
   {"counter-increment", {CSS_TYPE_UNUSED}, NULL},
   {"counter-reset", {CSS_TYPE_UNUSED}, NULL},
   {"cursor", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_cursor_enum_vals},
   {"direction", {CSS_TYPE_UNUSED}, NULL},
   {"display", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_display_enum_vals},
   {"empty-cells", {CSS_TYPE_UNUSED}, NULL},
   {"float", {CSS_TYPE_UNUSED}, NULL},
   {"font-family", {CSS_TYPE_SYMBOL, CSS_TYPE_UNUSED}, NULL},
   {"font-size", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_UNUSED},
    Css_font_size_enum_vals},
   {"font-size-adjust", {CSS_TYPE_UNUSED}, NULL},
   {"font-stretch", {CSS_TYPE_UNUSED}, NULL},
   {"font-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_font_style_enum_vals},
   {"font-variant", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_font_variant_enum_vals},
   {"font-weight", {CSS_TYPE_ENUM, CSS_TYPE_FONT_WEIGHT, CSS_TYPE_UNUSED},
    Css_font_weight_enum_vals},
   {"height", {CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"left", {CSS_TYPE_UNUSED}, NULL},
   {"letter-spacing", {CSS_TYPE_ENUM, CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_UNUSED},
    Css_letter_spacing_enum_vals},
   {"line-height",
    {CSS_TYPE_ENUM, CSS_TYPE_LENGTH_PERCENTAGE_NUMBER, CSS_TYPE_UNUSED},
    Css_line_height_enum_vals},
   {"list-style-image", {CSS_TYPE_UNUSED}, NULL},
   {"list-style-position", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_list_style_position_enum_vals},
   {"list-style-type", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_list_style_type_enum_vals},
   {"margin-bottom",
    {CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"margin-left",
    {CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"margin-right",
    {CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"margin-top",
    {CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"marker-offset", {CSS_TYPE_UNUSED}, NULL},
   {"marks", {CSS_TYPE_UNUSED}, NULL},
   {"max-height", {CSS_TYPE_UNUSED}, NULL},
   {"max-width", {CSS_TYPE_UNUSED}, NULL},
   {"min-height", {CSS_TYPE_UNUSED}, NULL},
   {"min-width", {CSS_TYPE_UNUSED}, NULL},
   {"outline-color", {CSS_TYPE_UNUSED}, NULL},
   {"outline-style", {CSS_TYPE_UNUSED}, NULL},
   {"outline-width", {CSS_TYPE_UNUSED}, NULL},
   {"overflow", {CSS_TYPE_UNUSED}, NULL},
   {"padding-bottom", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"padding-left", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"padding-right", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"padding-top", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"position", {CSS_TYPE_UNUSED}, NULL},
   {"quotes", {CSS_TYPE_UNUSED}, NULL},
   {"right", {CSS_TYPE_UNUSED}, NULL},
   {"text-align", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_text_align_enum_vals},
   {"text-decoration", {CSS_TYPE_MULTI_ENUM, CSS_TYPE_UNUSED},
    Css_text_decoration_enum_vals},
   {"text-indent", {CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_UNUSED}, NULL},
   {"text-shadow", {CSS_TYPE_UNUSED}, NULL},
   {"text-transform", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED},
    Css_text_transform_enum_vals},
   {"top", {CSS_TYPE_UNUSED}, NULL},
   {"unicode-bidi", {CSS_TYPE_UNUSED}, NULL},
   {"vertical-align",{CSS_TYPE_ENUM, CSS_TYPE_UNUSED},Css_vertical_align_vals},
   {"visibility", {CSS_TYPE_UNUSED}, NULL},
   {"white-space", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_white_space_vals},
   {"width", {CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_AUTO, CSS_TYPE_UNUSED}, NULL},
   {"word-spacing", {CSS_TYPE_ENUM, CSS_TYPE_SIGNED_LENGTH, CSS_TYPE_UNUSED},
    Css_word_spacing_enum_vals},
   {"z-index", {CSS_TYPE_UNUSED}, NULL},

   /* These are extensions, for internal used, and never parsed. */
   {"x-link", {CSS_TYPE_INTEGER, CSS_TYPE_UNUSED}, NULL},
   {"x-colspan", {CSS_TYPE_INTEGER, CSS_TYPE_UNUSED}, NULL},
   {"x-rowspan", {CSS_TYPE_INTEGER, CSS_TYPE_UNUSED}, NULL},
   {"last", {CSS_TYPE_UNUSED}, NULL},
};

typedef struct {
   const char *symbol;
   enum {
      CSS_SHORTHAND_MULTIPLE,   /* [ p1 || p2 || ...], the property pi is
                                 * determined  by the type */
      CSS_SHORTHAND_DIRECTIONS, /* <t>{1,4} */
      CSS_SHORTHAND_BORDER,     /* special, used for 'border' */
      CSS_SHORTHAND_FONT,       /* special, used for 'font' */
   } type;
   const CssPropertyName *properties; /* CSS_SHORTHAND_MULTIPLE:
                                       *   must be terminated by
                                       *   CSS_PROPERTY_END 
                                       * CSS_SHORTHAND_DIRECTIONS:
                                       *   must have length 4
                                       * CSS_SHORTHAND_BORDERS:
                                       *   must have length 12
                                       * CSS_SHORTHAND_FONT:
                                       *   unused */
} CssShorthandInfo;

const CssPropertyName Css_background_properties[] = {
   CSS_PROPERTY_BACKGROUND_COLOR,
   CSS_PROPERTY_BACKGROUND_IMAGE,
   CSS_PROPERTY_BACKGROUND_REPEAT,
   CSS_PROPERTY_BACKGROUND_ATTACHMENT,
   CSS_PROPERTY_BACKGROUND_POSITION,
   CSS_PROPERTY_END
};

const CssPropertyName Css_border_bottom_properties[] = {
   CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
   CSS_PROPERTY_BORDER_BOTTOM_STYLE,
   CSS_PROPERTY_BORDER_BOTTOM_COLOR,
   CSS_PROPERTY_END
};

const CssPropertyName Css_border_color_properties[4] = {
   CSS_PROPERTY_BORDER_TOP_COLOR,
   CSS_PROPERTY_BORDER_BOTTOM_COLOR,
   CSS_PROPERTY_BORDER_LEFT_COLOR,
   CSS_PROPERTY_BORDER_RIGHT_COLOR
};

const CssPropertyName Css_border_left_properties[] = {
   CSS_PROPERTY_BORDER_LEFT_WIDTH,
   CSS_PROPERTY_BORDER_LEFT_STYLE,
   CSS_PROPERTY_BORDER_LEFT_COLOR,
   CSS_PROPERTY_END
};

const CssPropertyName Css_border_right_properties[] = {
   CSS_PROPERTY_BORDER_RIGHT_WIDTH,
   CSS_PROPERTY_BORDER_RIGHT_STYLE,
   CSS_PROPERTY_BORDER_RIGHT_COLOR,
   CSS_PROPERTY_END
};

const CssPropertyName Css_border_style_properties[] = {
   CSS_PROPERTY_BORDER_TOP_STYLE,
   CSS_PROPERTY_BORDER_BOTTOM_STYLE,
   CSS_PROPERTY_BORDER_LEFT_STYLE,
   CSS_PROPERTY_BORDER_RIGHT_STYLE
};

const CssPropertyName Css_border_top_properties[] = {
   CSS_PROPERTY_BORDER_TOP_WIDTH,
   CSS_PROPERTY_BORDER_TOP_STYLE,
   CSS_PROPERTY_BORDER_TOP_COLOR,
   CSS_PROPERTY_END
};

const CssPropertyName Css_border_width_properties[] = {
   CSS_PROPERTY_BORDER_TOP_WIDTH,
   CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
   CSS_PROPERTY_BORDER_LEFT_WIDTH,
   CSS_PROPERTY_BORDER_RIGHT_WIDTH
};

const CssPropertyName Css_list_style_properties[] = {
   CSS_PROPERTY_LIST_STYLE_TYPE,
   CSS_PROPERTY_LIST_STYLE_POSITION,
   CSS_PROPERTY_LIST_STYLE_IMAGE,
   CSS_PROPERTY_END
};

const CssPropertyName Css_margin_properties[] = {
   CSS_PROPERTY_MARGIN_TOP,
   CSS_PROPERTY_MARGIN_BOTTOM,
   CSS_PROPERTY_MARGIN_LEFT,
   CSS_PROPERTY_MARGIN_RIGHT
};

const CssPropertyName Css_outline_properties[] = {
   CSS_PROPERTY_OUTLINE_COLOR,
   CSS_PROPERTY_OUTLINE_STYLE,
   CSS_PROPERTY_OUTLINE_WIDTH,
   CSS_PROPERTY_END
};

const CssPropertyName Css_padding_properties[] = {
   CSS_PROPERTY_PADDING_TOP,
   CSS_PROPERTY_PADDING_BOTTOM,
   CSS_PROPERTY_PADDING_LEFT,
   CSS_PROPERTY_PADDING_RIGHT
};

const CssPropertyName Css_border_properties[] = {
   CSS_PROPERTY_BORDER_TOP_WIDTH,
   CSS_PROPERTY_BORDER_TOP_STYLE,
   CSS_PROPERTY_BORDER_TOP_COLOR,
   CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
   CSS_PROPERTY_BORDER_BOTTOM_STYLE,
   CSS_PROPERTY_BORDER_BOTTOM_COLOR,
   CSS_PROPERTY_BORDER_LEFT_WIDTH,
   CSS_PROPERTY_BORDER_LEFT_STYLE,
   CSS_PROPERTY_BORDER_LEFT_COLOR,
   CSS_PROPERTY_BORDER_RIGHT_WIDTH,
   CSS_PROPERTY_BORDER_RIGHT_STYLE,
   CSS_PROPERTY_BORDER_RIGHT_COLOR
};

const CssPropertyName Css_font_properties[] = {
   CSS_PROPERTY_FONT_SIZE,
   CSS_PROPERTY_FONT_STYLE,
   CSS_PROPERTY_FONT_VARIANT,
   CSS_PROPERTY_FONT_WEIGHT,
   CSS_PROPERTY_FONT_FAMILY,
   CSS_PROPERTY_END
};

static const CssShorthandInfo Css_shorthand_info[] = {
   {"background", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_background_properties},
   {"border", CssShorthandInfo::CSS_SHORTHAND_BORDER,
    Css_border_properties},
   {"border-bottom", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_border_bottom_properties},
   {"border-color", CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS,
    Css_border_color_properties},
   {"border-left", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_border_left_properties},
   {"border-right", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_border_right_properties},
   {"border-style", CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS,
    Css_border_style_properties},
   {"border-top", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_border_top_properties},
   {"border-width", CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS,
    Css_border_width_properties},
   {"font", CssShorthandInfo::CSS_SHORTHAND_FONT,
    Css_font_properties},
   {"list-style", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_list_style_properties},
   {"margin", CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS,
    Css_margin_properties},
   {"outline", CssShorthandInfo::CSS_SHORTHAND_MULTIPLE,
    Css_outline_properties},
   {"padding", CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS,
    Css_padding_properties},
};

#define CSS_SHORTHAND_NUM \
   (sizeof(Css_shorthand_info) / sizeof(Css_shorthand_info[0]))

/* ----------------------------------------------------------------------
 *    Parsing
 * ---------------------------------------------------------------------- */

CssParser::CssParser(CssContext *context, CssOrigin origin,
                     const DilloUrl *baseUrl,
                     const char *buf, int buflen)
{
   this->context = context;
   this->origin = origin;
   this->buf = buf;
   this->buflen = buflen;
   this->bufptr = 0;
   this->spaceSeparated = false;
   this->withinBlock = false;
   this->baseUrl = baseUrl;

   nextToken ();
}

/*
 * Gets the next character from the buffer, or EOF.
 */
int CssParser::getChar()
{
   int c;

   if (bufptr >= buflen)
      c = EOF;
   else
      c = buf[bufptr];

   /* The buffer pointer is increased in any case, so that ungetChar works
    * correctly at the end of the buffer. */
   bufptr++;
   return c;
}

/*
 * Undoes the last getChar().
 */
void CssParser::ungetChar()
{
   bufptr--;
}

/*
 * Skip string str if it is found in the input buffer.
 * If string is found leave bufptr pointing to last matched char.
 * If not wind back. The first char is passed as parameter c
 * to avoid unnecessary getChar() / ungetChar() calls.
 */
inline bool CssParser::skipString(int c, const char *str)
{
   for (int n = 0; str[n]; n++) {
      if (n > 0)
         c = getChar();

      if (str[n] != c) {
         while (n--)
            ungetChar();
         return false;
      }
   }

   return true;
}

void CssParser::nextToken()
{
   int c, c1, d, j;
   char hexbuf[5];
   int i = 0;

   ttype = CSS_TK_CHAR; /* init */
   spaceSeparated = false;

   while (true) {
      c = getChar();
      if (isspace(c)) {                    // ignore whitespace
         spaceSeparated = true;
      } else if (skipString(c, "/*")) {    // ignore comments
         do {
            c = getChar();
         } while (c != EOF && ! skipString(c, "*/"));
      } else if (skipString(c, "<!--")) {  // ignore XML comment markers
      } else if (skipString(c, "-->")) {
      } else {
         break;
      }
   }

   // handle negative numbers
   if (c == '-') {
      if (i < maxStrLen - 1)
         tval[i++] = c;
      c = getChar();
   }

   if (isdigit(c)) {
      ttype = CSS_TK_DECINT;
      do {
         if (i < maxStrLen - 1) {
            tval[i++] = c;
         }
         /* else silently truncated */
         c = getChar();
      } while (isdigit(c));
      if (c != '.')
         ungetChar();

      /* ...but keep going to see whether it's really a float */
   }

   if (c == '.') {
      c = getChar();
      if (isdigit(c)) {
         ttype = CSS_TK_FLOAT;
         if (i < maxStrLen - 1)
            tval[i++] = '.';
         do {
            if (i < maxStrLen - 1)
               tval[i++] = c;
            /* else silently truncated */
            c = getChar();
         } while (isdigit(c));

         ungetChar();
         tval[i] = 0;
         DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token number %s\n", tval);
         return;
      } else {
         ungetChar();
         if (ttype == CSS_TK_DECINT) {
            ungetChar();
         } else {
            c = '.';
         }
      }
   }

   if (ttype == CSS_TK_DECINT) {
      tval[i] = 0;
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token number %s\n", tval);
      return;
   }

   if (i) {
      ungetChar(); /* ungetChar '-' */
      i--;
      c = getChar();
   }

   if (isalpha(c) || c == '_' || c == '-') {
      ttype = CSS_TK_SYMBOL;

      tval[0] = c;
      i = 1;
      c = getChar();
      while (isalnum(c) || c == '_' || c == '-') {
         if (i < maxStrLen - 1) {
            tval[i] = c;
            i++;
         }                      /* else silently truncated */
         c = getChar();
      }
      tval[i] = 0;
      ungetChar();
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token symbol '%s'\n", tval);
      return;
   }

   if (c == '"' || c == '\'') {
      c1 = c;
      ttype = CSS_TK_STRING;

      i = 0;
      c = getChar();

      while (c != EOF && c != c1) {
         if (c == '\\') {
            d = getChar();
            if (isxdigit(d)) {
               /* Read hex Unicode char. (Actually, strings are yet only 8
                * bit.) */
               hexbuf[0] = d;
               j = 1;
               d = getChar();
               while (j < 4 && isxdigit(d)) {
                  hexbuf[j] = d;
                  j++;
                  d = getChar();
               }
               hexbuf[j] = 0;
               ungetChar();
               c = strtol(hexbuf, NULL, 16);
            } else {
               /* Take character literally. */
               c = d;
            }
         }

         if (i < maxStrLen - 1) {
            tval[i] = c;
            i++;
         }                      /* else silently truncated */
         c = getChar();
      }
      tval[i] = 0;
      /* No ungetChar(). */
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token string '%s'\n", tval);
      return;
   }

   /*
    * Within blocks, '#' starts a color, outside, it is used in selectors.
    */
   if (c == '#' && withinBlock) {
      ttype = CSS_TK_COLOR;

      tval[0] = c;
      i = 1;
      c = getChar();
      while (isxdigit(c)) {
         if (i < maxStrLen - 1) {
            tval[i] = c;
            i++;
         }                      /* else silently truncated */
         c = getChar();
      }
      tval[i] = 0;
      ungetChar();
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token color '%s'\n", tval);
      return;
   }

   if (c == EOF) {
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token %s\n", "EOF");
      ttype = CSS_TK_END;
      return;
   }

   ttype = CSS_TK_CHAR;
   tval[0] = c;
   tval[1] = 0;
   DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token char '%c'\n", c);
}


bool CssParser::tokenMatchesProperty(CssPropertyName prop, CssValueType *type)
{
   int i, err = 1;
   CssValueType savedType = *type;

   for (int j = 0; Css_property_info[prop].type[j] != CSS_TYPE_UNUSED; j++) {
      *type = Css_property_info[prop].type[j];

      switch (Css_property_info[prop].type[j]) {

      case CSS_TYPE_ENUM:
         if (ttype == CSS_TK_SYMBOL) {
            for (i = 0; Css_property_info[prop].enum_symbols[i]; i++)
               if (dStrAsciiCasecmp(tval,
                     Css_property_info[prop].enum_symbols[i]) == 0)
                  return true;
         }
         break;

      case CSS_TYPE_MULTI_ENUM:
         if (ttype == CSS_TK_SYMBOL) {
            if (dStrAsciiCasecmp(tval, "none") == 0) {
               return true;
            } else {
               for (i = 0; Css_property_info[prop].enum_symbols[i]; i++) {
                  if (dStrAsciiCasecmp(tval,
                        Css_property_info[prop].enum_symbols[i]) == 0)
                     return true;
               }
            }
         }
         break;

      case CSS_TYPE_BACKGROUND_POSITION:
         if (ttype == CSS_TK_SYMBOL &&
             (dStrAsciiCasecmp(tval, "center") == 0 ||
              dStrAsciiCasecmp(tval, "left") == 0 ||
              dStrAsciiCasecmp(tval, "right") == 0 ||
              dStrAsciiCasecmp(tval, "top") == 0 ||
              dStrAsciiCasecmp(tval, "bottom") == 0))
            return true;
         // Fall Through (lenght and percentage)
      case CSS_TYPE_LENGTH_PERCENTAGE:
      case CSS_TYPE_LENGTH_PERCENTAGE_NUMBER:
      case CSS_TYPE_LENGTH:
         if (tval[0] == '-')
            return false;
         // Fall Through
      case CSS_TYPE_SIGNED_LENGTH:
         if (ttype == CSS_TK_DECINT || ttype == CSS_TK_FLOAT)
            return true;
         break;

      case CSS_TYPE_AUTO:
         if (ttype == CSS_TK_SYMBOL && dStrAsciiCasecmp(tval, "auto") == 0)
            return true;
         break;

      case CSS_TYPE_COLOR:
         if ((ttype == CSS_TK_COLOR ||
              ttype == CSS_TK_SYMBOL) &&
            (dStrAsciiCasecmp(tval, "rgb") == 0 ||
             a_Color_parse(tval, -1, &err) != -1))
            return true;
         break;

      case CSS_TYPE_STRING:
         if (ttype == CSS_TK_STRING)
            return true;
         break;

      case CSS_TYPE_SYMBOL:
         if (ttype == CSS_TK_SYMBOL ||
             ttype == CSS_TK_STRING)
            return true;
         break;

      case CSS_TYPE_FONT_WEIGHT:
         if (ttype == CSS_TK_DECINT) {
            i = strtol(tval, NULL, 10);
            if (i >= 100 && i <= 900)
               return true;
         }
         break;

      case CSS_TYPE_URI:
         if (ttype == CSS_TK_SYMBOL &&
             dStrAsciiCasecmp(tval, "url") == 0)
            return true;
         break;

      case CSS_TYPE_UNUSED:
      case CSS_TYPE_INTEGER:
         /* Not used for parser values. */
      default:
         assert(false);
         break;
      }
   }

   *type = savedType;
   return false;
}

bool CssParser::parseRgbColorComponent(int32_t *cc, int *percentage) {
   if (ttype != CSS_TK_DECINT) {
      MSG_CSS("expected integer not found in %s color\n", "rgb");
      return false;
   }

   *cc = strtol(tval, NULL, 10);

   nextToken();
   if (ttype == CSS_TK_CHAR && tval[0] == '%') {
      if (*percentage == 0) {
         MSG_CSS("'%s' unexpected in rgb color\n", "%");
         return false;
      }
      *percentage = 1;
      *cc = *cc * 255 / 100;
      nextToken();
   } else {
      if (*percentage == 1) {
         MSG_CSS("expected '%s' not found in rgb color\n", "%");
         return false;
      }
      *percentage = 0;
   }

   if (*cc > 255)
      *cc = 255;
   if (*cc < 0)
      *cc = 0;

   return true;
}

bool CssParser::parseRgbColor(int32_t *c) {
   int32_t cc;
   int percentage = -1;

   *c = 0;

   if (ttype != CSS_TK_CHAR || tval[0] != '(') {
      MSG_CSS("expected '%s' not found in rgb color\n", "(");
      return false;
   }
   nextToken();

   if (!parseRgbColorComponent(&cc, &percentage))
      return false;
   *c |= cc << 16;

   if (ttype != CSS_TK_CHAR || tval[0] != ',') {
      MSG_CSS("expected '%s' not found in rgb color\n", ",");
      return false;
   }
   nextToken();

   if (!parseRgbColorComponent(&cc, &percentage))
      return false;
   *c |= cc << 8;

   if (ttype != CSS_TK_CHAR || tval[0] != ',') {
      MSG_CSS("expected '%s' not found in rgb color\n", ",");
      return false;
   }
   nextToken();

   if (!parseRgbColorComponent(&cc, &percentage))
      return false;
   *c |= cc;

   if (ttype != CSS_TK_CHAR || tval[0] != ')') {
      MSG_CSS("expected '%s' not found in rgb color\n", ")");
      return false;
   }

   return true;
}

bool CssParser::parseValue(CssPropertyName prop,
                           CssValueType type,
                           CssPropertyValue *val)
{
   CssLengthType lentype;
   bool found, ret = false;
   float fval;
   int i, ival, err = 1;
   Dstr *dstr;

   switch (type) {
   case CSS_TYPE_ENUM:
      if (ttype == CSS_TK_SYMBOL) {
         for (i = 0; Css_property_info[prop].enum_symbols[i]; i++)
            if (dStrAsciiCasecmp(tval,
                            Css_property_info[prop].enum_symbols[i]) == 0) {
               val->intVal = i;
               ret = true;
               break;
            }
         nextToken();
      }
      break;

   case CSS_TYPE_MULTI_ENUM:
      val->intVal = 0;
      ret = true;

      while (ttype == CSS_TK_SYMBOL) {
         if (dStrAsciiCasecmp(tval, "none") != 0) {
            for (i = 0, found = false;
                 !found && Css_property_info[prop].enum_symbols[i]; i++) {
               if (dStrAsciiCasecmp(tval,
                               Css_property_info[prop].enum_symbols[i]) == 0)
                  val->intVal |= (1 << i);
            }
         }
         nextToken();
      }
      break;

   case CSS_TYPE_LENGTH_PERCENTAGE:
   case CSS_TYPE_LENGTH_PERCENTAGE_NUMBER:
   case CSS_TYPE_LENGTH:
   case CSS_TYPE_SIGNED_LENGTH:
      if (ttype == CSS_TK_DECINT || ttype == CSS_TK_FLOAT) {
         fval = atof(tval);
         lentype = CSS_LENGTH_TYPE_NONE;

         nextToken();
         if (!spaceSeparated && ttype == CSS_TK_SYMBOL) {
            ret = true;

            if (dStrAsciiCasecmp(tval, "px") == 0) {
               lentype = CSS_LENGTH_TYPE_PX;
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "mm") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "cm") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= 10;
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "in") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= 25.4;
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "pt") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= (25.4 / 72);
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "pc") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= (25.4 / 6);
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "em") == 0) {
               lentype = CSS_LENGTH_TYPE_EM;
               nextToken();
            } else if (dStrAsciiCasecmp(tval, "ex") == 0) {
               lentype = CSS_LENGTH_TYPE_EX;
               nextToken();
            } else {
               ret = false;
            }
         } else if (!spaceSeparated &&
                    (type == CSS_TYPE_LENGTH_PERCENTAGE ||
                     type == CSS_TYPE_LENGTH_PERCENTAGE_NUMBER) &&
                    ttype == CSS_TK_CHAR &&
                    tval[0] == '%') {
            fval /= 100;
            lentype = CSS_LENGTH_TYPE_PERCENTAGE;
            ret = true;
            nextToken();
         }

         /* Allow numbers without unit only for 0 or
          * CSS_TYPE_LENGTH_PERCENTAGE_NUMBER
          */
         if (lentype == CSS_LENGTH_TYPE_NONE &&
            (type == CSS_TYPE_LENGTH_PERCENTAGE_NUMBER || fval == 0.0))
            ret = true;

         val->intVal = CSS_CREATE_LENGTH(fval, lentype);
      }
      break;

   case CSS_TYPE_AUTO:
      assert (ttype == CSS_TK_SYMBOL && !dStrAsciiCasecmp(tval, "auto"));
      ret = true;
      val->intVal = CSS_LENGTH_TYPE_AUTO;
      nextToken();
      break;

   case CSS_TYPE_COLOR:
      if (ttype == CSS_TK_COLOR) {
         val->intVal = a_Color_parse(tval, -1, &err);
         if (err)
            MSG_CSS("color is not in \"%s\" format\n", "#RRGGBB");
         else
            ret = true;
         nextToken();
      } else if (ttype == CSS_TK_SYMBOL) {
         if (dStrAsciiCasecmp(tval, "rgb") == 0) {
            nextToken();
            if (parseRgbColor(&val->intVal))
               ret = true;
            else
               MSG_CSS("Failed to parse %s color\n", "rgb(r,g,b)");
         } else {
            val->intVal = a_Color_parse(tval, -1, &err);
            if (err)
               MSG_CSS("color is not in \"%s\" format\n", "#RRGGBB");
            else
               ret = true;
         }
         nextToken();
      }
      break;

   case CSS_TYPE_STRING:
      if (ttype == CSS_TK_STRING) {
         val->strVal = dStrdup(tval);
         ret = true;
         nextToken();
      }
      break;

   case CSS_TYPE_SYMBOL:
      /* Read comma separated list of font family names */
      dstr = dStr_new("");
      while (ttype == CSS_TK_SYMBOL || ttype == CSS_TK_STRING ||
             (ttype == CSS_TK_CHAR && tval[0] == ',')) {
         if (spaceSeparated)
            dStr_append_c(dstr, ' ');
         dStr_append(dstr, tval);
         ret = true;
         nextToken();
      }

      if (ret) {
         val->strVal = dStrstrip(dstr->str);
         dStr_free(dstr, 0);
      } else {
         dStr_free(dstr, 1);
      }
      break;

   case CSS_TYPE_FONT_WEIGHT:
      ival = 0;
      if (ttype == CSS_TK_DECINT) {
         ival = strtol(tval, NULL, 10);
         if (ival < 100 || ival > 900)
            /* invalid */
            ival = 0;
      }

      if (ival != 0) {
         val->intVal = ival;
         ret = true;
         nextToken();
      }
      break;

   case CSS_TYPE_URI:
      if (ttype == CSS_TK_SYMBOL &&
          dStrAsciiCasecmp(tval, "url") == 0) {
         val->strVal = parseUrl();
         nextToken();
         if (val->strVal)
            ret = true;
      }
      break;

   case CSS_TYPE_BACKGROUND_POSITION:
      // 'background-position' consists of one or two values: vertical and
      // horizontal position; in most cases in this order. However, as long it
      // is unambigous, the order can be switched: "10px left" and "left 10px"
      // are both possible and have the same effect. For this reason, all
      // possibilities are tested in parallel.

      bool h[2], v[2];
      int pos[2];
      h[0] = v[0] = h[1] = v[1] = false;

      // First: collect values in pos[0] and pos[1], and determine whether
      // they can be used for a horizontal (h[i]) or vertical (v[i]) position
      // (or both). When neither h[i] or v[i] is set, pos[i] is undefined.
      for (i = 0; i < 2; i++) {
         CssValueType typeTmp;
         // tokenMatchesProperty will, for CSS_PROPERTY_BACKGROUND_POSITION,
         // work on both parts, since they are exchangable.
         if (tokenMatchesProperty (CSS_PROPERTY_BACKGROUND_POSITION,
                                   &typeTmp)) {
            h[i] = ttype != CSS_TK_SYMBOL ||
               (dStrAsciiCasecmp(tval, "top") != 0 &&
                dStrAsciiCasecmp(tval, "bottom") != 0);
            v[i] = ttype != CSS_TK_SYMBOL ||
               (dStrAsciiCasecmp(tval, "left") != 0 &&
                dStrAsciiCasecmp(tval, "right") != 0);
         } else
            // No match.
            h[i] = v[i] = false;

         if (h[i] || v[i]) {
            // Calculate values.
            if (ttype == CSS_TK_SYMBOL) {
               if (dStrAsciiCasecmp(tval, "top") == 0 ||
                   dStrAsciiCasecmp(tval, "left") == 0) {
                  pos[i] = CSS_CREATE_LENGTH (0.0, CSS_LENGTH_TYPE_PERCENTAGE);
                  nextToken();
               } else if (dStrAsciiCasecmp(tval, "center") == 0) {
                  pos[i] = CSS_CREATE_LENGTH (0.5, CSS_LENGTH_TYPE_PERCENTAGE);
                  nextToken();
               } else if (dStrAsciiCasecmp(tval, "bottom") == 0 ||
                          dStrAsciiCasecmp(tval, "right") == 0) {
                  pos[i] = CSS_CREATE_LENGTH (1.0, CSS_LENGTH_TYPE_PERCENTAGE);
                  nextToken();
               } else
                  // tokenMatchesProperty should have returned "false" already.
                  lout::misc::assertNotReached ();
            } else {
               // We can assume <length> or <percentage> here ...
               CssPropertyValue valTmp;
               if (parseValue(prop, CSS_TYPE_LENGTH_PERCENTAGE, &valTmp)) {
                  pos[i] = valTmp.intVal;
                  ret = true;
               } else
                  // ... but something may still fail.
                  h[i] = v[i] = false;
            }
         }

         // If the first value cannot be read, do not read the second.
         if (!h[i] && !v[i])
            break;
      }

      // Second: Create the final value. Order will be determined here.
      if (v[0] || h[0]) {
         // If second value is not set, it is set to "center", i. e. 50%, (see
         // CSS specification), which is suitable for both dimensions.
         if (!h[1] && !v[1]) {
            pos[1] = CSS_CREATE_LENGTH (0.5, CSS_LENGTH_TYPE_PERCENTAGE);
            h[1] = v[1] = true;
         }

         // Only valid, when a combination h/v or v/h is possible.
         if ((h[0] && v[1]) || (v[0] && h[1])) {
            ret = true;
            val->posVal = dNew(CssBackgroundPosition, 1);

            // Prefer combination h/v:
            if (h[0] && v[1]) {
                val->posVal->posX = pos[0];
                val->posVal->posY = pos[1];
            } else {
               // This should be v/h:
                val->posVal->posX = pos[1];
                val->posVal->posY = pos[0];
            }
         }
      }
      break;

   case CSS_TYPE_UNUSED:
      /* nothing */
      break;

   case CSS_TYPE_INTEGER:
      /* Not used for parser values. */
   default:
      assert(false);            /* not reached */
   }

   return ret;
}

bool CssParser::parseWeight()
{
   if (ttype == CSS_TK_CHAR && tval[0] == '!') {
      nextToken();
      if (ttype == CSS_TK_SYMBOL &&
          dStrAsciiCasecmp(tval, "important") == 0) {
         nextToken();
         return true;
      }
   }

   return false;
}

/*
 * bsearch(3) compare function for searching properties
 */
static int Css_property_info_cmp(const void *a, const void *b)
{
   return dStrAsciiCasecmp(((CssPropertyInfo *) a)->symbol,
                      ((CssPropertyInfo *) b)->symbol);
}


/*
 * bsearch(3) compare function for searching shorthands
 */
static int Css_shorthand_info_cmp(const void *a, const void *b)
{
   return dStrAsciiCasecmp(((CssShorthandInfo *) a)->symbol,
                      ((CssShorthandInfo *) b)->symbol);
}

void CssParser::parseDeclaration(CssPropertyList *props,
                                 CssPropertyList *importantProps)
{
   CssPropertyInfo pi = {NULL, {CSS_TYPE_UNUSED}, NULL}, *pip;
   CssShorthandInfo *sip;
   CssValueType type = CSS_TYPE_UNUSED;

   CssPropertyName prop;
   CssPropertyValue val, dir_vals[4];
   CssValueType dir_types[4];
   bool found, weight;
   int sh_index, i, j, n;
   int dir_set[4][4] = {
      /* 1 value  */ {0, 0, 0, 0},
      /* 2 values */ {0, 0, 1, 1},
      /* 3 values */ {0, 2, 1, 1},
      /* 4 values */ {0, 2, 3, 1}
   };

   if (ttype == CSS_TK_SYMBOL) {
      pi.symbol = tval;
      pip =
          (CssPropertyInfo *) bsearch(&pi, Css_property_info,
                                      CSS_NUM_PARSED_PROPERTIES,
                                      sizeof(CssPropertyInfo),
                                      Css_property_info_cmp);
      if (pip) {
         prop = (CssPropertyName) (pip - Css_property_info);
         nextToken();
         if (ttype == CSS_TK_CHAR && tval[0] == ':') {
            nextToken();
            if (tokenMatchesProperty (prop, &type) &&
                parseValue(prop, type, &val)) {
               weight = parseWeight();
               if (weight && importantProps)
                  importantProps->set(prop, type, val);
               else
                  props->set(prop, type, val);
            }
         }
      } else {
         /* Try shorthands. */
         sip =
             (CssShorthandInfo *) bsearch(&pi, Css_shorthand_info,
                                          CSS_SHORTHAND_NUM,
                                          sizeof(CssShorthandInfo),
                                          Css_shorthand_info_cmp);
         if (sip) {
            sh_index = sip - Css_shorthand_info;
            nextToken();
            if (ttype == CSS_TK_CHAR && tval[0] == ':') {
               nextToken();

               switch (Css_shorthand_info[sh_index].type) {

               case CssShorthandInfo::CSS_SHORTHAND_FONT:
                  /* \todo Implement details. */
               case CssShorthandInfo::CSS_SHORTHAND_MULTIPLE:
                  do {
                     for (found = false, i = 0;
                          !found &&
                          Css_shorthand_info[sh_index].properties[i] !=
                          CSS_PROPERTY_END;
                          i++)
                        if (tokenMatchesProperty(Css_shorthand_info[sh_index].
                                                 properties[i], &type)) {
                           found = true;
                           DEBUG_MSG(DEBUG_PARSE_LEVEL,
                                     "will assign to '%s'\n",
                                     Css_property_info
                                     [Css_shorthand_info[sh_index]
                                      .properties[i]].symbol);
                           if (parseValue(Css_shorthand_info[sh_index]
                                          .properties[i], type, &val)) {
                              weight = parseWeight();
                              if (weight && importantProps)
                                 importantProps->
                                     set(Css_shorthand_info[sh_index].
                                         properties[i], type, val);
                              else
                                 props->set(Css_shorthand_info[sh_index].
                                            properties[i], type, val);
                           }
                        }
                  } while (found);
                  break;

               case CssShorthandInfo::CSS_SHORTHAND_DIRECTIONS:
                  n = 0;
                  while (n < 4) {
                     if (tokenMatchesProperty(Css_shorthand_info[sh_index].
                                              properties[0], &type) &&
                         parseValue(Css_shorthand_info[sh_index]
                                    .properties[0], type, &val)) {
                        dir_vals[n] = val;
                        dir_types[n] = type;
                        n++;
                     } else
                        break;
                  }

                  weight = parseWeight();
                  if (n > 0) {
                     for (i = 0; i < 4; i++)
                        if (weight && importantProps)
                           importantProps->set(Css_shorthand_info[sh_index]
                                               .properties[i],
                                               dir_types[dir_set[n - 1][i]],
                                               dir_vals[dir_set[n - 1][i]]);
                        else
                           props->set(Css_shorthand_info[sh_index]
                                      .properties[i],
                                      dir_types[dir_set[n - 1][i]],
                                      dir_vals[dir_set[n - 1][i]]);
                  } else
                     MSG_CSS("no values for shorthand property '%s'\n",
                             Css_shorthand_info[sh_index].symbol);

                  break;

               case CssShorthandInfo::CSS_SHORTHAND_BORDER:
                  do {
                     for (found = false, i = 0;
                          !found && i < 3;
                          i++)
                        if (tokenMatchesProperty(Css_shorthand_info[sh_index].
                                                 properties[i], &type)) {
                           found = true;
                           if (parseValue(Css_shorthand_info[sh_index]
                                          .properties[i], type, &val)) {
                              weight = parseWeight();
                              for (j = 0; j < 4; j++)
                                 if (weight && importantProps)
                                    importantProps->
                                       set(Css_shorthand_info[sh_index].
                                          properties[j * 3 + i], type, val);
                                 else
                                    props->set(Css_shorthand_info[sh_index].
                                       properties[j * 3 + i], type, val);
                           }
                        }
                  } while (found);
                  break;
               }
            }
         }
      }
   }

   /* Skip all tokens until the expected end. */
   while (!(ttype == CSS_TK_END ||
            (ttype == CSS_TK_CHAR &&
             (tval[0] == ';' || tval[0] == '}'))))
      nextToken();

   if (ttype == CSS_TK_CHAR && tval[0] == ';')
      nextToken();
}

bool CssParser::parseSimpleSelector(CssSimpleSelector *selector)
{
   CssSimpleSelector::SelectType selectType;

   if (ttype == CSS_TK_SYMBOL) {
      selector->setElement (a_Html_tag_index(tval));
      nextToken();
      if (spaceSeparated)
         return true;
   } else if (ttype == CSS_TK_CHAR && tval[0] == '*') {
      selector->setElement (CssSimpleSelector::ELEMENT_ANY);
      nextToken();
      if (spaceSeparated)
         return true;
   } else if (ttype == CSS_TK_CHAR &&
              (tval[0] == '#' ||
               tval[0] == '.' ||
               tval[0] == ':')) {
      // nothing to be done in this case
   } else {
      return false;
   }

   do {
      selectType = CssSimpleSelector::SELECT_NONE;
      if (ttype == CSS_TK_CHAR) {
         switch (tval[0]) {
         case '#':
            selectType = CssSimpleSelector::SELECT_ID;
            break;
         case '.':
            selectType = CssSimpleSelector::SELECT_CLASS;
            break;
         case ':':
            selectType = CssSimpleSelector::SELECT_PSEUDO_CLASS;
            if (selector->getPseudoClass ())
               // pseudo class has been set already.
               // As dillo currently only supports :link and :visisted, a
               // selector with more than one pseudo class will never match.
               // By returning false, the whole CssRule will be dropped.
               // \todo adapt this when supporting :hover, :active...
               return false;
            break;
         }
      }

      if (selectType != CssSimpleSelector::SELECT_NONE) {
         nextToken();
         if (spaceSeparated)
            return false;

         if (ttype == CSS_TK_SYMBOL) {
            selector->setSelect (selectType, tval);
            nextToken();
         } else {
            return false; // don't accept classes or id's starting with integer
         }
         if (spaceSeparated)
            return true;
      }
   } while (selectType != CssSimpleSelector::SELECT_NONE);

   DEBUG_MSG(DEBUG_PARSE_LEVEL, "end of simple selector (%s, %s, %s, %d)\n",
      selector->id, selector->klass,
      selector->pseudo, selector->element);

   return true;
}

CssSelector *CssParser::parseSelector()
{
   CssSelector *selector = new CssSelector ();

   while (true) {
      if (! parseSimpleSelector (selector->top ())) {
         delete selector;
         selector = NULL;
         break;
      }

      if (ttype == CSS_TK_CHAR &&
         (tval[0] == ',' || tval[0] == '{')) {
         break;
      } else if (ttype == CSS_TK_CHAR && tval[0] == '>') {
         selector->addSimpleSelector (CssSelector::COMB_CHILD);
         nextToken();
      } else if (ttype == CSS_TK_CHAR && tval[0] == '+') {
         selector->addSimpleSelector (CssSelector::COMB_ADJACENT_SIBLING);
         nextToken();
      } else if (ttype != CSS_TK_END && spaceSeparated) {
         selector->addSimpleSelector (CssSelector::COMB_DESCENDANT);
      } else {
         delete selector;
         selector = NULL;
         break;
      }
   }

   while (ttype != CSS_TK_END &&
          (ttype != CSS_TK_CHAR ||
           (tval[0] != ',' && tval[0] != '{')))
         nextToken();

   return selector;
}

void CssParser::parseRuleset()
{
   lout::misc::SimpleVector < CssSelector * >*list;
   CssPropertyList *props, *importantProps;
   CssSelector *selector;

   list = new lout::misc::SimpleVector < CssSelector * >(1);

   while (true) {
      selector = parseSelector();

      if (selector) {
         selector->ref();
         list->increase();
         list->set(list->size() - 1, selector);
      }

      // \todo dump whole ruleset in case of parse error as required by CSS 2.1
      //       however make sure we don't dump it if only dillo fails to parse
      //       valid CSS.

      if (ttype == CSS_TK_CHAR && tval[0] == ',')
         /* To read the next token. */
         nextToken();
      else
         /* No more selectors. */
         break;
   }

   DEBUG_MSG(DEBUG_PARSE_LEVEL, "end of %s\n", "selectors");

   props = new CssPropertyList(true);
   props->ref();
   importantProps = new CssPropertyList(true);
   importantProps->ref();

   /* Read block. ('{' has already been read.) */
   if (ttype != CSS_TK_END) {
      withinBlock = true;
      nextToken();
      do
         parseDeclaration(props, importantProps);
      while (!(ttype == CSS_TK_END ||
               (ttype == CSS_TK_CHAR && tval[0] == '}')));
      withinBlock = false;
   }

   for (int i = 0; i < list->size(); i++) {
      CssSelector *s = list->get(i);

      if (origin == CSS_ORIGIN_USER_AGENT) {
         context->addRule(s, props, CSS_PRIMARY_USER_AGENT);
      } else if (origin == CSS_ORIGIN_USER) {
         context->addRule(s, props, CSS_PRIMARY_USER);
         context->addRule(s, importantProps, CSS_PRIMARY_USER_IMPORTANT);
      } else if (origin == CSS_ORIGIN_AUTHOR) {
         context->addRule(s, props, CSS_PRIMARY_AUTHOR);
         context->addRule(s, importantProps, CSS_PRIMARY_AUTHOR_IMPORTANT);
      }

      s->unref();
   }

   props->unref();
   importantProps->unref();

   delete list;

   if (ttype == CSS_TK_CHAR && tval[0] == '}')
      nextToken();
}

char * CssParser::parseUrl()
{
   Dstr *urlStr = NULL;

   if (ttype != CSS_TK_SYMBOL ||
      dStrAsciiCasecmp(tval, "url") != 0)
      return NULL;

   nextToken();

   if (ttype != CSS_TK_CHAR || tval[0] != '(')
      return NULL;

   nextToken();

   if (ttype == CSS_TK_STRING) {
      urlStr = dStr_new(tval);
      nextToken();
   } else {
      urlStr = dStr_new("");
      while (ttype != CSS_TK_END &&
             (ttype != CSS_TK_CHAR || tval[0] != ')')) {
         dStr_append(urlStr, tval);
         nextToken();
      }
   }

   if (ttype != CSS_TK_CHAR || tval[0] != ')') {
      dStr_free(urlStr, 1);
      urlStr = NULL;
   }

   if (urlStr) {
      DilloUrl *dilloUrl = a_Url_new(urlStr->str, a_Url_str(this->baseUrl));
      char *url = dStrdup(a_Url_str(dilloUrl));
      a_Url_free(dilloUrl);
      dStr_free(urlStr, 1);
      return url;
   } else {
      return NULL;
   }
}

void CssParser::parseImport(DilloHtml *html)
{
   char *urlStr = NULL;
   bool importSyntaxIsOK = false;
   bool mediaSyntaxIsOK = true;
   bool mediaIsSelected = true;

   nextToken();

   if (ttype == CSS_TK_SYMBOL &&
       dStrAsciiCasecmp(tval, "url") == 0)
      urlStr = parseUrl();
   else if (ttype == CSS_TK_STRING)
      urlStr = dStrdup (tval);

   nextToken();

   /* parse a comma-separated list of media */
   if (ttype == CSS_TK_SYMBOL) {
      mediaSyntaxIsOK = false;
      mediaIsSelected = false;
      while (ttype == CSS_TK_SYMBOL) {
         if (dStrAsciiCasecmp(tval, "all") == 0 ||
             dStrAsciiCasecmp(tval, "screen") == 0)
            mediaIsSelected = true;
         nextToken();
         if (ttype == CSS_TK_CHAR && tval[0] == ',') {
            nextToken();
         } else {
            mediaSyntaxIsOK = true;
            break;
         }
      }
   }

   if (mediaSyntaxIsOK &&
       ttype == CSS_TK_CHAR &&
       tval[0] == ';') {
      importSyntaxIsOK = true;
      nextToken();
   } else
      ignoreStatement();

   if (urlStr) {
      if (importSyntaxIsOK && mediaIsSelected) {
         MSG("CssParser::parseImport(): @import %s\n", urlStr);
         DilloUrl *url = a_Html_url_new (html, urlStr, a_Url_str(this->baseUrl),
                                         this->baseUrl ? 1 : 0);
         a_Html_load_stylesheet(html, url);
         a_Url_free(url);
      }
      dFree (urlStr);
   }
}

void CssParser::parseMedia()
{
   bool mediaSyntaxIsOK = false;
   bool mediaIsSelected = false;

   nextToken();

   /* parse a comma-separated list of media */
   while (ttype == CSS_TK_SYMBOL) {
      if (dStrAsciiCasecmp(tval, "all") == 0 ||
          dStrAsciiCasecmp(tval, "screen") == 0)
         mediaIsSelected = true;
      nextToken();
      if (ttype == CSS_TK_CHAR && tval[0] == ',') {
         nextToken();
      } else {
         mediaSyntaxIsOK = true;
         break;
      }
   }

   /* check that the syntax is OK so far */
   if (!(mediaSyntaxIsOK &&
         ttype == CSS_TK_CHAR &&
         tval[0] == '{')) {
      ignoreStatement();
      return;
   }

   /* parse/ignore the block as required */
   if (mediaIsSelected) {
      nextToken();
      while (ttype != CSS_TK_END) {
         parseRuleset();
         if (ttype == CSS_TK_CHAR && tval[0] == '}') {
            nextToken();
            break;
         }
      }
   } else
      ignoreBlock();
}

const char * CssParser::propertyNameString(CssPropertyName name)
{
   return Css_property_info[name].symbol;
}

void CssParser::ignoreBlock()
{
   int depth = 0;

   while (ttype != CSS_TK_END) {
      if (ttype == CSS_TK_CHAR) {
         if (tval[0] == '{') {
            depth++;
         } else if (tval[0] == '}') {
            depth--;
            if (depth == 0) {
               nextToken();
               return;
            }
         }
      }
      nextToken();
   }
}

void CssParser::ignoreStatement()
{
   while (ttype != CSS_TK_END) {
      if (ttype == CSS_TK_CHAR) {
         if (tval[0] == ';') {
            nextToken();
            return;
         } else if (tval[0] =='{') {
            ignoreBlock();
            return;
         }
      }
      nextToken();
   }
}

void CssParser::parse(DilloHtml *html, const DilloUrl *baseUrl,
                      CssContext *context,
                      const char *buf,
                      int buflen, CssOrigin origin)
{
   CssParser parser (context, origin, baseUrl, buf, buflen);
   bool importsAreAllowed = true;

   while (parser.ttype != CSS_TK_END) {
      if (parser.ttype == CSS_TK_CHAR &&
          parser.tval[0] == '@') {
         parser.nextToken();
         if (parser.ttype == CSS_TK_SYMBOL) {
            if (dStrAsciiCasecmp(parser.tval, "import") == 0 &&
                html != NULL &&
                importsAreAllowed) {
               parser.parseImport(html);
            } else if (dStrAsciiCasecmp(parser.tval, "media") == 0) {
               parser.parseMedia();
            } else {
               parser.ignoreStatement();
            }
         } else {
            parser.ignoreStatement();
         }
      } else {
         importsAreAllowed = false;
         parser.parseRuleset();
      }
   }
}

void CssParser::parseDeclarationBlock(const DilloUrl *baseUrl,
                                      const char *buf, int buflen,
                                      CssPropertyList *props,
                                      CssPropertyList *propsImortant)
{
   CssParser parser (NULL, CSS_ORIGIN_AUTHOR, baseUrl, buf, buflen);

   parser.withinBlock = true;

   do
      parser.parseDeclaration(props, propsImortant);
   while (!(parser.ttype == CSS_TK_END ||
         (parser.ttype == CSS_TK_CHAR && parser.tval[0] == '}')));
}
