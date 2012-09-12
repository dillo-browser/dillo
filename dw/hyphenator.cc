#include "hyphenator.hh"

#include "../lout/misc.hh"
#include "../lout/unicode.hh"
#include <stdio.h>
#include <string.h>
#include <limits.h>

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

Hyphenator::Hyphenator (core::Platform *platform,
                        const char *patFile, const char *excFile)
{
   this->platform = platform;
   tree = NULL; // As long we are not sure whether a pattern file can be read.

   FILE *patF = fopen (patFile, "r");
   if (patF) {
      tree = new HashTable <Integer, Collection <Integer> > (true, true);
      while (!feof (patF)) {
         char buf[LEN + 1];
         char *s = fgets (buf, LEN, patF);
         if (s) {
             // TODO Better exit with an error, when the line is too long.
            int l = strlen (s);
            if (s[l - 1] == '\n')
               s[l - 1] = 0;
            insertPattern (s);
         }
      }
      fclose (patF);
   }

   exceptions = NULL; // Again, only instanciated when needed.

   FILE *excF = fopen (excFile, "r");
   if (excF) {
      exceptions = new HashTable <ConstString, Vector <Integer> > (true, true);
      while (!feof (excF)) {
         char buf[LEN + 1];
         char *s = fgets (buf, LEN, excF);
         if (s) {
             // TODO Better exit with an error, when the line is too long.
            int l = strlen (s);
            if (s[l - 1] == '\n')
               s[l - 1] = 0;
            insertException (s);
         }
      }
      fclose (excF);
   }
}

Hyphenator::~Hyphenator ()
{
   if (tree)
      delete tree;
   if (exceptions)
      delete exceptions;
}

