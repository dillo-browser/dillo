#include "../lout/misc.hh"
#include "../lout/object.hh"
#include "../lout/container.hh"
#include <stdio.h>
#include <string.h>

#define LEN 1000

/*
 * This is a direct translation of the Python implementation by Ned
 * Batchelder.
 */

class Hyphenator
{
private:
   lout::container::typed::HashTable <lout::object::Integer,
                                      lout::container::typed::Collection
                                      <lout::object::Integer> > *tree;
   void insertPattern (char *s);

public:
   Hyphenator (const char *filename);
   
   lout::container::typed::Vector <lout::object::String>
   *hyphenateWord(const char *word);
};

using namespace lout::object;
using namespace lout::container::typed;

Hyphenator::Hyphenator (const char *filename)
{
   tree = new HashTable <Integer, Collection <Integer> > (true, true);

   FILE *file = fopen (filename, "r");
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
 * Given a word, returns a list of pieces, broken at the possible
 * hyphenation points.
 */
Vector <String> *Hyphenator::hyphenateWord(const char *word)
{
   // Short words aren't hyphenated.
   if (strlen (word) <= 4) // TODO UTF-8
      return NULL; // TODO

   // If the word is an exception, get the stored points.
   // TODO

   char work[strlen (word) + 3];
   strcpy (work, ".");
   strcat (work, word); // TODO tolower
   strcat (work, ".");
   
   int l = strlen (work);
   Vector <Integer> points (l + 1, true);
   for (int i = 0; i < l + 1; i++)
      points.put (new Integer (0), i);

   Integer null (0);

   for (int i = 0; i < l; i++) {
      HashTable <Integer, Collection <Integer> > *t = tree;
      for (int j = i; j < l; j++) {
         Integer c (work[j]);
         if (t->contains (&c)) {
            t = (HashTable <Integer, Collection <Integer> >*) t->get (&c);
            if (t->contains (&null)) {
               Vector <Integer> *p = (Vector <Integer>*) t->get (&null);

               for (int k = 0; k < p->size (); k++) {
                  Integer *v1 = points.get (i + k);
                  Integer *v2 = p->get (k);
                  // TODO Not very efficient, especially here: too much
                  // calls of "new"
                  points.put(new Integer (lout::misc::max (v1->getValue (),
                                                           v2->getValue ())),
                             i + k);
               }
            }
         } else
            break;
      }
   }  

   // No hyphens in the first two chars or the last two.
   points.put (new Integer (0), 1);
   points.put (new Integer (0), 2);
   points.put (new Integer (0), points.size () - 2);
   points.put (new Integer (0), points.size () - 3);
 
   // Examine the points to build the pieces list.
   Vector <String> *pieces = new Vector <String> (1, true);
   char temp[strlen (word) + 1], *ptemp = temp;

   int n = lout::misc::min ((int)strlen (word), points.size () - 2);
   for (int i = 0; i < n; i++) {
      char c = word[i];
      int p = points.get(i + 2)->getValue ();
      
      *(ptemp++) = c;
      if (p % 2) {
         *ptemp = 0;
         printf ("'%s'\n", temp);
         ptemp = temp;
      }
   }

   *ptemp = 0;
   printf ("'%s'\n", temp);

   return pieces;
}

int main (int argc, char *argv[])
{
   Hyphenator h ("test/hyph-de-1996.pat");
   h.hyphenateWord ("jahrhundertroman");
   puts ("---");
   h.hyphenateWord ("währenddessen");
   puts ("---");
   h.hyphenateWord ("ückerdorf");
   puts ("---");
   h.hyphenateWord ("über");
   puts ("---");
   h.hyphenateWord ("aber");
   puts ("---");

   return 0;
}
