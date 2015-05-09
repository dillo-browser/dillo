/*
 * File: styleengine.cc
 *
 * Copyright 2008-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "../dlib/dlib.h"
#include "msg.h"
#include "prefs.h"
#include "misc.h"
#include "html_common.hh"
#include "styleengine.hh"
#include "web.hh"
#include "capi.h"

using namespace lout::misc;
using namespace dw::core::style;

/**
 * Signal handler for "delete": This handles the case when an instance
 * of StyleImage is deleted, possibly when the cache client is still
 * active.
 *
 * \todo Not neccessary for dw::Image? (dw::Image also implements
 * lout::signal::ObservedObject.)
 */
class StyleImageDeletionReceiver:
   public lout::signal::ObservedObject::DeletionReceiver
{
   int clientKey;

public:
   StyleImageDeletionReceiver (int clientKey);
   ~StyleImageDeletionReceiver ();

   void deleted (lout::signal::ObservedObject *object);
};

StyleImageDeletionReceiver::StyleImageDeletionReceiver (int clientKey)
{
   this->clientKey = clientKey;
}

StyleImageDeletionReceiver::~StyleImageDeletionReceiver ()
{
}

void StyleImageDeletionReceiver::deleted (lout::signal::ObservedObject *object)
{
   a_Capi_stop_client (clientKey, 0);
   delete this;
}

// ----------------------------------------------------------------------

StyleEngine::StyleEngine (dw::core::Layout *layout,
                          const DilloUrl *pageUrl, const DilloUrl *baseUrl) {
   StyleAttrs style_attrs;
   FontAttrs font_attrs;

   doctree = new Doctree ();
   stack = new lout::misc::SimpleVector <Node> (1);
   cssContext = new CssContext ();
   buildUserStyle ();
   this->layout = layout;
   this->pageUrl = pageUrl ? a_Url_dup(pageUrl) : NULL;
   this->baseUrl = baseUrl ? a_Url_dup(baseUrl) : NULL;
   importDepth = 0;

   stackPush ();
   Node *n = stack->getLastRef ();

   /* Create a dummy font, attribute, and tag for the bottom of the stack. */
   font_attrs.name = prefs.font_sans_serif;
   font_attrs.size = roundInt(14 * prefs.font_factor);
   if (font_attrs.size < prefs.font_min_size)
      font_attrs.size = prefs.font_min_size;
   if (font_attrs.size > prefs.font_max_size)
      font_attrs.size = prefs.font_max_size;
   font_attrs.weight = 400;
   font_attrs.style = FONT_STYLE_NORMAL;
   font_attrs.letterSpacing = 0;
   font_attrs.fontVariant = FONT_VARIANT_NORMAL;

   style_attrs.initValues ();
   style_attrs.font = Font::create (layout, &font_attrs);
   style_attrs.color = Color::create (layout, 0);
   style_attrs.backgroundColor = Color::create (layout, prefs.bg_color);

   n->style = Style::create (&style_attrs);
}

StyleEngine::~StyleEngine () {
   while (doctree->top ())
      endElement (doctree->top ()->element);

   stackPop (); // dummy node on the bottom of the stack
   assert (stack->size () == 0);

   a_Url_free(pageUrl);
   a_Url_free(baseUrl);

   delete stack;
   delete doctree;
   delete cssContext;
}

void StyleEngine::stackPush () {
   static const Node emptyNode = {
      NULL, NULL, NULL, NULL, NULL, NULL, false, false, NULL
   };

   stack->setSize (stack->size () + 1, emptyNode);
}

void StyleEngine::stackPop () {
   Node *n = stack->getRef (stack->size () - 1);

   delete n->styleAttrProperties;
   delete n->styleAttrPropertiesImportant;
   delete n->nonCssProperties;
   if (n->style)
      n->style->unref ();
   if (n->wordStyle)
      n->wordStyle->unref ();
   if (n->backgroundStyle)
      n->backgroundStyle->unref ();
   stack->setSize (stack->size () - 1);
}

/**
 * \brief tell the styleEngine that a new html element has started.
 */
void StyleEngine::startElement (int element, BrowserWindow *bw) {
   style (bw); // ensure that style of current node is computed

   stackPush ();
   Node *n = stack->getLastRef ();
   DoctreeNode *dn = doctree->push ();

   dn->element = element;
   n->doctreeNode = dn;
   if (stack->size () > 1)
      n->displayNone = stack->getRef (stack->size () - 2)->displayNone;
}

void StyleEngine::startElement (const char *tagname, BrowserWindow *bw) {
   startElement (a_Html_tag_index (tagname), bw);
}

void StyleEngine::setId (const char *id) {
   DoctreeNode *dn =  doctree->top ();
   assert (dn->id == NULL);
   dn->id = dStrdup (id);
}

