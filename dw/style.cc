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
#include <math.h>

#include "core.hh"
#include "../lout/msg.h"

using namespace lout;

namespace dw {
namespace core {
namespace style {

const bool drawBackgroundLineByLine = false;

const int MIN_BG_IMG_W = 10;
const int MIN_BG_IMG_H = 10;
const int OPT_BG_IMG_W = 50;
const int OPT_BG_IMG_H = 50;

static void calcBackgroundRelatedValues (StyleImage *backgroundImage,
                                         BackgroundRepeat backgroundRepeat,
                                         BackgroundAttachment
                                         backgroundAttachment,
                                         Length backgroundPositionX,
                                         Length backgroundPositionY,
                                         int xDraw, int yDraw, int widthDraw,
                                         int heightDraw, int xRef, int yRef,
                                         int widthRef, int heightRef,
                                         bool *repeatX, bool *repeatY,
                                         int *origX, int *origY,
                                         int *tileX1, int *tileX2, int *tileY1,
                                         int *tileY2, bool *doDraw);

void StyleAttrs::initValues ()
{
   x_link = -1;
   x_lang[0] = x_lang[1] = 0;
   x_img = -1;
   x_tooltip = NULL;
   textDecoration = TEXT_DECORATION_NONE;
   textAlign = TEXT_ALIGN_LEFT;
   textAlignChar = '.';
   textTransform = TEXT_TRANSFORM_NONE;
   listStylePosition = LIST_STYLE_POSITION_OUTSIDE;
   listStyleType = LIST_STYLE_TYPE_DISC;
   valign = VALIGN_BASELINE;
   backgroundColor = NULL;
   backgroundImage = NULL;
   backgroundRepeat = BACKGROUND_REPEAT;
   backgroundAttachment = BACKGROUND_ATTACHMENT_SCROLL;
   backgroundPositionX = createPerLength (0);
   backgroundPositionY = createPerLength (0);
   width = height = lineHeight = LENGTH_AUTO;
   textIndent = 0;
   margin.setVal (0);
   borderWidth.setVal (0);
   padding.setVal (0);
   borderCollapse = BORDER_MODEL_SEPARATE;
   setBorderColor (NULL);
   setBorderStyle (BORDER_NONE);
   hBorderSpacing = 0;
   vBorderSpacing = 0;
   wordSpacing = 0;

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
   backgroundImage = NULL;
   backgroundRepeat = BACKGROUND_REPEAT;
   backgroundAttachment = BACKGROUND_ATTACHMENT_SCROLL;
   backgroundPositionX = createPerLength (0);
   backgroundPositionY = createPerLength (0);
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
       backgroundImage == otherAttrs->backgroundImage &&
       backgroundRepeat == otherAttrs->backgroundRepeat &&
       backgroundAttachment == otherAttrs->backgroundAttachment &&
       backgroundPositionX == otherAttrs->backgroundPositionX &&
       backgroundPositionY == otherAttrs->backgroundPositionY &&
       textAlign == otherAttrs->textAlign &&
       valign == otherAttrs->valign &&
       textAlignChar == otherAttrs->textAlignChar &&
       textTransform == otherAttrs->textTransform &&
       hBorderSpacing == otherAttrs->hBorderSpacing &&
       vBorderSpacing == otherAttrs->vBorderSpacing &&
       wordSpacing == otherAttrs->wordSpacing &&
       width == otherAttrs->width &&
       height == otherAttrs->height &&
       lineHeight == otherAttrs->lineHeight &&
       textIndent == otherAttrs->textIndent &&
       margin.equals (&otherAttrs->margin) &&
       borderWidth.equals (&otherAttrs->borderWidth) &&
       padding.equals (&otherAttrs->padding) &&
       borderCollapse == otherAttrs->borderCollapse &&
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
       x_lang[0] == otherAttrs->x_lang[0] &&
       x_lang[1] == otherAttrs->x_lang[1] &&
       x_img == otherAttrs->x_img &&
       x_tooltip == otherAttrs->x_tooltip);
}

int StyleAttrs::hashValue () {
   return (intptr_t) font +
      textDecoration +
      (intptr_t) color +
      (intptr_t) backgroundColor +
      (intptr_t) backgroundImage +
      backgroundRepeat +
      backgroundAttachment +
      backgroundPositionX +
      backgroundPositionY +
      textAlign +
      valign +
      textAlignChar +
      textTransform +
      hBorderSpacing +
      vBorderSpacing +
      wordSpacing +
      width +
      height +
      lineHeight +
      textIndent +
      margin.hashValue () +
      borderWidth.hashValue () +
      padding.hashValue () +
      borderCollapse +
      (intptr_t) borderColor.top +
      (intptr_t) borderColor.right +
      (intptr_t) borderColor.bottom +
      (intptr_t) borderColor.left +
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
      x_lang[0] + x_lang[1] +
      x_img +
      (intptr_t) x_tooltip;
}

int Style::totalRef = 0;
container::typed::HashTable <StyleAttrs, Style> * Style::styleTable =
   new container::typed::HashTable <StyleAttrs, Style> (false, false, 1024);

Style::Style (StyleAttrs *attrs)
{
   DBG_OBJ_CREATE ("dw::core::style::Style");

   copyAttrs (attrs);

   DBG_OBJ_ASSOC_CHILD (font);
   DBG_OBJ_ASSOC_CHILD (color);
   DBG_OBJ_ASSOC_CHILD (backgroundColor);
   DBG_OBJ_ASSOC_CHILD (backgroundImage);
   DBG_OBJ_ASSOC_CHILD (borderColor.top);
   DBG_OBJ_ASSOC_CHILD (borderColor.bottom);
   DBG_OBJ_ASSOC_CHILD (borderColor.left);
   DBG_OBJ_ASSOC_CHILD (borderColor.right);
   //DBG_OBJ_ASSOC_CHILD (x_tooltip);

   refCount = 1;

   font->ref ();
   if (color)
      color->ref ();
   if (backgroundColor)
      backgroundColor->ref ();
   if (backgroundImage)
      backgroundImage->ref ();
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
   if (backgroundImage)
      backgroundImage->unref ();
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

   DBG_OBJ_DELETE ();
}

void Style::copyAttrs (StyleAttrs *attrs)
{
   font = attrs->font;
   textDecoration = attrs->textDecoration;
   color = attrs->color;
   backgroundColor = attrs->backgroundColor;
   backgroundImage = attrs->backgroundImage;
   backgroundRepeat = attrs->backgroundRepeat;
   backgroundAttachment = attrs->backgroundAttachment;
   backgroundPositionX = attrs->backgroundPositionX;
   backgroundPositionY = attrs->backgroundPositionY;
   textAlign = attrs->textAlign;
   valign = attrs->valign;
   textAlignChar = attrs->textAlignChar;
   textTransform = attrs->textTransform;
   hBorderSpacing = attrs->hBorderSpacing;
   vBorderSpacing = attrs->vBorderSpacing;
   wordSpacing = attrs->wordSpacing;
   width = attrs->width;
   height = attrs->height;
   lineHeight = attrs->lineHeight;
   textIndent = attrs->textIndent;
   margin = attrs->margin;
   borderWidth = attrs->borderWidth;
   padding = attrs->padding;
   borderCollapse = attrs->borderCollapse;
   borderColor = attrs->borderColor;
   borderStyle = attrs->borderStyle;
   display = attrs->display;
   whiteSpace = attrs->whiteSpace;
   listStylePosition = attrs->listStylePosition;
   listStyleType = attrs->listStyleType;
   cursor = attrs->cursor;
   x_link = attrs->x_link;
   x_lang[0] = attrs->x_lang[0];
   x_lang[1] = attrs->x_lang[1];
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
       fontVariant == otherAttrs->fontVariant &&
       strcmp (name, otherAttrs->name) == 0);
}

int FontAttrs::hashValue()
{
   int h = object::String::hashValue (name);
   h = (h << 5) - h + size;
   h = (h << 5) - h + weight;
   h = (h << 5) - h + style;
   h = (h << 5) - h + letterSpacing;
   h = (h << 5) - h + fontVariant;
   return h;
}

Font::~Font ()
{
   free ((char*)name);
   DBG_OBJ_DELETE ();
}

void Font::copyAttrs (FontAttrs *attrs)
{
   name = strdup (attrs->name);
   size = attrs->size;
   weight = attrs->weight;
   style = attrs->style;
   letterSpacing = attrs->letterSpacing;
   fontVariant = attrs->fontVariant;
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
   DBG_OBJ_DELETE ();
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

void StyleImage::StyleImgRenderer::setBuffer (core::Imgbuf *buffer, bool resize)
{
   if (image->imgbufSrc)
      image->imgbufSrc->unref ();
   if (image->imgbufTiled)
      image->imgbufTiled->unref ();

   image->imgbufTiled = NULL;

   image->imgbufSrc = buffer;
   DBG_OBJ_ASSOC (image, image->imgbufSrc);

   if (image->imgbufSrc) {
      image->imgbufSrc->ref ();

      // If the image is too small, drawing a background will cause
      // many calls of View::drawImgbuf. For this reason, we create
      // another image buffer, the "tiled" image buffer, which is
      // larger (the "optimal" size is defined as OPT_BG_IMG_W *
      // OPT_BG_IMG_H) and contains the "source" buffer several times.
      //
      // This "tiled" buffer is not used when 'background-repeat' has
      // another value than 'repeat', for obvious reasons. Image
      // buffers only "tiled" in one dimension (to optimize 'repeat-x'
      // and 'repeat-y') are not supported.

      if (image->imgbufSrc->getRootWidth() * image->imgbufSrc->getRootHeight()
          < MIN_BG_IMG_W * MIN_BG_IMG_H) {
         image->tilesX =
            misc::max (OPT_BG_IMG_W / image->imgbufSrc->getRootWidth(), 1);
         image->tilesY =
            misc::max (OPT_BG_IMG_H / image->imgbufSrc->getRootHeight(), 1);
         image->imgbufTiled =
            image->imgbufSrc->createSimilarBuf
               (image->tilesX * image->imgbufSrc->getRootWidth(),
                image->tilesY * image->imgbufSrc->getRootHeight());

         DBG_OBJ_ASSOC (image, image->imgbufTiled);
      }
   }
}

void StyleImage::StyleImgRenderer::drawRow (int row)
{
   if (image->imgbufTiled) {
      // A row of data has been copied to the source buffer, here it
      // is copied into the tiled buffer.

      // Unfortunately, this code may be called *after* some other
      // implementations of ImgRenderer::drawRow, which actually
      // *draw* the tiled buffer, which is so not up to date
      // (ImgRendererDist does not define an order). OTOH, these
      // drawing implementations calle Widget::queueResize, so the
      // actual drawing (and so access to the tiled buffer) is done
      // later.

      int w = image->imgbufSrc->getRootWidth ();
      int h = image->imgbufSrc->getRootHeight ();

      for (int x = 0; x < image->tilesX; x++)
         for (int y = 0; y < image->tilesX; y++)
            image->imgbufSrc->copyTo (image->imgbufTiled, x * w, y * h,
                                      0, row, w, 1);
   }
}

void StyleImage::StyleImgRenderer::finish ()
{
   // Nothing to do.
}

void StyleImage::StyleImgRenderer::fatal ()
{
   // Nothing to do.
}

StyleImage::StyleImage ()
{
   DBG_OBJ_CREATE ("dw::core::style::StyleImage");

   refCount = 0;
   imgbufSrc = NULL;
   imgbufTiled = NULL;

   imgRendererDist = new ImgRendererDist ();
   styleImgRenderer = new StyleImgRenderer (this);
   imgRendererDist->put (styleImgRenderer);
}

StyleImage::~StyleImage ()
{
   if (imgbufSrc)
      imgbufSrc->unref ();
   if (imgbufTiled)
      imgbufTiled->unref ();

   delete imgRendererDist;
   delete styleImgRenderer;

   DBG_OBJ_DELETE ();
}

void StyleImage::ExternalImgRenderer::setBuffer (core::Imgbuf *buffer,
                                                 bool resize)
{
   // Nothing to do?
}

void StyleImage::ExternalImgRenderer::drawRow (int row)
{
   if (drawBackgroundLineByLine) {
      StyleImage *backgroundImage;
      if (readyToDraw () && (backgroundImage = getBackgroundImage ())) {
         // All single rows are drawn.

         Imgbuf *imgbuf = backgroundImage->getImgbufSrc();
         int imgWidth = imgbuf->getRootWidth ();
         int imgHeight = imgbuf->getRootHeight ();

         int x, y, width, height;
         getBgArea (&x, &y, &width, &height);

         int xRef, yRef, widthRef, heightRef;
         getRefArea (&xRef, &yRef, &widthRef, &heightRef);

         bool repeatX, repeatY, doDraw;
         int origX, origY, tileX1, tileX2, tileY1, tileY2;

         calcBackgroundRelatedValues (backgroundImage,
                                      getBackgroundRepeat (),
                                      getBackgroundAttachment (),
                                      getBackgroundPositionX (),
                                      getBackgroundPositionY (),
                                      x, y, width, height, xRef, yRef, widthRef,
                                      heightRef, &repeatX, &repeatY, &origX,
                                      &origY, &tileX1, &tileX2, &tileY1,
                                      &tileY2, &doDraw);

         //printf ("tileX1 = %d, tileX2 = %d, tileY1 = %d, tileY2 = %d\n",
         //        tileX1, tileX2, tileY1, tileY2);

         if (doDraw)
            // Only iterate over y, because the rows can be combined
            // horizontally.
            for (int tileY = tileY1; tileY <= tileY2; tileY++) {
               int x1 = misc::max (origX + tileX1 * imgWidth, x);
               int x2 = misc::min (origX + (tileX2 + 1) * imgWidth, x + width);

               int yt = origY + tileY * imgHeight + row;
               if (yt >= y && yt < y + height)
                  draw (x1, yt, x2 - x1, 1);
            }
      }
   }
}

void StyleImage::ExternalImgRenderer::finish ()
{
   if (!drawBackgroundLineByLine) {
      if (readyToDraw ()) {
         // Draw total area, as a whole.
         int x, y, width, height;
         getBgArea (&x, &y, &width, &height);
         draw (x, y, width, height);
      }
   }
}

void StyleImage::ExternalImgRenderer::fatal ()
{
   // Nothing to do.
}

// ----------------------------------------------------------------------

StyleImage *StyleImage::ExternalWidgetImgRenderer::getBackgroundImage ()
{
   Style *style = getStyle ();
   return style ? style->backgroundImage : NULL;
}

BackgroundRepeat StyleImage::ExternalWidgetImgRenderer::getBackgroundRepeat ()
{
   Style *style = getStyle ();
   return style ? style->backgroundRepeat : BACKGROUND_REPEAT;
}

BackgroundAttachment
   StyleImage::ExternalWidgetImgRenderer::getBackgroundAttachment ()
{
   Style *style = getStyle ();
   return style ? style->backgroundAttachment : BACKGROUND_ATTACHMENT_SCROLL;
}

Length StyleImage::ExternalWidgetImgRenderer::getBackgroundPositionX ()
{
   Style *style = getStyle ();
   return style ? style->backgroundPositionX : createPerLength (0);
}

Length StyleImage::ExternalWidgetImgRenderer::getBackgroundPositionY ()
{
   Style *style = getStyle ();
   return style ? style->backgroundPositionY : createPerLength (0);
}

// ----------------------------------------------------------------------

/*
 * The drawBorder{Top,Bottom,Left,Right} functions are similar. They
 * use a trapezium as draw polygon, or drawTypedLine() for dots and dashes.
 * Although the concept is simple, achieving pixel accuracy is laborious [1].
 *
 * [1] http://www.dillo.org/css_compat/tests/border-style.html
 */
static void drawBorderTop(View *view, Style *style,
                          int x1, int y1, int x2, int y2)

{
   int d, w;
   Point points[4];
   const bool filled = true, convex = true;
   bool ridge = false, inset = false, dotted = false;
   Color::Shading shading = Color::SHADING_NORMAL;

   if (!style->borderColor.top || style->borderWidth.top == 0)
      return;

   switch (style->borderStyle.top) {
   case BORDER_NONE:
   case BORDER_HIDDEN:
      break;
   case BORDER_DOTTED:
      dotted = true;
   case BORDER_DASHED:
      w = style->borderWidth.top;
      view->drawTypedLine(style->borderColor.top, shading,
                          dotted ? LINE_DOTTED : LINE_DASHED,
                          w, x1+w/2, y1+w/2, x2-w/2, y2+w/2);
      break;
   case BORDER_SOLID:
   case BORDER_INSET:
      inset = true;
   case BORDER_OUTSET:
      if (style->borderStyle.top != BORDER_SOLID)
         shading = (inset) ? Color::SHADING_DARK : Color::SHADING_LIGHT;

      if (style->borderWidth.top == 1) {
         view->drawLine(style->borderColor.top, shading, x1, y1, x2, y2);
      } else {
         points[0].x = x1;
         points[1].x = x2 + 1;
         points[0].y = points[1].y = y1;
         points[2].x = points[1].x - style->borderWidth.right;
         points[3].x = x1 + style->borderWidth.left;
         points[2].y = points[3].y = points[0].y + style->borderWidth.top;
         view->drawPolygon (style->borderColor.top, shading, filled, convex,
                            points, 4);
      }
      break;
   case BORDER_RIDGE:
      ridge = true;
   case BORDER_GROOVE:
      d = style->borderWidth.top & 1;
      points[0].x = x1;
      points[1].x = x2 + 1;
      points[0].y = points[1].y = y1;
      points[2].x = x2 - style->borderWidth.right / 2;
      points[3].x = x1 + style->borderWidth.left / 2;
      points[2].y = points[3].y = y1 + style->borderWidth.top / 2 + d;
      shading = (ridge) ? Color::SHADING_LIGHT : Color::SHADING_DARK;
      view->drawPolygon (style->borderColor.top, shading, filled, convex,
                         points, 4);
      points[0].x = x1 + style->borderWidth.left / 2 + d;
      points[1].x = x2 - style->borderWidth.right / 2 + 1 - d;
      points[0].y = points[1].y = y1 + style->borderWidth.top / 2 + d;
      points[2].x = x2 - style->borderWidth.right + 1 - d;
      points[3].x = x1 + style->borderWidth.left;
      points[2].y = points[3].y = y1 + style->borderWidth.top;
      shading = (ridge) ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      view->drawPolygon (style->borderColor.top, shading, filled, convex,
                         points, 4);
      break;
   case BORDER_DOUBLE:
      w = (int) rint(style->borderWidth.top / 3.0);
      d = w ? style->borderWidth.top - 2 * w : 0;
      int w_l = (int) rint(style->borderWidth.left / 3.0);
      int w_r = (int) rint(style->borderWidth.right / 3.0);
      if (style->borderWidth.top == 1) {
         view->drawLine(style->borderColor.top, shading, x1, y1, x2, y2);
         break;
      }
      points[0].x = x1;
      points[1].x = x2 + 1;
      points[0].y = points[1].y = y1;
      points[2].x = points[1].x - w_r;
      points[3].x = points[0].x + w_l;
      points[2].y = points[3].y = points[0].y + w;
      view->drawPolygon (style->borderColor.top, shading, filled, convex,
                         points, 4);
      points[0].x = x1 + style->borderWidth.left - w_l;
      points[1].x = x2 + 1 - style->borderWidth.right + w_r;
      points[0].y = points[1].y = y1 + w + d;
      points[2].x = x2 + 1 - style->borderWidth.right;
      points[3].x = x1 + style->borderWidth.left;
      points[2].y = points[3].y = y1 + style->borderWidth.top;
      view->drawPolygon (style->borderColor.top, shading, filled, convex,
                         points, 4);
      break;
   }
}

static void drawBorderBottom(View *view, Style *style,
                             int x1, int y1, int x2, int y2)

{
   int d, w;
   Point points[4];
   const bool filled = true, convex = true;
   bool ridge = false, inset = false, dotted = false;
   Color::Shading shading = Color::SHADING_NORMAL;

   if (!style->borderColor.bottom || style->borderWidth.bottom == 0)
      return;

   switch (style->borderStyle.bottom) {
   case BORDER_NONE:
   case BORDER_HIDDEN:
      break;
   case BORDER_DOTTED:
      dotted = true;
   case BORDER_DASHED:
      w = style->borderWidth.bottom;
      view->drawTypedLine(style->borderColor.bottom, shading,
                          dotted ? LINE_DOTTED : LINE_DASHED,
                          w, x1+w/2, y1-w/2, x2-w/2, y2-w/2);
      break;
   case BORDER_SOLID:
   case BORDER_INSET:
      inset = true;
   case BORDER_OUTSET:
      if (style->borderStyle.bottom != BORDER_SOLID)
         shading = (inset) ? Color::SHADING_LIGHT : Color::SHADING_DARK;

      if (style->borderWidth.bottom == 1) { /* 1 pixel line */
         view->drawLine(style->borderColor.bottom, shading, x1, y1, x2, y2);
      } else {
         points[0].x = x1 - 1;
         points[1].x = x2 + 2;
         points[0].y = points[1].y = y1 + 1;
         points[2].x = points[1].x - style->borderWidth.right;
         points[3].x = points[0].x + style->borderWidth.left;
         points[2].y = points[3].y = points[0].y-style->borderWidth.bottom;
         view->drawPolygon (style->borderColor.bottom, shading, filled, convex,
                            points, 4);
      }
      break;
   case BORDER_RIDGE:
      ridge = true;
   case BORDER_GROOVE:
      w = style->borderWidth.bottom;
      d = w & 1;
      points[0].x = x1 - 1;
      points[1].x = x2 + 2 - d;
      points[0].y = points[1].y = y1 + 1;
      points[2].x = points[1].x - style->borderWidth.right / 2;
      points[3].x = points[0].x + style->borderWidth.left / 2 + d;
      points[2].y = points[3].y = points[0].y - w/2 - d;
      shading = (ridge) ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      view->drawPolygon (style->borderColor.bottom, shading, filled, convex,
                         points, 4);
      // clockwise
      points[0].x = x1 + style->borderWidth.left - 1;
      points[1].x = x2 + 1 - style->borderWidth.right + 1;
      points[0].y = points[1].y = y1 - w + 1;
      points[2].x = points[1].x + style->borderWidth.right / 2;
      points[3].x = points[0].x - style->borderWidth.left / 2;
      points[2].y = points[3].y = points[0].y + w/2;
      shading = (ridge) ? Color::SHADING_LIGHT : Color::SHADING_DARK;
      view->drawPolygon (style->borderColor.bottom, shading, filled, convex,
                         points, 4);
      break;
   case BORDER_DOUBLE:
      w = (int) rint(style->borderWidth.bottom / 3.0);
      d = w ? style->borderWidth.bottom - 2 * w : 0;
      int w_l = (int) rint(style->borderWidth.left / 3.0);
      int w_r = (int) rint(style->borderWidth.right / 3.0);
      if (style->borderWidth.bottom == 1) {
         view->drawLine(style->borderColor.bottom, shading, x1, y1, x2, y2);
         break;
      }
      points[0].x = x2 + 2;
      points[1].x = x1 - 1;
      points[0].y = points[1].y = y1 + 1;
      points[2].x = points[1].x + w_l;
      points[3].x = points[0].x - w_r;
      points[2].y = points[3].y = points[0].y - w;
      view->drawPolygon (style->borderColor.bottom, shading, filled, convex,
                         points, 4);
      points[0].x = x2 + 2 - style->borderWidth.right + w_r;
      points[1].x = x1 - 1 + style->borderWidth.left - w_l;
      points[0].y = points[1].y = y1 + 1 - w - d;
      points[2].x = x1 - 1 + style->borderWidth.left;
      points[3].x = x2 + 2 - style->borderWidth.right;
      points[2].y = points[3].y = y1 + 1 - style->borderWidth.bottom;
      view->drawPolygon (style->borderColor.bottom, shading, filled, convex,
                         points, 4);
      break;
   }
}

static void drawBorderLeft(View *view, Style *style,
                           int x1, int y1, int x2, int y2)

{
   int d, w;
   Point points[4];
   bool filled = true, convex = true;
   bool ridge = false, inset = false, dotted = false;
   Color::Shading shading = Color::SHADING_NORMAL;

   if (!style->borderColor.left || style->borderWidth.left == 0)
      return;

   switch (style->borderStyle.left) {
   case BORDER_NONE:
   case BORDER_HIDDEN:
      break;
   case BORDER_DOTTED:
      dotted = true;
   case BORDER_DASHED:
      w = style->borderWidth.left;
      view->drawTypedLine(style->borderColor.left, shading,
                          dotted ? LINE_DOTTED : LINE_DASHED,
                          w, x1+w/2, y1+w/2, x1+w/2, y2-w/2);
      break;
   case BORDER_SOLID:
   case BORDER_INSET:
      inset = true;
   case BORDER_OUTSET:
      if (style->borderStyle.left != BORDER_SOLID)
         shading = (inset) ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      if (style->borderWidth.left == 1) { /* 1 pixel line */
         view->drawLine(style->borderColor.left, shading, x1, y1, x2, y2);
      } else {
         points[0].x = points[1].x = x1;
         points[0].y = y1 - 1;
         points[1].y = y2 + 1;
         points[2].x = points[3].x = points[0].x + style->borderWidth.left;
         points[2].y = points[1].y - style->borderWidth.bottom;
         points[3].y = points[0].y + style->borderWidth.top;
         view->drawPolygon (style->borderColor.left, shading, filled, convex,
                            points, 4);
      }
      break;
   case BORDER_RIDGE:
      ridge = true;
   case BORDER_GROOVE:
      w = style->borderWidth.left;
      d = w & 1;
      points[0].x = points[1].x = x1;
      points[0].y = y1;
      points[1].y = y2;
      points[2].x = points[3].x = x1 + w / 2 + d;
      points[2].y = y2 - style->borderWidth.bottom / 2;
      points[3].y = y1 + style->borderWidth.top / 2;
      shading = (ridge) ? Color::SHADING_LIGHT : Color::SHADING_DARK;
      view->drawPolygon (style->borderColor.left, shading, filled, convex,
                         points, 4);
      points[0].x = points[1].x = x1 + w / 2 + d;
      points[0].y = y1 + style->borderWidth.top / 2;
      points[1].y = y2 - style->borderWidth.bottom / 2;
      points[2].x = points[3].x = x1 + w;
      points[2].y = y2 - style->borderWidth.bottom;
      points[3].y = y1 + style->borderWidth.top;
      shading = (ridge) ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      view->drawPolygon (style->borderColor.left, shading, filled, convex,
                         points, 4);
      break;
   case BORDER_DOUBLE:
      w = (int) rint(style->borderWidth.left / 3.0);
      d = w ? style->borderWidth.left - 2 * w : 0;
      int w_b = (int) rint(style->borderWidth.bottom / 3.0);
      int w_t = (int) rint(style->borderWidth.top / 3.0);
      if (style->borderWidth.left == 1) {
         view->drawLine(style->borderColor.left, shading, x1, y1, x2, y2-1);
         break;
      }
      points[0].x = points[1].x = x1;
      points[0].y = y1 - 1;
      points[1].y = y2 + 1;
      points[2].x = points[3].x = points[0].x + w;
      points[2].y = points[1].y - w_b;
      points[3].y = points[0].y + w_t;
      view->drawPolygon (style->borderColor.left, shading, filled, convex,
                         points, 4);
      points[0].x = points[1].x = x1 + w + d;
      points[0].y = y1 - 1 + style->borderWidth.top - w_t;
      points[1].y = y2 + 1 - style->borderWidth.bottom + w_b;
      points[2].x = points[3].x = points[0].x + w;
      points[2].y = y2 + 1 - style->borderWidth.bottom;
      points[3].y = y1 - 1 + style->borderWidth.top;
      view->drawPolygon (style->borderColor.left, shading, filled, convex,
                         points, 4);
      break;
   }
}

static void drawBorderRight(View *view, Style *style,
                            int x1, int y1, int x2, int y2)

{
   int d, w;
   Point points[4];
   const bool filled = true, convex = true;
   bool ridge = false, inset = false, dotted = false;
   Color::Shading shading = Color::SHADING_NORMAL;

   if (!style->borderColor.right || style->borderWidth.right == 0)
      return;

   switch (style->borderStyle.right) {
   case BORDER_NONE:
   case BORDER_HIDDEN:
      break;
   case BORDER_DOTTED:
      dotted = true;
   case BORDER_DASHED:
      w = style->borderWidth.right;
      view->drawTypedLine(style->borderColor.right, shading,
                          dotted ? LINE_DOTTED : LINE_DASHED,
                          w, x1 - w/2, y1 + w/2, x1 - w/2, y2 - w/2);
      break;
   case BORDER_SOLID:
   case BORDER_INSET:
      inset = true;
   case BORDER_OUTSET:
      if (style->borderStyle.right != BORDER_SOLID)
         shading = (inset) ? Color::SHADING_LIGHT : Color::SHADING_DARK;
      if (style->borderWidth.right == 1) { /* 1 pixel line */
         view->drawLine(style->borderColor.right, shading, x1, y1, x2, y2);
      } else {
         points[0].x = points[1].x = x1 + 1;
         points[0].y = y1 - 1;
         points[1].y = y2 + 1;
         points[2].x = points[3].x = points[0].x-style->borderWidth.right;
         points[2].y = points[1].y - style->borderWidth.bottom;
         points[3].y = points[0].y + style->borderWidth.top;
         view->drawPolygon (style->borderColor.right, shading, filled, convex,
                            points,4);
      }
      break;
   case BORDER_RIDGE:
      ridge = true;
   case BORDER_GROOVE:
      w = style->borderWidth.right;
      d = w & 1;
      points[0].x = points[1].x = x1 + 1;
      points[0].y = y1;
      points[1].y = y2;
      points[2].x = points[3].x = points[0].x - w / 2 - d;
      points[2].y = y2 - style->borderWidth.bottom / 2;
      points[3].y = points[0].y + style->borderWidth.top / 2;
      shading = (ridge) ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      view->drawPolygon (style->borderColor.right, shading, filled, convex,
                         points, 4);
      points[0].x = points[1].x = x1 + 1 - w / 2 - d;
      points[0].y = y1 + style->borderWidth.top / 2;
      points[1].y = y2 - style->borderWidth.bottom / 2;
      points[2].x = points[3].x = x1 + 1 - w;
      points[2].y = y2 - style->borderWidth.bottom;
      points[3].y = y1 + style->borderWidth.top;
      shading = (ridge) ? Color::SHADING_LIGHT: Color::SHADING_DARK;
      view->drawPolygon (style->borderColor.right, shading, filled, convex,
                         points, 4);
      break;
   case BORDER_DOUBLE:
      w = (int) rint(style->borderWidth.right / 3.0);
      d = w ? style->borderWidth.right - 2 * w : 0;
      int w_b = (int) rint(style->borderWidth.bottom / 3.0);
      int w_t = (int) rint(style->borderWidth.top / 3.0);
      if (style->borderWidth.right == 1) {
         view->drawLine(style->borderColor.right, shading, x1, y1, x2, y2);
         break;
      }
      points[0].x = points[1].x = x1 + 1;
      points[0].y = y1 - 1;
      points[1].y = y2 + 1;
      points[2].x = points[3].x = points[0].x - w;
      points[2].y = points[1].y - w_b;
      points[3].y = points[0].y + w_t;
      view->drawPolygon (style->borderColor.right, shading, filled, convex,
                         points, 4);
      points[0].x = points[1].x = x1 + 1 - w - d;
      points[0].y = y1 - 1 + style->borderWidth.top - w_t;
      points[1].y = y2 + 1 - style->borderWidth.bottom + w_b;
      points[2].x = points[3].x = points[0].x - w;
      points[2].y = y2 + 1 - style->borderWidth.bottom;
      points[3].y = y1 - 1 + style->borderWidth.top;
      view->drawPolygon (style->borderColor.right, shading, filled, convex,
                         points, 4);
      break;
   }
}

/**
 * \brief Draw the border of a region in window, according to style.
 *
 * Used by dw::core::Widget::drawBox and dw::core::Widget::drawWidgetBox.
 *
 * "area" is the area to be drawn, "x", "y", "width" and "height"
 * define the box itself. All are given in canvas coordinates.
 */
void drawBorder (View *view, Layout *layout, Rectangle *area,
                 int x, int y, int width, int height,
                 Style *style, bool inverse)
{
   /** \todo a lot! */
   int xb1, yb1, xb2, yb2;

   // top left and bottom right point of outer border boundary
   xb1 = x + style->margin.left;
   yb1 = y + style->margin.top;
   xb2 = x + (width > 0 ? width - 1 : 0) - style->margin.right;
   yb2 = y + (height > 0 ? height - 1 : 0) - style->margin.bottom;

   /*
      // top left and bottom right point of inner border boundary
      xp1 = xb1 + style->borderWidth.left;
      yp1 = yb1 + style->borderWidth.top;
      xp2 = xb2 - style->borderWidth.right;
      yp2 = yb2 - style->borderWidth.bottom;

      light = inverse ? Color::SHADING_DARK : Color::SHADING_LIGHT;
      dark = inverse ? Color::SHADING_LIGHT : Color::SHADING_DARK;
      normal = inverse ? Color::SHADING_INVERSE : Color::SHADING_NORMAL;
   */

   drawBorderRight(view, style, xb2, yb1, xb2, yb2);
   drawBorderLeft(view, style, xb1, yb1, xb1, yb2);
   drawBorderTop(view, style, xb1, yb1, xb2, yb1);
   drawBorderBottom(view, style, xb1, yb2, xb2, yb2);
}


/**
 * \brief Draw the background (content plus padding) of a region in window,
 *    according to style.
 *
 * Used by dw::core::Widget::drawBox and dw::core::Widget::drawWidgetBox.
 *
 * "area" is the area to be drawn, "x", "y", "width" and "height"
 * define the box itself (padding box). "xRef", "yRef", "widthRef" and
 * "heightRef" define the reference area, which is important for the
 * tiling of background images (for position 0%/0%, a tile is set at
 * xRef/yRef; for position 100%/100%, a tile is set at xRef +
 * widthRef/yRef + widthRef). See calls for more informations; in most
 * cases, these boxes are identical (padding box). All these
 * coordinates are given in canvas coordinates.
 *
 * "atTop" should be true, only if the area is drawn directly on the
 * canvas, not on top of other areas; this is only true for the
 * toplevel widget itself (not parts of its contents). Toplevel widget
 * background colors are already set as viewport background color, so
 * that drawing again is is not neccessary, but some time can be
 * saved.
 *
 * Otherwise, the caller should not try to increase the performance by
 * doing some tests before; this is all done in this method.
 */
void drawBackground (View *view, Layout *layout, Rectangle *area,
                     int x, int y, int width, int height,
                     int xRef, int yRef, int widthRef, int heightRef,
                     Style *style, bool inverse, bool atTop)
{
   bool bgColor = style->backgroundColor != NULL &&
      // The test for background colors is rather simple, since only the color
      // has to be compared, ...
      (!atTop || layout->getBgColor () != style->backgroundColor);
   bool bgImage = (style->backgroundImage != NULL &&
                   style->backgroundImage->getImgbufSrc() != NULL) &&
      // ... but for backgrounds, it would be rather complicated. To handle the
      // two cases (normal HTML in a viewport, where the layout background
      // image is set, and contents of <button> within a flat view, where the
      // background image of the toplevel widget is set), only the background
      // images are compared. A full test, which also deals with all other
      // attributes related to background images (repeat, position etc.) would
      // be complicated and useless, so not worth the work.
      (!atTop || layout->getBgImage () != style->backgroundImage);

   // Since widgets are always drawn from top to bottom, it is *not*
   // necessary to draw the background if background color and image
   // are not set (NULL), i. e. shining through.

   if (bgColor || bgImage) {
      Rectangle bgArea, intersection;
      bgArea.x = x;
      bgArea.y = y;
      bgArea.width = width;
      bgArea.height = height;

      if (area->intersectsWith (&bgArea, &intersection)) {
         if (bgColor)
            view->drawRectangle (style->backgroundColor,
                                 inverse ?
                                 Color::SHADING_INVERSE : Color::SHADING_NORMAL,
                                 true, intersection.x, intersection.y,
                                 intersection.width, intersection.height);

         if (bgImage)
            drawBackgroundImage (view, style->backgroundImage,
                                 style->backgroundRepeat,
                                 style->backgroundAttachment,
                                 style->backgroundPositionX,
                                 style->backgroundPositionY,
                                 intersection.x, intersection.y,
                                 intersection.width, intersection.height,
                                 xRef, yRef, widthRef, heightRef);

      }
   }
}

void drawBackgroundImage (View *view, StyleImage *backgroundImage,
                          BackgroundRepeat backgroundRepeat,
                          BackgroundAttachment backgroundAttachment,
                          Length backgroundPositionX,
                          Length backgroundPositionY,
                          int x, int y, int width, int height,
                          int xRef, int yRef, int widthRef, int heightRef)
{
   //printf ("drawBackgroundImage (..., [img: %d, %d], ..., (%d, %d), %d x %d, "
   //        "(%d, %d), %d x %d)\n", imgWidth, imgHeight, x, y, width, height,
   //        xRef, yRef, widthRef, heightRef);

   bool repeatX, repeatY, doDraw;
   int origX, origY, tileX1, tileX2, tileY1, tileY2;

   calcBackgroundRelatedValues (backgroundImage, backgroundRepeat,
                                backgroundAttachment, backgroundPositionX,
                                backgroundPositionY, x, y, width, height,
                                xRef, yRef, widthRef, heightRef,
                                &repeatX, &repeatY, &origX, &origY,
                                &tileX1, &tileX2, &tileY1, &tileY2, &doDraw);

   //printf ("tileX1 = %d, tileX2 = %d, tileY1 = %d, tileY2 = %d\n",
   //        tileX1, tileX2, tileY1, tileY2);

   if (doDraw) {
      // Drawing is done with the "tiled" buffer, but all calculations
      // before have been done with the "source" buffer.

      Imgbuf *imgbufS = backgroundImage->getImgbufSrc();
      int imgWidthS = imgbufS->getRootWidth ();
      int imgHeightS = imgbufS->getRootHeight ();

      Imgbuf *imgbufT = backgroundImage->getImgbufTiled(repeatX, repeatY);
      int imgWidthT = imgbufT->getRootWidth ();
      int imgHeightT = imgbufT->getRootHeight ();
      int tilesX = backgroundImage->getTilesX (repeatX, repeatY);
      int tilesY = backgroundImage->getTilesY (repeatX, repeatY);

      for (int tileX = tileX1; tileX <= tileX2; tileX += tilesX)
         for (int tileY = tileY1; tileY <= tileY2; tileY += tilesY) {
            int xt = origX + tileX * imgWidthS;
            int x1 = misc::max (xt, x);
            int x2 = misc::min (xt + imgWidthT, x + width);
            int yt = origY + tileY * imgHeightS;
            int y1 = misc::max (yt, y);
            int y2 = misc::min (yt + imgHeightT, y + height);

            view->drawImage (imgbufT, xt, yt, x1 - xt, y1 - yt,
                             x2 - x1, y2 - y1);
         }
   }
}

void calcBackgroundRelatedValues (StyleImage *backgroundImage,
                                  BackgroundRepeat backgroundRepeat,
                                  BackgroundAttachment backgroundAttachment,
                                  Length backgroundPositionX,
                                  Length backgroundPositionY,
                                  int xDraw, int yDraw, int widthDraw,
                                  int heightDraw, int xRef, int yRef,
                                  int widthRef, int heightRef, bool *repeatX,
                                  bool *repeatY, int *origX, int *origY,
                                  int *tileX1, int *tileX2, int *tileY1,
                                  int *tileY2, bool *doDraw)
{
   Imgbuf *imgbuf = backgroundImage->getImgbufSrc();
   int imgWidth = imgbuf->getRootWidth ();
   int imgHeight = imgbuf->getRootHeight ();

   *repeatX = backgroundRepeat == BACKGROUND_REPEAT ||
      backgroundRepeat == BACKGROUND_REPEAT_X;
   *repeatY = backgroundRepeat == BACKGROUND_REPEAT ||
      backgroundRepeat == BACKGROUND_REPEAT_Y;

   *origX = xRef +
      (isPerLength (backgroundPositionX) ?
       multiplyWithPerLength (widthRef - imgWidth, backgroundPositionX) :
       absLengthVal (backgroundPositionX));
   *origY = yRef +
      (isPerLength (backgroundPositionY) ?
       multiplyWithPerLength (heightRef - imgHeight, backgroundPositionY) :
       absLengthVal (backgroundPositionY));

   *tileX1 = xDraw < *origX ?
      - (*origX - xDraw + imgWidth - 1) / imgWidth :
      (xDraw - *origX) / imgWidth;
   *tileX2 = *origX < xDraw + widthDraw ?
      (xDraw + widthDraw - *origX - 1) / imgWidth :
      - (*origX - (xDraw + widthDraw) + imgWidth - 1) / imgWidth;
   *tileY1 = yDraw < *origY ?
      - (*origY - yDraw + imgHeight - 1) / imgHeight :
      (yDraw - *origY) / imgHeight;
   *tileY2 = *origY < yDraw + heightDraw ?
      (yDraw + heightDraw - *origY - 1) / imgHeight :
      - (*origY - (yDraw + heightDraw) + imgHeight - 1) / imgHeight;

   *doDraw = true;
   if (!*repeatX) {
      // Only center tile (tileX = 0) is drawn, ...
      if (*tileX1 <= 0 && *tileX2 >= 0)
         // ... and is visible.
         *tileX1 = *tileX2 = 0;
      else
         // ... but is not visible.
         *doDraw = false;
   }

   if (!*repeatY) {
      // Analogue.
      if (*tileY1 <= 0 && *tileY2 >= 0)
         *tileY1 = *tileY2 = 0;
      else
         *doDraw = false;
   }
}

// ----------------------------------------------------------------------

static const char
   *const roman_I0[] = { "","I","II","III","IV","V","VI","VII","VIII","IX" },
   *const roman_I1[] = { "","X","XX","XXX","XL","L","LX","LXX","LXXX","XC" },
   *const roman_I2[] = { "","C","CC","CCC","CD","D","DC","DCC","DCCC","CM" },
   *const roman_I3[] = { "","M","MM","MMM","MMMM" };

static void strAsciiTolower (char *s)
{
   for ( ; *s; s++)
      *s = misc::AsciiTolower (*s);
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
      strAsciiTolower(buf);

}

} // namespace style
} // namespace core
} // namespace dw
