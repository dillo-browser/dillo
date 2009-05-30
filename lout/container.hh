#ifndef __LOUT_CONTAINER_HH_
#define __LOUT_CONTAINER_HH_

#include "object.hh"

/**
 * \brief This namespace contains a framework for container classes, which
 *    members are instances of object::Object.
 *
 * A common problem in languanges without garbage collection is, where the
 * children belong to, and so, who is responsible to delete them (instantiation
 * is always done by the caller). This information is here told to the
 * collections, each container has a constructor with the parameter
 * "ownerOfObjects" (HashTable has two such parameters, for keys and values).
 *
 * \sa container::untyped, container::typed
 */
namespace lout {

namespace container {

/**
 * \brief The container classes defined here contain instances of
 *    object::Object.
 *
 * Different sub-classes may be mixed, and you have to care about casting,
 * there is no type-safety.
 */
namespace untyped {

/**
 * \brief ...
 */
class Collection0: public object::Object
{
   friend class Iterator;

protected:
   /**
    * \brief The base class for all iterators, as created by
    *   container::untyped::Collection::createIterator.
    */
   class AbstractIterator: public object::Object
   {
   private:
      int refcount;

   public:
      AbstractIterator() { refcount = 1; }

      void ref () { refcount++; }
      void unref () { refcount--; if (refcount == 0) delete this; }

      virtual bool hasNext () = 0;
      virtual Object *getNext () = 0;
   };

protected:
   virtual AbstractIterator* createIterator() = 0;
};

/**
 * \brief This is a small wrapper for AbstractIterator, which may be used
 *    directly, not as a pointer, to makes memory management simpler.
 */
class Iterator
{
   friend class Collection;

private:
   Collection0::AbstractIterator *impl;

   // Should not instantiated directly.
   inline Iterator(Collection0::AbstractIterator *impl) { this->impl = impl; }

public:
   Iterator();
   Iterator(const Iterator &it2);
   Iterator(Iterator &it2);
   ~Iterator();
   Iterator &operator=(const Iterator &it2);
   Iterator &operator=(Iterator &it2);

   inline bool hasNext() { return impl->hasNext(); }
   inline object::Object *getNext() { return impl->getNext(); }
};

/**
 * \brief Abstract base class for all container objects in container::untyped.
 */
class Collection: public Collection0
{
public:
   void intoStringBuffer(misc::StringBuffer *sb);
   inline Iterator iterator() { Iterator it(createIterator()); return it; }
};


/**
 * \brief Container, which is implemented by an array, which is
 *    dynamically resized.
 */
class Vector: public Collection
{
private:
   object::Object **array;
   int numAlloc, numElements;
   bool ownerOfObjects;

protected:
   AbstractIterator* createIterator();

public:
   Vector(int initSize, bool ownerOfObjects);
   ~Vector();

   void put(object::Object *newElement, int newPos = -1);
   void insert(object::Object *newElement, int pos);
   void remove(int pos);
   inline object::Object *get(int pos)
   { return (pos >= 0 && pos < numElements) ? array[pos] : NULL; }
   inline int size() { return numElements; }
   void clear();
   void sort();
};


/**
 * \brief A single-linked list.
 */
class List: public Collection
{
   friend class ListIterator;

private:
   struct Node
   {
   public:
      object::Object *object;
      Node *next;
   };

   class ListIterator: public AbstractIterator
   {
   private:
      List::Node *current;
   public:
      ListIterator(List::Node *node) { current = node; }
      bool hasNext();
      Object *getNext();
   };

   Node *first, *last;
   bool ownerOfObjects;
   int numElements;

   bool remove0(object::Object *element, bool compare, bool doNotDeleteAtAll);

protected:
   AbstractIterator* createIterator();

public:
   List(bool ownerOfObjects);
   ~List();

   void clear();
   void append(object::Object *element);
   inline bool removeRef(object::Object *element)
   { return remove0(element, false, false); }
   inline bool remove(object::Object *element)
   { return remove0(element, true, false); }
   inline bool detachRef(object::Object *element)
   { return remove0(element, false, true); }
   inline int size() { return numElements; }
   inline bool isEmpty() { return numElements == 0; }
   inline object::Object *getFirst() { return first->object; }
   inline object::Object *getLast() { return last->object; }
};


/**
 * \brief A hash table.
 */
class HashTable: public Collection
{
   friend class HashTableIterator;

private:
   struct Node
   {
      object::Object *key, *value;
      Node *next;
   };

   class HashTableIterator: public Collection0::AbstractIterator
   {
   private:
      HashTable *table;
      HashTable::Node *node;
      int pos;

      void gotoNext();

   public:
      HashTableIterator(HashTable *table);
      bool hasNext();
      Object *getNext();
   };

   Node **table;
   int tableSize;
   bool ownerOfKeys, ownerOfValues;

private:
   inline int calcHashValue(object::Object *key)
   {
      return abs(key->hashValue()) % tableSize;
   }

protected:
   AbstractIterator* createIterator();

public:
   HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize = 251);
   ~HashTable();

   void intoStringBuffer(misc::StringBuffer *sb);

   void put (object::Object *key, object::Object *value);
   bool contains (object::Object *key);
   Object *get (object::Object *key);
   bool remove (object::Object *key);
   Object *getKey (Object *key);
};

/**
 * \brief A stack (LIFO).
 *
 * Note that the iterator returns all elements in the reversed order they have
 * been put on the stack.
 */
class Stack: public Collection
{
   friend class StackIterator;

private:
   class Node
   {
   public:
      object::Object *object;
      Node *prev;
   };

   class StackIterator: public AbstractIterator
   {
   private:
      Stack::Node *current;
   public:
      StackIterator(Stack::Node *node) { current = node; }
      bool hasNext();
      Object *getNext();
   };

