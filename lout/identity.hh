#ifndef __LOUT_OBJECTX_HH__
#define __LOUT_OBJECTX_HH__

#include "object.hh"
#include "container.hh"
#include "signal.hh"

namespace lout {

/**
 * \brief Some stuff to identify classes of objects at run-time.
 */
namespace identity {

/**
 * \brief Instances of classes, which are sub classes of this class, may
 *    be identified at run-time.
 *
 * <h3>Testing the class</h3>
 *
 * Since e.g. dw::Textblock is a sub class of IdentifiableObject, and
 * implemented in the correct way (as described below), for any given
 * IdentifiableObject the following test can be done:
 *
 * \code
 * identity::IdentifiableObject *o;
 * // ...
 * bool isATextblock = o->instanceOf(dw::Textblock::CLASS_ID);
 * \endcode
 *
 * \em isATextblock is true, when \em o is an instance of dw::Textblock,
 * or of a sub class of dw::Textblock. Otherwise, \em isATextblock is false.
 *
 * It is also possible to get the class identifier of an
 * identity::IdentifiableObject, e.g.
 *
 * \code
 * bool isOnlyATextblock = o->getClassId() == dw::Textblock::CLASS_ID;
 * \endcode
 *
 * would result in true, if o is an instance of dw::Textblock, but not an
 * instance of a sub class of dw::Textblock.
 *
 * <h3>Defining Sub Classes</h3>
 *
 * Each direct or indirect sub class of IdentifiableObject must
 *
 * <ul>
 * <li> add a static int CLASS_ID with -1 as initial value, and
 * <li> call registerName (\em name, &CLASS_ID) in the constructor, where
 *      \em name should be unique, e.g. the fully qualified class name.
 * </ul>
 *
 * After this, <i>class</i>::CLASS_ID refers to a number, which denotes the
 * class. (If this is still -1, since the class has not yet been instantiated,
 * any test will fail, which is correct.)
 *
 * <h3>Notes on implementation</h3>
 *
 * If there are some classes like this:
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10];
 *    edge [arrowhead="none", arrowtail="empty", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080"];
 *    fontname=Helvetica; fontsize=10;
 *    IdentifiableObject [color="#a0a0a0"];
 *    A;
 *    B [color="#a0a0a0"];
 *    C;
 *    IdentifiableObject -> A;
 *    IdentifiableObject -> B;
 *    B -> C;
 * }
 * \enddot
 *
 * <center>[\ref uml-legend "legend"]</center>
 *
 * and first, an instance of A, and then an instance of C is created, there
 * will be the following calls of functions and constructors:
 *
 * <ol>
 * <li> %IdentifiableObject ();
 * <li> %registerName ("A", &A::CLASS_ID);
 * <li> %IdentifiableObject ();
 * <li> %registerName ("B", &B::CLASS_ID);
 * <li> %registerName ("C", &C::CLASS_ID);
 * </ol>
 *
 * From this, the class hierarchy above can easily constructed, and stored
 * in identity::IdentifiableObject::classesByName and
 * in identity::IdentifiableObject::classesById. See the code for details.
 *
 * N.b. Multiple inheritance is not supported, the construction of the
 * tree would become confused.
 */
class IdentifiableObject: public object::Object
{
private:
   class Class: public object::Object
   {
   public:
      Class *parent;
      int id;
      const char *className;

      Class (Class *parent, int id, const char *className);
   };

   static container::typed::HashTable <object::ConstString,
                                       Class> *classesByName;
   static container::typed::Vector <Class> *classesById;
   static Class *currentlyConstructedClass;

   int classId;

protected:
   void registerName (const char *className, int *classId);

public:
   IdentifiableObject ();

   virtual void intoStringBuffer(misc::StringBuffer *sb);

   /**
    * \brief Returns the class identifier.
    *
    * This is rarely used, rather, tests with
    * identity::IdentifiableObject::instanceOf are done.
    */
   int getClassId () { return classId; }

   /**
    * \brief Return the name, under which the class of this object was
    *    registered.
    */
   const char *getClassName() { return classesById->get(classId)->className; }

   bool instanceOf (int otherClassId);
};

} // namespace identity

} // namespace lout

#endif // __LOUT_OBJECTX_HH__
