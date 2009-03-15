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
#include <string.h>

#include "msg.h"
#include "colors.h"
#include "html_common.hh"
#include "css.hh"
#include "cssparser.hh"

using namespace dw::core::style;

#define DEBUG_MSG(A, B, ...) _MSG(B, __VA_ARGS__)
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

static const char *const Css_border_style_enum_vals[] = {
   "none", "hidden", "dotted", "dashed", "solid", "double", "groove",
   "ridge", "inset", "outset", NULL
};

static const char *const Css_cursor_enum_vals[] = {
   "crosshair", "default", "pointer", "move", "e_resize", "ne_resize",
   "nw_resize", "n_resize", "se_resize", "sw_resize", "s_resize",
   "w_resize", "text", "wait", "help", NULL
};

static const char *const Css_display_enum_vals[DISPLAY_LAST + 1] = {
   "block", "inline", "list-item", "table", "table-row-group",
   "table-header-group", "table-footer-group", "table-row",
   "table-cell", NULL
};

static const char *const Css_font_size_enum_vals[] = {
   "large", "larger", "medium", "small", "smaller", "xx-large", "xx-small", "x-large", "x-small", NULL
};

static const char *const Css_font_style_enum_vals[] = {
   "normal", "italic", "oblique", NULL
};

static const char *const Css_font_weight_enum_vals[] = {
   "bold", "bolder", "light", "lighter", "normal", NULL
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

static const char *const Css_vertical_align_vals[] = {
   "top", "bottom", "middle", "baseline", "sub", "super", NULL
};

static const char *const Css_white_space_vals[] = {
   "normal", "pre", "nowrap", NULL
};

const CssPropertyInfo Css_property_info[CSS_PROPERTY_LAST] = {
   {"background-attachment", {CSS_TYPE_UNUSED}, NULL},
   {"background-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"background-image", {CSS_TYPE_UNUSED}, NULL},
   {"background-position", {CSS_TYPE_UNUSED}, NULL},
   {"background-repeat", {CSS_TYPE_UNUSED}, NULL},
   {"border-bottom-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"border-bottom-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_border_style_enum_vals},
   {"border-bottom-width", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"border-collapse", {CSS_TYPE_UNUSED}, NULL},
   {"border-left-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"border-left-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_border_style_enum_vals},
   {"border-left-width", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"border-right-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"border-right-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_border_style_enum_vals},
   {"border-right-width", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"border-spacing", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"border-top-color", {CSS_TYPE_COLOR, CSS_TYPE_UNUSED}, NULL},
   {"border-top-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_border_style_enum_vals},
   {"border-top-width", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
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
   {"font-size", {CSS_TYPE_ENUM, CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_UNUSED}, Css_font_size_enum_vals},
   {"font-size-adjust", {CSS_TYPE_UNUSED}, NULL},
   {"font-stretch", {CSS_TYPE_UNUSED}, NULL},
   {"font-style", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_font_style_enum_vals},
   {"font-variant", {CSS_TYPE_UNUSED}, NULL},
   {"font-weight", {CSS_TYPE_ENUM, CSS_TYPE_FONT_WEIGHT, CSS_TYPE_UNUSED}, Css_font_weight_enum_vals},
   {"height", {CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_UNUSED}, NULL},
   {"left", {CSS_TYPE_UNUSED}, NULL},
   {"letter-spacing", {CSS_TYPE_UNUSED}, NULL},
   {"line-height", {CSS_TYPE_UNUSED}, NULL},
   {"list-style-image", {CSS_TYPE_UNUSED}, NULL},
   {"list-style-position", {CSS_TYPE_UNUSED}, NULL},
   {"list-style-type", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_list_style_type_enum_vals},
   {"margin-bottom", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"margin-left", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"margin-right", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
   {"margin-top", {CSS_TYPE_LENGTH, CSS_TYPE_UNUSED}, NULL},
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
   {"text-decoration", {CSS_TYPE_MULTI_ENUM, CSS_TYPE_UNUSED}, Css_text_decoration_enum_vals},
   {"text-indent", {CSS_TYPE_UNUSED}, NULL},
   {"text-shadow", {CSS_TYPE_UNUSED}, NULL},
   {"text-transform", {CSS_TYPE_UNUSED}, NULL},
   {"top", {CSS_TYPE_UNUSED}, NULL},
   {"unicode-bidi", {CSS_TYPE_UNUSED}, NULL},
   {"vertical-align", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_vertical_align_vals},
   {"visibility", {CSS_TYPE_UNUSED}, NULL},
   {"white-space", {CSS_TYPE_ENUM, CSS_TYPE_UNUSED}, Css_white_space_vals},
   {"width", {CSS_TYPE_LENGTH_PERCENTAGE, CSS_TYPE_UNUSED}, NULL},
   {"word-spacing", {CSS_TYPE_UNUSED}, NULL},
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
   const CssPropertyName * properties;/* CSS_SHORTHAND_MULTIPLE:
                                       *   must be terminated by -1
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
   (CssPropertyName) - 1
};

const CssPropertyName Css_border_bottom_properties[] = {
   CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
   CSS_PROPERTY_BORDER_BOTTOM_STYLE,
   CSS_PROPERTY_BORDER_BOTTOM_COLOR,
   (CssPropertyName) - 1
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
   (CssPropertyName) - 1
};

const CssPropertyName Css_border_right_properties[] = {
   CSS_PROPERTY_BORDER_RIGHT_WIDTH,
   CSS_PROPERTY_BORDER_RIGHT_STYLE,
   CSS_PROPERTY_BORDER_RIGHT_COLOR,
   (CssPropertyName) - 1
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
   (CssPropertyName) - 1
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
   (CssPropertyName) - 1
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
   (CssPropertyName) - 1
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
   (CssPropertyName) - 1
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
   (sizeof(Css_shorthand_info) / sizeof(CssShorthandInfo))

/* ----------------------------------------------------------------------
 *    Parsing
 * ---------------------------------------------------------------------- */

CssParser::CssParser(CssContext *context, CssOrigin origin,
                     const char *buf, int buflen)
{
   this->context = context;
   this->origin = origin;
   this->buf = buf;
   this->buflen = buflen;
   this->bufptr = 0;
   this->spaceSeparated = false;
   this->withinBlock = false;

   nextToken ();
}

/*
 * Gets the next character from the buffer, or EOF.
 */
int CssParser::getc()
{
   int c;

   if (bufptr >= buflen)
      c = EOF;
   else
      c = buf[bufptr];

   /* The buffer pointer is increased in any case, so that Css_ungetc works
    * correctly at the end of the buffer. */
   bufptr++;
   return c;
}

/*
 * Undoes the last Css_getc().
 */
void CssParser::ungetc()
{
   bufptr--;
}

void CssParser::nextToken()
{
   int c, c1, d, j;
   char hexbuf[5];
   int i = 0;

   ttype = CSS_TK_CHAR; /* init */
   spaceSeparated = false;

   c = getc();

   while (true) {
      if (isspace(c)) { // ignore whitespace
         spaceSeparated = true;
         c = getc();
      } else if (c == '/') { // ignore comments
         d = getc();
         if (d == '*') {
            c = getc();
            d = getc();
            while (d != EOF && (c != '*' || d != '/')) {
               c = d;
               d = getc();
            }
            c = getc();
         } else {
            ungetc();
            break;
         }
      } else {
         break;
      }
   }

   // handle negative numbers
   if (c == '-') {
      if (i < maxStrLen - 1)
         tval[i++] = c;
      c = getc();
   }

   if (isdigit(c)) {
      ttype = CSS_TK_DECINT;
      do {
         if (i < maxStrLen - 1) {
            tval[i++] = c;
         }
         /* else silently truncated */
         c = getc();
      } while (isdigit(c));
      if (c != '.')
         ungetc();

      /* ...but keep going to see whether it's really a float */
   }

   if (c == '.') {
      c = getc();
      if (isdigit(c)) {
         ttype = CSS_TK_FLOAT;
         if (i < maxStrLen - 1)
            tval[i++] = '.';
         do {
            if (i < maxStrLen - 1)
               tval[i++] = c;
            /* else silently truncated */
            c = getc();
         } while (isdigit(c));

         ungetc();
         tval[i] = 0;
         DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token number %s\n", tval);
         return;
      } else {
         ungetc();
         if (ttype == CSS_TK_DECINT) {
            ungetc();
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
      ungetc(); /* ungetc '-' */
      i--;
      c = getc();
   }

   if (isalpha(c) || c == '_' || c == '-') {
      ttype = CSS_TK_SYMBOL;

      tval[0] = c;
      i = 1;
      c = getc();
      while (isalnum(c) || c == '_' || c == '-') {
         if (i < maxStrLen - 1) {
            tval[i] = c;
            i++;
         }                      /* else silently truncated */
         c = getc();
      }
      tval[i] = 0;
      ungetc();
      DEBUG_MSG(DEBUG_TOKEN_LEVEL, "token symbol '%s'\n", tval);
      return;
   }

   if (c == '"' || c == '\'') {
      c1 = c;
      ttype = CSS_TK_STRING;

      i = 0;
      c = getc();

      while (c != EOF && c != c1) {
         if (c == '\\') {
            d = getc();
            if (isxdigit(d)) {
               /* Read hex Unicode char. (Actually, strings are yet only 8
                * bit.) */
               hexbuf[0] = d;
               j = 1;
               d = getc();
               while (j < 4 && isxdigit(d)) {
                  hexbuf[j] = d;
                  j++;
                  d = getc();
               }
               hexbuf[j] = 0;
               ungetc();
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
         c = getc();
      }
      tval[i] = 0;
      /* No Css_ungetc(). */
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
      c = getc();
      while (isxdigit(c)) {
         if (i < maxStrLen - 1) {
            tval[i] = c;
            i++;
         }                      /* else silently truncated */
         c = getc();
      }
      tval[i] = 0;
      ungetc();
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


bool CssParser::tokenMatchesProperty(CssPropertyName prop, CssValueType * type)
{
   int i, err = 1;

   for (int j = 0; Css_property_info[prop].type[j] != CSS_TYPE_UNUSED; j++) {
      *type = Css_property_info[prop].type[j];

      switch (Css_property_info[prop].type[j]) {

         case CSS_TYPE_ENUM:
            if (ttype == CSS_TK_SYMBOL) {
               for (i = 0; Css_property_info[prop].enum_symbols[i]; i++)
                  if (dStrcasecmp(tval,
                        Css_property_info[prop].enum_symbols[i]) == 0)
                     return true;
            }
            break;

         case CSS_TYPE_MULTI_ENUM:
            if (ttype == CSS_TK_SYMBOL) {
               if (dStrcasecmp(tval, "none") == 0)
                     return true;
               else {
                  for (i = 0; Css_property_info[prop].enum_symbols[i]; i++) {
                     if (dStrcasecmp(tval,
                           Css_property_info[prop].enum_symbols[i]) == 0)
                        return true;
                  }
               }
            }
            break;

         case CSS_TYPE_LENGTH_PERCENTAGE:
         case CSS_TYPE_LENGTH:
            if (ttype == CSS_TK_DECINT ||
               ttype == CSS_TK_FLOAT ||
               (ttype == CSS_TK_SYMBOL && dStrcasecmp(tval,
                                                              "auto") == 0))
               return true;
            break;

         case CSS_TYPE_COLOR:
            if ((ttype == CSS_TK_COLOR ||
                  ttype == CSS_TK_SYMBOL) &&
               a_Color_parse(tval, -1, &err) != -1)
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

         case CSS_TYPE_UNUSED:
         case CSS_TYPE_INTEGER:
            /* Not used for parser values. */
         default:
            assert(false);
            break;
      }
   }

   *type = CSS_TYPE_UNUSED;
   return false;
}

bool CssParser::parseValue(CssPropertyName prop,
                           CssValueType type,
                           CssPropertyValue * val)
{
   CssLengthType lentype;
   bool found, ret = false;
   float fval;
   int i, ival, err = 1;

   switch (type) {
   case CSS_TYPE_ENUM:
      if (ttype == CSS_TK_SYMBOL) {
         for (i = 0; Css_property_info[prop].enum_symbols[i]; i++)
            if (dStrcasecmp(tval,
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
         if (dStrcasecmp(tval, "none") != 0) {
            for (i = 0, found = false;
                 !found && Css_property_info[prop].enum_symbols[i]; i++) {
               if (dStrcasecmp(tval,
                               Css_property_info[prop].enum_symbols[i]) == 0)
                  val->intVal |= (1 << i);
            }
         }
         nextToken();
      }
      break;

   case CSS_TYPE_LENGTH_PERCENTAGE:
   case CSS_TYPE_LENGTH:
      if (ttype == CSS_TK_DECINT || ttype == CSS_TK_FLOAT) {
         fval = atof(tval);
         lentype = CSS_LENGTH_TYPE_PX;  /* Actually, there must be a unit,
                                         * except for num == 0. */

         ret = true;

         nextToken();
         if (ttype == CSS_TK_SYMBOL) {
            if (dStrcasecmp(tval, "px") == 0) {
               lentype = CSS_LENGTH_TYPE_PX;
               nextToken();
            } else if (dStrcasecmp(tval, "mm") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               nextToken();
            } else if (dStrcasecmp(tval, "cm") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= 10;
               nextToken();
            } else if (dStrcasecmp(tval, "in") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= 25.4;
               nextToken();
            } else if (dStrcasecmp(tval, "pt") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= (25.4 / 72);
               nextToken();
            } else if (dStrcasecmp(tval, "pc") == 0) {
               lentype = CSS_LENGTH_TYPE_MM;
               fval *= (25.4 / 6);
               nextToken();
            } else if (dStrcasecmp(tval, "em") == 0) {
               lentype = CSS_LENGTH_TYPE_EM;
               nextToken();
            } else if (dStrcasecmp(tval, "ex") == 0) {
               lentype = CSS_LENGTH_TYPE_EX;
               nextToken();
            }
         } else if (type == CSS_TYPE_LENGTH_PERCENTAGE &&
                    ttype == CSS_TK_CHAR &&
                    tval[0] == '%') {
            fval /= 100;
            lentype = CSS_LENGTH_TYPE_PERCENTAGE;
            nextToken();
         }

         val->intVal = CSS_CREATE_LENGTH(fval, lentype);
      } else if (ttype == CSS_TK_SYMBOL &&
                 dStrcasecmp(tval, "auto") == 0) {
         ret = true;
         val->intVal = CSS_LENGTH_TYPE_AUTO;
         nextToken();
      }
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
         val->intVal = a_Color_parse(tval, -1, &err);
         if (err)
            MSG_CSS("color is not in \"%s\" format\n", "#RRGGBB");
         else
            ret = true;
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
      if (ttype == CSS_TK_SYMBOL || ttype == CSS_TK_STRING) {
         val->strVal = dStrdup(tval);
         ret = true;
         nextToken();
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
          dStrcasecmp(tval, "important") == 0) {
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
   return dStrcasecmp(((CssPropertyInfo *) a)->symbol,
                      ((CssPropertyInfo *) b)->symbol);
}


/*
 * bsearch(3) compare function for searching shorthands
 */
static int Css_shorthand_info_cmp(const void *a, const void *b)
{
   return dStrcasecmp(((CssShorthandInfo *) a)->symbol,
                      ((CssShorthandInfo *) b)->symbol);
}

void CssParser::parseDeclaration(CssPropertyList * props,
                                 CssPropertyList * importantProps)
{
   CssPropertyInfo pi = {NULL, {CSS_TYPE_UNUSED}, NULL}, *pip;
   CssShorthandInfo si, *sip;
   CssValueType type = CSS_TYPE_UNUSED;

   CssPropertyName prop;
   CssPropertyValue val, dir_vals[4];
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
         si.symbol = tval;
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
                          Css_shorthand_info[sh_index].properties[i] != -1;
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
                                               type,
                                               dir_vals[dir_set[n - 1]
                                                        [i]]);
                        else
                           props->set(Css_shorthand_info[sh_index]
                                      .properties[i],
                                      type,
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
   char **pp;

   if (ttype == CSS_TK_SYMBOL) {
      selector->element = a_Html_tag_index(tval);
      nextToken();
      if (spaceSeparated)
         return true;
   } else if (ttype == CSS_TK_CHAR && tval[0] == '*') {
      selector->element = CssSimpleSelector::ELEMENT_ANY;
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
      pp = NULL;
      if (ttype == CSS_TK_CHAR) {
         switch (tval[0]) {
            case '#':
               pp = &selector->id;
               break;
            case '.':
               pp = &selector->klass;
               break;
            case ':':
               pp = &selector->pseudo;
               if (*pp)
                  // pseudo class has been set already.
                  // As dillo currently only supports :link and :visisted, a
                  // selector with more than one pseudo class will never match.
                  // By returning false, the whole CssRule will be dropped.
                  // \todo adapt this when supporting :hover, :active...
                  return false;
               break;
         }
      }

      if (pp) {
         nextToken();
         if (spaceSeparated)
            return true;

         if (ttype == CSS_TK_SYMBOL) {
               if (*pp == NULL)
                  *pp = dStrdup(tval);
               nextToken();
         } else {
            return false; // don't accept classes or id's starting with integer
         }
         if (spaceSeparated)
            return true;
      }
   } while (pp);

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
         selector->addSimpleSelector (CssSelector::CHILD);
         nextToken();
      } else if (ttype != CSS_TK_END && spaceSeparated) {
         selector->addSimpleSelector (CssSelector::DESCENDANT);
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
         context->addRule(s, importantProps,
                                  CSS_PRIMARY_USER_IMPORTANT);
      } else if (origin == CSS_ORIGIN_AUTHOR) {
         context->addRule(s, props, CSS_PRIMARY_AUTHOR);
         context->addRule(s, importantProps,
                                  CSS_PRIMARY_AUTHOR_IMPORTANT);
      }

      s->unref();
   }

   props->unref();
   importantProps->unref();

   delete list;

   if (ttype == CSS_TK_CHAR && tval[0] == '}')
      nextToken();
}

const char * CssParser::propertyNameString(CssPropertyName name)
{
   return Css_property_info[name].symbol;
}

void CssParser::parse(CssContext * context,
                      const char *buf,
                      int buflen, CssOrigin origin)
{
   CssParser parser (context, origin, buf, buflen);

   while (parser.ttype != CSS_TK_END)
      parser.parseRuleset();
}

CssPropertyList *CssParser::parseDeclarationBlock(const char *buf, int buflen)
{
   CssPropertyList *props = new CssPropertyList (true);
   CssParser parser (NULL, CSS_ORIGIN_AUTHOR, buf, buflen);

   parser.withinBlock = true;

   do
      parser.parseDeclaration(props, NULL);
   while (!(parser.ttype == CSS_TK_END ||
         (parser.ttype == CSS_TK_CHAR && parser.tval[0] == '}')));

   if (props->size () == 0) {
      delete props;
      props = NULL;
   }

   return props;
}
