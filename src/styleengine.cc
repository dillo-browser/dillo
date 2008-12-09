/*
 * File: styleengine.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <math.h>
#include "../dlib/dlib.h"
#include "styleengine.hh"

using namespace dw::core::style;

StyleEngine::StyleEngine (dw::core::Layout *layout) {
   StyleAttrs style_attrs;
   FontAttrs font_attrs;

   stack = new lout::misc::SimpleVector <Node> (1);
   cssContext = new CssContext ();
   this->layout = layout;

   stack->increase ();
   Node *n =  stack->getRef (stack->size () - 1);

   /* Create a dummy font, attribute, and tag for the bottom of the stack. */
   font_attrs.name = "helvetica";
   font_attrs.size = 12;
   font_attrs.weight = 400;
   font_attrs.style = FONT_STYLE_NORMAL;
 
   style_attrs.initValues ();
   style_attrs.font = Font::create (layout, &font_attrs);
   style_attrs.color = Color::createSimple (layout, 0);
   style_attrs.backgroundColor = Color::createSimple (layout, 0xffffff);
   
   n->style = Style::create (layout, &style_attrs);
}

StyleEngine::~StyleEngine () {
   /* \todo clear stack if not empty */
   delete stack;
   delete cssContext;
}

/**
 * \brief tell the styleEngine that a new html element has started.
 */
void StyleEngine::startElement (int element) {
//   fprintf(stderr, "===> START %d\n", element);

   if (stack->getRef (stack->size () - 1)->style == NULL)
      style0 ();

   stack->increase ();
   Node *n =  stack->getRef (stack->size () - 1);
   n->style = NULL;
   n->wordStyle = NULL;
   n->depth = stack->size () - 1;
   n->element = element;
   n->id = NULL;
   n->klass = NULL;
   n->pseudo = NULL;
   n->styleAttribute = NULL;
   n->inheritBackgroundColor = false;
}

void StyleEngine::setId (const char *id) {
   Node *n =  stack->getRef (stack->size () - 1);
   assert (n->id == NULL);
   n->id = dStrdup (id);
};

void StyleEngine::setClass (const char *klass) {
   Node *n =  stack->getRef (stack->size () - 1);
   assert (n->klass == NULL);
   n->klass = dStrdup (klass);
};

void StyleEngine::setStyle (const char *style) {
   Node *n =  stack->getRef (stack->size () - 1);
   assert (n->styleAttribute == NULL);
   n->styleAttribute = dStrdup (style);
};

/**
 * \brief set properties that were definded using (mostly deprecated) HTML
 *    attributes (e.g. bgColor).
 */
