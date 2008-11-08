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
   
   n->style = Style::create (layout, &style_attrs);
}

StyleEngine::~StyleEngine () {
   delete stack;
}

/**
 * \brief tell the styleEngine that a new html element has started.
 */
void StyleEngine::startElement (int tag, const char *id, const char *klass,
   const char *style) {
//   fprintf(stderr, "===> START %d %s %s %s\n", tag, id, klass, style);

   if (stack->getRef (stack->size () - 1)->style == NULL)
      style0 ();

   stack->increase ();
   Node *n =  stack->getRef (stack->size () - 1);
   n->style = NULL;
   n->depth = stack->size ();
   n->tag = tag;
   n->id = id;
   n->klass = klass;
   n->pseudoClass = NULL;
   n->styleAttribute = style;
}

/**
 * \brief set properties that were definded using (mostly deprecated) HTML
 * attributes (e.g. bgColor).
 */
void StyleEngine::setNonCssProperties (CssPropertyList *props) {
   style0 (props); // evaluate now, so caller can free props
}

/**
 * \brief set the CSS pseudo class (e.g. "link", "visited").
 */
void StyleEngine::setPseudoClass (const char *pseudoClass) {
   stack->getRef (stack->size () - 1)->pseudoClass = pseudoClass;
}

/**
 * \brief tell the styleEngine that a html element has ended.
 */
void StyleEngine::endElement (int tag) {
//   fprintf(stderr, "===> END %d\n", tag);
   assert (stack->size () > 1);
   assert (tag == stack->getRef (stack->size () - 1)->tag);

   Node *n =  stack->getRef (stack->size () - 1);
   if (n->style)
      n->style->unref ();
   
   stack->setSize (stack->size () - 1);
}

void StyleEngine::apply (StyleAttrs *attrs, CssPropertyList *props) {
   FontAttrs fontAttrs = *attrs->font;

   for (int i = 0; i < props->size (); i++) {
      CssProperty *p = props->getRef (i);
      
      switch (p->name) {
         /* \todo missing cases */
         case CssProperty::CSS_PROPERTY_BACKGROUND_COLOR:
            attrs->backgroundColor =
               Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_COLOR:
            attrs->borderColor.bottom =
              Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_STYLE:
            attrs->borderStyle.bottom = (BorderStyle) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_COLOR:
            attrs->color = Color::createSimple (layout, p->value.intVal);
            break; 
         case CssProperty::CSS_PROPERTY_CURSOR:
            attrs->cursor = (Cursor) p->value.intVal;
            break; 
         case CssProperty::CSS_PROPERTY_FONT_FAMILY:
            fontAttrs.name = p->value.strVal;
            break;
         case CssProperty::CSS_PROPERTY_FONT_SIZE:
            fontAttrs.size = p->value.intVal;
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
         case CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE:
            attrs->listStyleType = (ListStyleType) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_TEXT_ALIGN:
            attrs->textAlign = (TextAlignType) p->value.intVal;
            break;
         case CssProperty::CSS_PROPERTY_TEXT_DECORATION:
            attrs->textDecoration |= p->value.intVal;
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

   attrs->font = Font::create (layout, &fontAttrs);
}

/**
 * \brief Create a new style object based on the previously opened / closed
 *    HTML elements and the nonCssProperties that have been set.
 *    This method is private. Call style() to get a current style object.
 */
Style * StyleEngine::style0 (CssPropertyList *nonCssProperties) {
   CssPropertyList props;
   CssPropertyList *tagStyleProps = CssPropertyList::parse (
      stack->getRef (stack->size () - 1)->styleAttribute);

   // get previous style from the stack
   StyleAttrs attrs = *stack->getRef (stack->size () - 2)->style;

   cssContext->apply (&props, this, tagStyleProps, nonCssProperties);

   apply (&attrs, &props);

   stack->getRef (stack->size () - 1)->style = Style::create (layout, &attrs);
   
   return stack->getRef (stack->size () - 1)->style;
}
