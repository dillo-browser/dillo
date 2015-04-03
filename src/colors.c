/*
 * File: colors.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "colors.h"

#include "msg.h"

/*
 * If EXTENDED_COLOR is defined, the extended set of named colors is supported.
 * These colors're not standard but they're supported in most browsers.
 * NOTE: The colors MUST be in alphabetical order and lower case because the
 *       code uses a binary search.
 */

#define EXTENDED_COLOR

static const struct key {
   char *key;
   int32_t val;
} color_keyword [] = {
#ifdef EXTENDED_COLOR
   { "aliceblue", 0xf0f8ff},
   { "antiquewhite", 0xfaebd7},
#endif
   { "aqua", 0x00ffff},
#ifdef EXTENDED_COLOR
   { "aquamarine", 0x7fffd4},
   { "azure", 0xf0ffff},
   { "beige", 0xf5f5dc},
   { "bisque", 0xffe4c4},
#endif
   { "black", 0x000000},
#ifdef EXTENDED_COLOR
   { "blanchedalmond", 0xffebcd},
#endif
   {"blue", 0x0000ff},
#ifdef EXTENDED_COLOR
   { "blueviolet", 0x8a2be2},
   { "brown", 0xa52a2a},
   { "burlywood", 0xdeb887},
   { "cadetblue", 0x5f9ea0},
   { "chartreuse", 0x7fff00},
   { "chocolate", 0xd2691e},
   { "coral", 0xff7f50},
   { "cornflowerblue", 0x6495ed},
   { "cornsilk", 0xfff8dc},
   { "crimson", 0xdc1436},
   { "cyan", 0x00ffff},
   { "darkblue", 0x00008b},
   { "darkcyan", 0x008b8b},
   { "darkgoldenrod", 0xb8860b},
   { "darkgray", 0xa9a9a9},
   { "darkgreen", 0x006400},
   { "darkgrey", 0xa9a9a9},
   { "darkkhaki", 0xbdb76b},
   { "darkmagenta", 0x8b008b},
   { "darkolivegreen", 0x556b2f},
   { "darkorange", 0xff8c00},
   { "darkorchid", 0x9932cc},
   { "darkred", 0x8b0000},
   { "darksalmon", 0xe9967a},
   { "darkseagreen", 0x8fbc8f},
   { "darkslateblue", 0x483d8b},
   { "darkslategray", 0x2f4f4f},
   { "darkslategrey", 0x2f4f4f},
   { "darkturquoise", 0x00ced1},
   { "darkviolet", 0x9400d3},
   { "deeppink", 0xff1493},
   { "deepskyblue", 0x00bfff},
   { "dimgray", 0x696969},
   { "dimgrey", 0x696969},
   { "dodgerblue", 0x1e90ff},
   { "firebrick", 0xb22222},
   { "floralwhite", 0xfffaf0},
   { "forestgreen", 0x228b22},
#endif
   { "fuchsia", 0xff00ff},
#ifdef EXTENDED_COLOR
   { "gainsboro", 0xdcdcdc},
   { "ghostwhite", 0xf8f8ff},
   { "gold", 0xffd700},
   { "goldenrod", 0xdaa520},
#endif
   { "gray", 0x808080},
   { "green", 0x008000},
#ifdef EXTENDED_COLOR
   { "greenyellow", 0xadff2f},
   { "grey", 0x808080},
   { "honeydew", 0xf0fff0},
   { "hotpink", 0xff69b4},
   { "indianred", 0xcd5c5c},
   { "indigo", 0x4b0082},
   { "ivory", 0xfffff0},
   { "khaki", 0xf0e68c},
   { "lavender", 0xe6e6fa},
   { "lavenderblush", 0xfff0f5},
   { "lawngreen", 0x7cfc00},
   { "lemonchiffon", 0xfffacd},
   { "lightblue", 0xadd8e6},
   { "lightcoral", 0xf08080},
   { "lightcyan", 0xe0ffff},
   { "lightgoldenrodyellow", 0xfafad2},
   { "lightgray", 0xd3d3d3},
   { "lightgreen", 0x90ee90},
   { "lightgrey", 0xd3d3d3},
   { "lightpink", 0xffb6c1},
   { "lightsalmon", 0xffa07a},
   { "lightseagreen", 0x20b2aa},
   { "lightskyblue", 0x87cefa},
   { "lightslategray", 0x778899},
   { "lightslategrey", 0x778899},
   { "lightsteelblue", 0xb0c4de},
   { "lightyellow", 0xffffe0},
#endif
   { "lime", 0x00ff00},
#ifdef EXTENDED_COLOR
   { "limegreen", 0x32cd32},
   { "linen", 0xfaf0e6},
   { "magenta", 0xff00ff},
#endif
   { "maroon", 0x800000},
#ifdef EXTENDED_COLOR
   { "mediumaquamarine", 0x66cdaa},
   { "mediumblue", 0x0000cd},
   { "mediumorchid", 0xba55d3},
   { "mediumpurple", 0x9370db},
   { "mediumseagreen", 0x3cb371},
   { "mediumslateblue", 0x7b68ee},
   { "mediumspringgreen", 0x00fa9a},
   { "mediumturquoise", 0x48d1cc},
   { "mediumvioletred", 0xc71585},
   { "midnightblue", 0x191970},
   { "mintcream", 0xf5fffa},
   { "mistyrose", 0xffe4e1},
   { "moccasin", 0xffe4b5},
   { "navajowhite", 0xffdead},
#endif
   { "navy", 0x000080},
#ifdef EXTENDED_COLOR
   { "oldlace", 0xfdf5e6},
#endif
   { "olive", 0x808000},
#ifdef EXTENDED_COLOR
   { "olivedrab", 0x6b8e23},
   { "orange", 0xffa500},
   { "orangered", 0xff4500},
   { "orchid", 0xda70d6},
   { "palegoldenrod", 0xeee8aa},
   { "palegreen", 0x98fb98},
   { "paleturquoise", 0xafeeee},
   { "palevioletred", 0xdb7093},
   { "papayawhip", 0xffefd5},
   { "peachpuff", 0xffdab9},
   { "peru", 0xcd853f},
   { "pink", 0xffc0cb},
   { "plum", 0xdda0dd},
   { "powderblue", 0xb0e0e6},
#endif
   { "purple", 0x800080},
   { "red", 0xff0000},
#ifdef EXTENDED_COLOR
   { "rosybrown", 0xbc8f8f},
   { "royalblue", 0x4169e1},
   { "saddlebrown", 0x8b4513},
   { "salmon", 0xfa8072},
   { "sandybrown", 0xf4a460},
   { "seagreen", 0x2e8b57},
   { "seashell", 0xfff5ee},
   { "sienna", 0xa0522d},
#endif
   { "silver", 0xc0c0c0},
#ifdef EXTENDED_COLOR
   { "skyblue", 0x87ceeb},
   { "slateblue", 0x6a5acd},
   { "slategray", 0x708090},
   { "slategrey", 0x708090},
   { "snow", 0xfffafa},
   { "springgreen", 0x00ff7f},
   { "steelblue", 0x4682b4},
   { "tan", 0xd2b48c},
#endif
   { "teal", 0x008080},
#ifdef EXTENDED_COLOR
   { "thistle", 0xd8bfd8},
   { "tomato", 0xff6347},
   { "turquoise", 0x40e0d0},
   { "violet", 0xee82ee},
   { "wheat", 0xf5deb3},
#endif
   { "white", 0xffffff},
#ifdef EXTENDED_COLOR
   { "whitesmoke", 0xf5f5f5},
#endif
   { "yellow", 0xffff00},
#ifdef EXTENDED_COLOR
   { "yellowgreen", 0x9acd32},
#endif
};

