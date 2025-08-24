/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
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

#include "lout/object.hh"
#include "lout/container.hh"

using namespace lout::object;
using namespace lout::container::typed;

class ReverseComparator: public Comparator
{
private:
   Comparator *reversed;

public:
   ReverseComparator (Comparator *reversed) { this->reversed = reversed; }
   int compare(Object *o1, Object *o2) { return - reversed->compare (o1, o2); }
};

void testHashSet ()
{
   puts ("--- testHashSet ---");

   HashSet<String> h(true);

   h.put (new String ("one"));
   h.put (new String ("two"));
   h.put (new String ("three"));

   char *p;
   puts (p = h.toString());
   dFree(p);
}

void testHashTable ()
{
   puts ("--- testHashTable ---");

   HashTable<String, Integer> h(true, true);

   h.put (new String ("one"), new Integer (1));
   h.put (new String ("two"), new Integer (2));
   h.put (new String ("three"), new Integer (3));

   char *p;
   puts (p = h.toString());
   dFree(p);

   h.put (new String ("one"), new Integer (4));
   h.put (new String ("two"), new Integer (5));
   h.put (new String ("three"), new Integer (6));

   puts (p = h.toString());
   dFree(p);
}

void testVector1 ()
{
   ReverseComparator reverse (&standardComparator);

   puts ("--- testVector (1) ---");

   Vector<String> v (true, 1);

   char *p;

   v.put (new String ("one"));
   v.put (new String ("two"));
   v.put (new String ("three"));
   puts (p = v.toString());
   dFree(p);

   v.sort (&reverse);
   puts (p = v.toString());
   dFree(p);

   v.sort ();
   puts (p = v.toString());
   dFree(p);
}

void testVector2 ()
{
   puts ("--- testVector (2) ---");

   Vector<String> v (1, true);
   char *p;

   v.insertSorted (new String ("one"));
   puts (p = v.toString());
   dFree(p);

   v.insertSorted (new String ("two"));
   puts (p = v.toString());
   dFree(p);

   v.insertSorted (new String ("three"));
   puts (p = v.toString());
   dFree(p);

   v.insertSorted (new String ("five"));
   puts (p = v.toString());
   dFree(p);

   v.insertSorted (new String ("six"));
   puts (p = v.toString());
   dFree(p);

   v.insertSorted (new String ("four"));
   puts (p = v.toString());
   dFree(p);

   for (int b = 0; b < 2; b++) {
      bool mustExist = b;
      printf ("mustExist = %s\n", mustExist ? "true" : "false");

      String k ("alpha");
      printf ("   '%s' -> %d\n", k.chars(), v.bsearch (&k, mustExist));

      for (Iterator<String> it = v.iterator(); it.hasNext(); ) {
         String *k1 = it.getNext();
         printf ("   '%s' -> %d\n", k1->chars(), v.bsearch (k1, mustExist));

         char buf[64];
         strcpy (buf, k1->chars());
         strcat (buf, "-var");
         String k2 (buf);
         printf ("   '%s' -> %d\n", k2.chars(), v.bsearch (&k2, mustExist));
      }
   }
}

void testVector3 ()
{
   // Regression test: resulted once incorrectly (0, 2, 3), should
   // result in (1, 2, 3).

   puts ("--- testVector (3) ---");

   Vector<String> v (true, 1);
   String k ("omega");

   v.put (new String ("alpha"));
   printf ("   -> %d\n", v.bsearch (&k, false));
   v.put (new String ("beta"));
   printf ("   -> %d\n", v.bsearch (&k, false));
   v.put (new String ("gamma"));
   printf ("   -> %d\n", v.bsearch (&k, false));
}

void testStackAsQueue ()
{
   puts ("--- testStackAsQueue ---");

   Stack<Integer> s (true);

   for (int i = 1; i <= 10; i++)
      s.pushUnder (new Integer (i));

   while (s.size () > 0) {
      Integer *i = s.getTop ();
      printf ("%d\n", i->getValue ());
      s.pop ();
   }
}

int main (int argc, char *argv[])
{
   testHashSet ();
   testHashTable ();
   testVector1 ();
   testVector2 ();
   testVector3 ();
   testStackAsQueue ();

   return 0;
}
