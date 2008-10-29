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

/** \todo dummy only */
CssPropertyList *CssPropertyList::parse (const char *buf) {
   return NULL;
}

void CssPropertyList::set (CssProperty::Name name, CssProperty::Value value) {
   for (int i = 0; i < size (); i++)
      if (getRef (i)->name == name) {
         getRef (i)->value = value;
         return;
      }

   increase ();
   getRef (size () - 1)->name = name;
   getRef (size () - 1)->value = value;
}

void CssPropertyList::apply (CssPropertySet *props) {
   for (int i = 0; i < size (); i++)
      props->set (getRef (i)->name, getRef (i)->value);
}

/** \todo dummy only */
CssSelector *CssSelector::parse (const char *buf) {
   return NULL;
}

/** \todo dummy only */
bool CssSelector::match (Doctree *docTree) {
   return tag < 0 || tag == docTree->top ()->tag;
}

void CssRule::apply (CssPropertySet *props, Doctree *docTree) {
   if (selector->match (docTree))
      this->props->apply (props);
}

void CssStyleSheet::apply (CssPropertySet *props, Doctree *docTree) {
   for (int i = 0; i < size (); i++)
      get (i)->apply (props, docTree);
}

void CssContext::addRule (CssRule *rule, PrimaryOrder order) {
   sheet[order].increase ();
   sheet[order].set (sheet[order].size () - 1, rule);
};

void CssContext::apply (CssPropertySet *props, Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCss) {

   sheet[CSS_PRIMARY_USER_AGENT].apply (props, docTree);
   if (nonCss)
        nonCss->apply (props);
   for (int o = CSS_PRIMARY_USER; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      sheet[o].apply (props, docTree);
   if (tagStyle)
        nonCss->apply (props);
}
