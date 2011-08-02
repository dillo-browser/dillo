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
#include "../dlib/dlib.h"
#include "misc.h"
#include "msg.h"
#include "html_common.hh"
#include "css.hh"
#include "cssparser.hh"

using namespace dw::core::style;

void CssProperty::print () {
   fprintf (stderr, "%s - %d\n",
            CssParser::propertyNameString((CssPropertyName)name),
            (int)value.intVal);
}

CssPropertyList::CssPropertyList (const CssPropertyList &p, bool deep) :
   lout::misc::SimpleVector <CssProperty> (p)
{
   refCount = 0;
   if (deep) {
      for (int i = 0; i < size (); i++) {
         CssProperty *p = getRef(i);
         switch (p->type) {
            case CSS_TYPE_STRING:
            case CSS_TYPE_SYMBOL:
               p->value.strVal = dStrdup (p->value.strVal);
               break;
            default:
               break;
         }
      }
      ownerOfStrings = true;
   } else {
      ownerOfStrings = false;
   }
};

CssPropertyList::~CssPropertyList () {
   if (ownerOfStrings)
      for (int i = 0; i < size (); i++)
         getRef (i)->free ();
}

/**
 * \brief Set property to a given name and type.
 */
void CssPropertyList::set (CssPropertyName name, CssValueType type,
                           CssPropertyValue value) {
   CssProperty *prop;

   for (int i = 0; i < size (); i++) {
      prop = getRef (i);

      if (prop->name == name) {
         if (ownerOfStrings)
            prop->free ();
         prop->type = type;
         prop->value = value;
         return;
      }
   }

   increase ();
   prop = getRef (size () - 1);
   prop->name = name;
   prop->type = type;
   prop->value = value;
}

/**
 * \brief Merge properties into argument property list.
 */
void CssPropertyList::apply (CssPropertyList *props) {
   for (int i = 0; i < size (); i++)
      props->set ((CssPropertyName) getRef (i)->name,
                  (CssValueType) getRef (i)->type,
                  getRef (i)->value);
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
   cs->selector = new CssSimpleSelector ();
};

CssSelector::~CssSelector () {
   for (int i = selectorList->size () - 1; i >= 0; i--)
      delete selectorList->getRef (i)->selector;
   delete selectorList;
}

/**
 * \brief Return whether selector matches at a given node in the document tree.
 */
