#include "../lout/misc.hh"

static void print (lout::misc::NotSoSimpleVector<int> *v)
{
   for (int i = 0; i < v->size(); i++) {
      // Uncomment for debugging, after making the respective members public.
      //if (v->startExtra != -1 && i == v->startExtra + v->numExtra)
      //   printf (" ]");
      if (i > 0)
         printf (", ");
      //if (i == v->startExtra)
      //   printf ("[ ");

      printf ("%d", v->get(i));
   }

   printf (" (%d elements)\n", v->size ());
}

int main (int argc, char *argv[])
{
   lout::misc::NotSoSimpleVector<int> v(1);

   for (int i = 1; i <= 10; i++) {
      v.increase ();
      v.set(v.size () - 1, i);
   }

   print (&v);

   v.insert (2, 4);
   for (int i = 0; i < 5; i++)
      v.set (2 + i, 31 + i);

   print (&v);

   v.insert (8, 4);
   for (int i = 0; i < 5; i++)
      v.set (8 + i, 51 + i);

   print (&v);

   v.insert (10, 4);
   for (int i = 0; i < 5; i++)
      v.set (10 + i, 531 + i);

   print (&v);

   v.insert (1, 4);
   for (int i = 0; i < 5; i++)
      v.set (1 + i, 21 + i);

   print (&v);

   int n = v.size ();
   v.insert (n, 5);
   for (int i = 0; i < 5; i++)
      v.set (n + i, 101 + i);

   print (&v);

   return 0;
}
