/*
 * This small program tests how IdentifiableObject works with multiple
 * inheritance ("diamond" inheritance, more precisely, since all
 * classes have there root in IdentifiableObject.) With virtual
 * superclasses, this example works actually (for comprehensible
 * reasons), but I'm not sure about more compex class hierarchies.
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
   C x;
   assert (x.instanceOf (A::CLASS_ID));
   assert (x.instanceOf (B::CLASS_ID));
   assert (x.instanceOf (C::CLASS_ID));
   printf ("A: %d, B: %d, C: %d, x: %d\n",
           A::CLASS_ID, B::CLASS_ID, C::CLASS_ID, x.getClassId ());
   return 0;
}