/**
 * \brief split a string at sep chars and return a SimpleVector of strings
 */
static lout::misc::SimpleVector<char *> *splitStr (const char *str, char sep) {
   const char *p1 = NULL;
   lout::misc::SimpleVector<char *> *list =
      new lout::misc::SimpleVector<char *> (1);

   for (;; str++) {
      if (*str != '\0' && *str != sep) {
         if (!p1)
            p1 = str;
      } else if (p1) {
         list->increase ();
         list->set (list->size () - 1, dStrndup (p1, str - p1));
         p1 = NULL;
      }

      if (*str == '\0')
         break;
   }

   return list;
}

void StyleEngine::setClass (const char *klass) {
   DoctreeNode *dn = doctree->top ();
   assert (dn->klass == NULL);
   dn->klass = splitStr (klass, ' ');
}

void StyleEngine::setStyle (const char *styleAttr) {
   Node *n = stack->getRef (stack->size () - 1);
   assert (n->styleAttrProperties == NULL);
   // parse style information from style="" attribute, if it exists
   if (styleAttr && prefs.parse_embedded_css) {
      n->styleAttrProperties = new CssPropertyList (true);
      n->styleAttrPropertiesImportant = new CssPropertyList (true);

      CssParser::parseDeclarationBlock (baseUrl, styleAttr, strlen (styleAttr),
                                        n->styleAttrProperties,
                                        n->styleAttrPropertiesImportant);
   }
}

/**
 * \brief Instruct StyleEngine to use the nonCssHints from parent element
 * This is only used for tables where nonCssHints on the TABLE-element
 * (e.g. border=1) also affect child elements like TD.
 */
void StyleEngine::inheritNonCssHints () {
   Node *pn = stack->getRef (stack->size () - 2);

   if (pn->nonCssProperties) {
      Node *n = stack->getRef (stack->size () - 1);
      CssPropertyList *origNonCssProperties = n->nonCssProperties;

      n->nonCssProperties = new CssPropertyList(*pn->nonCssProperties, true);

      if (origNonCssProperties) // original nonCssProperties have precedence
         origNonCssProperties->apply (n->nonCssProperties);

      delete origNonCssProperties;
   }
}

void StyleEngine::clearNonCssHints () {
   Node *n = stack->getRef (stack->size () - 1);

   delete n->nonCssProperties;
   n->nonCssProperties = NULL;
}

/**
 * \brief Use of the background color of the parent style as default.
 *   This is only used in table code to allow for colors specified for
 *   table rows as table rows are currently no widgets and therefore
 *   don't draw any background.
 */
void StyleEngine::inheritBackgroundColor () {
   stack->getRef (stack->size () - 1)->inheritBackgroundColor = true;
}

dw::core::style::Color *StyleEngine::backgroundColor () {
   for (int i = 1; i < stack->size (); i++) {
      Node *n = stack->getRef (i);

      if (n->style && n->style->backgroundColor)
         return n->style->backgroundColor;
   }

   return NULL;
}

dw::core::style::StyleImage *StyleEngine::backgroundImage
   (dw::core::style::BackgroundRepeat *bgRepeat,
    dw::core::style::BackgroundAttachment *bgAttachment,
    dw::core::style::Length *bgPositionX,
    dw::core::style::Length *bgPositionY) {
   for (int i = 1; i < stack->size (); i++) {
      Node *n = stack->getRef (i);

      if (n->style && n->style->backgroundImage) {
         *bgRepeat = n->style->backgroundRepeat;
         *bgAttachment = n->style->backgroundAttachment;
         *bgPositionX = n->style->backgroundPositionX;
         *bgPositionY = n->style->backgroundPositionY;
         return n->style->backgroundImage;
      }
   }

   return NULL;
}

/**
 * \brief set the CSS pseudo class :link.
 */
void StyleEngine::setPseudoLink () {
   DoctreeNode *dn = doctree->top ();
   dn->pseudo = "link";
}

/**
 * \brief set the CSS pseudo class :visited.
 */
void StyleEngine::setPseudoVisited () {
   DoctreeNode *dn = doctree->top ();
   dn->pseudo = "visited";
}

/**
 * \brief tell the styleEngine that a html element has ended.
 */
void StyleEngine::endElement (int element) {
   assert (element == doctree->top ()->element);

   stackPop ();
   doctree->pop ();
}

