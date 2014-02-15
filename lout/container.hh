#ifndef __LOUT_CONTAINER_HH_
#define __LOUT_CONTAINER_HH_

#include "object.hh"

namespace lout {

/**
 * \brief This namespace contains a framework for container classes, which
 *    members are instances of object::Object.
 *
 * A common problem in languages without garbage collection is, where the
 * children belong to, and so, who is responsible to delete them (instantiation
 * is always done by the caller). This information is here told to the
 * collections, each container has a constructor with the parameter
 * "ownerOfObjects" (HashTable has two such parameters, for keys and values).
 *
 * \sa container::untyped, container::typed
 */
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
   friend class VectorIterator;

private:
   object::Object **array;
   int numAlloc, numElements;
   bool ownerOfObjects;

   class VectorIterator: public AbstractIterator
   {
   private:
      Vector *vector;
      int index;

   public:
      VectorIterator(Vector *vector) { this->vector = vector; index = -1; }
      bool hasNext();
      Object *getNext();
   };

protected:
   AbstractIterator* createIterator();

public:
   Vector(int initSize, bool ownerOfObjects);
   ~Vector();

   void put(object::Object *newElement, int newPos = -1);
   void insert(object::Object *newElement, int pos);

   /**
    * \brief Insert into an already sorted vector.
    *
    * Notice that insertion is not very efficient, unless the position
    * is rather at the end.
    */
   inline void insertSorted(object::Object *newElement)
   { insert (newElement, bsearch (newElement, false)); }

   void remove(int pos);
   inline object::Object *get(int pos) const
   { return (pos >= 0 && pos < numElements) ? array[pos] : NULL; }
   inline int size() { return numElements; }
   void clear();
   void sort();
   int bsearch(Object *key, bool mustExist);
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
   inline int size() const { return numElements; }
   inline bool isEmpty() const { return numElements == 0; }
   inline object::Object *getFirst() const { return first->object; }
   inline object::Object *getLast() const { return last->object; }
};


/**
 * \brief A hash set.
 */
class HashSet: public Collection
{
   friend class HashSetIterator;

protected:
   struct Node
   {
      object::Object *object;
      Node *next;
   };

   Node **table;
   int tableSize;
   bool ownerOfObjects;

   inline int calcHashValue(object::Object *object) const
   {
      return abs(object->hashValue()) % tableSize;
   }

   virtual Node *createNode();
   virtual void clearNode(Node *node);

   Node *findNode(object::Object *object) const;
   Node *insertNode(object::Object *object);

   AbstractIterator* createIterator();

private:
   class HashSetIterator: public Collection0::AbstractIterator
   {
   private:
      HashSet *set;
      HashSet::Node *node;
      int pos;

      void gotoNext();

   public:
      HashSetIterator(HashSet *set);
      bool hasNext();
      Object *getNext();
   };

public:
   HashSet(bool ownerOfObjects, int tableSize = 251);
   ~HashSet();

   void put (object::Object *object);
   bool contains (object::Object *key) const;
   bool remove (object::Object *key);
   //Object *getReference (object::Object *object);
};

/**
 * \brief A hash table.
 */
class HashTable: public HashSet
{
private:
   bool ownerOfValues;

