#include "hyphenator.hh"

#include "../lout/misc.hh"
#include "../lout/unicode.hh"
#include <limits.h>
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

Hyphenator::Hyphenator (core::Platform *platform,
                        const char *patFile, const char *excFile, int pack)
{
   this->platform = platform;
   trie = NULL; // As long we are not sure whether a pattern file can be read.

   char buf[PATH_MAX + 1];
   snprintf(buf, sizeof (buf), "%s.trie", patFile);
   FILE *trieF = fopen (buf, "r");

   if (trieF) {
      trie = new Trie ();
      if (trie->load (trieF) != 0) {
         delete trie;
         trie = NULL;
      }
      fclose (trieF);
   }

   if (trie == NULL) {
      TrieBuilder trieBuilder(pack);
      FILE *patF = fopen (patFile, "r");
      if (patF) {

         while (!feof (patF)) {
            char buf[LEN + 1];
            char *s = fgets (buf, LEN, patF);
            if (s) {
               // TODO Better exit with an error, when the line is too long.
               int l = strlen (s);
               if (s[l - 1] == '\n')
                  s[l - 1] = 0;
               insertPattern (&trieBuilder, s);
            }
         }

         trie = trieBuilder.createTrie ();
         fclose (patF);
      }
   }

   exceptions = NULL; // Again, only instantiated when needed.

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
   delete trie;
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
      char patFile [PATH_MAX];
      snprintf (patFile, sizeof (patFile), "%s/hyphenation/%s.pat",
                DILLO_LIBDIR, lang);
      char excFile [PATH_MAX];
      snprintf (excFile, sizeof(excFile), "%s/hyphenation/%s.exc",
                DILLO_LIBDIR, lang);

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

void Hyphenator::insertPattern (TrieBuilder *trieBuilder, char *s)
{
   // Convert the a pattern like 'a1bc3d4' into a string of chars 'abcd'
   // and a list of points [ 0, 1, 0, 3, 4 ].
   int l = strlen (s);
   char chars [l + 1];
   SimpleVector<char> points (1);

   // TODO numbers consisting of multiple digits?
   // TODO Encoding: This implementation works exactly like the Python
   // implementation, based on UTF-8. Does this always work?
   int numChars = 0;
   for (int i = 0; s[i]; i++) {
      if (s[i] >= '0' && s[i] <= '9') {
         points.setSize(numChars + 1, '0');
         points.set(numChars, s[i]);
      } else {
         chars[numChars++] = s[i];
      }
   }
   chars[numChars] = 0;

   points.setSize(numChars + 2, '0');
   points.set(numChars + 1, '\0');

   // Insert the pattern into the tree.  Each character finds a dict
   // another level down in the tree, and leaf nodes have the list of
   // points.

   //printf("insertPattern %s\n", chars);

   trieBuilder->insert (chars, points.getArray ());
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
   if ((trie == NULL && exceptions ==NULL) || !isHyphenationCandidate (word)) {
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
      free (wordLc);
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
      free (wordLc);
      *numBreaks = exceptionalBreaks->size();
      return result;
   }

   // trie == NULL means that there is no pattern file.
   if (trie == NULL) {
      free (wordLc);
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

   for (int i = 0; i < l; i++) {
      int state = trie->root;

      for (int j = i; j < l && trie->validState (state); j++) {
         const char *p = trie->getData((unsigned char) work[j], &state);

         if (p) {
            for (int k = 0; p[k]; k++)
               points.set(i + k,
                          lout::misc::max (points.get (i + k), p[k] - '0'));
         }
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

   free (wordLc);

   *numBreaks = breakPos.size ();
   if (*numBreaks == 0)
      return NULL;
   else {
      return breakPos.detachArray ();
   }
}

Trie::TrieNode TrieBuilder::trieNodeNull = {'\0', 0, NULL};

TrieBuilder::TrieBuilder (int pack)
{
   this->pack = pack;
   dataList = new SimpleVector <DataEntry> (10000);
   stateStack = new SimpleVector <StackEntry> (10);
   tree = new SimpleVector <Trie::TrieNode> (20000);
   dataZone = new ZoneAllocator (1024);
   stateStackPush(0);
}

TrieBuilder::~TrieBuilder ()
{
   delete dataList;
   delete stateStack;
   delete tree;
   delete dataZone;
}

void TrieBuilder::insert (const char *key, const char *value)
{
   dataList->increase ();
   dataList->getLastRef ()->key = (unsigned char *) strdup(key);
   dataList->getLastRef ()->value = dataZone->strdup (value);
}

int TrieBuilder::keyCompare (const void *p1, const void *p2)
{
   DataEntry *pd1 = (DataEntry *) p1;
   DataEntry *pd2 = (DataEntry *) p2;

   return strcmp ((char *) pd1->key, (char *) pd2->key);
}

int TrieBuilder::insertState (StackEntry *state, bool root)
{
   int i, j;

   if (state->count == 0)
      return 0;

   if (root) {
      i = 0; // we reseve slot 0 for the root state
   } else {
      /* The bigger pack is the more slots we check and the smaller
       * the trie will be, but CPU consumption also increases.
       * Reasonable values for pack seemt to be between 256 and 1024.
       */
      i = tree->size () - pack + 2 * state->count;

      if (i < 256) // reserve first 256 entries for the root state
         i = 256;
   }

   for (;; i++) {
      if (i + 256 > tree->size ())
         tree->setSize (i + 256, trieNodeNull);

      for (j = 1; j < 256; j++) {
         Trie::TrieNode *tn = tree->getRef(i + j);

         if (tn->c == j || ((state->next[j] || state->data[j]) && tn->c != 0))
            break;
      }

      if (j == 256) // found a suitable slot
         break;
   }

   for (int j = 1; j < 256; j++) {
      Trie::TrieNode *tn = tree->getRef(i + j);

      if (state->next[j] || state->data[j]) {
         tn->c = j;
         tn->next = state->next[j];
         tn->data = state->data[j];
      }
   }

   assert (root || i >= 256);
   assert (!root || i == 0);
   return i;
}

void TrieBuilder::stateStackPush (unsigned char c)
{
   stateStack->increase ();
   StackEntry *e = stateStack->getLastRef ();
   memset (e, 0, sizeof (StackEntry));
   e->c = c;
}

int TrieBuilder::stateStackPop ()
{
   int next = insertState (stateStack->getLastRef (), stateStack->size () == 1);
   unsigned char c = stateStack->getLastRef ()->c;
   const char *data = stateStack->getLastRef ()->data1;

   stateStack->setSize (stateStack->size () - 1);

   if (stateStack->size () > 0) {
      assert (stateStack->getLastRef ()->next[c] == 0);
      assert (stateStack->getLastRef ()->data[c] == NULL);
      stateStack->getLastRef ()->next[c] = next;
      stateStack->getLastRef ()->data[c] = data;
      stateStack->getLastRef ()->count++;
   }

   return next;
}

Trie *TrieBuilder::createTrie ()
{
   // we need to sort the patterns as byte strings not as unicode
   qsort (dataList->getArray (), dataList->size (),
      sizeof (DataEntry), keyCompare);

   for (int i = 0; i < dataList->size (); i++) {
      insertSorted (dataList->getRef (i)->key, dataList->getRef (i)->value);
      free (dataList->getRef (i)->key);
   }

   while (stateStack->size ())
      stateStackPop ();

   int size = tree->size ();
   Trie *trie = new Trie(tree->detachArray(), size, true, dataZone);
   dataZone = NULL;
   return trie;
}

void TrieBuilder::insertSorted (unsigned char *s, const char *data)
{
   int len = strlen((char*)s);

   for (int i = 0; i < len; i++) {
      if (stateStack->size () > i + 1 &&
          stateStack->getRef (i + 1)->c != s[i]) {
         for (int j = stateStack->size () - 1; j >= i + 1; j--)
            stateStackPop();
      }

      if (i + 1 >= stateStack->size ())
         stateStackPush(s[i]);
   }

   while (stateStack->size () > len + 1)
      stateStackPop();

   assert (stateStack->size () == len + 1);
   stateStack->getLastRef ()->data1 = data;
}

Trie::Trie (TrieNode *array, int size, bool freeArray, ZoneAllocator *dataZone)
{
   this->array = array;
   this->size = size;
   this->freeArray = freeArray;
   this->dataZone = dataZone;
}

Trie::~Trie ()
{
   delete dataZone;
   if (freeArray)
      free(array);
}

void Trie::save (FILE *file)
{
   for (int i = 0; i < size; i++) {
      Trie::TrieNode *tn = &array[i];

      if (tn->data)
         fprintf(file, "%u, %u, %s\n", tn->c, tn->next, tn->data);
      else
         fprintf(file, "%u, %u\n", tn->c, tn->next);
   }
}

int Trie::load (FILE *file)
{
   int next, c, maxNext = 0;
   SimpleVector <TrieNode> tree (100);
   dataZone = new ZoneAllocator (1024);

   while (!feof (file)) {
      char buf[LEN + 1];
      char *s = fgets (buf, LEN, file);

      if (!s)
         continue;

      char data[LEN + 1];
      int n = sscanf (s, "%u, %u, %s", &c, &next, data);

      if (n >= 2 && c >= 0 && c < 256 && next >= 0) {
         tree.increase ();
         tree.getLastRef ()->c = c;
         tree.getLastRef ()->next = next;
         if (n >= 3)
            tree.getLastRef ()->data = dataZone->strdup (data);
         else
            tree.getLastRef ()->data = NULL;

         if (next > maxNext)
            maxNext = next;
      } else {
         goto error;
      }
   }

   if (maxNext >= tree.size ())
      goto error;

   size = tree.size ();
   array = tree.detachArray ();
   freeArray = true;
   return 0;

error:
   delete dataZone;
   dataZone = NULL;
   return 1;
}

} // namespace dw
