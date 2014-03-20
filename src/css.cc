/*
 * File: css.cc
 *
 * Copyright 2008-2014 Johannes Hofmann <Johannes.Hofmann@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include "../dlib/dlib.h"
#include "msg.h"
#include "html_common.hh"
#include "css.hh"

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
   safe = p.safe;
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
}

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

   if (name == CSS_PROPERTY_DISPLAY || name == CSS_PROPERTY_BACKGROUND_IMAGE)
      safe = false;

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
   for (int i = 0; i < size (); i++) {
      CssPropertyValue value = getRef (i)->value;

      if (props->ownerOfStrings &&
          (getRef (i)->type == CSS_TYPE_STRING ||
           getRef (i)->type == CSS_TYPE_SYMBOL))
         value.strVal = strdup(value.strVal);

      props->set ((CssPropertyName) getRef (i)->name,
                  (CssValueType) getRef (i)->type,
                  value);
   }
}

void CssPropertyList::print () {
   for (int i = 0; i < size (); i++)
      getRef (i)->print ();
}

CssSelector::CssSelector () {
   struct CombinatorAndSelector *cs;

   refCount = 0;
   matchCacheOffset = -1;
   selectorList.increase ();
   cs = selectorList.getRef (selectorList.size () - 1);

   cs->combinator = COMB_NONE;
   cs->selector = new CssSimpleSelector ();
}

CssSelector::~CssSelector () {
   for (int i = selectorList.size () - 1; i >= 0; i--)
      delete selectorList.getRef (i)->selector;
}

/**
 * \brief Return whether selector matches at a given node in the document tree.
 */
bool CssSelector::match (Doctree *docTree, const DoctreeNode *node,
                         int i, Combinator comb, MatchCache *matchCache) {
   int *matchCacheEntry;
   assert (node);

   if (i < 0)
      return true;

   struct CombinatorAndSelector *cs = selectorList.getRef (i);
   CssSimpleSelector *sel = cs->selector;

   switch (comb) {
      case COMB_NONE:
         break;
      case COMB_CHILD:
         node = docTree->parent (node);
         break;
      case COMB_ADJACENT_SIBLING:
         node = docTree->sibling (node);
         break;
      case COMB_DESCENDANT:
         node = docTree->parent (node);
         matchCacheEntry = matchCache->getRef(matchCacheOffset + i);

         for (const DoctreeNode *n = node;
              n && n->num > *matchCacheEntry; n = docTree->parent (n))
            if (sel->match (n) &&
                match (docTree, n, i - 1, cs->combinator, matchCache))
               return true;

         if (node) // remember that it didn't match to avoid future tests
            *matchCacheEntry = node->num;

         return false;
         break;
      default:
         return false; // \todo implement other combinators
   }

   if (!node || !sel->match (node))
      return false;

   // tail recursion should be optimized by the compiler
   return match (docTree, node, i - 1, cs->combinator, matchCache);
}

void CssSelector::addSimpleSelector (Combinator c) {
   struct CombinatorAndSelector *cs;

   assert (matchCacheOffset == -1);
   selectorList.increase ();
   cs = selectorList.getRef (selectorList.size () - 1);

   cs->combinator = c;
   cs->selector = new CssSimpleSelector ();
}

bool CssSelector::checksPseudoClass () {
   for (int i = 0; i < selectorList.size (); i++)
      if (selectorList.getRef (i)->selector->getPseudoClass ())
         return true;
   return false;
}

/**
 * \brief Return the specificity of the selector.
 *
 * The specificity of a CSS selector is defined in
 * http://www.w3.org/TR/CSS21/cascade.html#specificity
 */
int CssSelector::specificity () {
   int spec = 0;

   for (int i = 0; i < selectorList.size (); i++)
      spec += selectorList.getRef (i)->selector->specificity ();

   return spec;
}

