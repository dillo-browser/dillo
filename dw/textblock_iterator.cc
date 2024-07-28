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
   OOFAwareWidgetIterator (textblock, mask, atEnd, textblock->words->size ())
{
}

Textblock::TextblockIterator
   *Textblock::TextblockIterator::createWordIndexIterator (Textblock *textblock,
                                                           core::Content::Type
                                                           mask,
                                                           int wordIndex)
{
   TextblockIterator *tbIt = new TextblockIterator (textblock, mask, false);
   tbIt->setValues (0, wordIndex);
   return tbIt;
}

object::Object *Textblock::TextblockIterator::clone()
{
   TextblockIterator *tbIt =
      new TextblockIterator ((Textblock*)getWidget(), getMask(), false);
   cloneValues (tbIt);
   return tbIt;
}

void Textblock::TextblockIterator::highlight (int start, int end,
                                              core::HighlightLayer layer)
{
   DBG_OBJ_ENTER_O ("iterator", 0, getWidget (), "TextblockIterator::highlight",
                    "..., %d, %d, %d", start, end, layer);

   DBG_IF_RTFL {
      misc::StringBuffer sb;
      intoStringBuffer (&sb);
      DBG_OBJ_MSGF_O ("iterator", 1, getWidget (), "iterator: %s",
                      sb.getChars ());
   }

   if (inFlow ()) {
      DBG_OBJ_MSGF_O ("iterator", 1, getWidget (), "in-flow index: %d",
                      getInFlowIndex ());

      Textblock *textblock = (Textblock*)getWidget();
      int index = getInFlowIndex (), index1 = index, index2 = index;

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

      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlStart", layer, "index",
                                textblock->hlStart[layer].index);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlStart", layer, "nChar",
                                textblock->hlStart[layer].nChar);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlEnd", layer, "index",
                                textblock->hlEnd[layer].index);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlEnd", layer, "nChar",
                                textblock->hlEnd[layer].nChar);

      if (oldStartIndex != textblock->hlStart[layer].index ||
          oldStartChar != textblock->hlStart[layer].nChar ||
          oldEndIndex != textblock->hlEnd[layer].index ||
          oldEndChar != textblock->hlEnd[layer].nChar)
         textblock->queueDrawRange (index1, index2);
   } else {
      highlightOOF (start, end, layer);
   }

   DBG_OBJ_LEAVE_O (getWidget ());
}

void Textblock::TextblockIterator::unhighlight (int direction,
                                                core::HighlightLayer layer)
{
   DBG_OBJ_ENTER_O ("iterator", 0, getWidget (),
                    "TextblockIterator/unhighlight", "..., %d, %d",
                    direction, layer);

   DBG_IF_RTFL {
      misc::StringBuffer sb;
      intoStringBuffer (&sb);
      DBG_OBJ_MSGF_O ("iterator", 1, getWidget (), "iterator: %s",
                      sb.getChars ());
   }

   if (inFlow ()) {
      DBG_OBJ_MSGF_O ("iterator", 1, getWidget (), "in-flow index: %d",
                      getInFlowIndex ());

      Textblock *textblock = (Textblock*)getWidget();
      int index = getInFlowIndex (), index1 = index, index2 = index;

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

      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlStart", layer, "index",
                                textblock->hlStart[layer].index);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlStart", layer, "nChar",
                                textblock->hlStart[layer].nChar);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlEnd", layer, "index",
                                textblock->hlEnd[layer].index);
      DBG_OBJ_ARRATTRSET_NUM_O (textblock, "hlEnd", layer, "nChar",
                                textblock->hlEnd[layer].nChar);

      if (oldStartIndex != textblock->hlStart[layer].index ||
          oldStartChar != textblock->hlStart[layer].nChar ||
          oldEndIndex != textblock->hlEnd[layer].index ||
          oldEndChar != textblock->hlEnd[layer].nChar)
         textblock->queueDrawRange (index1, index2);
   } else
      unhighlightOOF (direction, layer);

   DBG_OBJ_LEAVE_O (getWidget ());
}

void Textblock::TextblockIterator::getAllocation (int start, int end,
                                                  core::Allocation *allocation)
{
   if (inFlow ()) {
      Textblock *textblock = (Textblock*)getWidget();

      int index = getInFlowIndex ();
      Word *word = textblock->words->getRef (index);
      int firstWordOfLine, textOffset, lineYOffsetCanvas, lineBorderAscent;

      int lineIndex = textblock->findLineOfWord (index);

      // It may be that the line does not exist yet.
      if (lineIndex != -1) {
         // Line exists: simple.
         Line *line = textblock->lines->getRef (lineIndex);
         firstWordOfLine = line->firstWord;
         textOffset = line->textOffset;
         lineYOffsetCanvas = textblock->lineYOffsetCanvas (line);
         lineBorderAscent = line->borderAscent;
      } else {        
         // Line does not exist. Calculate the values in a similar way as in
         // Textblock::addLine().
         Line *prevLine = textblock->lines->size () > 0 ?
            textblock->lines->getLastRef () : NULL;
         firstWordOfLine = prevLine ? prevLine->lastWord + 1 : 0;

         // The variable textOffset, defined below, is what Line::leftOffset
         // will be for the next line; Line::textOffset itself cannot be
         // calculated before the line is complete.
         bool regardBorder =
             textblock->mustBorderBeRegarded (textblock->lines->size ());
         textOffset =
            misc::max (regardBorder ?  textblock->newLineLeftBorder : 0,
                       textblock->boxOffsetX () +  textblock->leftInnerPadding
                       + (textblock->lines->size () == 0 ?
                             textblock->line1OffsetEff : 0));

         lineYOffsetCanvas = textblock->yOffsetOfLineToBeCreated ();

         lineBorderAscent = 0;
         for (int i = firstWordOfLine; i < textblock->words->size (); i++) {
            Word *w = textblock->words->getRef (i);
            int borderAscent =
               w->content.type == core::Content::WIDGET_IN_FLOW ?
               w->size.ascent - w->content.widget->getStyle()->margin.top :
               w->size.ascent;
            lineBorderAscent = misc::max (lineBorderAscent, borderAscent);
         }
      }

      allocation->x = textblock->allocation.x + textOffset;
      for (int i = firstWordOfLine; i < index; i++) {
         Word *w = textblock->words->getRef(i);
         allocation->x += w->size.width + w->effSpace;
      }
      if (start > 0 && word->content.type == core::Content::TEXT) {
         allocation->x += textblock->textWidth (word->content.text, 0, start,
                                                word->style,
                                                word->flags & Word::WORD_START,
                                                (word->flags & Word::WORD_END)
                                                && word->content.text[start]
                                                == 0);
      }
      allocation->y = lineYOffsetCanvas + lineBorderAscent - word->size.ascent;

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
   } else
      getAllocationOOF (start, end, allocation);
}

int Textblock::TextblockIterator::numContentsInFlow ()
{
   return ((Textblock*)getWidget())->words->size ();
}

void Textblock::TextblockIterator::getContentInFlow (int index,
                                                     core::Content *content)
{
   *content = ((Textblock*)getWidget())->words->getRef(index)->content;
}

} // namespace dw
