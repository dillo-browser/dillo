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

int main (int argc, char *argv[])
{
   testHashSet ();
   testHashTable ();

   return 0;
}
