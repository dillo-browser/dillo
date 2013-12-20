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



#include "tablecell.hh"
#include "../lout/debug.hh"
#include <stdio.h>

namespace dw {

int TableCell::CLASS_ID = -1;

TableCell::TableCell (TableCell *ref, bool limitTextWidth):
   AlignedTextblock (limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::TableCell");
   registerName ("dw::TableCell", &CLASS_ID);

   /** \bug ignoreLine1OffsetSometimes does not work? */
   //ignoreLine1OffsetSometimes = true;
   charWordIndex = -1;
   setRefTextblock (ref);
   setButtonSensitive(true);
}

TableCell::~TableCell()
{
   DBG_OBJ_DELETE ();
}

bool TableCell::wordWrap(int wordIndex, bool wrapAll)
{
   Textblock::Word *word;
   const char *p;

   bool ret = Textblock::wordWrap (wordIndex, wrapAll);

   if (charWordIndex == -1) {
      word = words->getRef (wordIndex);
      if (word->content.type == core::Content::TEXT) {
         if ((p = strchr (word->content.text,
                          word->style->textAlignChar))) {
            charWordIndex = wordIndex;
            charWordPos = p - word->content.text + 1;
         } else if (word->style->textAlignChar == ' ' &&
                    word->content.space) {
            charWordIndex = wordIndex + 1;
            charWordPos = 0;
         }
      }
   }

   if (wordIndex == charWordIndex)
      updateValue ();

   return ret;
}

int TableCell::getValue ()
{
   Textblock::Word *word;
   int i, wordIndex;
   int w;

   if (charWordIndex == -1)
      wordIndex = words->size () -1;
   else
      wordIndex = charWordIndex;

   w = 0;
   for (i = 0; i < wordIndex; i++) {
      word = words->getRef (i);
      w += word->size.width + word->origSpace;
   }

   if (charWordIndex == -1) {
      if (words->size () > 0) {
         word = words->getRef (words->size () - 1);
         w += word->size.width;
      }
   } else {
      word = words->getRef (charWordIndex);
      w += layout->textWidth (word->style->font, word->content.text,
                              charWordPos);
   }

   return w;
}

void TableCell::setMaxValue (int maxValue, int value)
{
   line1Offset = maxValue - value;
   queueResize (0, true);
}

} // namespace dw
