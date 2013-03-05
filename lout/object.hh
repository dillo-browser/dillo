#ifndef __LOUT_OBJECT_HH__
#define __LOUT_OBJECT_HH__

#include <stdlib.h>
#include <string.h>

#include "misc.hh"

namespace lout {

/**
 * \brief Here, some common classes (or interfaces) are defined, to standardize
 *    the access to other classes.
 */
namespace object {

/**
 * \brief This is the base class for many other classes, which defines very
 *    common virtual methods.
 *
 * For convenience, none of them are abstract, but they
 * must be defined, when they are needed, especially for containers.
 */
class Object
{
public:
   virtual ~Object();
   virtual bool equals(Object *other);
   virtual int hashValue();
   virtual Object *clone();
   virtual void intoStringBuffer(misc::StringBuffer *sb);
   const char *toString();
   virtual size_t sizeOf();
};

/**
 * \brief Instances of a sub class of may be compared (less, greater).
 *
 * Used for sorting etc.
 */
class Comparable: public Object
{
public:
   /**
    * \brief Compare two objects c1 and c2.
    *
    * Return a value < 0, when c1 is less than c2, a value > 0, when c1
    * is greater than c2, or 0, when c1 and c2 are equal.
    *
    * If c1.equals(c2) (as defined in Object), c1.compareTo(c2) must
    * be 0, but, unlike you may expect, the reversed is not
    * necessarily true. This method returns 0, if, according to the
    * rules for sorting, there is no difference, but there may still
    * be differences (not relevant for sorting), which "equals" will
    * care about.
    */
   virtual int compareTo(Comparable *other) = 0;

   static int compareFun(const void *p1, const void *p2);
};

/**
 * \brief An object::Object wrapper for void pointers.
 */
class Pointer: public Object
{
private:
   void *value;

public:
   Pointer(void *value) { this->value = value; }
   bool equals(Object *other);
   int hashValue();
   void intoStringBuffer(misc::StringBuffer *sb);
   inline void *getValue() { return value; }
};

/**
 * \brief A typed version of object::Pointer.
 */
template <class T> class TypedPointer: public Pointer
{
public:
   inline TypedPointer(T *value) : Pointer ((void*)value) { }
   inline T *getTypedValue() { return (T*)getValue(); }
};


/**
 * \brief An object::Object wrapper for int's.
 */
class Integer: public Comparable
{
   int value;

public:
   Integer(int value) { this->value = value; }
   bool equals(Object *other);
   int hashValue();
   void intoStringBuffer(misc::StringBuffer *sb);
   int compareTo(Comparable *other);
   inline int getValue() { return value; }
};


/**
 * \brief An object::Object wrapper for constant strings (char*).
 *
 * As opposed to object::String, the char array is not copied.
 */
class ConstString: public Comparable
{
protected:
   const char *str;

public:
   ConstString(const char *str) { this->str = str; }
   bool equals(Object *other);
   int hashValue();
   int compareTo(Comparable *other);
   void intoStringBuffer(misc::StringBuffer *sb);

   inline const char *chars() { return str; }

   static int hashValue(const char *str);
};


/**
 * \brief An object::Object wrapper for strings (char*).
 *
 * As opposed to object::ConstantString, the char array is copied.
 */
class String: public ConstString
{
public:
   String(const char *str);
   ~String();
};

/**
 * \todo Comment
 */
class PairBase: public Object
{
protected:
   Object *first, *second;

public:
   PairBase(Object *first, Object *second);
   ~PairBase();

   bool equals(Object *other);
   int hashValue();
   void intoStringBuffer(misc::StringBuffer *sb);
   size_t sizeOf();
};

/**
 * \todo Comment
 */
class Pair: public PairBase
{
public:
   Pair(Object *first, Object *second): PairBase (first, second) { }

   inline Object *getFirst () { return first; }
   inline Object *getSecond () { return second; }
};

/**
 * \todo Comment
 */
template <class F, class S> class TypedPair: public PairBase
{
public:
   TypedPair(F *first, S *second): PairBase (first, second) { }

   inline F *getFirst () { return first; }
   inline S *getSecond () { return second; }
};

} // namespace object

} // namespace lout

#endif // __LOUT_OBJECT_HH__
