/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "container.hh"
#include "misc.hh"

namespace lout {

using namespace object;

namespace container {

namespace untyped {

// -------------
//    Iterator
// -------------

Iterator::Iterator()
{
   impl = NULL;
}

Iterator::Iterator(const Iterator &it2)
{
   impl = it2.impl;
   if (impl)
      impl->ref();
}

Iterator::Iterator(Iterator &it2)
{
   impl = it2.impl;
   if (impl)
      impl->ref();
}

Iterator &Iterator::operator=(const Iterator &it2)
{
   if (impl)
      impl->unref();
   impl = it2.impl;
   if (impl)
      impl->ref();
   return *this;
}

Iterator &Iterator::operator=(Iterator &it2)
{
   if (impl)
      impl->unref();
   impl = it2.impl;
   if (impl)
      impl->ref();
   return *this;
}

Iterator::~Iterator()
{
   if (impl)
      impl->unref();
}

// ----------------
//    Collection
// ----------------

void Collection::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append("{ ");
   bool first = true;
   for (Iterator it = iterator(); it.hasNext(); ) {
      if (!first)
         sb->append(", ");
      it.getNext()->intoStringBuffer(sb);
      first = false;
   }
   sb->append(" }");
}

// ------------
//    Vector
// ------------

Vector::Vector(int initSize, bool ownerOfObjects)
{
   numAlloc = initSize == 0 ? 1 : initSize;
   this->ownerOfObjects = ownerOfObjects;
   numElements = 0;
   array = (Object**)malloc(numAlloc * sizeof(Object*));
}

Vector::~Vector()
{
   clear();
   free(array);
}

void Vector::put(Object *newElement, int newPos)
{
   if (newPos == -1)
      newPos = numElements;

   // Old entry is overwritten.
   if (newPos < numElements) {
      if (ownerOfObjects && array[newPos])
         delete array[newPos];
   }

   // Allocated memory has to be increased.
   if (newPos >= numAlloc) {
      while (newPos >= numAlloc)
         numAlloc *= 2;
      array = (Object**)realloc(array, numAlloc * sizeof(Object*));
   }

   // Insert NULL's into possible gap before new position.
   for (int i = numElements; i < newPos; i++)
      array[i] = NULL;

   if (newPos >= numElements)
      numElements = newPos + 1;

   array[newPos] = newElement;
}

void Vector::clear()
{
   if (ownerOfObjects) {
      for (int i = 0; i < numElements; i++)
         if (array[i])
            delete array[i];
   }

   numElements = 0;
}

void Vector::insert(Object *newElement, int pos)
{
   if (pos >= numElements)
      put(newElement, pos);
   else {
      numElements++;

      // Allocated memory has to be increased.
      if (numElements >= numAlloc) {
         numAlloc *= 2;
         array = (Object**)realloc(array, numAlloc * sizeof(Object*));
      }

      for (int i = numElements - 1; i > pos; i--)
         array[i] = array[i - 1];

      array[pos] = newElement;
   }
}

void Vector::remove(int pos)
{
   if (ownerOfObjects && array[pos])
      delete array[pos];

   for (int i = pos + 1; i < numElements; i++)
      array[i - 1] = array[i];

   numElements--;
}

/**
 * Sort the elements in the vector. Assumes that all elements are Comparable's.
 */
void Vector::sort()
{
   qsort(array, numElements, sizeof(Object*), misc::Comparable::compareFun);
}


/**
 * \bug Not implemented.
 */
Collection0::AbstractIterator* Vector::createIterator()
{
   return NULL;
}

// ------------
//    List
// ------------

List::List(bool ownerOfObjects)
{
   this->ownerOfObjects = ownerOfObjects;
   first = last = NULL;
   numElements = 0;
}

List::~List()
{
   clear();
}

void List::clear()
{
   while (first) {
      if (ownerOfObjects && first->object)
         delete first->object;
      Node *next = first->next;
      delete first;
      first = next;
   }

   last = NULL;
   numElements = 0;
}

void List::append(Object *element)
{
   Node *newLast = new Node;
   newLast->next = NULL;
   newLast->object = element;

   if (last) {
      last->next = newLast;
      last = newLast;
   } else
      first = last = newLast;

   numElements++;
}


bool List::remove0(Object *element, bool compare, bool doNotDeleteAtAll)
{
   Node *beforeCur, *cur;

   for (beforeCur = NULL, cur = first; cur; beforeCur = cur, cur = cur->next) {
      if (compare ?
          (cur->object && element->equals(cur->object)) :
          element == cur->object) {
         if (beforeCur) {
            beforeCur->next = cur->next;
            if (cur->next == NULL)
               last = beforeCur;
         } else {
            first = cur->next;
            if (first == NULL)
               last = NULL;
         }

         if (ownerOfObjects && cur->object && !doNotDeleteAtAll)
            delete cur->object;
         delete cur;

         numElements--;
         return true;
      }
   }

   return false;
}

Object *List::ListIterator::getNext()
{
   Object *object;

   if (current) {
      object = current->object;
      current = current->next;
   } else
      object = NULL;

   return object;
}

bool List::ListIterator::hasNext()
{
   return current != NULL;
}

Collection0::AbstractIterator* List::createIterator()
{
   return new ListIterator(first);
}


// ---------------
//    HashTable
// ---------------

HashTable::HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize)
{
   this->ownerOfKeys = ownerOfKeys;
   this->ownerOfValues = ownerOfValues;
   this->tableSize = tableSize;

   table = new Node*[tableSize];
   for (int i = 0; i < tableSize; i++)
      table[i] = NULL;
}

HashTable::~HashTable()
{
   for (int i = 0; i < tableSize; i++) {
      Node *n1 = table[i];
      while (n1) {
         Node *n2 = n1->next;

         if (ownerOfValues && n1->value)
            delete n1->value;
         if (ownerOfKeys)
            delete n1->key;
         delete n1;

         n1 = n2;
      }
   }

   delete[] table;
}

void HashTable::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append("{ ");

