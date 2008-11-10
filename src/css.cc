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
#include "prefs.h"
#include "html_common.hh"
#include "css.hh"

using namespace dw::core::style;

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

void CssPropertyList::apply (CssPropertyList *props) {
   for (int i = 0; i < size (); i++)
      props->set (getRef (i)->name, getRef (i)->value);
}

/** \todo dummy only */
CssSelector *CssSelector::parse (const char *buf) {
   return NULL;
}

/** \todo implement all selection option CSS offers */
bool CssSelector::match (Doctree *docTree) {
   const DoctreeNode *n = docTree-> top ();

   if (tag >= 0 && tag != n->tag)
      return false;
   if (klass != NULL &&
      (n->klass == NULL || strcmp (klass, n->klass) != 0) &&
      (n->pseudoClass == NULL || strcmp (klass, n->pseudoClass) != 0))
      return false;
   if (id != NULL && (n->id == NULL || strcmp (id, n->id) != 0))
      return false;
   
   return true;
}

void CssRule::apply (CssPropertyList *props, Doctree *docTree) {
   if (selector->match (docTree))
      this->props->apply (props);
}

void CssStyleSheet::addRule (CssSelector *selector, CssPropertyList *props) {
   CssRule *rule = new CssRule (selector, props);
   increase ();
   set (size () - 1, rule);
}

void CssStyleSheet::apply (CssPropertyList *props, Doctree *docTree) {
   for (int i = 0; i < size (); i++)
      get (i)->apply (props, docTree);
}

CssStyleSheet *CssContext::userAgentStyle;
CssStyleSheet *CssContext::userStyle;
CssStyleSheet *CssContext::userImportantStyle;

CssContext::CssContext () {
   for (int o = CSS_PRIMARY_USER_AGENT; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      sheet[o] = NULL;

   if (userAgentStyle == NULL) {
      userAgentStyle = buildUserAgentStyle ();
      userStyle = buildUserStyle (false);
      userImportantStyle = buildUserStyle (true);
   }

   sheet[CSS_PRIMARY_USER_AGENT] = userAgentStyle;
   sheet[CSS_PRIMARY_USER] = userStyle;
   sheet[CSS_PRIMARY_USER_IMPORTANT] = userImportantStyle;
}

void CssContext::apply (CssPropertyList *props, Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCss) {

   for (int o = CSS_PRIMARY_USER_AGENT; o <= CSS_PRIMARY_USER; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (nonCss)
        nonCss->apply (props);

   for (int o = CSS_PRIMARY_AUTHOR; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (tagStyle)
        tagStyle->apply (props);
}

CssStyleSheet * CssContext::buildUserAgentStyle () {
   CssStyleSheet *s = new CssStyleSheet ();
   CssPropertyList *props;

   // :link
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_COLOR, 0x0000ff);
   props->set (CssProperty::CSS_PROPERTY_TEXT_DECORATION, TEXT_DECORATION_UNDERLINE);
   props->set (CssProperty::CSS_PROPERTY_CURSOR, CURSOR_POINTER);
   s->addRule (new CssSelector(-1, "link", NULL), props);

   // :visited
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_COLOR, 0x800080);
   props->set (CssProperty::CSS_PROPERTY_TEXT_DECORATION, TEXT_DECORATION_UNDERLINE);
   props->set (CssProperty::CSS_PROPERTY_CURSOR, CURSOR_POINTER);
   s->addRule (new CssSelector(-1, "visited", NULL), props);

   // <b>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_WEIGHT, CssProperty::CSS_FONT_WEIGHT_BOLDER);
   s->addRule (new CssSelector(a_Html_tag_index("b"), NULL, NULL), props);

   // <i>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_STYLE, FONT_STYLE_ITALIC);
   s->addRule (new CssSelector(a_Html_tag_index("i"), NULL, NULL), props);

   // <h1>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, 40);
   s->addRule (new CssSelector(a_Html_tag_index("h1"), NULL, NULL), props);

   // <h2>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, 30);
   s->addRule (new CssSelector(a_Html_tag_index("h2"), NULL, NULL), props);

   // <h3>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, 20);
   s->addRule (new CssSelector(a_Html_tag_index("h3"), NULL, NULL), props);

   // <h4>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, 12);
   props->set (CssProperty::CSS_PROPERTY_FONT_WEIGHT, CssProperty::CSS_FONT_WEIGHT_BOLD);
   s->addRule (new CssSelector(a_Html_tag_index("h4"), NULL, NULL), props);

   // <ol>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE, LIST_STYLE_TYPE_DECIMAL);
   s->addRule (new CssSelector(a_Html_tag_index("ol"), NULL, NULL), props);

   // <pre>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, "DejaVu Sans Mono");
   s->addRule (new CssSelector(a_Html_tag_index("pre"), NULL, NULL), props);

   // <table>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_BORDER_STYLE, BORDER_OUTSET);
   s->addRule (new CssSelector(a_Html_tag_index("table"), NULL, NULL), props);

   // <td>
   props = new CssPropertyList ();
   props->set (CssProperty::CSS_PROPERTY_BORDER_STYLE, BORDER_INSET);
   s->addRule (new CssSelector(a_Html_tag_index("td"), NULL, NULL), props);
   
   return s;
}

CssStyleSheet * CssContext::buildUserStyle (bool important) {
   CssStyleSheet *s = new CssStyleSheet ();
   CssPropertyList *props;

   if (! important) {
      // :link 
      props = new CssPropertyList ();
      props->set (CssProperty::CSS_PROPERTY_COLOR, prefs.link_color);
      s->addRule (new CssSelector(-1, "link", NULL), props);

      // :visited 
      props = new CssPropertyList ();
      props->set (CssProperty::CSS_PROPERTY_COLOR, prefs.visited_color);
      s->addRule (new CssSelector(-1, "visited", NULL), props);

      // <body>
      props = new CssPropertyList ();
      props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, prefs.vw_fontname);
      props->set (CssProperty::CSS_PROPERTY_COLOR, prefs.text_color);
      s->addRule (new CssSelector(a_Html_tag_index("body"), NULL, NULL), props);

      // <pre>
      props = new CssPropertyList ();
      props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, prefs.fw_fontname);
      s->addRule (new CssSelector(a_Html_tag_index("pre"), NULL, NULL), props);
   }

   return s;
}
