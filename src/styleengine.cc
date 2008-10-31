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
   n->styleAttribute = style;
}

void StyleEngine::setNonCssProperties (CssPropertyList *props) {
   style0 (props); // evaluate now, so caller can free props
}

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
               Color::createSimple (layout, p->value.color);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_COLOR:
            attrs->borderColor.bottom =
              Color::createSimple (layout, p->value.color);
            break; 
         case CssProperty::CSS_PROPERTY_BORDER_BOTTOM_STYLE:
            attrs->borderStyle.bottom = p->value.borderStyle;
            break;
         case CssProperty::CSS_PROPERTY_COLOR:
            attrs->color = Color::createSimple (layout, p->value.color);
            break; 
         case CssProperty::CSS_PROPERTY_CURSOR:
            attrs->cursor = p->value.cursor;
            break; 
         case CssProperty::CSS_PROPERTY_FONT_FAMILY:
            fontAttrs.name = p->value.name;
            break;
         case CssProperty::CSS_PROPERTY_FONT_SIZE:
            fontAttrs.size = p->value.size;
            break;
         case CssProperty::CSS_PROPERTY_FONT_STYLE:
            fontAttrs.style = p->value.fontStyle;
            break;
         case CssProperty::CSS_PROPERTY_FONT_WEIGHT:
            fontAttrs.weight = p->value.weight;
            break;
         case CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE:
            attrs->listStyleType = p->value.listStyleType;
            break;
         case CssProperty::CSS_PROPERTY_TEXT_DECORATION:
            attrs->textDecoration |= p->value.textDecoration;
            break;
         case CssProperty::PROPERTY_X_LINK:
            attrs->x_link = p->value.x_link;
            break;
         case CssProperty::PROPERTY_X_IMG:
            attrs->x_img = p->value.x_img;
            break;

         default:
            break;
      }
   }

   attrs->font = Font::create (layout, &fontAttrs);
}

Style * StyleEngine::style0 (CssPropertyList *nonCssProperties) {
   CssPropertyList props;
   CssPropertyList *tagStyleProps = CssPropertyList::parse (
      stack->getRef (stack->size () - 1)->styleAttribute);

   StyleAttrs attrs = *stack->getRef (stack->size () - 2)->style;

   cssContext->apply (&props, this, tagStyleProps, nonCssProperties);

   apply (&attrs, &props);

   stack->getRef (stack->size () - 1)->style = Style::create (layout, &attrs);
   
   return stack->getRef (stack->size () - 1)->style;
}