   bool first = true;
   for (int i = 0; i < tableSize; i++) {
      for (Node *node = table[i]; node; node = node->next) {
         if (!first)
            sb->append(", ");
         node->key->intoStringBuffer(sb);
         sb->append(" => ");
         node->value->intoStringBuffer(sb);
         first = false;
      }
   }

   sb->append(" }");
}

void HashTable::put(Object *key, Object *value)
{
   int h = calcHashValue(key);
   Node *n = new Node;
   n->key = key;
   n->value = value;
   n->next = table[h];
   table[h] = n;
}

bool HashTable::contains(Object *key)
{
   int h = calcHashValue(key);
   for (Node *n = table[h]; n; n = n->next) {
      if (key->equals(n->key))
         return true;
   }

   return false;
}

Object *HashTable::get(Object *key)
{
   int h = calcHashValue(key);
   for (Node *n = table[h]; n; n = n->next) {
      if (key->equals(n->key))
         return n->value;
   }

   return NULL;
}

bool HashTable::remove(Object *key)
{
   int h = calcHashValue(key);
   Node *last, *cur;

   for (last = NULL, cur = table[h]; cur; last = cur, cur = cur->next) {
      if (key->equals(cur->key)) {
         if (last)
            last->next = cur->next;
         else
            table[h] = cur->next;

         if (ownerOfValues && cur->value)
            delete cur->value;
         if (ownerOfKeys)
            delete cur->key;
         delete cur;

         return true;
      }
   }

   return false;
}

Object *HashTable::getKey (Object *key)
{
   int h = calcHashValue(key);
   for (Node *n = table[h]; n; n = n->next) {
      if (key->equals(n->key))
         return n->key;
   }

   return NULL;
}

HashTable::HashTableIterator::HashTableIterator(HashTable *table)
{
   this->table = table;
   node = NULL;
   pos = -1;
   gotoNext();
}

void HashTable::HashTableIterator::gotoNext()
{
   if (node)
      node = node->next;

   while (node == NULL) {
      if (pos >= table->tableSize - 1)
         return;
      pos++;
      node = table->table[pos];
   }
}


Object *HashTable::HashTableIterator::getNext()
{
   Object *result;
   if (node)
      result = node->key;
   else
      result = NULL;

   gotoNext();
   return result;
}

bool HashTable::HashTableIterator::hasNext()
{
   return node != NULL;
}

Collection0::AbstractIterator* HashTable::createIterator()
{
   return new HashTableIterator(this);
}

// -----------
//    Stack
// -----------

Stack::Stack (bool ownerOfObjects)
{
   this->ownerOfObjects = ownerOfObjects;
   bottom = top = NULL;
   numElements = 0;
}

Stack::~Stack()
{
   while (top)
      pop ();
}

void Stack::push (object::Object *object)
{
   Node *newTop = new Node ();
   newTop->object = object;
   newTop->prev = top;

   top = newTop;
   if (bottom == NULL)
      bottom = top;

   numElements++;
}

void Stack::pushUnder (object::Object *object)
{
   Node *newBottom = new Node ();
   newBottom->object = object;
   newBottom->prev = NULL;
   if (bottom != NULL)
      bottom->prev = newBottom;

   bottom = newBottom;
   if (top == NULL)
      top = bottom;

   numElements++;
}

void Stack::pop ()
{
   Node *newTop = top->prev;

   if (ownerOfObjects)
      delete top->object;
   delete top;

   top = newTop;
   if (top == NULL)
      bottom = NULL;

   numElements--;
}

Object *Stack::StackIterator::getNext()
{
   Object *object;

   if (current) {
      object = current->object;
      current = current->prev;
   } else
      object = NULL;

   return object;
}

bool Stack::StackIterator::hasNext()
{
   return current != NULL;
}

Collection0::AbstractIterator* Stack::createIterator()
{
   return new StackIterator(top);
}

} // namespace untyped

} // namespace container

} // namespace lout
