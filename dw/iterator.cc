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



#include "core.hh"
#include <limits.h>

using namespace lout;

namespace dw {
namespace core {

// --------------
//    Iterator
// --------------

Iterator::Iterator(Widget *widget, Content::Type mask, bool atEnd)
{
   this->widget = widget;
   this->mask = mask;
}

Iterator::Iterator(Iterator &it): object::Comparable ()
{
   widget = it.widget;
   content = it.content;
}

Iterator::~Iterator()
{
}

bool Iterator::equals (Object *other)
{
   Iterator *otherIt = (Iterator*)other;
   return
      this == otherIt ||
      (getWidget() == otherIt->getWidget() && compareTo(otherIt) == 0);
}

/**
 * \brief Delete the iterator.
 *
 * The destructor is hidden, implementations may use optimizations for
 * the allocation. (Will soon be the case for dw::core::EmptyIteratorFactory.)
 */
void Iterator::unref ()
{
   delete this;
}

/**
 * \brief Scrolls the viewport, so that the region between \em it1 and
 * \em it2 is seen, according to \em hpos and \em vpos.
 *
 * The parameters \em start and \em end have the same meaning as in
 * dw::core::Iterator::getAllocation, \em start refers
 * to \em it1, while \em end rerers to \em it2.
 *
 * If \em it1 and \em it2 point to the same location (see code), only
 * \em it1 is regarded, and both belowstart and belowend refer to it.
 */
void Iterator::scrollTo (Iterator *it1, Iterator *it2, int start, int end,
                         HPosition hpos, VPosition vpos)
{
   Allocation alloc1, alloc2, alloc;
   int x1, x2, y1, y2;
   DeepIterator *eit1, *eit2, *eit3;
   int curStart, curEnd, cmp;
   bool atStart;

   if (it1->equals(it2)) {
      it1->getAllocation (start, end, &alloc);
      it1->getWidget()->getLayout()->scrollTo (hpos, vpos, alloc.x, alloc.y,
                                               alloc.width,
                                               alloc.ascent + alloc.descent);
   } else {
      // First, determine the rectangle all iterators from it1 and it2
      // allocate, i.e. the smallest rectangle containing all allocations of
      // these iterators.
      eit1 = new DeepIterator (it1);
      eit2 = new DeepIterator (it2);

      x1 = INT_MAX;
      x2 = INT_MIN;
      y1 = INT_MAX;
      y2 = INT_MIN;

      for (eit3 = (DeepIterator*)eit1->clone (), atStart = true;
           (cmp = eit3->compareTo (eit2)) <= 0;
           eit3->next (), atStart = false) {
         if (atStart)
            curStart = start;
         else
            curStart = 0;

         if (cmp == 0)
            curEnd = end;
         else
            curEnd = INT_MAX;

         eit3->getAllocation (curStart, curEnd, &alloc);
         x1 = misc::min (x1, alloc.x);
         x2 = misc::max (x2, alloc.x + alloc.width);
         y1 = misc::min (y1, alloc.y);
         y2 = misc::max (y2, alloc.y + alloc.ascent + alloc.descent);
      }

      delete eit3;
      delete eit2;
      delete eit1;

      it1->getAllocation (start, INT_MAX, &alloc1);
      it2->getAllocation (0, end, &alloc2);

      if (alloc1.x > alloc2.x) {
         //
         // This is due to a line break within the region. When the line is
         // longer than the viewport, and the region is actually quite short,
         // the user would not see anything of the region, as in this figure
         // (with region marked as "#"):
         //
         //            +----------+   ,-- alloc1
         //            |          |   V
         //            |          |  ### ###
         //   ### ###  |          |
         //        ^   |          | <-- viewport
         //        |   +----------+
         //        `-- alloc2
         //   |----------------------------|
         //               width
         //
         // Therefore, we make the region smaller, so that the region will be
         // displayed like this:
         //
         //                           ,-- alloc1
         //                      +----|-----+
         //                      |    V     |
         //                      |   ### ###|
         //   ### ###            |          |
         //        ^             |          | <-- viewport
         //        `-- alloc2    +----------+
         //                      |----------|
         //                         width
         //

         /** \todo Changes in the viewport size, until the idle function is
          *      called, are not regarded. */

         if (it1->getWidget()->getLayout()->getUsesViewport() &&
             x2 - x1 > it1->getWidget()->getLayout()->getWidthViewport()) {
            x1 = x2 - it1->getWidget()->getLayout()->getWidthViewport();
            x2 = x1 + it1->getWidget()->getLayout()->getWidthViewport();
         }
      }

      if (alloc1.y > alloc2.y) {
         // This is similar to the case above, e.g. if the region ends in
         // another table column.
         if (it1->getWidget()->getLayout()->getUsesViewport() &&
             y2 - y1 > it1->getWidget()->getLayout()->getHeightViewport()) {
            y1 = y2 - it1->getWidget()->getLayout()->getHeightViewport();
            y2 = y1 + it1->getWidget()->getLayout()->getHeightViewport();
         }
      }

      it1->getWidget()->getLayout()->scrollTo (hpos, vpos,
                                               x1, y1, x2 - x1, y2 - y1);
   }
}

// -------------------
//    EmptyIterator
// -------------------

EmptyIterator::EmptyIterator (Widget *widget, Content::Type mask, bool atEnd):
   Iterator (widget, mask, atEnd)
{
   this->content.type = (atEnd ? Content::END : Content::START);
}

EmptyIterator::EmptyIterator (EmptyIterator &it): Iterator (it)
{
}

object::Object *EmptyIterator::clone ()
{
   return new EmptyIterator (*this);
}

int EmptyIterator::compareTo (object::Comparable *other)
{
   EmptyIterator *otherIt = (EmptyIterator*)other;

   if (content.type == otherIt->content.type)
      return 0;
   else if (content.type == Content::START)
      return -1;
   else
      return +1;
}

bool EmptyIterator::next ()
{
   content.type = Content::END;
   return false;
}

bool EmptyIterator::prev ()
{
   content.type = Content::START;
   return false;
}

void EmptyIterator::highlight (int start, int end, HighlightLayer layer)
{
}

void EmptyIterator::unhighlight (int direction, HighlightLayer layer)
{
}

void EmptyIterator::getAllocation (int start, int end, Allocation *allocation)
{
}

// ------------------
//    TextIterator
// ------------------

TextIterator::TextIterator (Widget *widget, Content::Type mask, bool atEnd,
                            const char *text): Iterator (widget, mask, atEnd)
{
   this->content.type = (atEnd ? Content::END : Content::START);
   this->text = (mask & Content::TEXT) ? text : NULL;
}

TextIterator::TextIterator (TextIterator &it): Iterator (it)
{
   text = it.text;
}

int TextIterator::compareTo (object::Comparable *other)
{
   TextIterator *otherIt = (TextIterator*)other;

   if (content.type == otherIt->content.type)
      return 0;

   switch (content.type) {
   case Content::START:
      return -1;

   case Content::TEXT:
      if (otherIt->content.type == Content::START)
         return +1;
      else
         return -1;

   case Content::END:
      return +1;

   default:
      misc::assertNotReached();
      return 0;
   }
}

bool TextIterator::next ()
{
   if (content.type == Content::START && text != NULL) {
      content.type = Content::TEXT;
      content.text = text;
      return true;
   } else {
      content.type = Content::END;
      return false;
   }
}

bool TextIterator::prev ()
{
   if (content.type == Content::END && text != NULL) {
      content.type = Content::TEXT;
      content.text = text;
      return true;
   } else {
      content.type = Content::START;
      return false;
   }
}

void TextIterator::getAllocation (int start, int end, Allocation *allocation)
{
   // Return the allocation of the widget.
   *allocation = *(getWidget()->getAllocation ());
}

// ------------------
//    DeepIterator
// ------------------

DeepIterator::Stack::~Stack ()
{
   for (int i = 0; i < size (); i++)
      get(i)->unref ();
}

/*
 * The following two methods are used by dw::core::DeepIterator::DeepIterator,
 * when the passed dw::core::Iterator points to a widget. Since a
 * dw::core::DeepIterator never returns a widget, the dw::core::Iterator has
 * to be corrected, by searching for the next content downwards (within the
 * widget pointed to), forwards, and backwards (in the traversed tree).
 */

/*
 * Search downwards. If fromEnd is true, start search at the end,
 * otherwise at the beginning.
 */
Iterator *DeepIterator::searchDownward (Iterator *it, Content::Type mask,
                                        bool fromEnd)
{
   Iterator *it2, *it3;

   //DEBUG_MSG (1, "%*smoving down (%swards) from %s\n",
   //          indent, "", from_end ? "back" : "for", a_Dw_iterator_text (it));

   assert (it->getContent()->type == Content::WIDGET);
   it2 = it->getContent()->widget->iterator (mask, fromEnd);

   if (it2 == NULL) {
      // Moving downwards failed.
      //DEBUG_MSG (1, "%*smoving down failed\n", indent, "");
      return NULL;
   }

   while (fromEnd ? it2->prev () : it2->next ()) {
      //DEBUG_MSG (1, "%*sexamining %s\n",
      //           indent, "", a_Dw_iterator_text (it2));

      if (it2->getContent()->type == Content::WIDGET) {
         // Another widget. Search in it downwards.
         it3 = searchDownward (it2, mask, fromEnd);
         if (it3 != NULL) {
            it2->unref ();
            return it3;
         }
         // Else continue in this widget.
      } else {
         // Success!
         //DEBUG_MSG (1, "%*smoving down succeeded: %s\n",
         //           indent, "", a_Dw_iterator_text (it2));
         return it2;
      }
   }

   // Nothing found.
   it2->unref ();
   //DEBUG_MSG (1, "%*smoving down failed (nothing found)\n", indent, "");
   return NULL;
}

/*
 * Search sidewards. fromEnd specifies the direction, false means forwards,
 * true means backwards.
 */
Iterator *DeepIterator::searchSideward (Iterator *it, Content::Type mask,
                                        bool fromEnd)
{
   Iterator *it2, *it3;

   //DEBUG_MSG (1, "%*smoving %swards from %s\n",
   //          indent, "", from_end ? "back" : "for", a_Dw_iterator_text (it));

   assert (it->getContent()->type == Content::WIDGET);
   it2 = it->cloneIterator ();

   while (fromEnd ? it2->prev () : it2->next ()) {
      if (it2->getContent()->type == Content::WIDGET) {
         // Search downwards in this widget.
         it3 = searchDownward (it2, mask, fromEnd);
         if (it3 != NULL) {
            it2->unref ();
            //DEBUG_MSG (1, "%*smoving %swards succeeded: %s\n",
            //           indent, "", from_end ? "back" : "for",
            //           a_Dw_iterator_text (it3));
            return it3;
         }
         // Else continue in this widget.
      } else {
         // Success!
         // DEBUG_MSG (1, "%*smoving %swards succeeded: %s\n",
         //            indent, "", from_end ? "back" : "for",
         //            a_Dw_iterator_text (it2));
         return it2;
      }
   }

   /* Nothing found, go upwards in the tree (if possible). */
   it2->unref ();
   if (it->getWidget()->getParent ()) {
      it2 = it->getWidget()->getParent()->iterator (mask, false);
      while (true) {
         if (!it2->next ())
            misc::assertNotReached ();

         if (it2->getContent()->type == Content::WIDGET &&
             it2->getContent()->widget == it->getWidget ()) {
            it3 = searchSideward (it2, mask, fromEnd);
            it2->unref ();
            //DEBUG_MSG (1, "%*smoving %swards succeeded: %s\n",
            //           indent, "", from_end ? "back" : "for",
            //           a_Dw_iterator_text (it3));
            return it3;
         }
      }
   }

   // Nothing found at all.
   // DEBUG_MSG (1, "%*smoving %swards failed (nothing found)\n",
   //            indent, "", from_end ? "back" : "for");
   return NULL;
}

/**
 * \brief Create a new deep iterator from an existing dw::core::Iterator.
 *
 * The content of the return value will be the content of \em it. If within
 * the widget tree, there is no non-widget content, the resulting deep
 * iterator is empty (denoted by dw::core::DeepIterator::stack == NULL).
 *
 * Notes:
 *
 * <ol>
 * <li> The mask of \em i" must include DW_CONTENT_WIDGET, but
 *      dw::core::DeepIterator::next will never return widgets.
 * </ol>
 */
DeepIterator::DeepIterator (Iterator *it)
{
   //DEBUG_MSG (1, "a_Dw_ext_iterator_new: %s\n", a_Dw_iterator_text (it));

   // Clone input iterator, so the iterator passed as parameter
   // remains untouched.
   it = it->cloneIterator ();
   this->mask = it->getMask ();

   hasContents = true;

   // If it points to a widget, find a near non-widget content,
   // since an DeepIterator should never return widgets.
   if (it->getContent()->type == Content::WIDGET) {
      Iterator *it2;

      // The second argument of searchDownward is actually a matter of
      // taste :-)
      if ((it2 = searchDownward (it, mask, false)) ||
          (it2 = searchSideward (it, mask, false)) ||
          (it2 = searchSideward (it, mask, true))) {
         it->unref ();
         it = it2;
      } else {
         // This may happen, when a page does not contain any non-widget
         // content.
         //DEBUG_MSG (1, "a_Dw_ext_iterator_new got totally helpless!\n");
         it->unref ();
         hasContents = false;
      }
   }

   //DEBUG_MSG (1, "  => %s\n", a_Dw_iterator_text (it));

   if (hasContents) {
      // If this widget has parents, we must construct appropriate iterators.
      //
      // \todo There may be a faster way instead of iterating through the
      //    parent widgets.

      // Construct the iterators.
      int thisLevel = it->getWidget()->getLevel (), level;
      Widget *w;
      for (w = it->getWidget (), level = thisLevel; w->getParent() != NULL;
           w = w->getParent (), level--) {
         Iterator *it = w->getParent()->iterator (mask, false);
         stack.put (it, level - 1);
         while (true) {
            bool hasNext = it->next();
            assert (hasNext);

            if (it->getContent()->type == Content::WIDGET &&
                it->getContent()->widget == w)
               break;
         }
      }

      stack.put (it, thisLevel);
      content = *(it->getContent());
   }
}


DeepIterator::~DeepIterator ()
{
}

object::Object *DeepIterator::clone ()
{
   DeepIterator *it = new DeepIterator ();

