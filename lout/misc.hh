#ifndef __LOUT_MISC_HH__
#define __LOUT_MISC_HH__

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

namespace lout {

/**
 * \brief Miscellaneous stuff, which does not fit anywhere else.
 *
 * Actually, the other parts, beginning with ::object, depend on this.
 */
namespace misc {

template <class T> inline T min (T a, T b) { return a < b ? a : b; }
template <class T> inline T max (T a, T b) { return a > b ? a : b; }

template <class T> inline T min (T a, T b, T c)
{
   return (min (a, min (b, c)));
}
template <class T> inline T max (T a, T b, T c)
{
   return (max (a, max (b, c)));
}

extern const char *prgName;

void init (int argc, char *argv[]);

inline void assertNotReached ()
{
   fprintf (stderr, "*** [%s] This should not happen! ***\n", prgName);
   abort ();
}

inline int roundInt(double d)
{
   return (int) ((d > 0) ? (d + 0.5) : (d - 0.5));
}

inline int AsciiTolower(char c)
{
   return ((c >= 'A' && c <= 'Z') ? c + 0x20 : c);
}

inline int AsciiToupper(char c)
{
   return ((c >= 'a' && c <= 'z') ? c - 0x20 : c);
}

inline int AsciiStrcasecmp(const char *s1, const char *s2)
{
   int ret = 0;

   while ((*s1 || *s2) && !(ret = AsciiTolower(*s1) - AsciiTolower(*s2))) {
      s1++;
      s2++;
   }
   return ret;
}

/**
 * \brief Simple (simpler than container::untyped::Vector and
 *    container::typed::Vector) template based vector.
 */
template <class T> class SimpleVector
{
private:
   T *array;
   int num, numAlloc;

   inline void resize ()
   {
      /* This algorithm was tuned for memory&speed with this huge page:
       *   http://downloads.mysql.com/docs/refman-6.0-en.html.tar.gz
       */
      if (array == NULL) {
         this->numAlloc = 1;
         this->array = (T*) malloc (sizeof (T));
      }
      if (this->numAlloc < this->num) {
         this->numAlloc = (this->num < 100) ?
                          this->num : this->num + this->num/10;
         this->array =
            (T*) realloc(this->array, (this->numAlloc * sizeof (T)));
      }
   }

public:
   inline SimpleVector (int initAlloc = 1)
   {
      this->num = 0;
      this->numAlloc = initAlloc;
      this->array = NULL;
   }

   inline SimpleVector (const SimpleVector &o) {
      this->array = NULL;
      this->num = o.num;
      this->numAlloc = o.numAlloc;
      resize ();
      memcpy (this->array, o.array, sizeof (T) * num);
   }

   inline ~SimpleVector ()
   {
      if (this->array)
         free (this->array);
   }

   /**
    * \brief Return the number of elements put into this vector.
    */
   inline int size() const { return this->num; }

   inline T* getArray() const { return array; }

   inline T* detachArray() {
      T* arr = array;
      array = NULL;
      numAlloc = 0;
      num = 0;
      return arr;
   }

   /**
    * \brief Increase the vector size by one.
    *
    * May be necessary before calling misc::SimpleVector::set.
    */
   inline void increase() { setSize(this->num + 1); }

   /**
    * \brief Set the size explicitly.
    *
    * May be necessary before calling misc::SimpleVector::set.
    */
   inline void setSize(int newSize) {
      assert (newSize >= 0);
      this->num = newSize;
      this->resize ();
   }

   /**
    * \brief Set the size explicitly and initialize new values.
    *
    * May be necessary before calling misc::SimpleVector::set.
    */
   inline void setSize (int newSize, T t) {
      int oldSize = this->num;
      setSize (newSize);
      for (int i = oldSize; i < newSize; i++)
         set (i, t);
   }

   /**
    * \brief Return the reference of one element.
    *
    * \sa misc::SimpleVector::get
    */
   inline T* getRef (int i) const {
      assert (i >= 0 && this->num - i > 0);
      return array + i;
   }

   /**
    * \brief Return the one element, explicitly.
    *
    * The element is copied, so for complex elements, you should rather used
    * misc::SimpleVector::getRef.
    */
   inline T get (int i) const {
      assert (i >= 0 && this->num - i > 0);
      return this->array[i];
   }

   /**
    * \brief Return the reference of the first element (convenience method).
    */
   inline T* getFirstRef () const {
      assert (this->num > 0);
      return this->array;
   }

   /**
    * \brief Return the first element, explicitly.
    */
   inline T getFirst () const {
      assert (this->num > 0);
      return this->array[0];
   }

   /**
    * \brief Return the reference of the last element (convenience method).
    */
   inline T* getLastRef () const {
      assert (this->num > 0);
      return this->array + this->num - 1;
   }

   /**
    * \brief Return the last element, explicitly.
    */
   inline T getLast () const {
      assert (this->num > 0);
      return this->array[this->num - 1];
   }

   /**
    * \brief Store an object in the vector.
    *
    * Unlike in container::untyped::Vector and container::typed::Vector,
    * you have to care about the size, so a call to
    * misc::SimpleVector::increase or misc::SimpleVector::setSize may
    * be necessary before.
    */
   inline void set (int i, T t) {
      assert (i >= 0 && this->num - i > 0);
      this->array[i] = t;
   }
};

/**
 * \brief Container similar to lout::misc::SimpleVector, but some cases
 *    of insertion optimized (used for hyphenation).
 *
 * For hyphenation, words are often split, so that some space must be
 * inserted by the method NotSoSimpleVector::insert. Typically, some
 * elements are inserted quite at the beginning (when the word at the
 * end of the first or at the beginning of the second line is
 * hyphenated), then, a bit further (end of second line/beginning of
 * third line) and so on. In the first time, nearly all words must be
 * moved; in the second time, a bit less, etc. After all, using a
 * simple vector would result in O(n<sup>2</sup>) number of elements
 * moved total. With this class, however, the number can be kept at
 * O(n).
 *
 * The basic idea is to keep an extra array (actually two, of which
 * the second one is used temporarily), which is inserted in a logical
 * way. Since there is only one extra array at max, reading is rather
 * simple and fast (see NotSoSimpleVector::getRef): check whether the
 * position is before, within, or after the extra array. The first
 * insertion is also rather simple, when the extra array has to be
 * created. The following sketch illustrates the most complex case,
 * when an extra array exists, and something is inserted after it (the
 * case for which this class has been optimized):
 *
 * \image html not-so-simple-container.png
 *
 * Dotted lines are used to keep the boxes aligned.
 *
 * As you see, only a relatively small fraction of elements has to be
 * moved.
 *
 * There are some other cases, which have to be documented.
 */
template <class T> class NotSoSimpleVector
{
private:
   T *arrayMain, *arrayExtra1, *arrayExtra2;
   int numMain, numExtra, numAllocMain, numAllocExtra, startExtra;

   inline void resizeMain ()
   {
      /* This algorithm was tuned for memory&speed with this huge page:
       *   http://downloads.mysql.com/docs/refman-6.0-en.html.tar.gz
       */
      if (arrayMain == NULL) {
         this->numAllocMain = 1;
         this->arrayMain = (T*) malloc (sizeof (T));
      }
      if (this->numAllocMain < this->numMain) {
         this->numAllocMain = (this->numMain < 100) ?
                          this->numMain : this->numMain + this->numMain/10;
         this->arrayMain =
            (T*) realloc(this->arrayMain, (this->numAllocMain * sizeof (T)));
      }
   }

   inline void resizeExtra ()
   {
      /* This algorithm was tuned for memory&speed with this huge page:
       *   http://downloads.mysql.com/docs/refman-6.0-en.html.tar.gz
       */
      if (arrayExtra1 == NULL) {
         this->numAllocExtra = 1;
         this->arrayExtra1 = (T*) malloc (sizeof (T));
         this->arrayExtra2 = (T*) malloc (sizeof (T));
      }
      if (this->numAllocExtra < this->numExtra) {
         this->numAllocExtra = (this->numExtra < 100) ?
                          this->numExtra : this->numExtra + this->numExtra/10;
         this->arrayExtra1 =
            (T*) realloc(this->arrayExtra1, (this->numAllocExtra * sizeof (T)));
         this->arrayExtra2 =
            (T*) realloc(this->arrayExtra2, (this->numAllocExtra * sizeof (T)));
      }
   }

   void consolidate ()
   {
      if (startExtra != -1) {
         numMain += numExtra;
         resizeMain ();
         memmove (arrayMain + startExtra + numExtra, arrayMain + startExtra,
                  (numMain - (startExtra + numExtra)) * sizeof (T));
         memmove (arrayMain + startExtra, arrayExtra1, numExtra * sizeof (T));
         startExtra = -1;
         numExtra = 0;
      }
   }

public:
   inline NotSoSimpleVector (int initAlloc)
   {
      this->numMain = this->numExtra = 0;
      this->numAllocMain = initAlloc;
      this->numAllocExtra = initAlloc;
      this->arrayMain = this->arrayExtra1 = this->arrayExtra2 = NULL;
      this->startExtra = -1;
   }

   inline NotSoSimpleVector (const NotSoSimpleVector &o)
   {
      this->arrayMain = NULL;
      this->numMain = o.numMain;
      this->numAllocMain = o.numAllocMain;
      resizeMain ();
      memcpy (this->arrayMain, o.arrayMain, sizeof (T) * numMain);

      this->arrayExtra = NULL;
      this->numExtra = o.numExtra;
      this->numAllocExtra = o.numAllocExtra;
      resizeExtra ();
      memcpy (this->arrayExtra, o.arrayExtra, sizeof (T) * numExtra);

      this->startExtra = o.startExtra;
   }

   inline ~NotSoSimpleVector ()
   {
      if (this->arrayMain)
         free (this->arrayMain);
      if (this->arrayExtra1)
         free (this->arrayExtra1);
      if (this->arrayExtra2)
         free (this->arrayExtra2);
   }

   inline int size() const { return this->numMain + this->numExtra; }

   inline void increase() { setSize(size() + 1); }

   inline void setSize(int newSize)
   {
      assert (newSize >= 0);
      this->numMain = newSize - numExtra;
      this->resizeMain ();
   }

   void insert (int index, int numInsert)
   {
      assert (numInsert >= 0);

      // The following lines are a simple (but inefficient) replacement.
      //setSize (numMain + numInsert);
      //memmove (arrayMain + index + numInsert, arrayMain + index,
      //         (numMain - index - numInsert) * sizeof (T));
      //return;

      if (this->startExtra == -1) {
         // simple case
         this->numExtra = numInsert;
         this->startExtra = index;
         resizeExtra ();
      } else {
         if (index < startExtra)  {
            consolidate ();
            insert (index, numInsert);
         } else if (index < startExtra + numExtra) {
            int oldNumExtra = numExtra;
            numExtra += numInsert;
            resizeExtra ();

            int toMove = startExtra + oldNumExtra - index;
            memmove (arrayExtra1 + numExtra - toMove,
                     arrayExtra1 + index - startExtra,
                     toMove * sizeof (T));
         } else {
            int oldNumExtra = numExtra;
            numExtra += numInsert;
            resizeExtra ();

            // Note: index refers to the *logical* adress, not to the
            // *physical* one.
            int diff = index - this->startExtra - oldNumExtra;
            T *arrayMainI = arrayMain + this->startExtra;
            for (int i = diff + oldNumExtra - 1; i >= 0; i--) {
               T *src = i < oldNumExtra ?
                  this->arrayExtra1 + i : arrayMainI + (i - oldNumExtra);
               T *dest = i < diff ?
                  arrayMainI + i : arrayExtra2 + (i - diff);
               *dest = *src;
            }

            memcpy (arrayExtra1, arrayExtra2, sizeof (T) * oldNumExtra);
            startExtra = index - oldNumExtra;
         }
      }
   }

   /**
    * \brief Return the reference of one element.
    *
    * \sa misc::SimpleVector::get
    */
   inline T* getRef (int i) const
   {
      if (this->startExtra == -1)
         return this->arrayMain + i;
      else {
         if (i < this->startExtra)
            return this->arrayMain + i;
         else if (i >= this->startExtra + this->numExtra)
            return this->arrayMain + i - this->numExtra;
         else
            return this->arrayExtra1 + i - this->startExtra;
      }
   }

   /**
    * \brief Return the one element, explicitly.
    *
    * The element is copied, so for complex elements, you should rather used
    * misc::SimpleVector::getRef.
    */
   inline T get (int i) const
   {
      return *(this->getRef(i));
   }

   /**
    * \brief Return the reference of the first element (convenience method).
    */
   inline T* getFirstRef () const {
      assert (size () > 0);
      return this->getRef(0);
   }

   /**
    * \brief Return the first element, explicitly.
    */
   inline T getFirst () const {
      return *(this->getFirstRef());
   }

   /**
    * \brief Return the reference of the last element (convenience method).
    */
   inline T* getLastRef () const {
      assert (size () > 0);
      return this->getRef(size () - 1);
   }

   /**
    * \brief Return the last element, explicitly.
    */
   inline T getLast () const {
      return *(this->getLastRef());
   }

   /**
    * \brief Store an object in the vector.
    *
    * Unlike in container::untyped::Vector and container::typed::Vector,
    * you have to care about the size, so a call to
    * misc::SimpleVector::increase or misc::SimpleVector::setSize may
    * be necessary before.
    */
   inline void set (int i, T t) {
      *(this->getRef(i)) = t;
   }
};

/**
 * \brief A class for fast concatenation of a large number of strings.
 */
class StringBuffer
{
private:
   struct Node
   {
      char *data;
      Node *next;
   };

