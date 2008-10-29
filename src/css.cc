/*
 * File: css.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include "css.hh"

void CssProperty::apply (dw::core::style::StyleAttrs *styleAttrs) {
   switch (name) {
      

      default:
         break;
   }
}

void CssPropertyList::apply (dw::core::style::StyleAttrs *styleAttrs) {
   for (int i = 0; i < size (); i++)
      get (i)->apply (styleAttrs);
}

bool CssSelector::match (Doctree *docTree) {
   return tagIndex < 0 || tagIndex == docTree->top ()->tagIndex;
}

void CssRule::apply (dw::core::style::StyleAttrs *styleAttrs,
   Doctree *docTree) {

   if (selector->match (docTree))
      props->apply (styleAttrs);
}

void CssStyleSheet::apply (dw::core::style::StyleAttrs *styleAttrs,
   Doctree *docTree) {

   for (int i = 0; i < size (); i++)
      get (i)->apply (styleAttrs, docTree);
}

void CssContext::addRule (CssRule *rule, PrimaryOrder order) {
   sheet[order].increase ();
   sheet[order].set (sheet[order].size () - 1, rule);
};

void CssContext::apply (dw::core::style::StyleAttrs *styleAttrs,
         Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCss) {

   sheet[USER_AGENT].apply (styleAttrs, docTree);
   if (nonCss)
        nonCss->apply (styleAttrs);
   for (int o = USER; o <= USER_IMPORTANT; o++)
      sheet[o].apply (styleAttrs, docTree);
   if (tagStyle)
        nonCss->apply (styleAttrs);
}