void StyleEngine::preprocessAttrs (dw::core::style::StyleAttrs *attrs) {
   /* workaround for styling of inline elements */
   if (stack->getRef (stack->size () - 2)->inheritBackgroundColor) {
      attrs->backgroundColor =
         stack->getRef (stack->size () - 2)->style->backgroundColor;
      attrs->backgroundImage =
         stack->getRef (stack->size () - 2)->style->backgroundImage;
      attrs->backgroundRepeat =
         stack->getRef (stack->size () - 2)->style->backgroundRepeat;
      attrs->backgroundAttachment =
         stack->getRef (stack->size () - 2)->style->backgroundAttachment;
      attrs->backgroundPositionX =
         stack->getRef (stack->size () - 2)->style->backgroundPositionX;
      attrs->backgroundPositionY =
         stack->getRef (stack->size () - 2)->style->backgroundPositionY;

      attrs->valign = stack->getRef (stack->size () - 2)->style->valign;
   }
   attrs->borderColor.top = (Color *) -1;
   attrs->borderColor.bottom = (Color *) -1;
   attrs->borderColor.left = (Color *) -1;
   attrs->borderColor.right = (Color *) -1;
   /* initial value of border-width is 'medium' */
   attrs->borderWidth.top = 2;
   attrs->borderWidth.bottom = 2;
   attrs->borderWidth.left = 2;
   attrs->borderWidth.right = 2;
}

void StyleEngine::postprocessAttrs (dw::core::style::StyleAttrs *attrs) {
   /* if border-color is not specified, use color as computed value */
   if (attrs->borderColor.top == (Color *) -1)
      attrs->borderColor.top = attrs->color;
   if (attrs->borderColor.bottom == (Color *) -1)
      attrs->borderColor.bottom = attrs->color;
   if (attrs->borderColor.left == (Color *) -1)
      attrs->borderColor.left = attrs->color;
   if (attrs->borderColor.right == (Color *) -1)
      attrs->borderColor.right = attrs->color;
   /* computed value of border-width is 0 if border-style
      is 'none' or 'hidden' */
   if (attrs->borderStyle.top == BORDER_NONE ||
       attrs->borderStyle.top == BORDER_HIDDEN)
      attrs->borderWidth.top = 0;
   if (attrs->borderStyle.bottom == BORDER_NONE ||
       attrs->borderStyle.bottom == BORDER_HIDDEN)
      attrs->borderWidth.bottom = 0;
   if (attrs->borderStyle.left == BORDER_NONE ||
       attrs->borderStyle.left == BORDER_HIDDEN)
      attrs->borderWidth.left = 0;
   if (attrs->borderStyle.right == BORDER_NONE ||
       attrs->borderStyle.right == BORDER_HIDDEN)
      attrs->borderWidth.right = 0;
}

/**
 * \brief Make changes to StyleAttrs attrs according to CssPropertyList props.
 */