bool CssSelector::match (Doctree *docTree, const DoctreeNode *node) {
   CssSimpleSelector *sel;
   Combinator comb = CHILD;
   int *notMatchingBefore;
   const DoctreeNode *n;

   for (int i = selectorList->size () - 1; i >= 0; i--) {
      struct CombinatorAndSelector *cs = selectorList->getRef (i);

      sel = cs->selector;
      notMatchingBefore = &cs->notMatchingBefore;

      if (node == NULL)
         return false;

      switch (comb) {
         case CHILD:
            if (!sel->match (node))
               return false;
            break;
         case DESCENDANT:
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

      comb = cs->combinator;
      node = docTree->parent (node);
   }

   return true;
}

void CssSelector::addSimpleSelector (Combinator c) {
   struct CombinatorAndSelector *cs;

   selectorList->increase ();
   cs = selectorList->getRef (selectorList->size () - 1);

   cs->combinator = c;
   cs->notMatchingBefore = -1;
   cs->selector = new CssSimpleSelector ();
}

/**
 * \brief Return the specificity of the selector.
 *
 * The specificity of a CSS selector is defined in
 * http://www.w3.org/TR/CSS21/cascade.html#specificity
 */
int CssSelector::specificity () {
   int spec = 0;

   for (int i = 0; i < selectorList->size (); i++)
      spec += selectorList->getRef (i)->selector->specificity ();

   return spec;
}

void CssSelector::print () {
   for (int i = 0; i < selectorList->size (); i++) {
      selectorList->getRef (i)->selector->print ();

      if (i < selectorList->size () - 1) {
         switch (selectorList->getRef (i + 1)->combinator) {
            case CHILD:
               fprintf (stderr, "> ");
               break;
            case DESCENDANT:
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

CssSimpleSelector::CssSimpleSelector () {
   element = ELEMENT_ANY;
   klass = NULL;
   id = NULL;
   pseudo = NULL;
}

CssSimpleSelector::~CssSimpleSelector () {
   if (klass) {
      for (int i = 0; i < klass->size (); i++)
         dFree (klass->get (i));
      delete klass;
   }
   dFree (id);
   dFree (pseudo);
}

void CssSimpleSelector::setSelect (SelectType t, const char *v) {
   switch (t) {
      case SELECT_CLASS:
         if (klass == NULL)
            klass = new lout::misc::SimpleVector <char *> (1);
         klass->increase ();
         klass->set (klass->size () - 1, dStrdup (v));
         break;
      case SELECT_PSEUDO_CLASS:
         if (pseudo == NULL)
            pseudo = dStrdup (v);
         break;
      case SELECT_ID:
         if (id == NULL)
            id = dStrdup (v);
         break;
      default:
         break;
   }
}

/**
 * \brief Return whether simple selector matches at a given node of
 *        the document tree.
 */
bool CssSimpleSelector::match (const DoctreeNode *n) {
   if (element != ELEMENT_ANY && element != n->element)
      return false;
   if (pseudo != NULL &&
      (n->pseudo == NULL || dStrcasecmp (pseudo, n->pseudo) != 0))
      return false;
   if (id != NULL && (n->id == NULL || dStrcasecmp (id, n->id) != 0))
      return false;
   if (klass != NULL) {
      for (int i = 0; i < klass->size (); i++) {
         bool found = false;
         if (n->klass != NULL) {
            for (int j = 0; j < n->klass->size (); j++) {
               if (dStrcasecmp (klass->get(i), n->klass->get(j)) == 0) {
                  found = true;
                  break;
               }
            }
         }
         if (! found)
            return false;
      }
   }

   return true;
}

/**
 * \brief Return the specificity of the simple selector.
 *
 * The result is used in CssSelector::specificity ().
 */
int CssSimpleSelector::specificity () {
   int spec = 0;

   if (id)
      spec += 1 << 20;
   if (klass)
      spec += klass->size() << 10;
   if (pseudo)
      spec += 1 << 10;
   if (element != ELEMENT_ANY)
      spec += 1;

   return spec;
}

void CssSimpleSelector::print () {
   fprintf (stderr, "Element %d, pseudo %s, id %s ",
      element, pseudo, id);
   if (klass != NULL) {
      fprintf (stderr, "class ");
      for (int i = 0; i < klass->size (); i++)
         fprintf (stderr, ".%s", klass->get (i));
   }
}

CssRule::CssRule (CssSelector *selector, CssPropertyList *props, int pos) {
   assert (selector->size () > 0);

   this->selector = selector;
   this->selector->ref ();
   this->props = props;
   this->props->ref ();
   this->pos = pos;
   spec = selector->specificity ();
};

CssRule::~CssRule () {
   selector->unref ();
   props->unref ();
};

void CssRule::apply (CssPropertyList *props,
                     Doctree *docTree, const DoctreeNode *node) {
   if (selector->match (docTree, node))
      this->props->apply (props);
}

void CssRule::print () {
   selector->print ();
   props->print ();
}

/*
 * \brief Insert rule with increasing specificity.
 *
 * If two rules have the same specificity, the one that was added later
 * will be added behind the others.
 * This gives later added rules more weight.
 */
void CssStyleSheet::RuleList::insert (CssRule *rule) {
   increase ();
   int i = size () - 1;

   while (i > 0 && rule->specificity () < get (i - 1)->specificity ()) {
      *getRef (i) = get (i - 1);
      i--;
   }

   *getRef (i) = rule;
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

/**
 * \brief Insert a rule into CssStyleSheet.
 *
 * To improve matching performance the rules are organized into
 * rule lists based on the topmost simple selector of their selector.
 */
void CssStyleSheet::addRule (CssRule *rule) {
   CssSimpleSelector *top = rule->selector->top ();
   RuleList *ruleList = NULL;
   lout::object::ConstString *string;

   if (top->getId ()) {
      string = new lout::object::ConstString (top->getId ());
      ruleList = idTable->get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList ();
         idTable->put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->getClass () && top->getClass ()->size () > 0) {
      string = new lout::object::ConstString (top->getClass ()->get (0));
      ruleList = classTable->get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList;
         classTable->put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->getElement () >= 0 && top->getElement () < ntags) {
      ruleList = elementTable[top->getElement ()];
   } else if (top->getElement () == CssSimpleSelector::ELEMENT_ANY) {
      ruleList = anyTable;
   }

   if (ruleList) {
      ruleList->insert (rule);
   } else {
      assert (top->getElement () == CssSimpleSelector::ELEMENT_NONE);
      delete rule;
   }
}

/**
 * \brief Apply a stylesheet to a property list.
 *
 * The properties are set as defined by the rules in the stylesheet that
 * match at the given node in the document tree.
 */
void CssStyleSheet::apply (CssPropertyList *props,
                           Doctree *docTree, const DoctreeNode *node) {
   static const int maxLists = 32;
   RuleList *ruleList[maxLists];
   int numLists = 0, index[maxLists] = {0};

   if (node->id) {
      lout::object::ConstString idString (node->id);

      ruleList[numLists] = idTable->get (&idString);
      if (ruleList[numLists])
         numLists++;
   }

   if (node->klass) {
      for (int i = 0; i < node->klass->size (); i++) {
         if (i >= maxLists - 4) {
            MSG_WARN("Maximum number of classes per element exceeded.\n");
            break;
         }

         lout::object::ConstString classString (node->klass->get (i));

         ruleList[numLists] = classTable->get (&classString);
         if (ruleList[numLists])
            numLists++;
      }
   }

   ruleList[numLists] = elementTable[node->element];
   if (ruleList[numLists])
      numLists++;

   ruleList[numLists] = anyTable;
   if (ruleList[numLists])
      numLists++;

   // Apply potentially matching rules from ruleList[0-numLists] with
   // ascending specificity.
   // If specificity is equal, rules are applied in order of appearance.
   //  Each ruleList is sorted already.
   while (true) {
      int minSpec = 1 << 30;
      int minPos = 1 << 30;
      int minSpecIndex = -1;

      for (int i = 0; i < numLists; i++) {
         if (ruleList[i] && ruleList[i]->size () > index[i] &&
            (ruleList[i]->get(index[i])->specificity () < minSpec ||
             (ruleList[i]->get(index[i])->specificity () == minSpec &&
              ruleList[i]->get(index[i])->position () < minPos))) {

            minSpec = ruleList[i]->get(index[i])->specificity ();
            minPos = ruleList[i]->get(index[i])->position ();
            minSpecIndex = i;
         }
      }

      if (minSpecIndex >= 0) {
         ruleList[minSpecIndex]->get (index[minSpecIndex])->apply
                                                        (props, docTree, node);
         index[minSpecIndex]++;
      } else {
         break;
      }
   }
}

CssContext::CssContext () {
   pos = 0;

   memset (sheet, 0, sizeof(sheet));
   sheet[CSS_PRIMARY_USER_AGENT] = new CssStyleSheet ();
   sheet[CSS_PRIMARY_USER] = new CssStyleSheet ();
   sheet[CSS_PRIMARY_USER_IMPORTANT] = new CssStyleSheet ();

   buildUserAgentStyle ();
   buildUserStyle ();
}

CssContext::~CssContext () {
   for (int o = CSS_PRIMARY_USER_AGENT; o < CSS_PRIMARY_LAST; o++)
      delete sheet[o];
}

/**
 * \brief Apply a CSS context to a property list.
 *
 * The stylesheets in the context are applied one after the other
 * in the ordering defined by CSS 2.1.
 * Stylesheets that are applied later can overwrite properties set
 * by previous stylesheets.
 * This allows e.g. user styles to overwrite author styles.
 */
void CssContext::apply (CssPropertyList *props, Doctree *docTree,
         DoctreeNode *node,
         CssPropertyList *tagStyle, CssPropertyList *tagStyleImportant,
         CssPropertyList *nonCssHints) {
   if (sheet[CSS_PRIMARY_USER_AGENT])
      sheet[CSS_PRIMARY_USER_AGENT]->apply (props, docTree, node);

   if (sheet[CSS_PRIMARY_USER])
      sheet[CSS_PRIMARY_USER]->apply (props, docTree, node);

   if (nonCssHints)
        nonCssHints->apply (props);

   if (sheet[CSS_PRIMARY_AUTHOR])
      sheet[CSS_PRIMARY_AUTHOR]->apply (props, docTree, node);

   if (tagStyle)
        tagStyle->apply (props);

   if (sheet[CSS_PRIMARY_AUTHOR_IMPORTANT])
      sheet[CSS_PRIMARY_AUTHOR_IMPORTANT]->apply (props, docTree, node);

   if (tagStyleImportant)
        tagStyleImportant->apply (props);

   if (sheet[CSS_PRIMARY_USER_IMPORTANT])
      sheet[CSS_PRIMARY_USER_IMPORTANT]->apply (props, docTree, node);
}

void CssContext::addRule (CssSelector *sel, CssPropertyList *props,
                          CssPrimaryOrder order) {

   if (props->size () > 0) {
      CssRule *rule = new CssRule (sel, props, pos++);

      if (sheet[order] == NULL)
         sheet[order] = new CssStyleSheet ();

      sheet[order]->addRule (rule);
   }
}

/**
 * \brief Create the user agent style.
 *
 * The user agent style defines how dillo renders HTML in the absence of
 * author or user styles.
 */
void CssContext::buildUserAgentStyle () {
   const char *cssBuf =
     "body  {margin: 5px}"
     "big {font-size: 1.17em}"
     "blockquote, dd {margin-left: 40px; margin-right: 40px}"
     "center {text-align: center}"
     "dt {font-weight: bolder}"
     ":link {color: blue; text-decoration: underline; cursor: pointer}"
     ":visited {color: #800080; text-decoration: underline; cursor: pointer}"
     "h1, h2, h3, h4, h5, h6, b, strong {font-weight: bolder}"
     "i, em, cite, address, var {font-style: italic}"
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
     "ul ul {list-style-type: circle}"
     "ul ul ul {list-style-type: square}"
     "ul ul ul ul {list-style-type: disc}"
     "u {text-decoration: underline}"
     "small, sub, sup {font-size: 0.83em}"
     "sub {vertical-align: sub}"
     "sup {vertical-align: super}"
     "s, strike, del {text-decoration: line-through}"
     "table {border-spacing: 2px}"
     "td, th {padding: 2px}"
     "thead, tbody, tfoot {vertical-align: middle}"
     "th {font-weight: bolder; text-align: center}"
     "code, tt, pre, samp, kbd {font-family: monospace}"
     /* WORKAROUND: Reset font properties in tables as some
      * some pages rely on it (e.g. gmail).
      * http://developer.mozilla.org/En/Fixing_Table_Inheritance_in_Quirks_Mode
      * has a detailed description of the issue.
      */
     "table, caption {font-size: medium; font-weight: normal}";

   CssParser::parse (NULL, NULL, this, cssBuf, strlen (cssBuf),
                     CSS_ORIGIN_USER_AGENT);
}

void CssContext::buildUserStyle () {
   Dstr *style;
   char *filename = dStrconcat(dGethomedir(), "/.dillo/style.css", NULL);

   if ((style = a_Misc_file2dstr(filename))) {
      CssParser::parse (NULL,NULL,this,style->str, style->len,CSS_ORIGIN_USER);
      dStr_free (style, 1);
   }
   dFree (filename);
}