#define NCOLORS   (sizeof(color_keyword) / sizeof(color_keyword[0]))

/*
 * Parse a color in hex (RRGGBB) or (RGB)
 *
 * Return Value:
 *   parsed color if successful (err = 0),
 *   default_color on error (err = 1).
 */
static int32_t Color_parse_hex (const char *s, int32_t default_color, int *err)
{
  int32_t ret_color;
  char *tail;

  *err = 1;
  ret_color = strtol(s, &tail, 16);
  if (tail - s == 6)
     *err = 0;
  else if (tail - s == 3) {       /* #RGB as allowed by CSS */
     *err = 0;
         ret_color = ((ret_color & 0xf00) << 12) | ((ret_color & 0xf00) << 8) |
                     ((ret_color & 0x0f0) << 8)  | ((ret_color & 0x0f0) << 4) |
                     ((ret_color & 0x00f) << 4)  | ((ret_color & 0x00f) << 0);
  } else
     ret_color = default_color;

  return ret_color;
}

/*
 * Parse a color string.
 *
 * - If the string begins with # or with 0x, return the color number
 *   (with 'RGB' expanded to 'RRGGBB').
 * - Else search the set of named colors.
 * - As a last resort, treat it as bare hex as in the first case.
 *
 * Return Value:
 *    Parsed color if successful,
 *    default_color on error.
 *
 * "err" argument:
 *    0 if a color beginning with '#' is successfully parsed
 *      or the color is a recognized word.
 *    1 if the color is bare hex or can't be parsed at all.
 *    2 if a color beginning with 0[xX] is successfully parsed.
 */
