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
#include <math.h>
#include "prefs.h"
#include "html_common.hh"
#include "css.hh"
#include "cssparser.hh"

using namespace dw::core::style;

void CssProperty::print () {
   fprintf (stderr, "%s - %d\n", Css_property_info[name].symbol, value.intVal);
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

void CssPropertyList::apply (CssPropertyList *props) {
   for (int i = 0; i < size (); i++)
      props->set (getRef (i)->name, getRef (i)->value);
}

void CssPropertyList::print () {
   for (int i = 0; i < size (); i++)
      getRef (i)->print ();
}

CssSelector::CssSelector (int element, const char *klass,
                          const char *pseudo, const char *id) {
   refCount = 0;
   this->element = element;
   this->klass = klass;
   this->pseudo = pseudo;
   this->id = id;
};

/** \todo implement all selection option CSS offers */
bool CssSelector::match (Doctree *docTree) {
   const DoctreeNode *n = docTree-> top ();

   if (element >= 0 && element != n->element)
      return false;
   if (klass != NULL &&
      (n->klass == NULL || strcasecmp (klass, n->klass) != 0))
      return false;
   if (pseudo != NULL &&
      (n->pseudo == NULL || strcasecmp (pseudo, n->pseudo) != 0))
      return false;
   if (id != NULL && (n->id == NULL || strcasecmp (id, n->id) != 0))
      return false;
   
   return true;
}

void CssSelector::print () {
   fprintf (stderr, "Element %d, class %s, pseudo %s, id %s\n",
      element, klass, pseudo, id);
}

CssRule::CssRule (CssSelector *selector, CssPropertyList *props) {
   this->selector = selector;
   this->selector->ref ();
   this->props = props;
   this->props->ref ();
};

CssRule::~CssRule () {
   this->selector->unref ();
   this->props->unref ();
};

void CssRule::apply (CssPropertyList *props, Doctree *docTree) {
   if (selector->match (docTree))
      this->props->apply (props);
}

void CssRule::print () {
   selector->print ();
   props->print ();
}

void CssStyleSheet::addRule (CssRule *rule) {
   increase ();
   set (size () - 1, rule);
}

void CssStyleSheet::addRule (CssSelector *selector, CssPropertyList *props) {
   CssRule *rule = new CssRule (selector, props);
   addRule (rule);
}

void CssStyleSheet::apply (CssPropertyList *props, Doctree *docTree) {
   for (int i = 0; i < size (); i++)
      get (i)->apply (props, docTree);
}

CssStyleSheet::~CssStyleSheet () {
   for (int i = 0; i < size (); i++)
      delete get (i);
}

CssStyleSheet *CssContext::userAgentStyle;
CssStyleSheet *CssContext::userStyle;
CssStyleSheet *CssContext::userImportantStyle;

CssContext::CssContext () {
   for (int o = CSS_PRIMARY_USER_AGENT; o < CSS_PRIMARY_LAST; o++)
      sheet[o] = NULL;

   if (userAgentStyle == NULL) {
      userAgentStyle = new CssStyleSheet ();
      userStyle = new CssStyleSheet ();
      userImportantStyle = new CssStyleSheet ();

      sheet[CSS_PRIMARY_USER_AGENT] = userAgentStyle;
      sheet[CSS_PRIMARY_USER] = userStyle;
      sheet[CSS_PRIMARY_USER_IMPORTANT] = userImportantStyle;

      buildUserAgentStyle ();
      buildUserStyle ();
   }

   sheet[CSS_PRIMARY_USER_AGENT] = userAgentStyle;
   sheet[CSS_PRIMARY_USER] = userStyle;
   sheet[CSS_PRIMARY_USER_IMPORTANT] = userImportantStyle;
}

CssContext::~CssContext () {
   for (int o = CSS_PRIMARY_USER_AGENT; o < CSS_PRIMARY_LAST; o++)
      if (sheet[o] != userAgentStyle && sheet[o] != userStyle &&
          sheet[o] != userImportantStyle)
         delete sheet[o];
}

void CssContext::apply (CssPropertyList *props, Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCssHints) {

   for (int o = CSS_PRIMARY_USER_AGENT; o <= CSS_PRIMARY_USER; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (nonCssHints)
        nonCssHints->apply (props);

   for (int o = CSS_PRIMARY_AUTHOR; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (tagStyle)
        tagStyle->apply (props);
}

void CssContext::addRule (CssRule *rule, CssPrimaryOrder order) {
   if (sheet[order] == NULL)
      sheet[order] = new CssStyleSheet ();

   sheet[order]->addRule (rule);

//   fprintf(stderr, "Adding Rule (%d)\n", order);
//   rule->print ();
}

void CssContext::buildUserAgentStyle () {
   char *cssBuf =
     "body  {background-color: 0xdcd1ba; font-family: helvetica; color: black;" 
     "       margin: 5px; }"
     "big { font-size: 1.17em }"
     ":link {color: blue; text-decoration: underline; cursor: pointer; } "
     ":visited {color: green; text-decoration: underline; cursor: pointer; } "
     "h1, h2, h3, h4, h5, h6, b, strong {font-weight: bolder; } "
     "i, em, cite {font-style: italic; } "
     "h1 {font-size: 2em; margin-top: .67em; margin-bottom: 0em;} "
     "h2 {font-size: 1.5em; margin-top: .75em; margin-bottom: 0em;} "
     "h3 {font-size: 1.17em; margin-top: .83em; margin-bottom: 0em;} "
     "h4 {margin-top: 1.12em; margin-bottom: 0em;} "
     "h5 {font-size: 0.83em; margin-top: 1.5em; margin-bottom: 0em;} "
     "h6 {font-size: 0.75em; margin-top: 1.67em; margin-bottom: 0em;} "
     "small, sub, sup { font-size: 0.83em } "
     "sub { vertical-align: sub } "
     "sup { vertical-align: super } "
     "s, strike, del { text-decoration: line-through }"
     "table {border-top-style: outset; border-spacing: 1px} "
     "td {border-top-style: inset; padding: 2px;} "
     "thead, tbody, tfoot { vertical-align: middle }"
     "tt, pre {font-family: courier;} ";

   a_Css_parse (this, cssBuf, strlen (cssBuf), 0, CSS_ORIGIN_USER_AGENT);
}

void CssContext::buildUserStyle () {
   char *buf;
   char *filename;

   filename = dStrconcat(dGethomedir(), "/.dillo/style.css", NULL);

   FILE *fp = fopen (filename, "r");
   if (fp) {
      buf = (char*) dMalloc (100000); /* \todo proper buffer handling */
      fread (buf, 1, 100000, fp);

      a_Css_parse (this, buf, strlen (buf), 0, CSS_ORIGIN_USER);

      dFree (buf);
   }

   dFree (filename);
}
