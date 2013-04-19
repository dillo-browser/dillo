#include "../lout/object.hh"
#include "../lout/container.hh"

using namespace lout::object;
using namespace lout::container::typed;

void testHashSet ()
{
   puts ("--- testHashSet ---");

   HashSet<String> h(true);

   h.put (new String ("one"));
   h.put (new String ("two"));
   h.put (new String ("three"));

   puts (h.toString());
}

void testHashTable ()
{
   puts ("--- testHashTable ---");

   HashTable<String, Integer> h(true, true);

   h.put (new String ("one"), new Integer (1));
   h.put (new String ("two"), new Integer (2));
   h.put (new String ("three"), new Integer (3));

   puts (h.toString());

   h.put (new String ("one"), new Integer (4));
   h.put (new String ("two"), new Integer (5));
   h.put (new String ("three"), new Integer (6));

   puts (h.toString());
}

void testVector1 ()
{
   puts ("--- testVector (1) ---");

   Vector<String> v (true, 1);

   v.put (new String ("one"));
   v.put (new String ("two"));
   v.put (new String ("three"));
   puts (v.toString());

   v.sort ();
   puts (v.toString());
}

void testVector2 ()
{
   puts ("--- testVector (2) ---");

   Vector<String> v (true, 1);

   v.insertSorted (new String ("one"));
   puts (v.toString());

   v.insertSorted (new String ("two"));
   puts (v.toString());

   v.insertSorted (new String ("three"));
   puts (v.toString());

   v.insertSorted (new String ("five"));
   puts (v.toString());

   v.insertSorted (new String ("six"));
   puts (v.toString());

   v.insertSorted (new String ("four"));
   puts (v.toString());

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

int main (int argc, char *argv[])
{
   testHashSet ();
   testHashTable ();
   testVector1 ();
   testVector2 ();

   return 0;
}