   struct KeyValuePair: public Node
   {
      object::Object *value;
   };

protected:
   Node *createNode();
   void clearNode(Node *node);

public:
   HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize = 251);
   ~HashTable();

   void intoStringBuffer(misc::StringBuffer *sb);

   void put (object::Object *key, object::Object *value);
   object::Object *get (object::Object *key) const;
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
   inline object::Object *getTop () const { return top ? top->object : NULL; }
   void pop ();
   inline int size() const { return numElements; }
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
   Collection () { this->base = NULL; }
   ~Collection () { if (this->base) delete this->base; }

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

   inline void put(T *newElement, int newPos = -1)
   { ((untyped::Vector*)this->base)->put(newElement, newPos); }
   inline void insert(T *newElement, int pos)
   { ((untyped::Vector*)this->base)->insert(newElement, pos); }
   inline void insertSorted(T *newElement)
   { ((untyped::Vector*)this->base)->insertSorted(newElement); }
   inline void remove(int pos) { ((untyped::Vector*)this->base)->remove(pos); }
   inline T *get(int pos) const
   { return (T*)((untyped::Vector*)this->base)->get(pos); }
   inline int size() const { return ((untyped::Vector*)this->base)->size(); }
   inline void clear() { ((untyped::Vector*)this->base)->clear(); }
   inline void sort() { ((untyped::Vector*)this->base)->sort(); }
   inline int bsearch(T *key, bool mustExist)
   { return ((untyped::Vector*)this->base)->bsearch(key, mustExist); }
};


/**
 * \brief Typed version of container::untyped::List.
 */
template <class T> class List: public Collection <T>
{
public:
   inline List(bool ownerOfObjects)
   { this->base = new untyped::List(ownerOfObjects); }

   inline void clear() { ((untyped::List*)this->base)->clear(); }
   inline void append(T *element)
   { ((untyped::List*)this->base)->append(element); }
   inline bool removeRef(T *element) {
      return ((untyped::List*)this->base)->removeRef(element); }
   inline bool remove(T *element) {
      return ((untyped::List*)this->base)->remove(element); }
   inline bool detachRef(T *element) {
      return ((untyped::List*)this->base)->detachRef(element); }

   inline int size() const { return ((untyped::List*)this->base)->size(); }
   inline bool isEmpty() const
   { return ((untyped::List*)this->base)->isEmpty(); }
   inline T *getFirst() const
   { return (T*)((untyped::List*)this->base)->getFirst(); }
   inline T *getLast() const
   { return (T*)((untyped::List*)this->base)->getLast(); }
};

/**
 * \brief Typed version of container::untyped::HashSet.
 */
template <class T> class HashSet: public Collection <T>
{
protected:
   inline HashSet() { }

public:
   inline HashSet(bool owner, int tableSize = 251)
   { this->base = new untyped::HashSet(owner, tableSize); }

   inline void put(T *object)
   { return ((untyped::HashSet*)this->base)->put(object); }
   inline bool contains(T *object) const
   { return ((untyped::HashSet*)this->base)->contains(object); }
   inline bool remove(T *object)
   { return ((untyped::HashSet*)this->base)->remove(object); }
   //inline T *getReference(T *object)
   //{ return (T*)((untyped::HashSet*)this->base)->getReference(object); }
};

/**
 * \brief Typed version of container::untyped::HashTable.
 */
template <class K, class V> class HashTable: public HashSet <K>
{
public:
   inline HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize = 251)
   { this->base = new untyped::HashTable(ownerOfKeys, ownerOfValues,
                                         tableSize); }

   inline void put(K *key, V *value)
   { return ((untyped::HashTable*)this->base)->put(key, value); }
   inline V *get(K *key) const
   { return (V*)((untyped::HashTable*)this->base)->get(key); }
};

/**
 * \brief Typed version of container::untyped::Stack.
 */
template <class T> class Stack: public Collection <T>
{
public:
   inline Stack (bool ownerOfObjects)
   { this->base = new untyped::Stack (ownerOfObjects); }

   inline void push (T *object) {
      ((untyped::Stack*)this->base)->push (object); }
   inline void pushUnder (T *object)
   { ((untyped::Stack*)this->base)->pushUnder (object); }
   inline T *getTop () const
   { return (T*)((untyped::Stack*)this->base)->getTop (); }
   inline void pop () { ((untyped::Stack*)this->base)->pop (); }
   inline int size() const { return ((untyped::Stack*)this->base)->size(); }
};

} // namespace untyped

} // namespace container

} // namespace lout

#endif // __LOUT_CONTAINER_HH_