void StyleEngine::setNonCssHints (CssPropertyList *nonCssHints) {
   if (stack->getRef (stack->size () - 1)->style)
      stack->getRef (stack->size () - 1)->style->unref ();
   style0 (nonCssHints); // evaluate now, so caller can free nonCssHints
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

/**
 * \brief set the CSS pseudo class (e.g. "link", "visited").
 */
void StyleEngine::setPseudo (const char *pseudo) {
   Node *n =  stack->getRef (stack->size () - 1);
   assert (n->pseudo == NULL);
   n->pseudo = dStrdup (pseudo);
}

/**
 * \brief tell the styleEngine that a html element has ended.
 */
void StyleEngine::endElement (int element) {
//   fprintf(stderr, "===> END %d\n", element);
   assert (stack->size () > 1);
   assert (element == stack->getRef (stack->size () - 1)->element);

   Node *n =  stack->getRef (stack->size () - 1);

   if (n->style)
      n->style->unref ();
   if (n->wordStyle)
      n->wordStyle->unref ();
   if (n->id)
      dFree ((void*) n->id); 
   if (n->klass)
      dFree ((void*) n->klass); 
   if (n->styleAttribute)
      dFree ((void*) n->styleAttribute); 

   stack->setSize (stack->size () - 1);
}

/**
 * \brief Make changes to StyleAttrs attrs according to CssPropertyList props.
 */
void StyleEngine::apply (StyleAttrs *attrs, CssPropertyList *props) {
   FontAttrs fontAttrs = *attrs->font;
   Font *parentFont;

   /* Determine font first so it can be used to resolve relative lenths.
    * \todo Things should be rearranged so that just one pass is necessary.
    */
   for (int i = 0; i < props->size (); i++) {
      CssProperty *p = props->getRef (i);
      
      switch (p->name) {
         case CssProperty::CSS_PROPERTY_FONT_FAMILY:
            fontAttrs.name = p->value.strVal;
            break;
         case CssProperty::CSS_PROPERTY_FONT_SIZE:
            parentFont = stack->get (stack->size () - 2).style->font;
            computeValue (&fontAttrs.size, p->value.intVal, parentFont,
               parentFont->size);
            break;
         case CssProperty::CSS_PROPERTY_FONT_STYLE:
            fontAttrs.style = (FontStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_FONT_WEIGHT:
            switch (p->value.intVal) {
               case CssProperty::CSS_FONT_WEIGHT_LIGHTER:
                  fontAttrs.weight -= CssProperty::CSS_FONT_WEIGHT_STEP;
                  break;
               case CssProperty::CSS_FONT_WEIGHT_BOLDER:
                  fontAttrs.weight += CssProperty::CSS_FONT_WEIGHT_STEP;
                  break;
               default:
                  fontAttrs.weight = p->value.intVal;
                  break;
            }
            if (fontAttrs.weight < CssProperty::CSS_FONT_WEIGHT_MIN)
               fontAttrs.weight = CssProperty::CSS_FONT_WEIGHT_MIN;
            if (fontAttrs.weight > CssProperty::CSS_FONT_WEIGHT_MAX)
               fontAttrs.weight = CssProperty::CSS_FONT_WEIGHT_MAX;
            break;
         default:
            break;
      }
   }

   attrs->font = Font::create (layout, &fontAttrs);

   for (int i = 0; i < props->size (); i++) {
      CssProperty *p = props->getRef (i);
      
      switch (p->name) {
         /* \todo missing cases */
         case CssProperty::CSS_PROPERTY_BACKGROUND_COLOR:
            attrs->backgroundColor =
               Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_TOP_COLOR:
            attrs->borderColor.top =
              Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_COLOR:
            attrs->borderColor.bottom =
              Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_LEFT_COLOR:
            attrs->borderColor.left =
              Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_RIGHT_COLOR:
            attrs->borderColor.right =
              Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_STYLE:
            attrs->borderStyle.bottom = (BorderStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_BORDER_LEFT_STYLE:
            attrs->borderStyle.left = (BorderStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_BORDER_RIGHT_STYLE:
            attrs->borderStyle.right = (BorderStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_BORDER_TOP_STYLE:
            attrs->borderStyle.top = (BorderStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
            computeValue (&attrs->borderWidth.bottom, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_BORDER_LEFT_WIDTH:
            computeValue (&attrs->borderWidth.left, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_BORDER_RIGHT_WIDTH:
            computeValue (&attrs->borderWidth.right, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_BORDER_TOP_WIDTH:
            computeValue (&attrs->borderWidth.top, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_BORDER_SPACING:
            computeValue (&attrs->hBorderSpacing, p->value.intVal, attrs->font);
            computeValue (&attrs->vBorderSpacing, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_COLOR:
            attrs->color = Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_CURSOR:
            attrs->cursor = (Cursor) p->value.intVal;
            break; 
         case CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE:
            attrs->listStyleType = (ListStyleType) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_MARGIN_BOTTOM:
            computeValue (&attrs->margin.bottom, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_MARGIN_LEFT:
            computeValue (&attrs->margin.left, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_MARGIN_RIGHT:
            computeValue (&attrs->margin.right, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_MARGIN_TOP:
            computeValue (&attrs->margin.top, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_PADDING_TOP:
            computeValue (&attrs->padding.top, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_PADDING_BOTTOM:
            computeValue (&attrs->padding.bottom, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_PADDING_LEFT:
            computeValue (&attrs->padding.left, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_PADDING_RIGHT:
            computeValue (&attrs->padding.right, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_TEXT_ALIGN:
            attrs->textAlign = (TextAlignType) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_TEXT_DECORATION:
            attrs->textDecoration |= p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_VERTICAL_ALIGN:
            attrs->valign = (VAlignType) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_WIDTH:
            computeLength (&attrs->width, p->value.intVal, attrs->font);
            break;
         case CssProperty::CSS_PROPERTY_HEIGHT:
            computeLength (&attrs->height, p->value.intVal, attrs->font);
            break;
            break;
         case CssProperty::PROPERTY_X_LINK:
            attrs->x_link = p->value.intVal;
            break;
         case CssProperty::PROPERTY_X_IMG:
            attrs->x_img = p->value.intVal;
            break;

         default:
            break;
      }
   }
}

/**
 * \brief Resolve relative lengths to absolute values.
 */

void StyleEngine::computeValue (int *dest, CssLength value, Font *font) {
   static float dpmm;

   if (dpmm == 0.0)
      dpmm = layout->dpiX () / 25.4; /* assume dpiX == dpiY */

   switch (CSS_LENGTH_TYPE (value)) {
      case CSS_LENGTH_TYPE_PX:
         *dest = (int) CSS_LENGTH_VALUE (value);
         break;
      case CSS_LENGTH_TYPE_MM:
         *dest = (int) (CSS_LENGTH_VALUE (value) * dpmm);
         break;
      case CSS_LENGTH_TYPE_EM:
         *dest = (int) (CSS_LENGTH_VALUE (value) * font->size);
         break;
      case CSS_LENGTH_TYPE_EX:
         *dest = (int) (CSS_LENGTH_VALUE(value) * font->xHeight);
         break;
      default:
         break;
   }
}

void StyleEngine::computeValue (int *dest, CssLength value, Font *font,
                                int percentageBase) {
   if (CSS_LENGTH_TYPE (value) == CSS_LENGTH_TYPE_PERCENTAGE)
      *dest = (int) (CSS_LENGTH_VALUE (value) * percentageBase);
   else
      computeValue (dest, value, font);
}

void StyleEngine::computeLength (dw::core::style::Length *dest,
                                 CssLength value, Font *font) {
   int v;

   if (CSS_LENGTH_TYPE (value) == CSS_LENGTH_TYPE_PERCENTAGE)
      *dest = createPerLength (CSS_LENGTH_VALUE (value));
    else {
      computeValue (&v, value, font);
      *dest = createAbsLength (v);
   }
}

/**
 * \brief Create a new style object based on the previously opened / closed
 *    HTML elements and the nonCssProperties that have been set.
 *    This method is private. Call style() to get a current style object.
 */
Style * StyleEngine::style0 (CssPropertyList *nonCssProperties) {
   CssPropertyList props;
   CssPropertyList *tagStyleProps = NULL; /** \todo implementation */

   // get previous style from the stack
   StyleAttrs attrs = *stack->getRef (stack->size () - 2)->style;
   // reset values that are not inherited according to CSS
   attrs.resetValues ();

   if (stack->getRef (stack->size () - 2)->inheritBackgroundColor)
      attrs.backgroundColor =
         stack->getRef (stack->size () - 2)->style->backgroundColor;
  
   cssContext->apply (&props, this, tagStyleProps, nonCssProperties);

   apply (&attrs, &props);

   stack->getRef (stack->size () - 1)->style = Style::create (layout, &attrs);
   
   return stack->getRef (stack->size () - 1)->style;
}

Style * StyleEngine::wordStyle0 (CssPropertyList *nonCssProperties) {
   StyleAttrs attrs = *style ();
   attrs.resetValues ();

   if (stack->getRef (stack->size () - 1)->inheritBackgroundColor)
      attrs.backgroundColor = style ()->backgroundColor;

   stack->getRef (stack->size () - 1)->wordStyle = Style::create (layout, &attrs);
   return stack->getRef (stack->size () - 1)->wordStyle;
}

void StyleEngine::parse (const char *buf, int buflen,
                         int order_count, CssOrigin origin) {

   a_Css_parse (cssContext, buf, buflen, order_count, origin);
}