   Node *firstNode, *lastNode;
   int numChars;
   char *str;
   bool strValid;

public:
   StringBuffer();
   ~StringBuffer();

   /**
    * \brief Append a NUL-terminated string to the buffer, with copying.
    *
    * A copy is kept in the buffer, so the caller does not have to care
    * about memory management.
    */
   inline void append(const char *str) { appendNoCopy(strdup(str)); }
   void appendNoCopy(char *str);
   const char *getChars();
   void clear ();
};


/**
 * \brief A bit set, which automatically reallocates when needed.
 */
class BitSet
{
private:
   unsigned char *bits;
   int numBytes;

   inline int bytesForBits(int bits) { return bits == 0 ? 1 : (bits + 7) / 8; }

public:
   BitSet(int initBits);
   ~BitSet();
   void intoStringBuffer(misc::StringBuffer *sb);
   bool get(int i) const;
   void set(int i, bool val);
   void clear();
};

/**
 * \brief A simple allocator optimized to handle many small chunks of memory.
 * The chunks can not be free'd individually. Instead the whole zone must be
 * free'd with zoneFree().
 */
class ZoneAllocator
{
private:
   size_t poolSize, poolLimit, freeIdx;
   SimpleVector <char*> *pools;
   SimpleVector <char*> *bulk;

public:
   ZoneAllocator (size_t poolSize) {
      this->poolSize = poolSize;
      this->poolLimit = poolSize / 4;
      this->freeIdx = poolSize;
      this->pools = new SimpleVector <char*> (1);
      this->bulk = new SimpleVector <char*> (1);
   };

