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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include "core.hh"

namespace dw {
namespace core {

FindtextState::FindtextState ()
{
   key = NULL;
   nexttab = NULL;
   widget = NULL;
   iterator = NULL;
   hlIterator = NULL;
}

FindtextState::~FindtextState ()
{
   if (key)
      delete key;
   if (nexttab)
      delete[] nexttab;
   if (iterator)
      delete iterator;
   if (hlIterator)
      delete hlIterator;
}

void FindtextState::setWidget (Widget *widget)
{
   this->widget = widget;

   // A widget change will restart the search.
   if (key)
      delete key;
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

FindtextState::Result FindtextState::search (const char *key, bool caseSens)
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
         delete this->key;
      this->key = strdup (key);
      this->caseSens = caseSens;

      if (nexttab)
         delete[] nexttab;
      nexttab = createNexttab (key, caseSens);
      
      if (iterator)
         delete iterator;
      iterator = new CharIterator (widget);
      iterator->next ();
   } else
      newKey = false;

   bool firstTrial = !wasHighlighted || newKey;

   if (search0 ()) {
      // Highlighlighting is done with a clone.
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
      if (firstTrial)
         return NOT_FOUND;
      else {
         // Nothing found anymore, reset the state for the next trial.
         delete iterator;
         iterator = new CharIterator (widget);
         iterator->next ();
         
         // We expect a success.
         Result result2 = search (key, caseSens);
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
      delete key;
   key = NULL;  
}

int *FindtextState::createNexttab (const char *key, bool caseSens)
{
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

bool FindtextState::search0 ()
{
   if (iterator->getChar () == CharIterator::END)
      return false;

   int j = 0;
   bool nextit = true;
   int l = strlen (key);

   do {
      if (j == -1 || charsEqual (iterator->getChar (), key[j], caseSens)) {
         j++;
         nextit = iterator->next ();
      } else
         j = nexttab[j];
   } while (nextit && j < l);
   
   if (j >= l) {
      // Go back to where the word was found.
      for (int i = 0; i < l; i++)
         iterator->prev ();
      return true;
   } else
      return false;
}

} // namespace dw
} // namespace core
