/*
 * This small program tests how IdentifiableObject works with multiple
 * inheritance ("diamond" inheritance, more precisely, since all
 * classes have there root in IdentifiableObject.)
 *
 * Current status: With virtual superclasses, you get a class
 * hierarchie "root -> A -> B -> C", so that the first part of this
 * example works actually (C is a subclass of A and of B), but the
 * second fails (it should print "false", but it is erroneously
 * assumed that B is a subclass of A.)
 */

#include "../lout/identity.hh"

using namespace lout::identity;

class A: virtual public IdentifiableObject
{
public:
   static int CLASS_ID;
   inline A () { registerName ("A", &CLASS_ID); }
};

class B: virtual public IdentifiableObject
{
public:
   static int CLASS_ID;
   inline B () { registerName ("B", &CLASS_ID); }
};

class C: public A, public B
{
public:
   static int CLASS_ID;
   inline C () { registerName ("C", &CLASS_ID); }
};

int A::CLASS_ID = -1, B::CLASS_ID = -1, C::CLASS_ID = -1;

int main (int argc, char *argv[])
{
   printf ("A: %d, B: %d, C: %d\n", A::CLASS_ID, B::CLASS_ID, C::CLASS_ID);

   C x;
   assert (x.instanceOf (A::CLASS_ID));
   assert (x.instanceOf (B::CLASS_ID));
   assert (x.instanceOf (C::CLASS_ID));
   printf ("x: %d\n", x.getClassId ());

   B y;
   printf ("y: %d; instance of A: %s\n",
           y.getClassId (), y.instanceOf (B::CLASS_ID) ? "true" : "false");

   return 0;
}