   for (int i = 0; i < stack.size (); i++)
      it->stack.put (stack.get(i)->cloneIterator (), i);

   it->mask = mask;
   it->content = content;
   it->hasContents = hasContents;

   return it;
}

int DeepIterator::compareTo (object::Comparable *other)
{
   DeepIterator *otherDeepIterator = (DeepIterator*)other;

   // Search the highest level, where the widgets are the same.
   int level = 0;

   while (stack.get(level)->getWidget ()
          == otherDeepIterator->stack.get(level)->getWidget ()) {
      if (level == stack.size() - 1 ||
          level == otherDeepIterator->stack.size() - 1)
         break;
      level++;
   }

   while (stack.get(level)->getWidget ()
          != otherDeepIterator->stack.get(level)->getWidget ())
      level--;

   return stack.get(level)->compareTo (otherDeepIterator->stack.get(level));
}

DeepIterator *DeepIterator::createVariant(Iterator *it)
{
   /** \todo Not yet implemented, and actually not yet needed very much. */
   return new DeepIterator (it);
}

bool DeepIterator::isEmpty () {
   return !hasContents;
}

/**
 * \brief Move iterator forward and store content it.
 *
 * Returns true on success.
 */
bool DeepIterator::next ()
{
   Iterator *it = stack.getTop ();

   if (it->next ()) {
      if (it->getContent()->type == Content::WIDGET) {
         // Widget: new iterator on stack, to search in this widget.
         stack.push (it->getContent()->widget->iterator (mask, false));
         return next ();
      } else {
         // Simply return the content of the iterartor.
         content = *(it->getContent ());
         return true;
      }
   } else {
      // No more data in the top-most widget.
      if (stack.size () > 1) {
         // Pop iterator from stack, and move to next item in the old one.
         stack.pop ();
         return next ();
      } else {
         // Stack is empty.
         content.type = Content::END;
         return false;
      }
   }
}

/**
 * \brief Move iterator backward and store content it.
 *
 * Returns true on success.
 */
bool DeepIterator::prev ()
{
   Iterator *it = stack.getTop ();

   if (it->prev ()) {
      if (it->getContent()->type == Content::WIDGET) {
         // Widget: new iterator on stack, to search in this widget.
         stack.push (it->getContent()->widget->iterator (mask, true));
         return prev ();
      } else {
         // Simply return the content of the iterartor.
         content = *(it->getContent ());
         return true;
      }
   } else {
      // No more data in the top-most widget.
      if (stack.size () > 1) {
         // Pop iterator from stack, and move to next item in the old one.
         stack.pop ();
         return prev ();
      } else {
         // Stack is empty.
         content.type = Content::START;
         return false;
      }
   }
}

// -----------------
//    CharIterator
// -----------------

CharIterator::CharIterator ()
{
   it = NULL;
}

CharIterator::CharIterator (Widget *widget)
{
   Iterator *i = widget->iterator (Content::SELECTION_CONTENT, false);
   it = new DeepIterator (i);
   i->unref ();
   ch = START;
}

CharIterator::~CharIterator ()
{
   if (it)
      delete it;
}

object::Object *CharIterator::clone()
{
   CharIterator *cloned = new CharIterator ();
   cloned->it = it->cloneDeepIterator ();
   cloned->ch = ch;
   cloned->pos = pos;
   return cloned;
}

int CharIterator::compareTo(object::Comparable *other)
{
   CharIterator *otherIt = (CharIterator*)other;
   int c = it->compareTo(otherIt->it);
   if (c != 0)
      return c;
   else
      return pos - otherIt->pos;
}

bool CharIterator::next ()
{
   if (ch == START || it->getContent()->type == Content::BREAK ||
       (it->getContent()->type == Content::TEXT &&
        it->getContent()->text[pos] == 0)) {
      if (it->next()) {
         if (it->getContent()->type == Content::BREAK)
            ch = '\n';
         else { // if (it->getContent()->type == Content::TEXT)
            pos = 0;
            ch = it->getContent()->text[pos];
            if (ch == 0)
               // should not happen, actually
               return next ();
         }
         return true;
      }
      else {
         ch = END;
         return false;
      }
   } else if (ch == END)
      return false;
   else {
      // at this point, it->getContent()->type == Content::TEXT
      pos++;
      ch = it->getContent()->text[pos];
      if (ch == 0) {
         if (it->getContent()->space) {
            ch = ' ';
         } else {
            return next ();
         }
      }

      return true;
   }
}

bool CharIterator::prev ()
{
   if (ch == END || it->getContent()->type == Content::BREAK ||
       (it->getContent()->type == Content::TEXT && pos == 0)) {
      if (it->prev()) {
         if (it->getContent()->type == Content::BREAK)
            ch = '\n';
         else { // if (it->getContent()->type == Content::TEXT)
            if (it->getContent()->text[0] == 0)
               return prev ();
            else {
               pos = strlen (it->getContent()->text);
               if (it->getContent()->space) {
                  ch = ' ';
               } else {
                  pos--;
                  ch = it->getContent()->text[pos];
               }
            }
         }
         return true;
      }
      else {
         ch = START;
         return false;
      }
   } else if (ch == START)
      return false;
   else {
      // at this point, it->getContent()->type == Content::TEXT
      pos--;
      ch = it->getContent()->text[pos];
      return true;
   }
}

void CharIterator::highlight (CharIterator *it1, CharIterator *it2,
                              HighlightLayer layer)
{
   if (it2->getChar () == CharIterator::END)
      it2->prev ();

   if (it1->it->compareTo (it2->it) == 0)
      // Only one content => highlight part of it.
      it1->it->highlight (it1->pos, it2->pos, layer);
   else {
      DeepIterator *it = it1->it->cloneDeepIterator ();
      int c;
      bool start;
      for (start = true;
           (c = it->compareTo (it2->it)) <= 0;
           it->next (), start = false) {
         int endOfWord =
            it->getContent()->type == Content::TEXT ?
            strlen (it->getContent()->text) : 1;
         if (start) // first iteration
            it->highlight (it1->pos, endOfWord, layer);
         else if (c == 0) // last iteration
            it->highlight (0, it2->pos, layer);
         else
            it->highlight (0, endOfWord, layer);
      }
      delete it;
   }
}

void CharIterator::unhighlight (CharIterator *it1, CharIterator *it2,
                                HighlightLayer layer)
{
   if (it1->it->compareTo (it2->it) == 0)
      // Only one content => unhighlight it (only for efficiency).
      it1->it->unhighlight (0, layer);
   else {
      DeepIterator *it = it1->it->cloneDeepIterator ();
      for (; it->compareTo (it2->it) <= 0; it->next ())
         it->unhighlight (-1, layer);
      delete it;
   }
}

} // namespace core
} // namespace dw
