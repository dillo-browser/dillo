/*
 * File: css.cc
 *
 * Copyright 2008-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <math.h>
#include "../dlib/dlib.h"
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

CssSelector::CssSelector () {
   struct CombinatorAndSelector *cs;

   refCount = 0;
   selectorList = new lout::misc::SimpleVector
                                  <struct CombinatorAndSelector> (1);
   selectorList->increase ();
   cs = selectorList->getRef (selectorList->size () - 1);

   cs->notMatchingBefore = -1;
   cs->selector.element = CssSimpleSelector::ELEMENT_ANY;
   cs->selector.klass = NULL; 
   cs->selector.pseudo = NULL;
   cs->selector.id = NULL;
};

CssSelector::~CssSelector () {
   delete selectorList;
}

bool CssSelector::match (Doctree *docTree) {
   CssSimpleSelector *sel;
   Combinator comb;
   int *notMatchingBefore;
   const DoctreeNode *n, *node = docTree->top ();

   assert (selectorList->size () > 0);

   sel = top ();
   
   if (! sel->match (node))
      return false;

   for (int i = selectorList->size () - 2; i >= 0; i--) {
      sel = &selectorList->getRef (i)->selector;
      comb = selectorList->getRef (i + 1)->combinator;
      notMatchingBefore = &selectorList->getRef (i + 1)->notMatchingBefore;
      node = docTree->parent (node);

      if (node == NULL)
         return false;
     
      switch (comb) {
         case CHILD:
            if (!sel->match (node))
               return false;
            break;
         case DESCENDENT:
            n = node;

            while (true) {
               if (node == NULL || node->num <= *notMatchingBefore) {
                  *notMatchingBefore = n->num;
                  return false;
               }

               if (sel->match (node))
                  break;

               node = docTree->parent (node);
            }
            break;
         default:
            return false; // \todo implement other combinators
      }
   } 
   
   return true;
}

void CssSelector::addSimpleSelector (Combinator c) {
   struct CombinatorAndSelector *cs;

   selectorList->increase ();
   cs = selectorList->getRef (selectorList->size () - 1);

   cs->combinator = c;
   cs->notMatchingBefore = -1;
   cs->selector.element = CssSimpleSelector::ELEMENT_ANY;
   cs->selector.klass = NULL;
   cs->selector.pseudo = NULL;
   cs->selector.id = NULL;
}

void CssSelector::print () {
   for (int i = 0; i < selectorList->size (); i++) {
      selectorList->getRef (i)->selector.print ();

      if (i < selectorList->size () - 1) {
         switch (selectorList->getRef (i + 1)->combinator) {
            case CHILD:
               fprintf (stderr, "> ");
               break;
            case DESCENDENT:
               fprintf (stderr, "\" \" ");
               break;
            default:
               fprintf (stderr, "? ");
               break;
         }
      }
   }

   fprintf (stderr, "\n");
}

bool CssSimpleSelector::match (const DoctreeNode *n) {
   if (element != ELEMENT_ANY && element != n->element)
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

void CssSimpleSelector::print () {
   fprintf (stderr, "Element %d, class %s, pseudo %s, id %s ",
      element, klass, pseudo, id);
}

CssRule::CssRule (CssSelector *selector, CssPropertyList *props) {
   this->selector = selector;
   this->selector->ref ();
   this->props = props;
   this->props->ref ();
};

CssRule::~CssRule () {
   selector->unref ();
   props->unref ();
};

void CssRule::apply (CssPropertyList *props, Doctree *docTree) {
   if (selector->match (docTree))
      this->props->apply (props);
}

void CssRule::print () {
   selector->print ();
   props->print ();
}

CssStyleSheet::CssStyleSheet () {
   for (int i = 0; i < ntags; i++)
      elementTable[i] = new RuleList ();

   idTable = new RuleMap ();
   classTable = new RuleMap ();
   anyTable = new RuleList ();
}

CssStyleSheet::~CssStyleSheet () {
   for (int i = 0; i < ntags; i++)
      delete elementTable[i];
   delete idTable;
   delete classTable;
   delete anyTable;
}

void CssStyleSheet::addRule (CssRule *rule) {
   CssSimpleSelector *top = rule->selector->top ();
   RuleList *ruleList = NULL;
   lout::object::ConstString *string;
   
   if (top->id) {
      string = new lout::object::ConstString (top->id);
      ruleList = idTable->get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList ();
         idTable->put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->klass) {
      string = new lout::object::ConstString (top->klass);
      ruleList = classTable->get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList;
         classTable->put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->element >= 0 && top->element < ntags) {
      ruleList = elementTable[top->element];
   } else if (top->element == CssSimpleSelector::ELEMENT_ANY) {
      ruleList = anyTable;
   }

   if (ruleList) {
      ruleList->increase ();
      *ruleList->getRef (ruleList->size() - 1) = rule;
   }
}

void CssStyleSheet::addRule (CssSelector *selector, CssPropertyList *props) {
   CssRule *rule = new CssRule (selector, props);
   addRule (rule);
}

void CssStyleSheet::apply (CssPropertyList *props, Doctree *docTree) {
   RuleList *ruleList[4] = {NULL, NULL, NULL, NULL};
   const DoctreeNode *top = docTree->top ();
   
   if (top->id) {
      lout::object::String idString (top->id);

      ruleList[3] = idTable->get (&idString);
   }

   if (top->klass) {
      lout::object::String classString (top->klass);

      ruleList[2] = classTable->get (&classString);
   }

   ruleList[1] = elementTable[docTree->top ()->element];
   ruleList[0] = anyTable;

   for (int i = 0;; i++) {
      int n = 0;

      for (int j = 0; j < 4; j++) {
         if (ruleList[j] && ruleList[j]->size () > i) {
            ruleList[j]->get (i)->apply (props, docTree);
            n++;
         }
      }

      if (n == 0)
         break;
   } 
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

//  fprintf(stderr, "Adding Rule (%d)\n", order);
//  rule->print ();
}

void CssContext::buildUserAgentStyle () {
   const char *cssBuf =
     "body  {background-color: #dcd1ba; font-family: helvetica; color: black;" 
     "       margin: 5px}"
     "big {font-size: 1.17em}"
     "blockquote, dd {margin-left: 40px; margin-right: 40px}"
     "center {text-align: center}"
     "dt {font-weight: bolder}"
     ":link {color: blue; text-decoration: underline; cursor: pointer}"
     ":visited {color: #800080; text-decoration: underline; cursor: pointer}"
     "h1, h2, h3, h4, h5, h6, b, strong {font-weight: bolder}"
     "i, em, cite, address {font-style: italic}"
     ":link img, :visited img {border: 1px solid}"
     "frameset, ul, ol, dir {margin-left: 40px}"
     "h1 {font-size: 2em; margin-top: .67em; margin-bottom: 0}"
     "h2 {font-size: 1.5em; margin-top: .75em; margin-bottom: 0}"
     "h3 {font-size: 1.17em; margin-top: .83em; margin-bottom: 0}"
     "h4 {margin-top: 1.12em; margin-bottom: 0}"
     "h5 {font-size: 0.83em; margin-top: 1.5em; margin-bottom: 0}"
     "h6 {font-size: 0.75em; margin-top: 1.67em; margin-bottom: 0}"
     "hr {width: 100%; border: 1px inset}"
     "li {margin-top: 0.1em}"
     "pre {white-space: pre}"
     "ol {list-style-type: decimal}"
     "ul {list-style-type: disc}"
     "ul > ul {list-style-type: circle}"
     "ul > ul > ul {list-style-type: square}"
     "ul > ul > ul > ul {list-style-type: disc}"
     "u {text-decoration: underline}"
     "small, sub, sup { font-size: 0.83em}"
     "sub { vertical-align: sub}"
     "sup { vertical-align: super}"
     "s, strike, del { text-decoration: line-through}"
     "table {border-style: outset; border-spacing: 1px}"
     "td {border-style: inset; padding: 2px}"
     "thead, tbody, tfoot { vertical-align: middle}"
     "th { font-weight: bolder; text-align: center}"
     "code, tt, pre, samp, kbd {font-family: courier}";

   a_Css_parse (this, cssBuf, strlen (cssBuf), 0, CSS_ORIGIN_USER_AGENT);
}

void CssContext::buildUserStyle () {
   char buf[1024];
   char *filename;

   filename = dStrconcat(dGethomedir(), "/.dillo/style.css", NULL);
   FILE *fp = fopen (filename, "r");
   if (fp) {
      Dstr *style = dStr_sized_new (1024);
      size_t len;
   
      while ((len = fread (buf, 1, sizeof (buf), fp)))
         dStr_append_l (style, buf, len);

      a_Css_parse (this, style->str, style->len, 0, CSS_ORIGIN_USER);
      dStr_free (style, 1);
   }

   dFree (filename);
}
