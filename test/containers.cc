#include "../lout/object.hh"
#include "../lout/container.hh"

using namespace lout::object;
using namespace lout::container::typed;

void testHashSet ()
{
   HashSet<String> h(true);

   h.put (new String ("one"));
   h.put (new String ("two"));
   h.put (new String ("three"));
   
   Iterator<String> it = h.iterator ();
   while (it.hasNext ()) {
      String *o = it.getNext ();
      printf ("%s\n", o->chars());
   }
}

void testHashTable ()
{
   HashTable<String, Integer> h(true, true);

   h.put (new String ("one"), new Integer (1));
   h.put (new String ("two"), new Integer (2));
   h.put (new String ("three"), new Integer (3));
   
   for (Iterator<String> it = h.iterator (); it.hasNext (); ) {
      String *k = it.getNext ();
      Integer *v = h.get (k);
      printf ("%s -> %d\n", k->chars(), v->getValue());
   }

   h.put (new String ("one"), new Integer (4));
   h.put (new String ("two"), new Integer (5));
   h.put (new String ("three"), new Integer (6));
   
   for (Iterator<String> it = h.iterator (); it.hasNext (); ) {
      String *k = it.getNext ();
      Integer *v = h.get (k);
      printf ("%s -> %d\n", k->chars(), v->getValue());
   }
}

int main (int argc, char *argv[])
{
   testHashSet ();
   testHashTable ();

   return 0;
}