   ~ZoneAllocator () {
      zoneFree ();
      delete pools;
      delete bulk;
   }

   inline void * zoneAlloc (size_t t) {
      void *ret;

      if (t > poolLimit) {
         bulk->increase ();
         bulk->set (bulk->size () - 1, (char*) malloc (t));
         return bulk->get (bulk->size () - 1);
      }

      if (t > poolSize - freeIdx) {
         pools->increase ();
         pools->set (pools->size () - 1, (char*) malloc (poolSize));
         freeIdx = 0;
      }

      ret = pools->get (pools->size () - 1) + freeIdx;
      freeIdx += t;
      return ret;
   }

   inline void zoneFree () {
      for (int i = 0; i < pools->size (); i++)
         free (pools->get (i));
      pools->setSize (0);
      for (int i = 0; i < bulk->size (); i++)
         free (bulk->get (i));
      bulk->setSize (0);
      freeIdx = poolSize;
   }

   inline const char *strndup (const char *str, size_t t) {
      char *new_str = (char *) zoneAlloc (t + 1);
      memcpy (new_str, str, t);
      new_str[t] = '\0';
      return new_str;
   }

   inline const char *strdup (const char *str) {
      return strndup (str, strlen (str));
   }
};

} // namespace misc

} // namespace lout

#endif // __LOUT_MISC_HH__
