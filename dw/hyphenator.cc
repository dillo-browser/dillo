#include "hyphenator.hh"

#include "../lout/misc.hh"
#include <stdio.h>
#include <string.h>

#define LEN 1000

/*
 * This is a direct translation of the Python implementation by Ned
 * Batchelder.
 */

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;

namespace dw {

HashTable <TypedPair <TypedPointer <core::Platform>, ConstString>,
           Hyphenator> *Hyphenator::hyphenators =
   new HashTable <TypedPair <TypedPointer <core::Platform>, ConstString>,
                  Hyphenator> (true, true);

Hyphenator::Hyphenator (core::Platform *platform, const char *filename)
{
   this->platform = platform;
   tree = new HashTable <Integer, Collection <Integer> > (true, true);

   FILE *file = fopen (filename, "r");
   // TODO Error handling
   while (!feof (file)) {
      char buf[LEN + 1];
      char *s = fgets (buf, LEN, file);
      if (s) {
         int l = strlen (s);
         if (s[l - 1] == '\n')
            s[l - 1] = 0;
         insertPattern (s);
      }
   }
   fclose (file);
}

Hyphenator::~Hyphenator ()
{
   delete tree;
}

Hyphenator *Hyphenator::getHyphenator (core::Platform *platform,
                                       const char *language)
{
   // TODO Not very efficient. Other key than TypedPair?
   // (Keeping the parts of the pair on the stack does not help, since
   // ~TypedPair deletes them, so they have to be kept on the heap.)
   TypedPair <TypedPointer <core::Platform>, ConstString> *pair =
      new TypedPair <TypedPointer <core::Platform>,
                     ConstString> (new TypedPointer <core::Platform> (platform),
                                   new ConstString (language));

   Hyphenator *hyphenator = hyphenators->get (pair);
   if (hyphenator)
      delete pair;
   else
      {
      // TODO Much hard-coded!
      char filename [256];
      sprintf (filename, "/usr/local/lib/dillo/hyphenation/%s.pat", language);

      //printf ("Loading hyphenation patterns '%s' ...\n", filename);

      hyphenator = new Hyphenator (platform, filename);
      hyphenators->put (pair, hyphenator);
   }

   return hyphenator;
}

void Hyphenator::insertPattern (char *s)
{
   // Convert the a pattern like 'a1bc3d4' into a string of chars 'abcd'
   // and a list of points [ 0, 1, 0, 3, 4 ].
   int l = strlen (s);
   char chars [l + 1];
   Vector <Integer> *points = new Vector <Integer> (1, true);

   // TODO numbers consisting of multiple digits?
   // TODO Encoding: This implementation works exactly like the Python
   // implementation, based on UTF-8. Does this always work?
   int numChars = 0;
   for (int i = 0; s[i]; i++)
      if (s[i] >= '0' && s[i] <= '9')
         points->put (new Integer (s[i] - '0'), numChars);
      else
         chars[numChars++] = s[i];
   chars[numChars] = 0;

   for (int i = 0; i < numChars + 1; i++) {
      Integer *val = points->get (i);
      if (val == NULL)
         points->put (new Integer (0), i);
   }

   // Insert the pattern into the tree.  Each character finds a dict
   // another level down in the tree, and leaf nodes have the list of
   // points.

   HashTable <Integer, Collection <Integer> > *t = tree;
   for (int i = 0; chars[i]; i++) {
      Integer c (chars[i]);
      if (!t->contains(&c))
         t->put (new Integer (chars[i]),
                 new HashTable <Integer, Collection <Integer> > (true, true));
      t = (HashTable <Integer, Collection <Integer> >*) t->get (&c);
   }

   t->put (new Integer (0), points);
}

/**
 * Simple test to avoid much costs. Passing it does not mean that the word
 * can be hyphenated.
 */
bool Hyphenator::isHyphenationCandidate (const char *word)
{
   // Short words aren't hyphenated.
   return (strlen (word) > 4); // TODO UTF-8?
}   

/**
 * Given a word, returns a list of the possible hyphenation points.
 */
int *Hyphenator::hyphenateWord(const char *word, int *numBreaks)
{
   if (!isHyphenationCandidate (word)) {
      *numBreaks = 0;
      return NULL;
   }

   // If the word is an exception, get the stored points.
   // TODO
   char work[strlen (word) + 3];
   strcpy (work, ".");
   char *wordLc = platform->textToLower (word, strlen (word));
   strcat (work, wordLc);
   delete wordLc;
   strcat (work, ".");
   
   int l = strlen (work);
   SimpleVector <int> points (l + 1);
   points.setSize (l + 1, 0);

   Integer null (0);

   for (int i = 0; i < l; i++) {
      HashTable <Integer, Collection <Integer> > *t = tree;
      for (int j = i; j < l; j++) {
         Integer c (work[j]);
         if (t->contains (&c)) {
            t = (HashTable <Integer, Collection <Integer> >*) t->get (&c);
            if (t->contains (&null)) {
               Vector <Integer> *p = (Vector <Integer>*) t->get (&null);

               for (int k = 0; k < p->size (); k++)
                  points.set(i + k, lout::misc::max (points.get (i + k),
                                                     p->get(k)->getValue()));
            }
         } else
            break;
      }
   }  

   // No hyphens in the first two chars or the last two.
   points.set (1, 0);
   points.set (2, 0);
   points.set (points.size() - 2, 0);
   points.set (points.size() - 3, 0);
   
   // Examine the points to build the pieces list.
   SimpleVector <int> breakPos (1);

   int n = lout::misc::min ((int)strlen (word), points.size () - 2);
   for (int i = 0; i < n; i++) {
      if (points.get(i + 2) % 2) {
         breakPos.increase ();
         breakPos.set (breakPos.size() - 1, i + 1);
      }
   }

   *numBreaks = breakPos.size ();
   if (*numBreaks == 0)
      return NULL;
   else {
      // Could save some cycles by directly returning the array in the
      // SimpleVector.
      int *breakPosArray = new int[*numBreaks];
      for (int i = 0; i < *numBreaks; i++)
         breakPosArray[i] = breakPos.get(i);
      return breakPosArray;
   }
}

} // namespace dw