int32_t a_Color_parse (const char *str, int32_t default_color, int *err)
{
   const char *cp;
   int32_t ret_color;
   int ret, low, mid, high, st = 1;

   /* skip leading spaces */
   for (cp = str; dIsspace(*cp); cp++);

   ret_color = default_color;
   if (*cp == '#') {
      ret_color = Color_parse_hex(cp + 1, default_color, &st);

   } else if (*cp == '0' && (cp[1] == 'x' || cp[1] == 'X') ) {
      ret_color = Color_parse_hex(cp + 2, default_color, &st);
      if (!st)
         st = 2;
   } else {
      /* Binary search */
      low = 0;
      high = NCOLORS - 1;
      while (low <= high) {
         mid = (low + high) / 2;
         if ((ret = dStrAsciiCasecmp(cp, color_keyword[mid].key)) < 0)
            high = mid - 1;
         else if (ret > 0)
            low = mid + 1;
         else {
            ret_color = color_keyword[mid].val;
            st = 0;
            break;
         }
      }

      if (low > high) {
         /* try for RRGGBB lacking the leading '#' */
         ret_color = Color_parse_hex(cp, default_color, &st);
         st = 1;
      }
   }

   _MSG("color string: %s\n", str);
   _MSG("color :  %X\n", ret_color);

   *err = st;
   return ret_color;
}

#if 0
/*
 * Return a "distance" measure (between [0, 10])
 */
static int Color_distance(long c1, long c2)
{
   return (labs((c1 & 0x0000ff) - (c2 & 0x0000ff)) +
           labs(((c1 & 0x00ff00) - (c2 & 0x00ff00)) >> 8)  +
           labs(((c1 & 0xff0000) - (c2 & 0xff0000)) >> 16)) / 75;
}
#endif

/*
 * Return: [0-3]
 */
static int Color_distance2(long c1, long c2)
{
   return (labs((c1 & 0x0000ff) - (c2 & 0x0000ff)) >= 0x000060) +
          (labs((c1 & 0x00ff00) - (c2 & 0x00ff00)) >= 0x006000) +
          (labs((c1 & 0xff0000) - (c2 & 0xff0000)) >= 0x600000);
}

/*
 * Return: [0-3] (requires less contrast than distance2)
 */
static int Color_distance3(long c1, long c2)
{
   return (labs((c1 & 0x0000ff) - (c2 & 0x0000ff)) >= 0x000040) +
          (labs((c1 & 0x00ff00) - (c2 & 0x00ff00)) >= 0x004000) +
          (labs((c1 & 0xff0000) - (c2 & 0xff0000)) >= 0x400000);
}

/*
 * Return a suitable "visited link" color
 * Return value:
 *   if candidate has good contrast with C_txt, C_lnk and C_bg  -> candidate
 *   else another color (from the internal list)
 */
int32_t a_Color_vc(int32_t candidate,
                   int32_t C_txt, int32_t C_lnk, int32_t C_bg)
{
                    /* candidate purple    darkcyan  darkmagenta olive   */
  static int32_t v[] = {0x000000, 0x800080, 0x008b8b, 0x8b008b, 0x808000,
                    /* darkred   coral     black                        */
                       0x8b0000, 0xff7f50, 0x000000};
  int v_size = sizeof(v) / sizeof(v[0]);
  int i, max_i, score, max_score, d_bg, d_txt, d_lnk;


  /* set candidate in the list */
  v[0] = candidate;

  /* Try to get good overall and individual contrast */
  max_i = max_score = 0;
  for (i = 0; i < v_size; ++i) {
     _MSG("a_Color_vc: [%d]%.6x: %d %d %d\n", i, v[i],
         Color_distance2(C_txt, v[i]),
         Color_distance2(C_lnk, v[i]),
         Color_distance2(C_bg, v[i]));

     /* Tuned with: slashdot.org, paulgraham.com, newsforge.com,
      *             linuxjournal.com
      */
     d_txt = Color_distance2(C_txt, v[i]);
     d_lnk = Color_distance2(C_lnk, v[i]);
     d_bg  = Color_distance2(C_bg, v[i]);
     score = (d_bg >= 2 ? 4 : 2 * d_bg) +
             (d_txt + d_lnk >= 2 ? 2 : d_txt + d_lnk) +
             (Color_distance3(C_lnk, v[i]) >= 1 ? 1 : 0);
     if (score >= 7) {
        /* enough distance, use this color */
        max_i = i;
        break;
     } else if (score > max_score) {
        /* keep track of the best candidate so far */
        max_score = score;
        max_i = i;
     }
  }
  return v[max_i];
}