Hyphenator *Hyphenator::getHyphenator (core::Platform *platform,
                                       const char *lang)
{
   // TODO Not very efficient. Other key than TypedPair?
   // (Keeping the parts of the pair on the stack does not help, since
   // ~TypedPair deletes them, so they have to be kept on the heap.)
   TypedPair <TypedPointer <core::Platform>, ConstString> *pair =
      new TypedPair <TypedPointer <core::Platform>,
                     ConstString> (new TypedPointer <core::Platform> (platform),
                                   new String (lang));

   Hyphenator *hyphenator = hyphenators->get (pair);
   if (hyphenator)
      delete pair;
   else {
      // TODO Much hard-coded!
      char patFile [PATH_MAX];
      snprintf (patFile, sizeof (patFile), "%s/hyphenation/%s.pat", DILLO_LIB, lang);
      char excFile [PATH_MAX];
      snprintf (excFile, sizeof(excFile), "%s/hyphenation/%s.exc", DILLO_LIB, lang);

      //printf ("Loading hyphenation patterns for language '%s' from '%s' and "
      //        "exceptions from '%s' ...\n", lang, patFile, excFile);

      hyphenator = new Hyphenator (platform, patFile, excFile);
      hyphenators->put (pair, hyphenator);
   }

   //lout::misc::StringBuffer sb;
   //hyphenators->intoStringBuffer (&sb);
   //printf ("%s\n", sb.getChars ());

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

void Hyphenator::insertException (char *s)
{
   Vector<Integer> *breaks = new Vector<Integer> (1, true);

   int len = strlen (s);
   for (int i = 0; i < len - 1; i++)
      if((unsigned char)s[i] == 0xc2 && (unsigned char)s[i + 1] == 0xad)
         breaks->put (new Integer (i - 2 * breaks->size()));

   char noHyphens[len - 2 * breaks->size() + 1];
   int j = 0;
   for (int i = 0; i < len; ) {
      if(i < len - 1 &&
         (unsigned char)s[i] == 0xc2 && (unsigned char)s[i + 1] == 0xad)
         i += 2;
      else
         noHyphens[j++] = s[i++];
   }
   noHyphens[j] = 0;

   exceptions->put (new String (noHyphens), breaks);
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
 * Test whether the character on which "s" points (UTF-8) is an actual
 * part of the word. Other characters at the beginning and end are
 * ignored.
 *
 * TODO Currently only suitable for English and German.
 * TODO Only lowercase. (Uppercase not needed.)
 */
bool Hyphenator::isCharPartOfActualWord (char *s)
{
#if 0
   // Return true when "s" points to a letter.
   return (s[0] >= 'a' && s[0] <= 'z') ||
      // UTF-8: starts with 0xc3
      ((unsigned char)s[0] == 0xc3 &&
       ((unsigned char)s[1] == 0xa4 /* ä */ ||
        (unsigned char)s[1] == 0xb6 /* ö */ ||
        (unsigned char)s[1] == 0xbc /* ü */ ||
        (unsigned char)s[1] == 0x9f /* ß */ ));
#endif
   
   return lout::unicode::isAlpha (lout::unicode::decodeUtf8 (s));
}

/**
 * Given a word, returns a list of the possible hyphenation points.
 */
int *Hyphenator::hyphenateWord(const char *word, int *numBreaks)
{
   if ((tree == NULL && exceptions ==NULL) || !isHyphenationCandidate (word)) {
      *numBreaks = 0;
      return NULL;
   }

   char *wordLc = platform->textToLower (word, strlen (word));

   // Determine "actual" word. See isCharPartOfActualWord for exact definition.
   
   // Only this actual word is used, and "startActualWord" is added to the 
   // break positions, so that these refer to the total word.
   int startActualWord = 0;
   while (wordLc[startActualWord] &&
          !isCharPartOfActualWord (wordLc + startActualWord))
      startActualWord = platform->nextGlyph (wordLc, startActualWord);

   if (wordLc[startActualWord] == 0) {
      // No letters etc in word: do not hyphenate at all.
      delete wordLc;
      *numBreaks = 0;
      return NULL;
   }

   int endActualWord = startActualWord, i = endActualWord;
   while (wordLc[i]) {
      if (isCharPartOfActualWord (wordLc + i))
         endActualWord = i;
      i = platform->nextGlyph (wordLc, i);
   }
   
   endActualWord = platform->nextGlyph (wordLc, endActualWord);
   wordLc[endActualWord] = 0;

   // If the word is an exception, get the stored points.
   Vector <Integer> *exceptionalBreaks;
   ConstString key (wordLc + startActualWord);
   if (exceptions != NULL && (exceptionalBreaks = exceptions->get (&key))) {
      int *result = new int[exceptionalBreaks->size()];
      for (int i = 0; i < exceptionalBreaks->size(); i++)
         result[i] = exceptionalBreaks->get(i)->getValue() + startActualWord;
      delete wordLc;
      *numBreaks = exceptionalBreaks->size();
      return result;
   }

   // tree == NULL means that there is no pattern file.
   if (tree == NULL) {
      delete wordLc;
      *numBreaks = 0;
      return NULL;
   }

   char work[strlen (word) + 3];
   strcpy (work, ".");
   strcat (work, wordLc + startActualWord);
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
   // Characters are not bytes, so UTF-8 characters must be counted.
   int numBytes1 = platform->nextGlyph (wordLc + startActualWord, 0);
   int numBytes2 = platform->nextGlyph (wordLc + startActualWord, numBytes1);
   for (int i = 0; i < numBytes2; i++)
      points.set (i + 1, 0);

   // TODO: Characters, not bytes (as above).
   points.set (points.size() - 2, 0);
   points.set (points.size() - 3, 0);
   
   // Examine the points to build the pieces list.
   SimpleVector <int> breakPos (1);

   int n = lout::misc::min ((int)strlen (word), points.size () - 2);
   for (int i = 0; i < n; i++) {
      if (points.get(i + 2) % 2) {
         breakPos.increase ();
         breakPos.set (breakPos.size() - 1, i + 1 + startActualWord);
      }
   }

   delete wordLc;

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
