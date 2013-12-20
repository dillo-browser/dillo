/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "core.hh"
#include "../lout/debug.hh"
#include "../lout/msg.h"

namespace dw {
namespace core {

FindtextState::FindtextState ()
{
   DBG_OBJ_CREATE ("dw::core::FindtextState");

   key = NULL;
   nexttab = NULL;
   widget = NULL;
   iterator = NULL;
   hlIterator = NULL;
}

FindtextState::~FindtextState ()
{
   if (key)
      free(key);
   if (nexttab)
      delete[] nexttab;
   if (iterator)
      delete iterator;
   if (hlIterator)
      delete hlIterator;

   DBG_OBJ_DELETE ();
}

void FindtextState::setWidget (Widget *widget)
{
   this->widget = widget;

   // A widget change will restart the search.
   if (key)
      free(key);
   key = NULL;
   if (nexttab)
      delete[] nexttab;
   nexttab = NULL;

   if (iterator)
      delete iterator;
   iterator = NULL;
   if (hlIterator)
      delete hlIterator;
   hlIterator = NULL;
}

FindtextState::Result FindtextState::search (const char *key, bool caseSens,
                                             bool backwards)
{
   if (!widget || *key == 0) // empty keys are not found
      return NOT_FOUND;

   bool wasHighlighted = unhighlight ();
   bool newKey;

   // If the key (or the widget) changes (including case sensitivity),
   // the search is started from the beginning.
   if (this->key == NULL || this->caseSens != caseSens ||
       strcmp (this->key, key) != 0) {
      newKey = true;
      if (this->key)
         free(this->key);
      this->key = strdup (key);
      this->caseSens = caseSens;

      if (nexttab)
         delete[] nexttab;
      nexttab = createNexttab (key, caseSens, backwards);

      if (iterator)
         delete iterator;
      iterator = new CharIterator (widget);

      if (backwards) {
         /* Go to end */
         while (iterator->next () ) ;
         iterator->prev (); //We don't want to be at CharIterator::END.
      } else {
         iterator->next ();
      }
   } else
      newKey = false;

   bool firstTrial = !wasHighlighted || newKey;

   if (search0 (backwards, firstTrial)) {
      // Highlighting is done with a clone.
      hlIterator = iterator->cloneCharIterator ();
      for (int i = 0; key[i]; i++)
         hlIterator->next ();
      CharIterator::highlight (iterator, hlIterator, HIGHLIGHT_FINDTEXT);
      CharIterator::scrollTo (iterator, hlIterator,
                              HPOS_INTO_VIEW, VPOS_CENTER);

      // The search will continue from the word after the found position.
      iterator->next ();
      return SUCCESS;
   } else {
      if (firstTrial) {
         return NOT_FOUND;
      } else {
         // Nothing found anymore, reset the state for the next trial.
         delete iterator;
         iterator = new CharIterator (widget);
         if (backwards) {
            /* Go to end */
            while (iterator->next ()) ;
            iterator->prev (); //We don't want to be at CharIterator::END.
         } else {
            iterator->next ();
         }
         // We expect a success.
         Result result2 = search (key, caseSens, backwards);
         assert (result2 == SUCCESS);
         return RESTART;
      }
   }
}

/**
 * \brief This method is called when the user closes the "find text" dialog.
 */
void FindtextState::resetSearch ()
{
   unhighlight ();

   if (key)
      free(key);
   key = NULL;
}

/*
 * Return a new string: with the reverse of the original.
 */
const char* FindtextState::rev(const char *str)
{
   if (!str)
      return NULL;

   int len = strlen(str);
   char *nstr = new char[len+1];
   for (int i = 0; i < len; ++i)
      nstr[i] = str[len-1 -i];
   nstr[len] = 0;

   return nstr;
}

int *FindtextState::createNexttab (const char *needle, bool caseSens,
                                   bool backwards)
{
   const char* key;

   key = (backwards) ? rev(needle) : needle;
   int i = 0;
   int j = -1;
   int l = strlen (key);
   int *nexttab = new int[l + 1]; // + 1 is necessary for l == 1 case
   nexttab[0] = -1;

   do {
      if (j == -1 || charsEqual (key[i], key[j], caseSens)) {
         i++;
         j++;
         nexttab[i] = j;
         //_MSG ("nexttab[%d] = %d\n", i, j);
      } else
         j = nexttab[j];
   } while (i < l - 1);

   if (backwards)
      delete [] key;

   return nexttab;
}

/**
 * \brief Unhighlight, and return whether a region was highlighted.
 */
bool FindtextState::unhighlight ()
{
   if (hlIterator) {
      CharIterator *start = hlIterator->cloneCharIterator ();
      for (int i = 0; key[i]; i++)
         start->prev ();

      CharIterator::unhighlight (start, hlIterator, HIGHLIGHT_FINDTEXT);
      delete start;
      delete hlIterator;
      hlIterator = NULL;

      return true;
   } else
      return false;
}

bool FindtextState::search0 (bool backwards,  bool firstTrial)
{
   if (iterator->getChar () == CharIterator::END)
      return false;

   bool ret = false;
   const char* searchKey = (backwards) ? rev(key) : key;
   int j = 0;
   bool nextit = true;
   int l = strlen (key);

   if (backwards && !firstTrial) {
      _MSG("Having to do.");
      /* Position correctly */
      /* In order to achieve good results (i.e: find a word that ends within
       * the previously searched word's limit) we have to position the
       * iterator in the semilast character of the previously searched word.
       *
       * Since we know that if a word was found before it was exactly the
       * same word as the one we are searching for now, we can apply the
       * following expression:
       *
       * Where l=length of the key and n=num of positions to move:
       *
       * n = l - 3
       *
       * If n is negative, we have to move backwards, but if it is
       * positive, we have to move forward. So, when l>=4, we start moving
       * the iterator forward. */

      if (l==1) {
         iterator->prev();
         iterator->prev();
      } else if (l==2) {
         iterator->prev();
      } else if (l>=4) {
         for (int i=0; i<l-3; i++) {
            iterator->next();
         }
      }

   } else if (backwards && l==1) {
      /* Particular case where we can't find the last character */
      iterator->next();
   }

   do {
      if (j == -1 || charsEqual (iterator->getChar(),searchKey[j],caseSens)) {
         j++;
         nextit = backwards ? iterator->prev () : iterator->next ();
      } else
         j = nexttab[j];
   } while (nextit && j < l);

   if (j >= l) {
      if (backwards) {
         //This is the location of the key
         iterator->next();
      } else {
         // Go back to where the key was found.
         for (int i = 0; i < l; i++)
            iterator->prev ();
      }
      ret = true;
   }

   if (backwards)
      delete [] searchKey;

   return ret;
}

} // namespace core
} // namespace dw
