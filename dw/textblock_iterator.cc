/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2012-2013 Sebastian Geerken <sgeerken@dillo.org>
 *
 * (Parts of this file were originally part of textblock.cc.)
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

#include "textblock.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <stdio.h>
#include <math.h>

using namespace lout;

namespace dw {

Textblock::TextblockIterator::TextblockIterator (Textblock *textblock,
                                                 core::Content::Type mask,
                                                 bool atEnd):
   core::Iterator (textblock, mask, atEnd)
{
   index = atEnd ? textblock->words->size () : -1;
   content.type = atEnd ? core::Content::END : core::Content::START;
}

Textblock::TextblockIterator::TextblockIterator (Textblock *textblock,
                                                 core::Content::Type mask,
                                                 int index):
   core::Iterator (textblock, mask, false)
{
   this->index = index;

   if (index < 0)
      content.type = core::Content::START;
   else if (index >= textblock->words->size ())
      content.type = core::Content::END;
   else
      content = textblock->words->getRef(index)->content;
}

object::Object *Textblock::TextblockIterator::clone()
{
   return new TextblockIterator ((Textblock*)getWidget(), getMask(), index);
}

int Textblock::TextblockIterator::compareTo(object::Comparable *other)
{
   return index - ((TextblockIterator*)other)->index;
}

bool Textblock::TextblockIterator::next ()
{
   Textblock *textblock = (Textblock*)getWidget();

   if (content.type == core::Content::END)
      return false;

   do {
      index++;
      if (index >= textblock->words->size ()) {
         content.type = core::Content::END;
         return false;
      }
   } while ((textblock->words->getRef(index)->content.type & getMask()) == 0);

   content = textblock->words->getRef(index)->content;
   return true;
}

bool Textblock::TextblockIterator::prev ()
{
   Textblock *textblock = (Textblock*)getWidget();

   if (content.type == core::Content::START)
      return false;

   do {
      index--;
      if (index < 0) {
         content.type = core::Content::START;
         return false;
      }
   } while ((textblock->words->getRef(index)->content.type & getMask()) == 0);

   content = textblock->words->getRef(index)->content;
   return true;
}

void Textblock::TextblockIterator::highlight (int start, int end,
                                              core::HighlightLayer layer)
{
   Textblock *textblock = (Textblock*)getWidget();
   int index1 = index, index2 = index;

   int oldStartIndex = textblock->hlStart[layer].index;
   int oldStartChar = textblock->hlStart[layer].nChar;
   int oldEndIndex = textblock->hlEnd[layer].index;
   int oldEndChar = textblock->hlEnd[layer].nChar;

   if (textblock->hlStart[layer].index > textblock->hlEnd[layer].index) {
      /* nothing is highlighted */
      textblock->hlStart[layer].index = index;
      textblock->hlEnd[layer].index = index;
   }

   if (textblock->hlStart[layer].index >= index) {
      index2 = textblock->hlStart[layer].index;
      textblock->hlStart[layer].index = index;
      textblock->hlStart[layer].nChar = start;
   }

   if (textblock->hlEnd[layer].index <= index) {
      index2 = textblock->hlEnd[layer].index;
      textblock->hlEnd[layer].index = index;
      textblock->hlEnd[layer].nChar = end;
   }

   if (oldStartIndex != textblock->hlStart[layer].index ||
       oldStartChar != textblock->hlStart[layer].nChar ||
       oldEndIndex != textblock->hlEnd[layer].index ||
       oldEndChar != textblock->hlEnd[layer].nChar)
      textblock->queueDrawRange (index1, index2);
}

void Textblock::TextblockIterator::unhighlight (int direction,
                                                core::HighlightLayer layer)
{
   Textblock *textblock = (Textblock*)getWidget();
   int index1 = index, index2 = index;

   if (textblock->hlStart[layer].index > textblock->hlEnd[layer].index)
      return;

   int oldStartIndex = textblock->hlStart[layer].index;
   int oldStartChar = textblock->hlStart[layer].nChar;
   int oldEndIndex = textblock->hlEnd[layer].index;
   int oldEndChar = textblock->hlEnd[layer].nChar;

   if (direction == 0) {
      index1 = textblock->hlStart[layer].index;
      index2 = textblock->hlEnd[layer].index;
      textblock->hlStart[layer].index = 1;
      textblock->hlEnd[layer].index = 0;
   } else if (direction > 0 && textblock->hlStart[layer].index <= index) {
      index1 = textblock->hlStart[layer].index;
      textblock->hlStart[layer].index = index + 1;
      textblock->hlStart[layer].nChar = 0;
   } else if (direction < 0 && textblock->hlEnd[layer].index >= index) {
      index1 = textblock->hlEnd[layer].index;
      textblock->hlEnd[layer].index = index - 1;
      textblock->hlEnd[layer].nChar = INT_MAX;
   }

   if (oldStartIndex != textblock->hlStart[layer].index ||
       oldStartChar != textblock->hlStart[layer].nChar ||
       oldEndIndex != textblock->hlEnd[layer].index ||
       oldEndChar != textblock->hlEnd[layer].nChar)
      textblock->queueDrawRange (index1, index2);
}

void Textblock::TextblockIterator::getAllocation (int start, int end,
                                                  core::Allocation *allocation)
{
   Textblock *textblock = (Textblock*)getWidget();
   int lineIndex = textblock->findLineOfWord (index);
   Line *line = textblock->lines->getRef (lineIndex);
   Word *word = textblock->words->getRef (index);

   allocation->x =
      textblock->allocation.x + textblock->lineXOffsetWidget (line);

   for (int i = line->firstWord; i < index; i++) {
      Word *w = textblock->words->getRef(i);
      allocation->x += w->size.width + w->effSpace;
   }
   if (start > 0 && word->content.type == core::Content::TEXT) {
      allocation->x += textblock->textWidth (word->content.text, 0, start,
                                             word->style,
                                             word->flags & Word::WORD_START,
                                             (word->flags & Word::WORD_END)
                                             && word->content.text[start] == 0);
   }
   allocation->y = textblock->lineYOffsetCanvas (line) + line->boxAscent -
                   word->size.ascent;

   allocation->width = word->size.width;
   if (word->content.type == core::Content::TEXT) {
      int wordEnd = strlen(word->content.text);

      if (start > 0 || end < wordEnd) {
         end = misc::min(end, wordEnd); /* end could be INT_MAX */
         allocation->width =
            textblock->textWidth (word->content.text, start, end - start,
                                  word->style,
                                  (word->flags & Word::WORD_START)
                                  && start == 0,
                                  (word->flags & Word::WORD_END)
                                  && word->content.text[end] == 0);
      }
   }
   allocation->ascent = word->size.ascent;
   allocation->descent = word->size.descent;
}

} // namespace dw