void CssSelector::print () {
   for (int i = 0; i < selectorList.size (); i++) {
      selectorList.getRef (i)->selector->print ();

      if (i < selectorList.size () - 1) {
         switch (selectorList.getRef (i + 1)->combinator) {
            case COMB_CHILD:
               fprintf (stderr, "> ");
               break;
            case COMB_DESCENDANT:
               fprintf (stderr, "\" \" ");
               break;
            case COMB_ADJACENT_SIBLING:
               fprintf (stderr, "+ ");
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
   id = NULL;
   pseudo = NULL;
}

CssSimpleSelector::~CssSimpleSelector () {
   for (int i = 0; i < klass.size (); i++)
      dFree (klass.get (i));
   dFree (id);
   dFree (pseudo);
}

void CssSimpleSelector::setSelect (SelectType t, const char *v) {
   switch (t) {
      case SELECT_CLASS:
         klass.increase ();
         klass.set (klass.size () - 1, dStrdup (v));
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
   assert (n);
   if (element != ELEMENT_ANY && element != n->element)
      return false;
   if (pseudo != NULL &&
      (n->pseudo == NULL || dStrAsciiCasecmp (pseudo, n->pseudo) != 0))
      return false;
   if (id != NULL && (n->id == NULL || dStrAsciiCasecmp (id, n->id) != 0))
      return false;
   for (int i = 0; i < klass.size (); i++) {
      bool found = false;
      if (n->klass != NULL) {
         for (int j = 0; j < n->klass->size (); j++) {
            if (dStrAsciiCasecmp (klass.get(i), n->klass->get(j)) == 0) {
               found = true;
               break;
            }
         }
      }
      if (! found)
         return false;
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
   spec += klass.size() << 10;
   if (pseudo)
      spec += 1 << 10;
   if (element != ELEMENT_ANY)
      spec += 1;

   return spec;
}

void CssSimpleSelector::print () {
   fprintf (stderr, "Element %d, pseudo %s, id %s ",
      element, pseudo, id);
   fprintf (stderr, "class ");
   for (int i = 0; i < klass.size (); i++)
      fprintf (stderr, ".%s", klass.get (i));
}

CssRule::CssRule (CssSelector *selector, CssPropertyList *props, int pos) {
   assert (selector->size () > 0);

   this->selector = selector;
   this->selector->ref ();
   this->props = props;
   this->props->ref ();
   this->pos = pos;
   spec = selector->specificity ();
}

CssRule::~CssRule () {
   selector->unref ();
   props->unref ();
}

void CssRule::apply (CssPropertyList *props, Doctree *docTree,
                     const DoctreeNode *node, MatchCache *matchCache) const {
   if (selector->match (docTree, node, matchCache))
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
      ruleList = idTable.get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList ();
         idTable.put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->getClass () && top->getClass ()->size () > 0) {
      string = new lout::object::ConstString (top->getClass ()->get (0));
      ruleList = classTable.get (string);
      if (ruleList == NULL) {
         ruleList = new RuleList;
         classTable.put (string, ruleList);
      } else {
         delete string;
      }
   } else if (top->getElement () >= 0 && top->getElement () < ntags) {
      ruleList = &elementTable[top->getElement ()];
   } else if (top->getElement () == CssSimpleSelector::ELEMENT_ANY) {
      ruleList = &anyTable;
   }

   if (ruleList) {
      ruleList->insert (rule);
      if (rule->selector->getRequiredMatchCache () > requiredMatchCache)
         requiredMatchCache = rule->selector->getRequiredMatchCache ();
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
void CssStyleSheet::apply (CssPropertyList *props, Doctree *docTree,
                        const DoctreeNode *node, MatchCache *matchCache) const {
   static const int maxLists = 32;
   const RuleList *ruleList[maxLists];
   int numLists = 0, index[maxLists] = {0};

   if (node->id) {
      lout::object::ConstString idString (node->id);

      ruleList[numLists] = idTable.get (&idString);
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

         ruleList[numLists] = classTable.get (&classString);
         if (ruleList[numLists])
            numLists++;
      }
   }

   ruleList[numLists] = &elementTable[node->element];
   if (ruleList[numLists])
      numLists++;

   ruleList[numLists] = &anyTable;
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
         const RuleList *rl = ruleList[i];

         if (rl && rl->size () > index[i] &&
            (rl->get(index[i])->specificity () < minSpec ||
             (rl->get(index[i])->specificity () == minSpec &&
              rl->get(index[i])->position () < minPos))) {

            minSpec = rl->get(index[i])->specificity ();
            minPos = rl->get(index[i])->position ();
            minSpecIndex = i;
         }
      }

      if (minSpecIndex >= 0) {
         CssRule *rule = ruleList[minSpecIndex]->get (index[minSpecIndex]);
         rule->apply(props, docTree, node, matchCache);
         index[minSpecIndex]++;
      } else {
         break;
      }
   }
}

CssStyleSheet CssContext::userAgentSheet;

CssContext::CssContext () {
   pos = 0;
   matchCache.setSize (userAgentSheet.getRequiredMatchCache (), -1);
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

   userAgentSheet.apply (props, docTree, node, &matchCache);

   sheet[CSS_PRIMARY_USER].apply (props, docTree, node, &matchCache);

   if (nonCssHints)
        nonCssHints->apply (props);

   sheet[CSS_PRIMARY_AUTHOR].apply (props, docTree, node, &matchCache);

   if (tagStyle)
        tagStyle->apply (props);

   sheet[CSS_PRIMARY_AUTHOR_IMPORTANT].apply (props, docTree, node,
                                              &matchCache);

   if (tagStyleImportant)
        tagStyleImportant->apply (props);

   sheet[CSS_PRIMARY_USER_IMPORTANT].apply (props, docTree, node, &matchCache);
}

void CssContext::addRule (CssSelector *sel, CssPropertyList *props,
                          CssPrimaryOrder order) {

   if (props->size () > 0) {
      CssRule *rule = new CssRule (sel, props, pos++);

      if ((order == CSS_PRIMARY_AUTHOR ||
           order == CSS_PRIMARY_AUTHOR_IMPORTANT) &&
           !rule->isSafe ()) {
         MSG_WARN ("Ignoring unsafe author style that might reveal browsing history\n");
         delete rule;
      } else {
         rule->selector->setMatchCacheOffset(matchCache.size ());
         if (rule->selector->getRequiredMatchCache () > matchCache.size ())
            matchCache.setSize (rule->selector->getRequiredMatchCache (), -1);

         if (order == CSS_PRIMARY_USER_AGENT) {
            userAgentSheet.addRule (rule);
         } else { 
            sheet[order].addRule (rule);
         }
      }
   }
}