void StyleEngine::apply (int i, StyleAttrs *attrs, CssPropertyList *props,
                         BrowserWindow *bw) {
   FontAttrs fontAttrs = *attrs->font;
   Font *parentFont = stack->get (i - 1).style->font;
   char *c, *fontName;
   int lineHeight;
   DilloUrl *imgUrl = NULL;

   /* Determine font first so it can be used to resolve relative lengths. */
   for (int j = 0; j < props->size (); j++) {
      CssProperty *p = props->getRef (j);

      switch (p->name) {
         case CSS_PROPERTY_FONT_FAMILY:
            // Check font names in comma separated list.
            // Note, that p->value.strVal is modified, so that in future calls
            // the matching font name can be used directly.
            fontName = NULL;
            while (p->value.strVal) {
               if ((c = strchr(p->value.strVal, ',')))
                  *c = '\0';
               dStrstrip(p->value.strVal);

               if (dStrAsciiCasecmp (p->value.strVal, "serif") == 0)
                  fontName = prefs.font_serif;
               else if (dStrAsciiCasecmp (p->value.strVal, "sans-serif") == 0)
                  fontName = prefs.font_sans_serif;
               else if (dStrAsciiCasecmp (p->value.strVal, "cursive") == 0)
                  fontName = prefs.font_cursive;
               else if (dStrAsciiCasecmp (p->value.strVal, "fantasy") == 0)
                  fontName = prefs.font_fantasy;
               else if (dStrAsciiCasecmp (p->value.strVal, "monospace") == 0)
                  fontName = prefs.font_monospace;
               else if (Font::exists(layout, p->value.strVal))
                  fontName = p->value.strVal;

               if (fontName) {   // font found
                  fontAttrs.name = fontName;
                  break;
               } else if (c) {   // try next from list
                  memmove(p->value.strVal, c + 1, strlen(c + 1) + 1);
               } else {          // no font found
                  break;
               }
            }

            break;
         case CSS_PROPERTY_FONT_SIZE:
            if (p->type == CSS_TYPE_ENUM) {
               switch (p->value.intVal) {
                  case CSS_FONT_SIZE_XX_SMALL:
                     fontAttrs.size = roundInt(8.1 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_X_SMALL:
                     fontAttrs.size = roundInt(9.7 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_SMALL:
                     fontAttrs.size = roundInt(11.7 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_MEDIUM:
                     fontAttrs.size = roundInt(14.0 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_LARGE:
                     fontAttrs.size = roundInt(16.8 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_X_LARGE:
                     fontAttrs.size = roundInt(20.2 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_XX_LARGE:
                     fontAttrs.size = roundInt(24.2 * prefs.font_factor);
                     break;
                  case CSS_FONT_SIZE_SMALLER:
                     fontAttrs.size = roundInt(fontAttrs.size * 0.83);
                     break;
                  case CSS_FONT_SIZE_LARGER:
                     fontAttrs.size = roundInt(fontAttrs.size * 1.2);
                     break;
                  default:
                     assert(false); // invalid font-size enum
               }
            } else {
               computeValue (&fontAttrs.size, p->value.intVal, parentFont,
                  parentFont->size);
            }

            if (fontAttrs.size < prefs.font_min_size)
               fontAttrs.size = prefs.font_min_size;
            if (fontAttrs.size > prefs.font_max_size)
               fontAttrs.size = prefs.font_max_size;

            break;
         case CSS_PROPERTY_FONT_STYLE:
            fontAttrs.style = (FontStyle) p->value.intVal;
            break;
         case CSS_PROPERTY_FONT_WEIGHT:

            if (p->type == CSS_TYPE_ENUM) {
               switch (p->value.intVal) {
                  case CSS_FONT_WEIGHT_BOLD:
                     fontAttrs.weight = 700;
                     break;
                  case CSS_FONT_WEIGHT_BOLDER:
                     fontAttrs.weight += 300;
                     break;
                  case CSS_FONT_WEIGHT_LIGHT:
                     fontAttrs.weight = 100;
                     break;
                  case CSS_FONT_WEIGHT_LIGHTER:
                     fontAttrs.weight -= 300;
                     break;
                  case CSS_FONT_WEIGHT_NORMAL:
                     fontAttrs.weight = 400;
                     break;
                  default:
                     assert(false); // invalid font weight value
                     break;
               }
            } else {
               fontAttrs.weight = p->value.intVal;
            }

            if (fontAttrs.weight < 100)
               fontAttrs.weight = 100;
            if (fontAttrs.weight > 900)
               fontAttrs.weight = 900;

            break;
         case CSS_PROPERTY_LETTER_SPACING:
            if (p->type == CSS_TYPE_ENUM) {
               if (p->value.intVal == CSS_LETTER_SPACING_NORMAL) {
                  fontAttrs.letterSpacing = 0;
               }
            } else {
               computeValue (&fontAttrs.letterSpacing, p->value.intVal,
                  parentFont, parentFont->size);
            }

            /* Limit letterSpacing to reasonable values to avoid overflows e.g,
             * when measuring word width.
             */
            if (fontAttrs.letterSpacing > 1000)
               fontAttrs.letterSpacing = 1000;
            else if (fontAttrs.letterSpacing < -1000)
               fontAttrs.letterSpacing = -1000;
            break;
         case CSS_PROPERTY_FONT_VARIANT:
            fontAttrs.fontVariant = (FontVariant) p->value.intVal;
            break;
         default:
            break;
      }
   }

   attrs->font = Font::create (layout, &fontAttrs);

   for (int j = 0; j < props->size (); j++) {
      CssProperty *p = props->getRef (j);

      switch (p->name) {
         /* \todo missing cases */
         case CSS_PROPERTY_BACKGROUND_ATTACHMENT:
            attrs->backgroundAttachment =
               (BackgroundAttachment) p->value.intVal;
            break;
         case CSS_PROPERTY_BACKGROUND_COLOR:
            if (prefs.allow_white_bg || p->value.intVal != 0xffffff)
               attrs->backgroundColor = Color::create(layout, p->value.intVal);
            else
               attrs->backgroundColor =
                  Color::create(layout, prefs.white_bg_replacement);
            break;
         case CSS_PROPERTY_BACKGROUND_IMAGE:
            // p->value.strVal should be absolute, so baseUrl is not needed
            imgUrl = a_Url_new (p->value.strVal, NULL);
            break;
         case CSS_PROPERTY_BACKGROUND_POSITION:
            computeLength (&attrs->backgroundPositionX, p->value.posVal->posX,
                           attrs->font);
            computeLength (&attrs->backgroundPositionY, p->value.posVal->posY,
                           attrs->font);
            break;
         case CSS_PROPERTY_BACKGROUND_REPEAT:
            attrs->backgroundRepeat = (BackgroundRepeat) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_COLLAPSE:
            attrs->borderCollapse = (BorderCollapse) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_TOP_COLOR:
            attrs->borderColor.top = (p->type == CSS_TYPE_ENUM) ? NULL :
                                     Color::create (layout, p->value.intVal);
            break;
         case CSS_PROPERTY_BORDER_BOTTOM_COLOR:
            attrs->borderColor.bottom = (p->type == CSS_TYPE_ENUM) ? NULL :
                                       Color::create (layout, p->value.intVal);
            break;
         case CSS_PROPERTY_BORDER_LEFT_COLOR:
            attrs->borderColor.left = (p->type == CSS_TYPE_ENUM) ? NULL :
                                      Color::create (layout, p->value.intVal);
            break;
         case CSS_PROPERTY_BORDER_RIGHT_COLOR:
            attrs->borderColor.right = (p->type == CSS_TYPE_ENUM) ? NULL :
                                       Color::create (layout, p->value.intVal);
            break;
         case CSS_PROPERTY_BORDER_BOTTOM_STYLE:
            attrs->borderStyle.bottom = (BorderStyle) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_LEFT_STYLE:
            attrs->borderStyle.left = (BorderStyle) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_RIGHT_STYLE:
            attrs->borderStyle.right = (BorderStyle) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_TOP_STYLE:
            attrs->borderStyle.top = (BorderStyle) p->value.intVal;
            break;
         case CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
            computeBorderWidth (&attrs->borderWidth.bottom, p, attrs->font);
            break;
         case CSS_PROPERTY_BORDER_LEFT_WIDTH:
            computeBorderWidth (&attrs->borderWidth.left, p, attrs->font);
            break;
         case CSS_PROPERTY_BORDER_RIGHT_WIDTH:
            computeBorderWidth (&attrs->borderWidth.right, p, attrs->font);
            break;
         case CSS_PROPERTY_BORDER_TOP_WIDTH:
            computeBorderWidth (&attrs->borderWidth.top, p, attrs->font);
            break;
         case CSS_PROPERTY_BORDER_SPACING:
            computeValue (&attrs->hBorderSpacing, p->value.intVal,attrs->font);
            computeValue (&attrs->vBorderSpacing, p->value.intVal,attrs->font);
            break;
         case CSS_PROPERTY_COLOR:
            attrs->color = Color::create (layout, p->value.intVal);
            break;
         case CSS_PROPERTY_CURSOR:
            attrs->cursor = (Cursor) p->value.intVal;
            break;
         case CSS_PROPERTY_DISPLAY:
            attrs->display = (DisplayType) p->value.intVal;
            if (attrs->display == DISPLAY_NONE)
               stack->getRef (i)->displayNone = true;
            break;
         case CSS_PROPERTY_LINE_HEIGHT:
            if (p->type == CSS_TYPE_ENUM) { //only valid enum value is "normal"
               attrs->lineHeight = dw::core::style::LENGTH_AUTO;
            } else if (p->type == CSS_TYPE_LENGTH_PERCENTAGE_NUMBER) {
               if (CSS_LENGTH_TYPE (p->value.intVal) == CSS_LENGTH_TYPE_NONE) {
                  attrs->lineHeight =
                     createPerLength(CSS_LENGTH_VALUE(p->value.intVal));
               } else if (computeValue (&lineHeight, p->value.intVal,
                                        attrs->font, attrs->font->size)) {
                  attrs->lineHeight = createAbsLength(lineHeight);
               }
            }
            break;
         case CSS_PROPERTY_LIST_STYLE_POSITION:
            attrs->listStylePosition = (ListStylePosition) p->value.intVal;
            break;
         case CSS_PROPERTY_LIST_STYLE_TYPE:
            attrs->listStyleType = (ListStyleType) p->value.intVal;
            break;
         case CSS_PROPERTY_MARGIN_BOTTOM:
            computeValue (&attrs->margin.bottom, p->value.intVal, attrs->font);
            if (attrs->margin.bottom < 0) // \todo fix negative margins in dw/*
               attrs->margin.bottom = 0;
            break;
         case CSS_PROPERTY_MARGIN_LEFT:
            computeValue (&attrs->margin.left, p->value.intVal, attrs->font);
            if (attrs->margin.left < 0) // \todo fix negative margins in dw/*
               attrs->margin.left = 0;
            break;
         case CSS_PROPERTY_MARGIN_RIGHT:
            computeValue (&attrs->margin.right, p->value.intVal, attrs->font);
            if (attrs->margin.right < 0) // \todo fix negative margins in dw/*
               attrs->margin.right = 0;
            break;
         case CSS_PROPERTY_MARGIN_TOP:
            computeValue (&attrs->margin.top, p->value.intVal, attrs->font);
            if (attrs->margin.top < 0) // \todo fix negative margins in dw/*
               attrs->margin.top = 0;
            break;
         case CSS_PROPERTY_PADDING_TOP:
            computeValue (&attrs->padding.top, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_PADDING_BOTTOM:
            computeValue (&attrs->padding.bottom, p->value.intVal,attrs->font);
            break;
         case CSS_PROPERTY_PADDING_LEFT:
            computeValue (&attrs->padding.left, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_PADDING_RIGHT:
            computeValue (&attrs->padding.right, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_TEXT_ALIGN:
            attrs->textAlign = (TextAlignType) p->value.intVal;
            break;
         case CSS_PROPERTY_TEXT_DECORATION:
            attrs->textDecoration |= p->value.intVal;
            break;
         case CSS_PROPERTY_TEXT_INDENT:
            computeLength (&attrs->textIndent, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_TEXT_TRANSFORM:
            attrs->textTransform = (TextTransform) p->value.intVal;
            break;
         case CSS_PROPERTY_VERTICAL_ALIGN:
            attrs->valign = (VAlignType) p->value.intVal;
            break;
         case CSS_PROPERTY_WHITE_SPACE:
            attrs->whiteSpace = (WhiteSpace) p->value.intVal;
            break;
         case CSS_PROPERTY_WIDTH:
            computeLength (&attrs->width, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_HEIGHT:
            computeLength (&attrs->height, p->value.intVal, attrs->font);
            break;
         case CSS_PROPERTY_WORD_SPACING:
            if (p->type == CSS_TYPE_ENUM) {
               if (p->value.intVal == CSS_WORD_SPACING_NORMAL) {
                  attrs->wordSpacing = 0;
               }
            } else {
               computeValue(&attrs->wordSpacing, p->value.intVal, attrs->font);
            }

            /* Limit to reasonable values to avoid overflows */
            if (attrs->wordSpacing > 1000)
               attrs->wordSpacing = 1000;
            else if (attrs->wordSpacing < -1000)
               attrs->wordSpacing = -1000;
            break;
         case PROPERTY_X_LINK:
            attrs->x_link = p->value.intVal;
            break;
         case PROPERTY_X_LANG:
            attrs->x_lang[0] = D_ASCII_TOLOWER(p->value.strVal[0]);
            if (attrs->x_lang[0])
               attrs->x_lang[1] = D_ASCII_TOLOWER(p->value.strVal[1]);
            else
               attrs->x_lang[1] = 0;
            break;
         case PROPERTY_X_IMG:
            attrs->x_img = p->value.intVal;
            break;
         case PROPERTY_X_TOOLTIP:
            attrs->x_tooltip = Tooltip::create(layout, p->value.strVal);
            break;
         default:
            break;
      }
   }

   if (imgUrl && prefs.load_background_images &&
       !stack->getRef (i)->displayNone &&
       !(URL_FLAGS(pageUrl) & URL_SpamSafe))
   {
      attrs->backgroundImage = StyleImage::create();
      DilloImage *image =
         a_Image_new(layout,
            (void*)attrs->backgroundImage
            ->getMainImgRenderer(),
            0xffffff);

      // we use the pageUrl as requester to prevent cross
      // domain requests as specified in domainrc
      DilloWeb *web = a_Web_new(bw, imgUrl, pageUrl);
      web->Image = image;
      a_Image_ref(image);
      web->flags |= WEB_Image;

      int clientKey;
      if ((clientKey = a_Capi_open_url(web, NULL, NULL)) != 0) {
                  a_Bw_add_client(bw, clientKey, 0);
                  a_Bw_add_url(bw, imgUrl);
                  attrs->backgroundImage->connectDeletion
                     (new StyleImageDeletionReceiver (clientKey));
      }
   }
   a_Url_free (imgUrl);
}

/**
 * \brief Resolve relative lengths to absolute values.
 */
bool StyleEngine::computeValue (int *dest, CssLength value, Font *font) {
   static float dpmm;

   if (dpmm == 0.0)
      dpmm = layout->dpiX () / 25.4; /* assume dpiX == dpiY */

   switch (CSS_LENGTH_TYPE (value)) {
      case CSS_LENGTH_TYPE_PX:
         *dest = (int) CSS_LENGTH_VALUE (value);
         return true;
      case CSS_LENGTH_TYPE_MM:
         *dest = roundInt (CSS_LENGTH_VALUE (value) * dpmm);
         return true;
      case CSS_LENGTH_TYPE_EM:
         *dest = roundInt (CSS_LENGTH_VALUE (value) * font->size);
         return true;
      case CSS_LENGTH_TYPE_EX:
         *dest = roundInt (CSS_LENGTH_VALUE(value) * font->xHeight);
         return true;
      case CSS_LENGTH_TYPE_NONE:
         // length values other than 0 without unit are only allowed
         // in special cases (line-height) and have to be handled
         // separately.
         assert ((int) CSS_LENGTH_VALUE (value) == 0);
         *dest = 0;
         return true;
      default:
         break;
   }

   return false;
}

bool StyleEngine::computeValue (int *dest, CssLength value, Font *font,
                                int percentageBase) {
   if (CSS_LENGTH_TYPE (value) == CSS_LENGTH_TYPE_PERCENTAGE) {
      *dest = roundInt (CSS_LENGTH_VALUE (value) * percentageBase);
      return true;
   } else
      return computeValue (dest, value, font);
}

bool StyleEngine::computeLength (dw::core::style::Length *dest,
                                 CssLength value, Font *font) {
   int v;

   if (CSS_LENGTH_TYPE (value) == CSS_LENGTH_TYPE_PERCENTAGE) {
      *dest = createPerLength (CSS_LENGTH_VALUE (value));
      return true;
   } else if (CSS_LENGTH_TYPE (value) == CSS_LENGTH_TYPE_AUTO) {
      *dest = dw::core::style::LENGTH_AUTO;
      return true;
   } else if (computeValue (&v, value, font)) {
      *dest = createAbsLength (v);
      return true;
   }

   return false;
}

void StyleEngine::computeBorderWidth (int *dest, CssProperty *p,
                                      dw::core::style::Font *font) {
   if (p->type == CSS_TYPE_ENUM) {
      switch (p->value.intVal) {
         case CSS_BORDER_WIDTH_THIN:
            *dest = 1;
            break;
         case CSS_BORDER_WIDTH_MEDIUM:
            *dest = 2;
            break;
         case CSS_BORDER_WIDTH_THICK:
            *dest = 3;
            break;
         default:
            assert(false);
      }
   } else {
      computeValue (dest, p->value.intVal, font);
   }
}

/**
 * \brief Similar to StyleEngine::style(), but with backgroundColor set.
 * A normal style might have backgroundColor == NULL to indicate a transparent
 * background. This method ensures that backgroundColor is set.
 */
Style * StyleEngine::backgroundStyle (BrowserWindow *bw) {
   if (!stack->getRef (stack->size () - 1)->backgroundStyle) {
      StyleAttrs attrs = *style (bw);

      for (int i = stack->size () - 1; i >= 0 && ! attrs.backgroundColor; i--)
         attrs.backgroundColor = stack->getRef (i)->style->backgroundColor;

      assert (attrs.backgroundColor);
      stack->getRef (stack->size () - 1)->backgroundStyle =
         Style::create (&attrs);
   }
   return stack->getRef (stack->size () - 1)->backgroundStyle;
}

/**
 * \brief Create a new style object based on the previously opened / closed
 * HTML elements and the nonCssProperties that have been set.
 * This method is private. Call style() to get a current style object.
 */
Style * StyleEngine::style0 (int i, BrowserWindow *bw) {
   CssPropertyList props, *styleAttrProperties, *styleAttrPropertiesImportant;
   CssPropertyList *nonCssProperties;
   // get previous style from the stack
   StyleAttrs attrs = *stack->getRef (i - 1)->style;

   // Ensure that StyleEngine::style0() has not been called before for
   // this element.
   // Style computation is expensive so limit it as much as possible.
   // If this assertion is hit, you need to rearrange the code that is
   // doing styleEngine calls to call setNonCssHint() before calling
   // style() or wordStyle() for each new element.
   assert (stack->getRef (i)->style == NULL);

   // reset values that are not inherited according to CSS
   attrs.resetValues ();
   preprocessAttrs (&attrs);

   styleAttrProperties = stack->getRef (i)->styleAttrProperties;
   styleAttrPropertiesImportant = stack->getRef(i)->styleAttrPropertiesImportant;
   nonCssProperties = stack->getRef (i)->nonCssProperties;

   // merge style information
   cssContext->apply (&props, doctree, stack->getRef(i)->doctreeNode,
                      styleAttrProperties, styleAttrPropertiesImportant,
                      nonCssProperties);

   // apply style
   apply (i, &attrs, &props, bw);

   postprocessAttrs (&attrs);

   stack->getRef (i)->style = Style::create (&attrs);

   return stack->getRef (i)->style;
}

Style * StyleEngine::wordStyle0 (BrowserWindow *bw) {
   StyleAttrs attrs = *style (bw);
   attrs.resetValues ();

   if (stack->getRef (stack->size() - 1)->inheritBackgroundColor) {
      attrs.backgroundColor = style (bw)->backgroundColor;
      attrs.backgroundImage = style (bw)->backgroundImage;
      attrs.backgroundRepeat = style (bw)->backgroundRepeat;
      attrs.backgroundAttachment = style (bw)->backgroundAttachment;
      attrs.backgroundPositionX = style (bw)->backgroundPositionX;
      attrs.backgroundPositionY = style (bw)->backgroundPositionY;
   }

   attrs.valign = style (bw)->valign;

   stack->getRef(stack->size() - 1)->wordStyle = Style::create(&attrs);
   return stack->getRef (stack->size () - 1)->wordStyle;
}

/**
 * \brief Recompute all style information from scratch
 * This is used to take into account CSS styles for the HTML-element.
 * The CSS data is only completely available after parsing the HEAD-section
 * and thereby after the HTML-element has been opened.
 * Note that restyle() does not change any styles in the widget tree.
 */
void StyleEngine::restyle (BrowserWindow *bw) {
   for (int i = 1; i < stack->size (); i++) {
      Node *n = stack->getRef (i);
      if (n->style) {
         n->style->unref ();
         n->style = NULL;
      }
      if (n->wordStyle) {
         n->wordStyle->unref ();
         n->wordStyle = NULL;
      }
      if (n->backgroundStyle) {
         n->backgroundStyle->unref ();
         n->backgroundStyle = NULL;
      }

      style0 (i, bw);
   }
}

void StyleEngine::parse (DilloHtml *html, DilloUrl *url, const char *buf,
                         int buflen, CssOrigin origin) {
   if (importDepth > 10) { // avoid looping with recursive @import directives
      MSG_WARN("Maximum depth of CSS @import reached--ignoring stylesheet.\n");
      return;
   }

   importDepth++;
   CssParser::parse (html, url, cssContext, buf, buflen, origin);
   importDepth--;
}

/**
 * \brief Create the user agent style.
 *
 * The user agent style defines how dillo renders HTML in the absence of
 * author or user styles.
 */
void StyleEngine::init () {
   const char *cssBuf =
      "body  {margin: 5px}"
      "big {font-size: 1.17em}"
      "blockquote, dd {margin-left: 40px; margin-right: 40px}"
      "center {text-align: center}"
      "dt {font-weight: bolder}"
      ":link {color: blue; text-decoration: underline; cursor: pointer}"
      ":visited {color: #800080; text-decoration: underline; cursor: pointer}"
      "h1, h2, h3, h4, h5, h6, b, strong {font-weight: bolder}"
      "address, article, aside, center, div, figure, figcaption, footer,"
      " h1, h2, h3, h4, h5, h6, header, nav, ol, p, pre, section, ul"
      " {display: block}"
      "i, em, cite, address, var {font-style: italic}"
      ":link img, :visited img {border: 1px solid}"
      "frameset, ul, ol, dir {margin-left: 40px}"
      /* WORKAROUND: It should be margin: 1em 0
       * but as we don't collapse these margins yet, it
       * look better like this.
       */
      "p {margin: 0.5em 0}"
      "figure {margin: 1em 40px}"
      "h1 {font-size: 2em; margin-top: .67em; margin-bottom: 0}"
      "h2 {font-size: 1.5em; margin-top: .75em; margin-bottom: 0}"
      "h3 {font-size: 1.17em; margin-top: .83em; margin-bottom: 0}"
      "h4 {margin-top: 1.12em; margin-bottom: 0}"
      "h5 {font-size: 0.83em; margin-top: 1.5em; margin-bottom: 0}"
      "h6 {font-size: 0.75em; margin-top: 1.67em; margin-bottom: 0}"
      "hr {width: 100%; border: 1px inset}"
      "li {margin-top: 0.1em; display: list-item}"
      "pre {white-space: pre}"
      "ol {list-style-type: decimal}"
      "ul {list-style-type: disc}"
      "ul ul {list-style-type: circle}"
      "ul ul ul {list-style-type: square}"
      "ul ul ul ul {list-style-type: disc}"
      "ins, u {text-decoration: underline}"
      "small, sub, sup {font-size: 0.83em}"
      "sub {vertical-align: sub}"
      "sup {vertical-align: super}"
      "s, strike, del {text-decoration: line-through}"
      /* HTML5 spec notes that mark styling "is just a suggestion and can be
       * changed based on implementation feedback"
       */
      "mark {background: yellow; color: black;}"
      "table {border-spacing: 2px}"
      "td, th {padding: 2px}"
      "thead, tbody, tfoot {vertical-align: middle}"
      "th {font-weight: bolder; text-align: center}"
      "code, tt, pre, samp, kbd {font-family: monospace}"
      /* WORKAROUND: Reset font properties in tables as some
       * pages rely on it (e.g. gmail).
       * http://developer.mozilla.org/En/Fixing_Table_Inheritance_in_Quirks_Mode
       * has a detailed description of the issue.
       */
      "table, caption {font-size: medium; font-weight: normal}";

   CssContext context;
   CssParser::parse (NULL, NULL, &context, cssBuf, strlen (cssBuf),
                     CSS_ORIGIN_USER_AGENT);
}

void StyleEngine::buildUserStyle () {
   Dstr *style;
   char *filename = dStrconcat(dGethomedir(), "/.dillo/style.css", NULL);

   if ((style = a_Misc_file2dstr(filename))) {
      CssParser::parse (NULL,NULL,cssContext,style->str, style->len,CSS_ORIGIN_USER);
      dStr_free (style, 1);
   }
   dFree (filename);
}
