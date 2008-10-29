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
   stack->setSize (stack->size () - 1);
}

void StyleEngine::apply (dw::core::style::StyleAttrs *attr,
   CssPropertyList *props) {

   for (int i = 0; i < props->size (); i++) {
      CssProperty *p = props->getRef (i);
      
      switch (p->name) {


         default:
            break;
      }
   }
}

dw::core::style::Style * StyleEngine::style0 () {
   CssPropertyList props;
   CssPropertyList *tagStyleProps = CssPropertyList::parse (
      stack->getRef (stack->size () - 1)->styleAttribute);

   dw::core::style::StyleAttrs attrs =
      *stack->getRef (stack->size () - 1)->style;

   cssContext->apply (&props, this, tagStyleProps,
      stack->getRef (stack->size () - 1)->nonCssProperties);

   apply (&attrs, &props);

   stack->getRef (stack->size () - 1)->style =
      dw::core::style::Style::create (layout, &attrs);
   
   return NULL;
}