   Node *bottom, *top;
   bool ownerOfObjects;
   int numElements;

protected:
   AbstractIterator* createIterator();

public:
   Stack (bool ownerOfObjects);
   ~Stack();

   void push (object::Object *object);
   void pushUnder (object::Object *object);
   inline object::Object *getTop () { return top ? top->object : NULL; }
   void pop ();
   inline int size() { return numElements; }
};

} // namespace untyped

/**
 * \brief This namespace provides thin wrappers, implemented as C++ templates,
 *    to gain type-safety.
 *
 * Notice, that nevertheless, all objects still have to be instances of
 * object::Object.
 */
namespace typed {

template <class T> class Collection;

/**
 * \brief Typed version of container::untyped::Iterator.
 */
template <class T> class Iterator
{
   friend class Collection<T>;

private:
   untyped::Iterator base;

public:
   inline Iterator() { }
   inline Iterator(const Iterator<T> &it2) { this->base = it2.base; }
   inline Iterator(Iterator<T> &it2) { this->base = it2.base; }
   ~Iterator() { }
   inline Iterator &operator=(const Iterator<T> &it2)
   { this->base = it2.base; return *this; }
   inline Iterator &operator=(Iterator<T> &it2)
   { this->base = it2.base; return *this; }

   inline bool hasNext() { return this->base.hasNext(); }
   inline T *getNext() { return (T*)this->base.getNext(); }
};

/**
 * \brief Abstract base class template for all container objects in
 *    container::typed.
 *
 * Actually, a wrapper for container::untyped::Collection.
 */
template <class T> class Collection: public object::Object
{
protected:
   untyped::Collection *base;

public:
   void intoStringBuffer(misc::StringBuffer *sb)
   { this->base->intoStringBuffer(sb); }

   inline Iterator<T> iterator() {
      Iterator<T> it; it.base = this->base->iterator(); return it; }
};


/**
 * \brief Typed version of container::untyped::Vector.
 */
template <class T> class Vector: public Collection <T>
{
public:
   inline Vector(int initSize, bool ownerOfObjects) {
      this->base = new untyped::Vector(initSize, ownerOfObjects); }
   ~Vector() { delete this->base; }

   inline void put(T *newElement, int newPos = -1)
   { ((untyped::Vector*)this->base)->put(newElement, newPos); }
   inline void insert(T *newElement, int pos)
   { ((untyped::Vector*)this->base)->insert(newElement, pos); }
   inline void remove(int pos) { ((untyped::Vector*)this->base)->remove(pos); }
   inline T *get(int pos)
   { return (T*)((untyped::Vector*)this->base)->get(pos); }
   inline int size() { return ((untyped::Vector*)this->base)->size(); }
   inline void clear() { ((untyped::Vector*)this->base)->clear(); }
   inline void sort() { ((untyped::Vector*)this->base)->sort(); }
};


/**
 * \brief Typed version of container::untyped::List.
 */
template <class T> class List: public Collection <T>
{
public:
   inline List(bool ownerOfObjects)
   { this->base = new untyped::List(ownerOfObjects); }
   ~List() { delete this->base; }

   inline void clear() { ((untyped::List*)this->base)->clear(); }
   inline void append(T *element)
   { ((untyped::List*)this->base)->append(element); }
   inline bool removeRef(T *element) {
      return ((untyped::List*)this->base)->removeRef(element); }
   inline bool remove(T *element) {
      return ((untyped::List*)this->base)->remove(element); }
   inline bool detachRef(T *element) {
      return ((untyped::List*)this->base)->detachRef(element); }

   inline int size() { return ((untyped::List*)this->base)->size(); }
   inline bool isEmpty()
   { return ((untyped::List*)this->base)->isEmpty(); }
   inline T *getFirst()
   { return (T*)((untyped::List*)this->base)->getFirst(); }
   inline T *getLast()
   { return (T*)((untyped::List*)this->base)->getLast(); }
};


/**
 * \brief Typed version of container::untyped::HashTable.
 */
template <class K, class V> class HashTable: public Collection <K>
{
public:
   inline HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize = 251)
   { this->base = new untyped::HashTable(ownerOfKeys, ownerOfValues,
                                         tableSize); }
   ~HashTable() { delete this->base; }

   inline void put(K *key, V *value)
   { return ((untyped::HashTable*)this->base)->put(key, value); }
   inline bool contains(K *key)
   { return ((untyped::HashTable*)this->base)->contains(key); }
   inline V *get(K *key)
   { return (V*)((untyped::HashTable*)this->base)->get(key); }
   inline bool remove(K *key)
   { return ((untyped::HashTable*)this->base)->remove(key); }
   inline K *getKey(K *key)
   { return (K*)((untyped::HashTable*)this->base)->getKey(key); }
};

/**
 * \brief Typed version of container::untyped::Stack.
 */
template <class T> class Stack: public Collection <T>
{
public:
   inline Stack (bool ownerOfObjects)
   { this->base = new untyped::Stack (ownerOfObjects); }
   ~Stack() { delete this->base; }

   inline void push (T *object) {
      ((untyped::Stack*)this->base)->push (object); }
   inline void pushUnder (T *object)
   { ((untyped::Stack*)this->base)->pushUnder (object); }
   inline T *getTop ()
   { return (T*)((untyped::Stack*)this->base)->getTop (); }
   inline void pop () { ((untyped::Stack*)this->base)->pop (); }
   inline int size() {  return ((untyped::Stack*)this->base)->size(); }
};

} // namespace untyped

} // namespace container

} // namespace lout

#endif // __LOUT_CONTAINER_HH_
