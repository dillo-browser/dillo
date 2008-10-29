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
   stack = new lout::misc::SimpleVector <Node> (1);
   cssContext = new CssContext ();
   this->layout = layout;
}

StyleEngine::~StyleEngine () {
   delete stack;
}

void
StyleEngine::startElement (int tag, const char *id, const char *klass,
   const char *style) {
   fprintf(stderr, "===> START %d %s %s %s\n", tag, id, klass, style);

   if (stack->getRef (stack->size () - 1)->style == NULL)
      stack->getRef (stack->size () - 1)->style = style0 ();

   stack->increase ();
   Node *n =  stack->getRef (stack->size () - 1);
   n->style = NULL;
   n->nonCssProperties = NULL;
   n->depth = stack->size ();
   n->tag = tag;
   n->id = id;
   n->klass = klass;
   n->styleAttribute = style;
}

void
StyleEngine::setNonCssProperties (CssPropertyList *props) {
   stack->getRef (stack->size () - 1)->nonCssProperties = props;
}

void
StyleEngine::endElement (int tag) {
   fprintf(stderr, "===> END %d\n", tag);
   assert (stack->size () > 0);

   Node *n =  stack->getRef (stack->size () - 1);
   if (n->style)
      n->style->unref ();
   if (n->nonCssProperties)
      delete n->nonCssProperties;
   
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
         case CssProperty::CSS_PROPERTY_FONT_FAMILY:
            fontAttrs.name = p->value.name;
            break;
         case CssProperty::CSS_PROPERTY_FONT_SIZE:
            fontAttrs.size = p->value.size;
            break;

         default:
            break;
      }
   }

   attrs->font = Font::create (layout, &fontAttrs);
}

Style * StyleEngine::style0 () {
   CssPropertyList props;
   CssPropertyList *tagStyleProps = CssPropertyList::parse (
      stack->getRef (stack->size () - 1)->styleAttribute);

   StyleAttrs attrs = *stack->getRef (stack->size () - 1)->style;

   cssContext->apply (&props, this, tagStyleProps,
      stack->getRef (stack->size () - 1)->nonCssProperties);

   apply (&attrs, &props);

   stack->getRef (stack->size () - 1)->style = Style::create (layout, &attrs);
   
   return NULL;
}
