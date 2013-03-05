#ifndef __ITERATOR_HH__
#define __ITERATOR_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief Iterators are used to iterate through the contents of a widget.
 *
 * When using iterators, you should care about the results of
 * dw::core::Widget::hasContents.
 *
 * \sa dw::core::Widget::iterator
 */
class Iterator: public lout::object::Comparable
{
protected:
   Iterator(Widget *widget, Content::Type mask, bool atEnd);
   Iterator(Iterator &it);
   ~Iterator();

   Content content;

private:
   Widget *widget;
   Content::Type mask;

public:
   bool equals (Object *other);

   inline Widget *getWidget () { return widget; }
   inline Content *getContent () { return &content; }
   inline Content::Type getMask () { return mask; }

   virtual void unref ();

   /**
    * \brief Move iterator forward and store content it.
    *
    * Returns true on success.
    */
   virtual bool next () = 0;

   /**
    * \brief Move iterator backward and store content it.
    *
    * Returns true on success.
    */
   virtual bool prev () = 0;

   /**
    * \brief Extend highlighted region to contain part of the current content.
    *
    * For text, start and end define the
    * characters, otherwise, the shape is defined as [0, 1], i.e. for
    * highlighting a whole dw::core::Content, pass 0 and >= 1.
    * To unhighlight see also dw::core::Iterator::unhighlight.
    */
   virtual void highlight (int start, int end, HighlightLayer layer) = 0;

   /**
    * \brief Shrink highlighted region to no longer contain the
    *    current content.
    *
    * The direction parameter indicates whether the highlighted region should
    * be reduced from the start (direction > 0) or from the end
    * (direction < 0). If direction is 0 all content is unhighlighted.
    */
   virtual void unhighlight (int direction, HighlightLayer layer) = 0;

   /**
    * \brief Return the shape, which a part of the item, the iterator points
    *    on, allocates.
    *
    * The parameters start and end have the same meaning as in
    * DwIterator::highlight().
    */
   virtual void getAllocation (int start, int end, Allocation *allocation) = 0;

   inline Iterator *cloneIterator () { return (Iterator*)clone(); }

   static void scrollTo (Iterator *it1, Iterator *it2, int start, int end,
                         HPosition hpos, VPosition vpos);
};


/**
 * \brief This implementation of dw::core::Iterator can be used by widgets
 *    with no contents.
 */
class EmptyIterator: public Iterator
{
private:
   EmptyIterator (EmptyIterator &it);

public:
   EmptyIterator (Widget *widget, Content::Type mask, bool atEnd);

   lout::object::Object *clone();
   int compareTo(lout::object::Comparable *other);
   bool next ();
   bool prev ();
   void highlight (int start, int end, HighlightLayer layer);
   void unhighlight (int direction, HighlightLayer layer);
   void getAllocation (int start, int end, Allocation *allocation);
};


/**
 * \brief This implementation of dw::core::Iterator can be used by widgets
 *    having one text word as contents
 */
class TextIterator: public Iterator
{
private:
   /** May be NULL, in this case, the next is skipped. */
   const char *text;

   TextIterator (TextIterator &it);

public:
   TextIterator (Widget *widget, Content::Type mask, bool atEnd,
                 const char *text);

   int compareTo(lout::object::Comparable *other);

   bool next ();
   bool prev ();
   void getAllocation (int start, int end, Allocation *allocation);
};


/**
 * \brief A stack of iterators, to iterate recursively through a widget tree.
 *
 * This class is similar to dw::core::Iterator, but not
 * created by a widget, but explicitly from another iterator. Deep
 * iterators do not have the limitation, that iteration is only done within
 * a widget, instead, child widgets are iterated through recursively.
 */
class DeepIterator: public lout::object::Comparable
{
private:
   class Stack: public lout::container::typed::Vector<Iterator>
   {
   public:
      inline Stack (): lout::container::typed::Vector<Iterator> (4, false) { }
      ~Stack ();
      inline Iterator *getTop () { return get (size () - 1); }
      inline void push (Iterator *it) { put(it, -1); }
      inline void pop() { getTop()->unref (); remove (size () - 1); }
   };

   Stack stack;

   static Iterator *searchDownward (Iterator *it, Content::Type mask,
                                    bool fromEnd);
   static Iterator *searchSideward (Iterator *it, Content::Type mask,
                                    bool fromEnd);

   Content::Type mask;
   Content content;
   bool hasContents;

   inline DeepIterator () { }

public:
   DeepIterator(Iterator *it);
   ~DeepIterator();

   lout::object::Object *clone ();

   DeepIterator *createVariant(Iterator *it);
   inline Iterator *getTopIterator () { return stack.getTop(); }
   inline Content *getContent () { return &content; }

   bool isEmpty ();

   bool next ();
   bool prev ();
   inline DeepIterator *cloneDeepIterator() { return (DeepIterator*)clone(); }
   int compareTo(lout::object::Comparable *other);

   /**
    * \brief Highlight a part of the current content.
    *
    * Unhighlight the current content by passing -1 as start (see also
    * (dw::core::Iterator::unhighlight). For text, start and end define the
    * characters, otherwise, the shape is defined as [0, 1], i.e. for
    * highlighting a whole dw::core::Content, pass 0 and >= 1.
    */
   inline void highlight (int start, int end, HighlightLayer layer)
   { stack.getTop()->highlight (start, end, layer); }

   /**
    * \brief Return the shape, which a part of the item, the iterator points
    *    on, allocates.
    *
    * The parameters start and end have the same meaning as in
    * DwIterator::highlight().
    */
   inline void getAllocation (int start, int end, Allocation *allocation)
   { stack.getTop()->getAllocation (start, end, allocation); }

   inline void unhighlight (int direction, HighlightLayer layer)
   { stack.getTop()->unhighlight (direction, layer); }

   inline static void scrollTo (DeepIterator *it1, DeepIterator *it2,
                                int start, int end,
                                HPosition hpos, VPosition vpos)
   { Iterator::scrollTo(it1->stack.getTop(), it2->stack.getTop(),
                         start, end, hpos, vpos); }
};

class CharIterator: public lout::object::Comparable
{
public:
   // START and END must not clash with any char value
   // neither for signed nor unsigned char.
   enum { START = 257, END = 258 };

private:
   DeepIterator *it;
   int pos, ch;

   CharIterator ();

public:
   CharIterator (Widget *widget);
   ~CharIterator ();

   lout::object::Object *clone();
   int compareTo(lout::object::Comparable *other);

   bool next ();
   bool prev ();
   inline int getChar() { return ch; }
   inline CharIterator *cloneCharIterator() { return (CharIterator*)clone(); }

   static void highlight (CharIterator *it1, CharIterator *it2,
                          HighlightLayer layer);
   static void unhighlight (CharIterator *it1, CharIterator *it2,
                            HighlightLayer layer);

   inline static void scrollTo (CharIterator *it1, CharIterator *it2,
                                HPosition hpos, VPosition vpos)
   { DeepIterator::scrollTo(it1->it, it2->it, it1->pos, it2->pos,
                            hpos, vpos); }
};

} // namespace core
} // namespace dw

#endif // __ITERATOR_HH__
