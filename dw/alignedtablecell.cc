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



#include "alignedtablecell.hh"
#include "table.hh"
#include "tablecell.hh"
#include "../lout/debug.hh"
#include <stdio.h>

namespace dw {

int AlignedTableCell::CLASS_ID = -1;

AlignedTableCell::AlignedTableCell (AlignedTableCell *ref, bool limitTextWidth):
   AlignedTextblock (limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::AlignedTableCell");
   registerName ("dw::AlignedTableCell", &CLASS_ID);

   /** \bug ignoreLine1OffsetSometimes does not work? */
   //ignoreLine1OffsetSometimes = true;
   charWordIndex = -1;
   setRefTextblock (ref);
   setButtonSensitive(true);
}

AlignedTableCell::~AlignedTableCell()
{
   DBG_OBJ_DELETE ();
}


bool AlignedTableCell::getAdjustMinWidth ()
{
   return tablecell::getAdjustMinWidth ();
}

bool AlignedTableCell::isBlockLevel ()
{
   return tablecell::isBlockLevel ();
}

bool AlignedTableCell::mustBeWidenedToAvailWidth ()
{
   return tablecell::mustBeWidenedToAvailWidth ();
}

int AlignedTableCell::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "AlignedTableCell/getAvailWidthOfChild",
                  "%p, %s", child, forceValue ? "true" : "false");

   int width = tablecell::correctAvailWidthOfChild
      (this, child, Textblock::getAvailWidthOfChild (child, forceValue),
       forceValue);

   DBG_OBJ_LEAVE ();
   return width;
}

int AlignedTableCell::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "AlignedTableCell/getAvailHeightOfChild",
                  "%p, %s", child, forceValue ? "true" : "false");

   int height = tablecell::correctAvailHeightOfChild
      (this, child, Textblock::getAvailHeightOfChild (child, forceValue),
       forceValue);

   DBG_OBJ_LEAVE ();
   return height;
}

void AlignedTableCell::correctRequisitionOfChild (Widget *child,
                                                  core::Requisition
                                                  *requisition,
                                                  void (*splitHeightFun) (int,
                                                                          int*,
                                                                          int*))
{
   DBG_OBJ_ENTER ("resize", 0, "AlignedTableCell/correctRequisitionOfChild",
                  "%p, %d * (%d + %d), ...", child, requisition->width,
                  requisition->ascent, requisition->descent);

   AlignedTextblock::correctRequisitionOfChild (child, requisition,
                                                splitHeightFun);
   tablecell::correctCorrectedRequisitionOfChild (this, child, requisition,
                                                  splitHeightFun);

   DBG_OBJ_LEAVE ();
}

void AlignedTableCell::correctExtremesOfChild (Widget *child,
                                               core::Extremes *extremes,
                                               bool useAdjustmentWidth)
{
   DBG_OBJ_ENTER ("resize", 0, "AlignedTableCell/correctExtremesOfChild",
                  "%p, %d (%d) / %d (%d)",
                  child, extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   AlignedTextblock::correctExtremesOfChild (child, extremes,
                                             useAdjustmentWidth);
   tablecell::correctCorrectedExtremesOfChild (this, child, extremes,
                                               useAdjustmentWidth);

   DBG_OBJ_LEAVE ();
}

int AlignedTableCell::applyPerWidth (int containerWidth,
                                     core::style::Length perWidth)
{
   return tablecell::applyPerWidth (this, containerWidth, perWidth);
}

int AlignedTableCell::applyPerHeight (int containerHeight,
                                      core::style::Length perHeight)
{
   return tablecell::applyPerHeight (this, containerHeight, perHeight);
}

int AlignedTableCell::wordWrap(int wordIndex, bool wrapAll)
{
   Textblock::Word *word;
   const char *p;

   int ret = Textblock::wordWrap (wordIndex, wrapAll);

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

int AlignedTableCell::getValue ()
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

void AlignedTableCell::setMaxValue (int maxValue, int value)
{
   line1Offset = maxValue - value;
   queueResize (OutOfFlowMgr::createRefNormalFlow (0), true);
}

} // namespace dw
