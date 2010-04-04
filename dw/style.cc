/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "core.hh"
#include "../lout/msg.h"

using namespace lout;

namespace dw {
namespace core {
namespace style {

void StyleAttrs::initValues ()
{
   x_link = -1;
   x_img = -1;
   x_tooltip = NULL;
   textDecoration = TEXT_DECORATION_NONE;
   textAlign = TEXT_ALIGN_LEFT;
   textAlignChar = '.';
   listStylePosition = LIST_STYLE_POSITION_OUTSIDE;
   listStyleType = LIST_STYLE_TYPE_DISC;
   valign = VALIGN_BASELINE;
   backgroundColor = NULL;
   width = height = lineHeight = LENGTH_AUTO;
   margin.setVal (0);
   borderWidth.setVal (0);
   padding.setVal (0);
   setBorderColor (NULL);
   setBorderStyle (BORDER_NONE);
   hBorderSpacing = 0;
   vBorderSpacing = 0;

   display = DISPLAY_INLINE;
   whiteSpace = WHITE_SPACE_NORMAL;
   cursor = CURSOR_DEFAULT;
}

/**
 * \brief Reset those style attributes to their standard values, which are
 *    not inherited, according to CSS.
 */
void StyleAttrs::resetValues ()
{
   x_img = -1;

   valign = VALIGN_BASELINE;
   textAlignChar = '.';
   backgroundColor = NULL;
   width = LENGTH_AUTO;
   height = LENGTH_AUTO;

   margin.setVal (0);
   borderWidth.setVal (0);
   padding.setVal (0);
   setBorderColor (NULL);
   setBorderStyle (BORDER_NONE);
   hBorderSpacing = 0;
   vBorderSpacing = 0;

   display = DISPLAY_INLINE;
}

/**
 * \brief This method returns whether something may change its size, when
 *    its style changes from this style to \em otherStyle.
 *
 * It is mainly for optimizing style changes where only colors etc change
 * (where false would be returned), in some cases it may return true, although
 * a size change does not actually happen (e.g. when in a certain
 * context a particular attribute is ignored).
 *
 * \todo Should for CSS implemented properly. Currently, size changes are
 * not needed, so always false is returned. See also
 * dw::core::Widget::setStyle.
 */
bool StyleAttrs::sizeDiffs (StyleAttrs *otherStyle)
{
   return false;
}

bool StyleAttrs::equals (object::Object *other) {
   StyleAttrs *otherAttrs = (StyleAttrs *) other;

   return this == otherAttrs ||
      (font == otherAttrs->font &&
       textDecoration == otherAttrs->textDecoration &&
       color == otherAttrs->color &&
       backgroundColor == otherAttrs->backgroundColor &&
       textAlign == otherAttrs->textAlign &&
       valign == otherAttrs->valign &&
       textAlignChar == otherAttrs->textAlignChar &&
       hBorderSpacing == otherAttrs->hBorderSpacing &&
       vBorderSpacing == otherAttrs->vBorderSpacing &&
       width == otherAttrs->width &&
       height == otherAttrs->height &&
       lineHeight == otherAttrs->lineHeight &&
       margin.equals (&otherAttrs->margin) &&
       borderWidth.equals (&otherAttrs->borderWidth) &&
       padding.equals (&otherAttrs->padding) &&
       borderColor.top == otherAttrs->borderColor.top &&
       borderColor.right == otherAttrs->borderColor.right &&
       borderColor.bottom == otherAttrs->borderColor.bottom &&
       borderColor.left == otherAttrs->borderColor.left &&
       borderStyle.top == otherAttrs->borderStyle.top &&
       borderStyle.right == otherAttrs->borderStyle.right &&
       borderStyle.bottom == otherAttrs->borderStyle.bottom &&
       borderStyle.left == otherAttrs->borderStyle.left &&
       display == otherAttrs->display &&
       whiteSpace == otherAttrs->whiteSpace &&
       listStylePosition == otherAttrs->listStylePosition &&
       listStyleType == otherAttrs->listStyleType &&
       cursor == otherAttrs->cursor &&
       x_link == otherAttrs->x_link &&
       x_img == otherAttrs->x_img &&
       x_tooltip == otherAttrs->x_tooltip);
}

int StyleAttrs::hashValue () {
   return (intptr_t) font +
      textDecoration +
      (intptr_t) color +
      (intptr_t) backgroundColor +
      textAlign +
      valign +
      textAlignChar +
      hBorderSpacing +
      vBorderSpacing +
      width +
      height +
      lineHeight +
      margin.hashValue () +
      borderWidth.hashValue () +
      padding.hashValue () +
      (intptr_t) borderColor.top +
      (intptr_t) borderColor.right +
      (intptr_t) borderColor.bottom  +
      (intptr_t) borderColor.left  +
      borderStyle.top +
      borderStyle.right +
      borderStyle.bottom +
      borderStyle.left +
      display +
      whiteSpace +
      listStylePosition +
      listStyleType +
      cursor +
      x_link +
      x_img +
      (intptr_t) x_tooltip;
}

int Style::totalRef = 0;
container::typed::HashTable <StyleAttrs, Style> * Style::styleTable =
   new container::typed::HashTable <StyleAttrs, Style> (false, false, 1024);

Style::Style (StyleAttrs *attrs)
{
   copyAttrs (attrs);

   refCount = 1;

   font->ref ();
   if (color)
      color->ref ();
   if (backgroundColor)
      backgroundColor->ref ();
   if (borderColor.top)
      borderColor.top->ref();
   if (borderColor.bottom)
      borderColor.bottom->ref();
   if (borderColor.left)
      borderColor.left->ref();
   if (borderColor.right)
      borderColor.right->ref();
   if (x_tooltip)
      x_tooltip->ref();

   totalRef++;
}

Style::~Style ()
{
   font->unref ();

   if (color)
      color->unref ();
   if (backgroundColor)
      backgroundColor->unref ();
   if (borderColor.top)
      borderColor.top->unref();
   if (borderColor.bottom)
      borderColor.bottom->unref();
   if (borderColor.left)
      borderColor.left->unref();
   if (borderColor.right)
      borderColor.right->unref();
   if (x_tooltip)
      x_tooltip->unref();

   styleTable->remove (this);
   totalRef--;
}

void Style::copyAttrs (StyleAttrs *attrs)
{
   font = attrs->font;
   textDecoration = attrs->textDecoration;
   color = attrs->color;
   backgroundColor = attrs->backgroundColor;
   textAlign = attrs->textAlign;
   valign = attrs->valign;
   textAlignChar = attrs->textAlignChar;
   hBorderSpacing = attrs->hBorderSpacing;
   vBorderSpacing = attrs->vBorderSpacing;
   width = attrs->width;
   height = attrs->height;
   lineHeight = attrs->lineHeight;
   margin = attrs->margin;
   borderWidth = attrs->borderWidth;
   padding = attrs->padding;
   borderColor = attrs->borderColor;
   borderStyle = attrs->borderStyle;
   display = attrs->display;
   whiteSpace = attrs->whiteSpace;
   listStylePosition = attrs->listStylePosition;
   listStyleType = attrs->listStyleType;
   cursor = attrs->cursor;
   x_link = attrs->x_link;
   x_img = attrs->x_img;
   x_tooltip = attrs->x_tooltip;
}

// ----------------------------------------------------------------------

bool FontAttrs::equals(object::Object *other)
{
   FontAttrs *otherAttrs = (FontAttrs*)other;
   return
      this == otherAttrs ||
      (size == otherAttrs->size &&
       weight == otherAttrs->weight &&
       style == otherAttrs->style &&
       letterSpacing == otherAttrs->letterSpacing &&
       strcmp (name, otherAttrs->name) == 0);
}

int FontAttrs::hashValue()
{
   int h = object::String::hashValue (name);
   h = (h << 5) - h + size;
   h = (h << 5) - h + weight;
   h = (h << 5) - h + style;
   h = (h << 5) - h + letterSpacing;
   return h;
}

Font::~Font ()
{
   free ((char*)name);
}

void Font::copyAttrs (FontAttrs *attrs)
{
   name = strdup (attrs->name);
   size = attrs->size;
   weight = attrs->weight;
   style = attrs->style;
   letterSpacing = attrs->letterSpacing;
}

Font *Font::create0 (Layout *layout, FontAttrs *attrs,
                     bool tryEverything)
{
   return layout->createFont (attrs, tryEverything);
}

Font *Font::create (Layout *layout, FontAttrs *attrs)
{
   return create0 (layout, attrs, false);
}

bool Font::exists (Layout *layout, const char *name)
{
   return layout->fontExists (name);
}

// ----------------------------------------------------------------------

bool ColorAttrs::equals(object::Object *other)
{
   ColorAttrs *oc = (ColorAttrs*)other;
   return this == oc || (color == oc->color);
}

int ColorAttrs::hashValue()
{
   return color;
}

Color::~Color ()
{
}

int Color::shadeColor (int color, int d)
{
   int red = (color >> 16) & 255;
   int green = (color >> 8) & 255;
   int blue = color & 255;

   double oldLightness = ((double) misc::max (red, green, blue)) / 255;
   double newLightness;

   if (oldLightness > 0.8) {
      if (d > 0)
         newLightness = oldLightness - 0.2;
      else
         newLightness = oldLightness - 0.4;
   } else if (oldLightness < 0.2) {
      if (d > 0)
         newLightness = oldLightness + 0.4;
      else
         newLightness = oldLightness + 0.2;
   } else
      newLightness = oldLightness + d * 0.2;

   if (oldLightness) {
      double f = (newLightness / oldLightness);
      red = (int)(red * f);
      green = (int)(green * f);
      blue = (int)(blue * f);
   } else {
      red = green = blue = (int)(newLightness * 255);
   }

   return (red << 16) | (green << 8) | blue;
}

int Color::shadeColor (int color, Shading shading)
{
   switch (shading) {
   case SHADING_NORMAL:
      return color;

   case SHADING_LIGHT:
      return shadeColor(color, +1);

   case SHADING_INVERSE:
      return color ^ 0xffffff;

  case SHADING_DARK:
      return shadeColor(color, -1);

   default:
      // compiler happiness
      misc::assertNotReached ();
      return -1;
   }
}


Color *Color::create (Layout *layout, int col)
{
   ColorAttrs attrs(col);

   return layout->createColor (col);
}

Tooltip *Tooltip::create (Layout *layout, const char *text)
{
   return layout->createTooltip (text);
}

// ----------------------------------------------------------------------

static void drawTriangle (View *view, Color *color, Color::Shading shading,
                          int x1, int y1, int x2, int y2, int x3, int y3) {
   int points[3][2];

   points[0][0] = x1;
   points[0][1] = y1;
   points[1][0] = x2;
   points[1][1] = y2;
   points[2][0] = x3;
   points[2][1] = y3;

   view->drawPolygon (color, shading, true, points, 3);
}

/**
 * \brief Draw the border of a region in window, according to style.
 *
 * Used by dw::core::Widget::drawBox and dw::core::Widget::drawWidgetBox.
 */
void drawBorder (View *view, Rectangle *area,
                 int x, int y, int width, int height,
                 Style *style, bool inverse)
{
   /** \todo a lot! */
   Color::Shading dark, light, normal;
   Color::Shading top, right, bottom, left;
   int xb1, yb1, xb2, yb2, xp1, yp1, xp2, yp2;

   // top left and bottom right point of outer border boundary
   xb1 = x + style->margin.left;
   yb1 = y + style->margin.top;
   xb2 = x + width - style->margin.right;
   yb2 = y + height - style->margin.bottom;

   // top left and bottom right point of inner border boundary
   xp1 = xb1 + style->borderWidth.left;
   yp1 = yb1 + style->borderWidth.top;
   xp2 = xb2 - style->borderWidth.right;
   yp2 = yb2 - style->borderWidth.bottom;

   light = inverse ? Color::SHADING_DARK : Color::SHADING_LIGHT;
   dark = inverse ? Color::SHADING_LIGHT : Color::SHADING_DARK;
   normal = inverse ? Color::SHADING_INVERSE : Color::SHADING_NORMAL;

   switch (style->borderStyle.top) {
   case BORDER_INSET:
      top = left = dark;
      right = bottom = light;
      break;

   case BORDER_OUTSET:
      top = left = light;
      right = bottom = dark;
      break;

   default:
      top = right = bottom = left = normal;
      break;
   }

   if (style->borderStyle.top != BORDER_NONE && style->borderColor.top)
      view->drawRectangle(style->borderColor.top, top, true,
                          xb1, yb1, xb2 - xb1, style->borderWidth.top);

   if (style->borderStyle.bottom != BORDER_NONE && style->borderColor.bottom)
      view->drawRectangle(style->borderColor.bottom, bottom, true,
                          xb1, yb2, xb2 - xb1, - style->borderWidth.bottom);

   if (style->borderStyle.left != BORDER_NONE && style->borderColor.left)
      view->drawRectangle(style->borderColor.left, left, true,
                          xb1, yp1, style->borderWidth.left, yp2 - yp1);

   if (style->borderWidth.left > 1) {
      if (style->borderWidth.top > 1 &&
          (style->borderColor.left != style->borderColor.top ||
           left != top))
         drawTriangle (view, style->borderColor.left, left,
                       xb1, yp1, xp1, yp1, xb1, yb1);
      if (style->borderWidth.bottom > 1 &&
          (style->borderColor.left != style->borderColor.bottom ||
           left != bottom))
         drawTriangle (view, style->borderColor.left, left,
                       xb1, yp2, xp1, yp2, xb1, yb2);
   }

   if (style->borderStyle.right != BORDER_NONE && style->borderColor.right)
      view->drawRectangle(style->borderColor.right, right, true,
                          xb2, yp1, - style->borderWidth.right, yp2 - yp1);

   if (style->borderWidth.right > 1) {
      if (style->borderWidth.top > 1 &&
          (style->borderColor.right != style->borderColor.top ||
           right != top))
         drawTriangle (view, style->borderColor.right, right,
                       xb2, yp1, xp2, yp1, xb2, yb1);
      if (style->borderWidth.bottom > 1 &&
          (style->borderColor.right != style->borderColor.bottom ||
           right != bottom))
         drawTriangle (view, style->borderColor.right, right,
                       xb2, yp2, xp2, yp2, xb2, yb2);
   }
}


/**
 * \brief Draw the background (content plus padding) of a region in window,
 *    according to style.
 *
 * Used by dw::core::Widget::drawBox and dw::core::Widget::drawWidgetBox.
 */
void drawBackground (View *view, Rectangle *area,
                     int x, int y, int width, int height,
                     Style *style, bool inverse)
{
   Rectangle bgArea, intersection;

   if (style->backgroundColor) {
      bgArea.x = x + style->margin.left + style->borderWidth.left;
      bgArea.y = y + style->margin.top + style->borderWidth.top;
      bgArea.width =
         width - style->margin.left - style->borderWidth.left -
         style->margin.right - style->borderWidth.right;
      bgArea.height =
         height - style->margin.top - style->borderWidth.top -
         style->margin.bottom - style->borderWidth.bottom;

      if (area->intersectsWith (&bgArea, &intersection))
         view->drawRectangle (style->backgroundColor,
                              inverse ?
                              Color::SHADING_INVERSE : Color::SHADING_NORMAL,
                              true, intersection.x, intersection.y,
                              intersection.width, intersection.height);
   }
}

// ----------------------------------------------------------------------

static const char
   *const roman_I0[] = { "","I","II","III","IV","V","VI","VII","VIII","IX" },
   *const roman_I1[] = { "","X","XX","XXX","XL","L","LX","LXX","LXXX","XC" },
   *const roman_I2[] = { "","C","CC","CCC","CD","D","DC","DCC","DCCC","CM" },
   *const roman_I3[] = { "","M","MM","MMM","MMMM" };

static void strtolower (char *s)
{
   for ( ; *s; s++)
      *s = tolower (*s);
}

/**
 * \brief Convert a number into a string, in a given list style.
 *
 * Used for ordered lists.
 */
void numtostr (int num, char *buf, int buflen, ListStyleType listStyleType)
{
   int i3, i2, i1, i0;
   bool low = false;
   int start_ch = 'A';

   if (buflen <= 0)
      return;

   switch(listStyleType){
   case LIST_STYLE_TYPE_LOWER_ALPHA:
   case LIST_STYLE_TYPE_LOWER_LATIN:
      start_ch = 'a';
   case LIST_STYLE_TYPE_UPPER_ALPHA:
   case LIST_STYLE_TYPE_UPPER_LATIN:
      i0 = num - 1;
      i1 = i0/26 - 1; i2 = i1/26 - 1;
      if (i2 > 25) /* more than 26+26^2+26^3=18278 elements ? */
         snprintf(buf, buflen, "****.");
      else
         snprintf(buf, buflen, "%c%c%c.",
                 i2<0 ? ' ' : start_ch + i2%26,
                 i1<0 ? ' ' : start_ch + i1%26,
                 i0<0 ? ' ' : start_ch + i0%26);
      break;
   case LIST_STYLE_TYPE_LOWER_ROMAN:
      low = true;
   case LIST_STYLE_TYPE_UPPER_ROMAN:
      i0 = num;
      i1 = i0/10; i2 = i1/10; i3 = i2/10;
      i0 %= 10;   i1 %= 10;   i2 %= 10;
      if (num < 0 || i3 > 4) /* more than 4999 elements ? */
         snprintf(buf, buflen, "****.");
      else
         snprintf(buf, buflen, "%s%s%s%s.", roman_I3[i3], roman_I2[i2],
                  roman_I1[i1], roman_I0[i0]);
      break;
   case LIST_STYLE_TYPE_DECIMAL:
   default:
      snprintf(buf, buflen, "%d.", num);
      break;
   }

   // ensure termination
   buf[buflen - 1] = '\0';

   if (low)
      strtolower(buf);

}

} // namespace style
} // namespace dw
} // namespace core
