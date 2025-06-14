/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2012-2014 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2023-2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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
#include "../lout/unicode.hh"
#include "../lout/debug.hh"

#include <stdio.h>
#include <math.h> // remove again?
#include <limits.h>

/*
 * Local variables
 */
/* The tooltip under mouse pointer in current textblock. No ref. hold.
 * (having one per view looks not worth the extra clutter). */
static dw::core::style::Tooltip *hoverTooltip = NULL;


using namespace lout;
using namespace lout::misc;
using namespace lout::unicode;

namespace dw {

int Textblock::CLASS_ID = -1;

Textblock::WordImgRenderer::WordImgRenderer (Textblock *textblock,
                                             int wordNo)
{
   //printf ("new WordImgRenderer %p\n", this);

   this->textblock = textblock;
   this->wordNo = wordNo;
   dataSet = false;
}

Textblock::WordImgRenderer::~WordImgRenderer ()
{
   //printf ("delete WordImgRenderer %p\n", this);
}

void Textblock::WordImgRenderer::setData (int xWordWidget, int lineNo)
{
   dataSet = true;
   this->xWordWidget = xWordWidget;
   this->lineNo = lineNo;
}

bool Textblock::WordImgRenderer::readyToDraw ()
{
   //print ();
   //printf ("\n");

   return dataSet && textblock->wasAllocated ()
      && wordNo < textblock->words->size()
      && lineNo < textblock->lines->size();
}

void Textblock::WordImgRenderer::getBgArea (int *x, int *y, int *width,
                                            int *height)
{
   // TODO Subtract margin and border (padding box)?
   Line *line = textblock->lines->getRef (lineNo);
   *x = textblock->allocation.x + this->xWordWidget;
   *y = textblock->lineYOffsetCanvas (line);
   *width = textblock->words->getRef(wordNo)->size.width;
   *height = line->borderAscent + line->borderDescent;
}

void Textblock::WordImgRenderer::getRefArea (int *xRef, int *yRef,
                                             int *widthRef, int *heightRef)
{
   // See comment in Widget::drawBox about the reference area.
   textblock->getPaddingArea (xRef, yRef, widthRef, heightRef);
}

core::style::Style *Textblock::WordImgRenderer::getStyle ()
{
   return textblock->words->getRef(wordNo)->style;
}

void Textblock::WordImgRenderer::draw (int x, int y, int width, int height)
{
   textblock->queueDrawArea (x - textblock->allocation.x,
                             y - textblock->allocation.y, width, height);

}

void Textblock::SpaceImgRenderer::getBgArea (int *x, int *y, int *width,
                                             int *height)
{
   WordImgRenderer::getBgArea (x, y, width, height);
   *x += *width;
   *width = textblock->words->getRef(wordNo)->effSpace;
}

core::style::Style *Textblock::SpaceImgRenderer::getStyle ()
{
   return textblock->words->getRef(wordNo)->spaceStyle;
}

// ----------------------------------------------------------------------

Textblock::DivChar Textblock::divChars[NUM_DIV_CHARS] = {
   // soft hyphen (U+00AD)
   { "\xc2\xad", true, false, true, PENALTY_HYPHEN, -1 },
   // simple hyphen-minus: same penalties like automatic or soft hyphens
   { "-", false, true, true, -1, PENALTY_HYPHEN },
   // (unconditional) hyphen (U+2010): handled exactly like minus-hyphen.
   { "\xe2\x80\x90", false, true, true, -1, PENALTY_HYPHEN },
   // em dash (U+2014): breaks on both sides are allowed (but see below).
   { "\xe2\x80\x94", false, true, false,
     PENALTY_EM_DASH_LEFT, PENALTY_EM_DASH_RIGHT }
};

// Standard values are defined here. The values are already multiplied
// with 100.
//
// Some examples (details are described in doc/dw-line-breaking.doc):
//
// 0 = Perfect line; as penalty used for normal spaces.

// 1 (100 here) = A justified line with spaces having 150% or 67% of
//                the ideal space width has this as badness.
//
// 8 (800 here) = A justified line with spaces twice as wide as
//                ideally has this as badness.
//
// The second value is used when the line before ends with a hyphen,
// dash etc.

int Textblock::penalties[PENALTY_NUM][2] = {
   // Penalties for all hyphens.
   { 100, 800 },
   // Penalties for a break point *left* of an em-dash: rather large,
   // so that a break on the *right* side is preferred.
   { 800, 800 },
   // Penalties for a break point *right* of an em-dash: like hyphens.
   { 100, 800 }
};

int Textblock::stretchabilityFactor = 100;

/**
 * The character which is used to draw a hyphen at the end of a line,
 * either caused by automatic hyphenation, or by soft hyphens.
 *
 * Initially, soft hyphens were used, but they are not drawn on some
 * platforms. Also, unconditional hyphens (U+2010) are not available
 * in many fonts; so, a simple hyphen-minus is used.
 */
const char *Textblock::hyphenDrawChar = "-";

void Textblock::setPenaltyHyphen (int penaltyHyphen)
{
   penalties[PENALTY_HYPHEN][0] = penaltyHyphen;
}

void Textblock::setPenaltyHyphen2 (int penaltyHyphen2)
{
   penalties[PENALTY_HYPHEN][1] = penaltyHyphen2;
}

void Textblock::setPenaltyEmDashLeft (int penaltyLeftEmDash)
{
   penalties[PENALTY_EM_DASH_LEFT][0] = penaltyLeftEmDash;
   penalties[PENALTY_EM_DASH_LEFT][1] = penaltyLeftEmDash;
}

void Textblock::setPenaltyEmDashRight (int penaltyRightEmDash)
{
   penalties[PENALTY_EM_DASH_RIGHT][0] = penaltyRightEmDash;
}

void Textblock::setPenaltyEmDashRight2 (int penaltyRightEmDash2)
{
   penalties[PENALTY_EM_DASH_RIGHT][1] = penaltyRightEmDash2;
}

void Textblock::setStretchabilityFactor (int stretchabilityFactor)
{
   Textblock::stretchabilityFactor = stretchabilityFactor;
}

Textblock::Textblock (bool limitTextWidth, bool treatAsInline)
{
   DBG_OBJ_CREATE ("dw::Textblock");
   registerName ("dw::Textblock", &CLASS_ID);
   setButtonSensitive(true);

   hasListitemValue = false;
   leftInnerPadding = 0;
   line1Offset = 0;
   ignoreLine1OffsetSometimes = false;
   mustQueueResize = false;
   DBG_OBJ_SET_BOOL ("mustQueueResize", mustQueueResize);
   redrawY = 0;
   DBG_OBJ_SET_NUM ("redrawY", redrawY);
   lastWordDrawn = -1;
   DBG_OBJ_SET_NUM ("lastWordDrawn", lastWordDrawn);

   DBG_OBJ_ASSOC_CHILD (&sizeRequestParams);
        
   /*
    * The initial sizes of lines and words should not be
    * too high, since this will waste much memory with tables
    * containing many small cells. The few more calls to realloc
    * should not decrease the speed considerably.
    * (Current setting is for minimal memory usage. An interesting fact
    * is that high values decrease speed due to memory handling overhead!)
    * TODO: Some tests would be useful.
    */
   paragraphs = new misc::SimpleVector <Paragraph> (1);
   lines = new misc::SimpleVector <Line> (1);
   nonTemporaryLines = 0;
   words = new misc::NotSoSimpleVector <Word> (1);
   anchors = new misc::SimpleVector <Anchor> (1);

   wrapRefLines = wrapRefParagraphs = -1;
   wrapRefLinesFCX = wrapRefLinesFCY = -1;

   DBG_OBJ_SET_NUM ("lines.size", lines->size ());
   DBG_OBJ_SET_NUM ("words.size", words->size ());
   DBG_OBJ_SET_NUM ("wrapRefLines", wrapRefLines);
   DBG_OBJ_SET_NUM ("wrapRefParagraphs", wrapRefParagraphs);
   DBG_OBJ_SET_NUM ("wrapRefLinesFCX", wrapRefLinesFCX);
   DBG_OBJ_SET_NUM ("wrapRefLinesFCY", wrapRefLinesFCY);

   hoverLink = -1;

   // -1 means undefined.
   lineBreakWidth = -1;
   DBG_OBJ_SET_NUM ("lineBreakWidth", lineBreakWidth);

   this->limitTextWidth = limitTextWidth;
   this->treatAsInline = treatAsInline;

   for (int layer = 0; layer < core::HIGHLIGHT_NUM_LAYERS; layer++) {
      /* hlStart[layer].index > hlEnd[layer].index means no highlighting */
      hlStart[layer].index = 1;
      hlStart[layer].nChar = 0;
      hlEnd[layer].index = 0;
      hlEnd[layer].nChar = 0;

      DBG_OBJ_ARRATTRSET_NUM ("hlStart", layer, "index", hlStart[layer].index);
      DBG_OBJ_ARRATTRSET_NUM ("hlStart", layer, "nChar", hlStart[layer].nChar);
      DBG_OBJ_ARRATTRSET_NUM ("hlEnd", layer, "index", hlEnd[layer].index);
      DBG_OBJ_ARRATTRSET_NUM ("hlEnd", layer, "nChar", hlEnd[layer].nChar);
   }

   numSizeReferences = 0;

   initNewLine ();
}

Textblock::~Textblock ()
{
   /* make sure not to call a free'd tooltip (very fast overkill) */
   hoverTooltip = NULL;

   for (int i = 0; i < words->size(); i++)
      cleanupWord (i);

   for (int i = 0; i < anchors->size(); i++) {
      Anchor *anchor = anchors->getRef (i);
      /* This also frees the names (see removeAnchor() and related). */
      removeAnchor(anchor->name);
   }

   delete paragraphs;
   delete lines;
   delete words;
   delete anchors;
 
   /* Make sure we don't own widgets anymore. Necessary before call of
      parent class destructor. (???) */
   words = NULL;

   DBG_OBJ_DELETE ();
}

/**
 * The ascent of a textblock is the ascent of the first line, plus
 * padding/border/margin. This can be used to align the first lines
 * of several textblocks in a horizontal line.
 */
void Textblock::sizeRequestImpl (core::Requisition *requisition, int numPos,
                                 Widget **references, int *x, int *y)
{
   DBG_OBJ_ENTER ("resize", 0, "sizeRequestImpl", "%d, ...", numPos);
  
   sizeRequestParams.fill (numPos, references, x, y);

   // We have to rewrap the whole textblock, if (i) the available width (which
   // is the line break width) has changed, or (ii) if the position within the
   // float container, and so possibly borders relative to this textblock, have
   // changed.
   //
   // (The latter is a simplification: an over-correct implementation would test
   // all OOFMs on whether affectsLeftBorder() or affectsRightBorder() returns
   // true. Also, this may be optimized by distinguishing between floats
   // generated by this textblock (which would not make rewrapping necessary)
   // and floats generated by other textblocks (which would).)
   
   int newLineBreakWidth = getAvailWidth (true);
   int newFCX, newFCY;
   bool fcDefined = findSizeRequestReference (OOFM_FLOATS, &newFCX, &newFCY);
   
   if (newLineBreakWidth != lineBreakWidth ||
       (fcDefined && (newFCX != wrapRefLinesFCX ||
                      newFCY != wrapRefLinesFCY))) {
      lineBreakWidth = newLineBreakWidth;
      wrapRefLines = 0;
      DBG_OBJ_SET_NUM ("lineBreakWidth", lineBreakWidth);
      DBG_OBJ_SET_NUM ("wrapRefLines", wrapRefLines);

      if (!fcDefined) {
         wrapRefLinesFCX = newFCX;
         wrapRefLinesFCY = newFCY;
         DBG_OBJ_SET_NUM ("wrapRefLinesFCX", wrapRefLinesFCX);
         DBG_OBJ_SET_NUM ("wrapRefLinesFCY", wrapRefLinesFCY);
      }
   }

   rewrap ();
   showMissingLines ();

   if (lines->size () > 0) {
      Line *firstLine = lines->getRef(0), *lastLine = lines->getLastRef ();

      // Note: the breakSpace of the last line is ignored, so breaks
      // at the end of a textblock are not visible.

      requisition->width =
         lastLine->maxLineWidth + leftInnerPadding + boxDiffWidth ();

      // Also regard collapsing of this widget top margin and the top
      // margin of the first line box:
      requisition->ascent = calcVerticalBorder (getStyle()->padding.top,
                                                getStyle()->borderWidth.top,
                                                getStyle()->marginTop()
                                                + extraSpace.top,
                                                firstLine->borderAscent,
                                                firstLine->marginAscent);

      // And here, regard collapsing of this widget bottom margin and the
      // bottom margin of the last line box:
      requisition->descent =
         // (BTW, this line:
         lastLine->top - firstLine->borderAscent + lastLine->borderAscent +
         // ... is 0 for a block with one line, so special handling
         // for this case is not necessary.)
         calcVerticalBorder (getStyle()->padding.bottom,
                             getStyle()->borderWidth.bottom,
                             getStyle()->marginBottom() + extraSpace.bottom,
                             lastLine->borderDescent, lastLine->marginDescent);
   } else {
      requisition->width = leftInnerPadding + boxDiffWidth ();
      requisition->ascent = boxOffsetY ();
      requisition->descent = boxRestHeight ();
   }

   if (usesMaxGeneratorWidth ()) {
      DBG_OBJ_MSGF ("resize", 1,
                    "before considering lineBreakWidth (= %d): %d * (%d + %d)",
                    lineBreakWidth, requisition->width, requisition->ascent,
                    requisition->descent);
      if (requisition->width < lineBreakWidth)
         requisition->width = lineBreakWidth;
   } else
      DBG_OBJ_MSG ("resize", 1, "lineBreakWidth needs no consideration");

   DBG_OBJ_MSGF ("resize", 1, "before correction: %d * (%d + %d)",
                 requisition->width, requisition->ascent, requisition->descent);

   correctRequisition (requisition, core::splitHeightPreserveAscent, true,
                       false);

   // Dealing with parts out of flow, which may overlap the borders of
   // the text block. Base lines are ignored here: they do not play a
   // role (currently) and caring about them (for the future) would
   // cause too much problems.

   // Notice that the order is not typical: correctRequisition should
   // be the last call. However, calling correctRequisition after
   // outOfFlowMgr->getSize may result again in a size which is too
   // small for floats, so triggering again (and again) the resize
   // idle function resulting in CPU hogging. See also
   // getExtremesImpl.
   //
   // Is this really what we want? An alternative could be that
   // OutOfFlowMgr::getSize honours CSS attributes an corrected sizes.

   correctRequisitionByOOF (requisition, core::splitHeightPreserveAscent);

   DBG_OBJ_MSGF ("resize", 1, "final: %d * (%d + %d)",
                 requisition->width, requisition->ascent, requisition->descent);
   DBG_OBJ_LEAVE ();
}

int Textblock::numSizeRequestReferences ()
{
   return numSizeReferences;
}

core::Widget *Textblock::sizeRequestReference (int index)
{
   return sizeReferences[index];
}

int Textblock::calcVerticalBorder (int widgetPadding, int widgetBorder,
                                   int widgetMargin, int lineBorderTotal,
                                   int lineMarginTotal)
{
   DBG_OBJ_ENTER ("resize", 0, "calcVerticalBorder", "%d, %d, %d, %d, %d",
                  widgetPadding, widgetBorder, widgetMargin, lineBorderTotal,
                  lineMarginTotal);
   
   int result;
   
   if (widgetPadding == 0 && widgetBorder == 0) {
      if (lineMarginTotal - lineBorderTotal >= widgetMargin)
         result = lineMarginTotal;
      else
         result = widgetMargin + lineBorderTotal;
   } else
      result = lineMarginTotal + widgetPadding + widgetBorder + widgetMargin;

   DBG_OBJ_LEAVE_VAL ("%d", result);
   return result;
}

/**
 * Get the extremes of a word within a textblock.
 */
void Textblock::getWordExtremes (Word *word, core::Extremes *extremes)
{
   if (word->content.type == core::Content::WIDGET_IN_FLOW)
      word->content.widget->getExtremes (extremes);
   else
      extremes->minWidth = extremes->minWidthIntrinsic = extremes->maxWidth =
         extremes->maxWidthIntrinsic = extremes->adjustmentWidth =
         word->size.width;
}

void Textblock::getExtremesSimpl (core::Extremes *extremes)
{
   DBG_OBJ_ENTER0 ("resize", 0, "getExtremesSimpl");

   fillParagraphs ();

   if (paragraphs->size () == 0) {
      /* empty page */
      extremes->minWidth = 0;
      extremes->minWidthIntrinsic = 0;
      extremes->maxWidth = 0;
      extremes->maxWidthIntrinsic = 0;
      extremes->adjustmentWidth = 0;
   } else  {
      Paragraph *lastPar = paragraphs->getLastRef ();
      extremes->minWidth = lastPar->maxParMin;
      extremes->minWidthIntrinsic = lastPar->maxParMinIntrinsic;
      extremes->maxWidth = lastPar->maxParMax;
      extremes->maxWidthIntrinsic = lastPar->maxParMaxIntrinsic;
      extremes->adjustmentWidth = lastPar->maxParAdjustmentWidth;

      DBG_OBJ_MSGF ("resize", 1, "paragraphs[%d]->maxParMin = %d (%d)",
                    paragraphs->size () - 1, lastPar->maxParMin,
                    lastPar->maxParMinIntrinsic);
      DBG_OBJ_MSGF ("resize", 1, "paragraphs[%d]->maxParMax = %d (%d)",
                    paragraphs->size () - 1, lastPar->maxParMax,
                    lastPar->maxParMaxIntrinsic);
   }

   DBG_OBJ_MSGF ("resize", 0, "after considering paragraphs: %d (%d) / %d (%d)",
                 extremes->minWidth, extremes->minWidthIntrinsic,
                 extremes->maxWidth, extremes->maxWidthIntrinsic);

   int diff = leftInnerPadding + boxDiffWidth ();
   extremes->minWidth += diff;
   extremes->minWidthIntrinsic += diff;
   extremes->maxWidth += diff;
   extremes->maxWidthIntrinsic += diff;
   extremes->adjustmentWidth += diff;

   DBG_OBJ_MSGF ("resize", 0, "after adding diff: %d (%d) / %d (%d)",
                 extremes->minWidth, extremes->minWidthIntrinsic,
                 extremes->maxWidth, extremes->maxWidthIntrinsic);

   // For the order, see similar reasoning in sizeRequestImpl.

   correctExtremes (extremes, true);

   DBG_OBJ_MSGF ("resize", 0, "after correction: %d (%d) / %d (%d)",
                 extremes->minWidth, extremes->minWidthIntrinsic,
                 extremes->maxWidth, extremes->maxWidthIntrinsic);

   correctExtremesByOOF (extremes);

   DBG_OBJ_MSGF ("resize", 0,
                 "finally, after considering OOFM: %d (%d) / %d (%d)",
                 extremes->minWidth, extremes->minWidthIntrinsic,
                 extremes->maxWidth, extremes->maxWidthIntrinsic);
   DBG_OBJ_LEAVE ();
}

int Textblock::numGetExtremesReferences ()
{
   return numSizeReferences;
}

core::Widget *Textblock::getExtremesReference (int index)
{
   return sizeReferences[index];
}

void Textblock::notifySetAsTopLevel ()
{
   OOFAwareWidget::notifySetAsTopLevel ();

   numSizeReferences = 0;
   DBG_OBJ_SET_NUM ("numSizeReferences", numSizeReferences);
}

void Textblock::notifySetParent ()
{
   OOFAwareWidget::notifySetParent ();

   numSizeReferences = 0;
   for (int i = 0; i < NUM_OOFM; i++) {
      if (oofContainer[i] != this) {
         // avoid duplicates
         bool found = false;
         for (int j = 0; !found && j < numSizeReferences; j++)
            if (oofContainer[i] == oofContainer[j])
               found = true;

         if (!found)
            sizeReferences[numSizeReferences++] = oofContainer[i];
      }
   }

   DBG_OBJ_SET_NUM ("numSizeReferences", numSizeReferences);
   for (int i = 0; i < numSizeReferences; i++)
      DBG_OBJ_ARRSET_PTR ("sizeReferences", i, sizeReferences[i]);
}

void Textblock::sizeAllocateImpl (core::Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize", 0, "sizeAllocateImpl", "%d, %d; %d * (%d + %d)",
                  allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   showMissingLines ();

   sizeAllocateStart (allocation);

   int lineIndex, wordIndex;
   Line *line;
   Word *word;
   int xCursor;
   core::Allocation childAllocation;
   core::Allocation *oldChildAllocation;

   if (allocation->x != this->allocation.x ||
       allocation->y != this->allocation.y ||
       allocation->width != this->allocation.width) {
      redrawY = 0;
      DBG_OBJ_SET_NUM ("redrawY", redrawY);
   }

   DBG_OBJ_MSG_START ();

   for (lineIndex = 0; lineIndex < lines->size (); lineIndex++) {
      DBG_OBJ_MSGF ("resize", 1, "line %d", lineIndex);
      DBG_OBJ_MSG_START ();

      // Especially for floats, allocation->width may be different
      // from the line break width, so that for centered and right
      // text, the offsets have to be recalculated again. However, if
      // the allocation width is greater than the line break width,
      // due to wide unbreakable lines (large image etc.), use the
      // original line break width.
      //
      // TODO: test case?
      
      calcTextOffset (lineIndex, misc::min (allocation->width, lineBreakWidth));

      line = lines->getRef (lineIndex);
      xCursor = line->textOffset;

      DBG_OBJ_MSGF ("resize", 1, "xCursor = %d (initially)", xCursor);

      for (wordIndex = line->firstWord; wordIndex <= line->lastWord;
           wordIndex++) {
         word = words->getRef (wordIndex);

         if (wordIndex == lastWordDrawn + 1) {
            redrawY = misc::min (redrawY, lineYOffsetWidget (line, allocation));
            DBG_OBJ_SET_NUM ("redrawY", redrawY);
         }

         if (word->content.type == core::Content::WIDGET_IN_FLOW) {
            DBG_OBJ_MSGF ("resize", 1,
                          "allocating widget in flow: line %d, word %d",
                          lineIndex, wordIndex);

            // TODO For word->flags & Word::TOPLEFT_OF_LINE, make
            // allocation consistent with calcSizeOfWidgetInFlow():

            childAllocation.x = xCursor + allocation->x;
            
            /** \todo Justification within the line is done here. */
            /* align=top:
               childAllocation.y = line->top + allocation->y;
            */
            /* align=bottom (base line) */
            /* Commented lines break the n2 and n3 test cases at
             * https://dillo-browser.github.io/old/test/img/ */
            childAllocation.y = lineYOffsetCanvas (line, allocation)
               + (line->borderAscent - word->size.ascent);

            childAllocation.width = word->size.width;
            childAllocation.ascent = word->size.ascent;
            childAllocation.descent = word->size.descent;
 
            oldChildAllocation = word->content.widget->getAllocation();

            if (childAllocation.x != oldChildAllocation->x ||
               childAllocation.y != oldChildAllocation->y ||
               childAllocation.width != oldChildAllocation->width) {
               /* The child widget has changed its position or its width
                * so we need to redraw from this line onwards.
                */
               redrawY =
                  misc::min (redrawY, lineYOffsetWidget (line, allocation));
               DBG_OBJ_SET_NUM ("redrawY", redrawY);
               if (word->content.widget->wasAllocated ()) {
                  redrawY = misc::min (redrawY,
                     oldChildAllocation->y - this->allocation.y);
                  DBG_OBJ_SET_NUM ("redrawY", redrawY);
               }

            } else if (childAllocation.ascent + childAllocation.descent !=
               oldChildAllocation->ascent + oldChildAllocation->descent) {
               /* The child widget has changed its height. We need to redraw
                * from where it changed.
                * It's important not to draw from the line base, because the
                * child might be a table covering the whole page so we would
                * end up redrawing the whole screen over and over.
                * The drawing of the child content is left to the child itself.
                * However this optimization is only possible if the widget is
                * the only word in the line apart from an optional BREAK.
                * Otherwise the height change of the widget could change the
                * position of other words in the line, requiring a
                * redraw of the complete line.
                */
               if (line->lastWord == line->firstWord ||
                   (line->lastWord == line->firstWord + 1 &&
                    words->getRef (line->lastWord)->content.type ==
                    core::Content::BREAK)) {

                  int childChangedY =
                     misc::min(childAllocation.y - allocation->y +
                        childAllocation.ascent + childAllocation.descent,
                        oldChildAllocation->y - this->allocation.y +
                        oldChildAllocation->ascent +
                        oldChildAllocation->descent);

                  redrawY = misc::min (redrawY, childChangedY);
                  DBG_OBJ_SET_NUM ("redrawY", redrawY);
               } else {
                  redrawY =
                     misc::min (redrawY, lineYOffsetWidget (line, allocation));
                  DBG_OBJ_SET_NUM ("redrawY", redrawY);
               }
            }
            word->content.widget->sizeAllocate (&childAllocation);
         }

         xCursor += (word->size.width + word->effSpace);
         DBG_OBJ_MSGF ("resize", 1, "xCursor = %d (after word %d)",
                       xCursor, wordIndex);
         DBG_MSG_WORD ("resize", 1, "<i>that is:</i> ", wordIndex, "");
      }

      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_MSG_END ();

   sizeAllocateEnd ();
      
   for (int i = 0; i < anchors->size(); i++) {
      Anchor *anchor = anchors->getRef(i);
      int y;

      if (anchor->wordIndex >= words->size() ||
          // Also regard not-yet-existing lines.
          lines->size () <= 0 ||
          anchor->wordIndex > lines->getLastRef()->lastWord) {
         y = allocation->y + allocation->ascent + allocation->descent;
      } else {
         Line *line = lines->getRef(findLineOfWord (anchor->wordIndex));
         y = lineYOffsetCanvas (line, allocation);
      }
      changeAnchor (anchor->name, y);
   }

   DBG_OBJ_LEAVE ();
}

void Textblock::calcExtraSpaceImpl (int numPos, Widget **references, int *x,
                                    int *y)
{
   DBG_OBJ_ENTER0 ("resize", 0, "Textblock::calcExtraSpaceImpl");

   sizeRequestParams.fill (numPos, references, x, y);

   OOFAwareWidget::calcExtraSpaceImpl (numPos, references, x, y);

   int clearPosition = 0;
   for (int i = 0; i < NUM_OOFM; i++)
      if (searchOutOfFlowMgr (i) && findSizeRequestReference (i, NULL, NULL))
         clearPosition =
            misc::max (clearPosition,
                       searchOutOfFlowMgr(i)->getClearPosition (this));
   
   extraSpace.top = misc::max (extraSpace.top, clearPosition);

   DBG_OBJ_LEAVE ();
}

int Textblock::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "Textblock::getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;

   if (isWidgetOOF (child) && getWidgetOutOfFlowMgr(child) &&
       getWidgetOutOfFlowMgr(child)->dealingWithSizeOfChild (child))
      width =
         getWidgetOutOfFlowMgr(child)->getAvailWidthOfChild (child,forceValue);
   else {
      if (child->getStyle()->width == core::style::LENGTH_AUTO) {
         // No width specified: similar to standard implementation (see
         // there), but "leftInnerPadding" has to be considered, too.
         DBG_OBJ_MSG ("resize", 1, "no specification");
         if (forceValue) {
            width = misc::max (getAvailWidth (true) - boxDiffWidth ()
                               - leftInnerPadding,
                               0);

            if (width != -1) {
               /* Clamp to min-width and max-width if given, taking into
                * account leftInnerPadding. */
               int maxWidth = child->calcWidth (child->getStyle()->maxWidth,
                     -1, this, -1, false).total;
               if (maxWidth != -1 && width > maxWidth - leftInnerPadding)
                  width = maxWidth - leftInnerPadding;

               int minWidth = child->calcWidth (child->getStyle()->minWidth,
                     -1, this, -1, false).total;
               if (minWidth != -1 && width < minWidth - leftInnerPadding)
                  width = minWidth - leftInnerPadding;
            }

         } else {
            width = -1;
         }
      } else
         width = Widget::getAvailWidthOfChild (child, forceValue);
      
      if (forceValue && this == child->getContainer () &&
          !usesMaxGeneratorWidth ()) {
         core::Extremes extremes;
         getExtremes (&extremes);
         if (width > extremes.maxWidth - boxDiffWidth () - leftInnerPadding)
            width = extremes.maxWidth - boxDiffWidth () - leftInnerPadding;
      }
   }

   DBG_OBJ_LEAVE_VAL ("%d", width);
   return width;
}

int Textblock::getAvailHeightOfChild (core::Widget *child, bool forceValue)
{
   if (isWidgetOOF(child) && getWidgetOutOfFlowMgr(child) &&
       getWidgetOutOfFlowMgr(child)->dealingWithSizeOfChild (child))
      return getWidgetOutOfFlowMgr(child)->getAvailHeightOfChild (child,
                                                                  forceValue);
   else
      return Widget::getAvailHeightOfChild (child, forceValue);
}

void Textblock::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int i = 0; i < words->size (); i++) {
      Word *word = words->getRef (i);
      if (word->content.type == core::Content::WIDGET_IN_FLOW)
         word->content.widget->containerSizeChanged ();
   }

   containerSizeChangedForChildrenOOF ();
   
   DBG_OBJ_LEAVE ();
}

bool Textblock::affectsSizeChangeContainerChild (Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0,
                  "Textblock/affectsSizeChangeContainerChild", "%p", child);

   // See Textblock::getAvailWidthOfChild() and Textblock::oofSizeChanged():
   // Extremes changes affect the size of the child, too:
   bool ret;
   if (!usesMaxGeneratorWidth () &&
       (extremesQueued () || extremesChanged ()))
      ret = true;
   else
      ret = Widget::affectsSizeChangeContainerChild (child);

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(ret));
   return ret;
}

bool Textblock::usesAvailWidth ()
{
   return true;
}

void Textblock::resizeDrawImpl ()
{
   DBG_OBJ_ENTER0 ("draw", 0, "resizeDrawImpl");

   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   if (lines->size () > 0) {
      Line *lastLine = lines->getRef (lines->size () - 1);
      /* Remember the last word that has been drawn so we can ensure to
       * draw any new added words (see sizeAllocateImpl()).
       */
      lastWordDrawn = lastLine->lastWord;
      DBG_OBJ_SET_NUM ("lastWordDrawn", lastWordDrawn);
   }

   redrawY = getHeight ();
   DBG_OBJ_SET_NUM ("redrawY", redrawY);

   DBG_OBJ_LEAVE ();
}

void Textblock::markSizeChange (int ref)
{
   DBG_OBJ_ENTER ("resize", 0, "markSizeChange", "%d", ref);

   if (isParentRefOOF (ref))
      getParentRefOutOfFlowMgr(ref)
         ->markSizeChange (getParentRefOOFSubRef (ref));
   else {
      /* By the way: ref == -1 may have two different causes: (i) flush()
         calls "queueResize (-1, true)", when no rewrapping is necessary;
         and (ii) a word may have parentRef == -1 , when it is not yet
         added to a line.  In the latter case, nothing has to be done
         now, but addLine(...) will do everything necessary. */
      if (ref != -1) {
         if (wrapRefLines == -1)
            wrapRefLines = getParentRefInFlowSubRef (ref);
         else
            wrapRefLines = misc::min (wrapRefLines,
                                      getParentRefInFlowSubRef (ref));
      }

      DBG_OBJ_SET_NUM ("wrapRefLines", wrapRefLines);

      // It seems that sometimes (even without floats) the lines
      // structure is changed, so that wrapRefLines may refers to a
      // line which does not exist anymore. Should be examined
      // again. Until then, setting wrapRefLines to the same value is
      // a workaround.
      markExtremesChange (ref);
   }

   DBG_OBJ_LEAVE ();
}

void Textblock::markExtremesChange (int ref)
{
   DBG_OBJ_ENTER ("resize", 1, "markExtremesChange", "%d", ref);

   if (isParentRefOOF (ref))
      getParentRefOutOfFlowMgr(ref)
         ->markExtremesChange (getParentRefOOFSubRef (ref));
   else {
      /* By the way: ref == -1 may have two different causes: (i) flush()
         calls "queueResize (-1, true)", when no rewrapping is necessary;
         and (ii) a word may have parentRef == -1 , when it is not yet
         added to a line.  In the latter case, nothing has to be done
         now, but addLine(...) will do everything necessary. */
      if (ref != -1) {
         if (wrapRefParagraphs == -1)
            wrapRefParagraphs = getParentRefInFlowSubRef (ref);
         else
            wrapRefParagraphs =
               misc::min (wrapRefParagraphs, getParentRefInFlowSubRef (ref));
      }

      DBG_OBJ_SET_NUM ("wrapRefParagraphs", wrapRefParagraphs);
   }

   DBG_OBJ_LEAVE ();
}

bool Textblock::isBlockLevel ()
{
   return true;
}

bool Textblock::buttonPressImpl (core::EventButton *event)
{
   return sendSelectionEvent (core::SelectionState::BUTTON_PRESS, event);
}

bool Textblock::buttonReleaseImpl (core::EventButton *event)
{
   return sendSelectionEvent (core::SelectionState::BUTTON_RELEASE, event);
}

/*
 * Handle motion inside the widget
 * (special care is necessary when switching from another widget,
 *  because hoverLink and hoverTooltip are meaningless then).
 */
bool Textblock::motionNotifyImpl (core::EventMotion *event)
{
   if (event->state & core::BUTTON1_MASK)
      return sendSelectionEvent (core::SelectionState::BUTTON_MOTION, event);
   else {
      bool inSpace;
      int linkOld = hoverLink;
      core::style::Tooltip *tooltipOld = hoverTooltip;
      const Word *word = findWord (event->xWidget, event->yWidget, &inSpace);

      // cursor from word or widget style
      if (word == NULL) {
         setCursor (getStyle()->cursor);
         hoverLink = -1;
         hoverTooltip = NULL;
      } else {
         core::style::Style *style = inSpace ? word->spaceStyle : word->style;
         setCursor (style->cursor);
         hoverLink = style->x_link;
         hoverTooltip = style->x_tooltip;
      }

      // Show/hide tooltip
      if (tooltipOld != hoverTooltip) {
         if (tooltipOld)
            tooltipOld->onLeave ();
         if (hoverTooltip)
            hoverTooltip->onEnter ();
      } else if (hoverTooltip)
         hoverTooltip->onMotion ();

      _MSG("MN tb=%p tooltipOld=%p hoverTooltip=%p\n",
          this, tooltipOld, hoverTooltip);
      if (hoverLink != linkOld) {
         /* LinkEnter with hoverLink == -1 is the same as LinkLeave */
         return layout->emitLinkEnter (this, hoverLink, -1, -1, -1);
      } else {
         return hoverLink != -1;
      }
   }
}

void Textblock::enterNotifyImpl (core::EventCrossing *event)
{
   _MSG(" tb=%p, ENTER NotifyImpl hoverTooltip=%p\n", this, hoverTooltip);
   /* reset hoverLink so linkEnter is detected */
   hoverLink = -2;
}

void Textblock::leaveNotifyImpl (core::EventCrossing *event)
{
   _MSG(" tb=%p, LEAVE NotifyImpl: hoverTooltip=%p\n", this, hoverTooltip);

   /* leaving the viewport can't be handled by motionNotifyImpl() */
   if (hoverLink >= 0)
      layout->emitLinkEnter (this, -1, -1, -1, -1);

   if (hoverTooltip) {
      hoverTooltip->onLeave();
      hoverTooltip = NULL;
   }
}

/**
 * \brief Send event to selection.
 */
bool Textblock::sendSelectionEvent (core::SelectionState::EventType eventType,
                                    core::MousePositionEvent *event)
{
   DBG_OBJ_ENTER0 ("events", 0, "sendSelectionEvent");

   core::Iterator *it;
   int wordIndex;
   int charPos = 0, link = -1;
   bool r;

   if (words->size () == 0) {
      wordIndex = -1;
   } else {
      Line *lastLine = lines->getRef (lines->size () - 1);
      int yFirst = lineYOffsetCanvas (0);
      int yLast = lineYOffsetCanvas (lastLine) + lastLine->borderAscent +
                  lastLine->borderDescent;
      if (event->yCanvas < yFirst) {
         // Above the first line: take the first word.
         wordIndex = 0;
      } else if (event->yCanvas >= yLast) {
         // Below the last line: take the last word.
         wordIndex = words->size () - 1;
         charPos = core::SelectionState::END_OF_WORD;
      } else {
         Line *line =
            lines->getRef (findLineIndexWhenAllocated (event->yWidget));

         // Pointer within the break space?
         if (event->yWidget > (lineYOffsetWidget (line) +
                               line->borderAscent + line->borderDescent)) {
            // Choose this break.
            wordIndex = line->lastWord;
            charPos = core::SelectionState::END_OF_WORD;
         } else if (event->xWidget < line->textOffset) {
            // Left of the first word in the line.
            wordIndex = line->firstWord;
         } else {
            int nextWordStartX = line->textOffset;

            for (wordIndex = line->firstWord;
                 wordIndex <= line->lastWord;
                 wordIndex++) {
               Word *word = words->getRef (wordIndex);
               int wordStartX = nextWordStartX;

               nextWordStartX += word->size.width + word->effSpace;

               if (event->xWidget >= wordStartX &&
                   event->xWidget < nextWordStartX) {
                  // We have found the word.
                  int yWidgetBase =
                     lineYOffsetWidget (line) + line->borderAscent;

                  if (event->xWidget >= nextWordStartX  - word->effSpace) {
                     charPos = core::SelectionState::END_OF_WORD;
                     if (wordIndex < line->lastWord &&
                         (words->getRef(wordIndex + 1)->content.type !=
                          core::Content::BREAK) &&
                         (event->yWidget <=
                          yWidgetBase + word->spaceStyle->font->descent) &&
                         (event->yWidget >
                          yWidgetBase - word->spaceStyle->font->ascent)) {
                        link = word->spaceStyle->x_link;
                     }
                  } else {
                     if (event->yWidget <= yWidgetBase + word->size.descent &&
                         event->yWidget > yWidgetBase - word->size.ascent) {
                        link = word->style->x_link;
                     }
                     if (word->content.type == core::Content::TEXT) {
                        int glyphX = wordStartX;
                        int isStartWord = word->flags & Word::WORD_START;
                        int isEndWord = word->flags & Word::WORD_END;

                        while (1) {
                           int nextCharPos =
                              layout->nextGlyph (word->content.text, charPos);
                           // TODO The width of a text is not the sum
                           // of the widths of the glyphs, because of
                           // ligatures, kerning etc., so textWidth
                           // should be applied to the text from 0 to
                           // nextCharPos. (Or not? See comment below.)

                           int glyphWidth =
                              textWidth (word->content.text, charPos,
                                         nextCharPos - charPos, word->style,
                                         isStartWord && charPos == 0,
                                         isEndWord &&
                                         word->content.text[nextCharPos] == 0);
                           if (event->xWidget > glyphX + glyphWidth) {
                              glyphX += glyphWidth;
                              charPos = nextCharPos;
                              continue;
                           } else if (event->xWidget >= glyphX + glyphWidth/2){
                              // On the right half of a character;
                              // now just look for combining chars
                              charPos = nextCharPos;
                              while (word->content.text[charPos]) {
                                 nextCharPos =
                                    layout->nextGlyph (word->content.text,
                                                       charPos);
                                 if (textWidth (word->content.text, charPos,
                                                nextCharPos - charPos,
                                                word->style,
                                                isStartWord && charPos == 0,
                                                isEndWord &&
                                                word->content.text[nextCharPos]
                                                == 0))
                                    break;
                                 charPos = nextCharPos;
                              }
                           }
                           break;
                        }
                     } else {
                        // Depends on whether the pointer is within the left or
                        // right half of the (non-text) word.
                        if (event->xWidget >= (wordStartX + nextWordStartX) /2)
                           charPos = core::SelectionState::END_OF_WORD;
                     }
                  }
                  break;
               }
            }
            if (wordIndex > line->lastWord) {
               // No word found in this line (i.e. we are on the right side),
               // take the last of this line.
               wordIndex = line->lastWord;
               charPos = core::SelectionState::END_OF_WORD;
            }
         }
      }
   }
   
   DBG_OBJ_MSGF ("events", 1, "wordIndex = %d", wordIndex);
   DBG_MSG_WORD ("events", 1, "<i>this is:</i> ", wordIndex, "");
      
   it = TextblockIterator::createWordIndexIterator
           (this, core::Content::maskForSelection (true), wordIndex);
   r = selectionHandleEvent (eventType, it, charPos, link, event);
   it->unref ();

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(r));
   return r;
}

void Textblock::removeChild (Widget *child)
{
   /** \bug Not implemented. */
}

core::Iterator *Textblock::iterator (core::Content::Type mask, bool atEnd)
{
   return new TextblockIterator (this, mask, atEnd);
}

/*
 * Draw the decorations on a word.
 */
void Textblock::decorateText (core::View *view, core::style::Style *style,
                              core::style::Color::Shading shading,
                              int x, int yBase, int width)
{
   int y, height;

   height = 1 + style->font->xHeight / 12;
   if (style->textDecoration & core::style::TEXT_DECORATION_UNDERLINE) {
      y = yBase + style->font->descent / 3;
      view->drawRectangle (style->color, shading, true, x, y, width, height);
   }
   if (style->textDecoration & core::style::TEXT_DECORATION_OVERLINE) {
      y = yBase - style->font->ascent;
      view->drawRectangle (style->color, shading, true, x, y, width, height);
   }
   if (style->textDecoration & core::style::TEXT_DECORATION_LINE_THROUGH) {
      y = yBase + (style->font->descent - style->font->ascent) / 2 +
          style->font->descent / 4;
      view->drawRectangle (style->color, shading, true, x, y, width, height);
   }
}

/*
 * Draw a string of text
 *
 * Arguments: ... "isStart" and "isEnd" are true, when the text
 * start/end represents the start/end of a "real" text word (before
 * hyphenation). This has an effect on text transformation. ("isEnd"
 * is not used yet, but here for symmetry.)
 */
void Textblock::drawText(core::View *view, core::style::Style *style,
                         core::style::Color::Shading shading, int x, int y,
                         const char *text, int start, int len, bool isStart,
                         bool isEnd)
{
   if (len > 0) {
      char *str = NULL;

      switch (style->textTransform) {
         case core::style::TEXT_TRANSFORM_NONE:
         default:
            break;
         case core::style::TEXT_TRANSFORM_UPPERCASE:
            str = layout->textToUpper(text + start, len);
            break;
         case core::style::TEXT_TRANSFORM_LOWERCASE:
            str = layout->textToLower(text + start, len);
            break;
         case core::style::TEXT_TRANSFORM_CAPITALIZE:
            // If "isStart" is false, the first letter of "text" is
            // not the first letter of the "real" text word, so no
            // transformation is necessary.
            if (isStart) {
               /* \bug No way to know about non-ASCII punctuation. */
               bool initial_seen = false;

               for (int i = 0; i < start; i++)
                  if (!ispunct(text[i]))
                     initial_seen = true;
               if (initial_seen)
                  break;

               int after = 0;
               text += start;
               while (ispunct(text[after]))
                  after++;
               if (text[after])
                  after = layout->nextGlyph(text, after);
               if (after > len)
                  after = len;

               char *initial = layout->textToUpper(text, after);
               int newlen = strlen(initial) + len-after;
               str = (char *)malloc(newlen + 1);
               strcpy(str, initial);
               strncpy(str + strlen(str), text+after, len-after);
               str[newlen] = '\0';
               free(initial);
            }
            break;
      }

      view->drawText(style->font, style->color, shading, x, y,
                     str ? str : text + start, str ? strlen(str) : len);
      if (str)
         free(str);
   }
}

/**
 * Draw a word of text.
 *
 * Since hyphenated words consist of multiple instance of
 * dw::Textblock::Word, which should be drawn as a whole (to preserve
 * kerning etc.; see \ref dw-line-breaking), two indices are
 * passed. See also drawLine(), where drawWord() is called.
 */
void Textblock::drawWord (Line *line, int wordIndex1, int wordIndex2,
                          core::View *view, core::Rectangle *area,
                          int xWidget, int yWidgetBase)
{
   core::style::Style *style = words->getRef(wordIndex1)->style;
   bool drawHyphen = wordIndex2 == line->lastWord
      && (words->getRef(wordIndex2)->flags & Word::DIV_CHAR_AT_EOL);

   if (style->hasBackground ()) {
      int w = 0;
      for (int i = wordIndex1; i <= wordIndex2; i++)
         w += words->getRef(i)->size.width;
      w += words->getRef(wordIndex2)->hyphenWidth;
      drawBox (view, style, area, xWidget, yWidgetBase - line->borderAscent,
               w, line->borderAscent + line->borderDescent, false);
   }

   if (wordIndex1 == wordIndex2 && !drawHyphen) {
      // Simple case, where copying in one buffer is not needed.
      Word *word = words->getRef (wordIndex1);
      drawWord0 (wordIndex1, wordIndex2, word->content.text, word->size.width,
                 false, style, view, area, xWidget, yWidgetBase);
   } else {
      // Concatenate all words in a new buffer.
      int l = 0, totalWidth = 0;
      for (int i = wordIndex1; i <= wordIndex2; i++) {
         Word *w = words->getRef (i);
         l += strlen (w->content.text);
         totalWidth += w->size.width;
      }

      char *text = new char[l + (drawHyphen ? strlen (hyphenDrawChar) : 0) + 1];
      int p = 0;
      for (int i = wordIndex1; i <= wordIndex2; i++) {
         const char * t = words->getRef(i)->content.text;
         strcpy (text + p, t);
         p += strlen (t);
      }

      if(drawHyphen) {
         for (int i = 0; hyphenDrawChar[i]; i++)
            text[p++] = hyphenDrawChar[i];
         text[p++] = 0;
      }

      drawWord0 (wordIndex1, wordIndex2, text, totalWidth, drawHyphen,
                 style, view, area, xWidget, yWidgetBase);

      delete[] text;
   }
}

/**
 * TODO Comment
 */
void Textblock::drawWord0 (int wordIndex1, int wordIndex2,
                           const char *text, int totalWidth, bool drawHyphen,
                           core::style::Style *style, core::View *view,
                           core::Rectangle *area, int xWidget, int yWidgetBase)
{
   int xWorld = allocation.x + xWidget;
   int yWorldBase;

   /* Adjust the text baseline if the word is <SUP>-ed or <SUB>-ed. */
   if (style->valign == core::style::VALIGN_SUB)
      yWidgetBase += style->font->ascent / 3;
   else if (style->valign == core::style::VALIGN_SUPER) {
      yWidgetBase -= style->font->ascent / 2;
   }
   yWorldBase = yWidgetBase + allocation.y;

   bool isStartTotal = words->getRef(wordIndex1)->flags & Word::WORD_START;
   bool isEndTotal = words->getRef(wordIndex2)->flags & Word::WORD_START;
   drawText (view, style, core::style::Color::SHADING_NORMAL, xWorld,
             yWorldBase, text, 0, strlen (text), isStartTotal, isEndTotal);

   if (style->textDecoration)
      decorateText(view, style, core::style::Color::SHADING_NORMAL, xWorld,
                   yWorldBase, totalWidth);

   for (int layer = 0; layer < core::HIGHLIGHT_NUM_LAYERS; layer++) {
      if (wordIndex1 <= hlEnd[layer].index &&
          wordIndex2 >= hlStart[layer].index) {
         const int wordLen = strlen (text);
         int xStart, width;
         int firstCharIdx;
         int lastCharIdx;

         if (hlStart[layer].index < wordIndex1)
            firstCharIdx = 0;
         else {
            firstCharIdx =
               misc::min (hlStart[layer].nChar,
                          (int)strlen (words->getRef(hlStart[layer].index)
                                       ->content.text));
            for (int i = wordIndex1; i < hlStart[layer].index; i++)
               // It can be assumed that all words from wordIndex1 to
               // wordIndex2 have content type TEXT.
               firstCharIdx += strlen (words->getRef(i)->content.text);
         }

         if (hlEnd[layer].index > wordIndex2)
            lastCharIdx = wordLen;
         else {
            lastCharIdx =
               misc::min (hlEnd[layer].nChar,
                          (int)strlen (words->getRef(hlEnd[layer].index)
                                       ->content.text));
            for (int i = wordIndex1; i < hlEnd[layer].index; i++)
               // It can be assumed that all words from wordIndex1 to
               // wordIndex2 have content type TEXT.
               lastCharIdx += strlen (words->getRef(i)->content.text);
         }

         xStart = xWorld;
         if (firstCharIdx)
            xStart += textWidth (text, 0, firstCharIdx, style,
                                 isStartTotal,
                                 isEndTotal && text[firstCharIdx] == 0);
         // With a hyphen, the width is a bit longer than totalWidth,
         // and so, the optimization to use totalWidth is not correct.
         if (!drawHyphen && firstCharIdx == 0 && lastCharIdx == wordLen)
            width = totalWidth;
         else
            width = textWidth (text, firstCharIdx,
                               lastCharIdx - firstCharIdx, style,
                               isStartTotal && firstCharIdx == 0,
                               isEndTotal && text[lastCharIdx] == 0);
         if (width > 0) {
            /* Highlight text */
            core::style::Color *wordBgColor;

            if (!(wordBgColor =  style->backgroundColor))
               wordBgColor = getBgColor();

            /* Draw background for highlighted text. */
            view->drawRectangle (
               wordBgColor, core::style::Color::SHADING_INVERSE, true, xStart,
               yWorldBase - style->font->ascent, width,
               style->font->ascent + style->font->descent);

            /* Highlight the text. */
            drawText (view, style, core::style::Color::SHADING_INVERSE, xStart,
                      yWorldBase, text, firstCharIdx,
                      lastCharIdx - firstCharIdx,
                      isStartTotal && firstCharIdx == 0,
                      isEndTotal && lastCharIdx == wordLen);

            if (style->textDecoration)
               decorateText(view, style, core::style::Color::SHADING_INVERSE,
                            xStart, yWorldBase, width);
         }
      }
   }
}

/*
 * Draw a space.
 */
void Textblock::drawSpace(int wordIndex, core::View *view,
                          core::Rectangle *area, int xWidget, int yWidgetBase)
{
   Word *word = words->getRef(wordIndex);
   int xWorld = allocation.x + xWidget;
   int yWorldBase;
   core::style::Style *style = word->spaceStyle;
   bool highlight = false;

   /* Adjust the space baseline if it is <SUP>-ed or <SUB>-ed */
   if (style->valign == core::style::VALIGN_SUB)
      yWidgetBase += style->font->ascent / 3;
   else if (style->valign == core::style::VALIGN_SUPER) {
      yWidgetBase -= style->font->ascent / 2;
   }
   yWorldBase = allocation.y + yWidgetBase;

   for (int layer = 0; layer < core::HIGHLIGHT_NUM_LAYERS; layer++) {
      if (hlStart[layer].index <= wordIndex &&
          hlEnd[layer].index > wordIndex) {
         highlight = true;
         break;
      }
   }
   if (highlight) {
      core::style::Color *spaceBgColor;

      if (!(spaceBgColor = style->backgroundColor))
         spaceBgColor = getBgColor();

      view->drawRectangle (
         spaceBgColor, core::style::Color::SHADING_INVERSE, true, xWorld,
         yWorldBase - style->font->ascent, word->effSpace,
         style->font->ascent + style->font->descent);
   }
   if (style->textDecoration) {
      core::style::Color::Shading shading = highlight ?
         core::style::Color::SHADING_INVERSE :
         core::style::Color::SHADING_NORMAL;

      decorateText(view, style, shading, xWorld, yWorldBase, word->effSpace);
   }
}

/*
 * Paint a line
 * - x and y are toplevel dw coordinates (Question: what Dw? Changed. Test!)
 * - area is used always (ev. set it to event->area)
 * - event is only used when is_expose
 */
void Textblock::drawLine (Line *line, core::View *view, core::Rectangle *area,
                          core::DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "drawLine", "..., [%d, %d, %d * %d]",
                  area->x, area->y, area->width, area->height);
  
   int xWidget = line->textOffset;
   int yWidgetBase = lineYOffsetWidget (line) + line->borderAscent;

   DBG_OBJ_MSGF ("draw", 1, "line from %d to %d (%d words), at (%d, %d)",
                 line->firstWord, line->lastWord, words->size (),
                 xWidget, yWidgetBase);
   DBG_MSG_WORD ("draw", 1, "<i>line starts with: </i>", line->firstWord, "");
   DBG_MSG_WORD ("draw", 1, "<i>line ends with: </i>", line->lastWord, "");

   DBG_OBJ_MSG_START ();

   for (int wordIndex = line->firstWord;
        wordIndex <= line->lastWord && xWidget < area->x + area->width;
        wordIndex++) {
      DBG_MSG_WORD ("draw", 2, "<i>drawing: </i>", wordIndex, "");

      Word *word = words->getRef (wordIndex);
      int wordSize = word->size.width;

      if (xWidget + wordSize + word->hyphenWidth + word->effSpace >= area->x) {
         if (word->content.type == core::Content::TEXT ||
             word->content.type == core::Content::WIDGET_IN_FLOW) {

            if (word->size.width > 0) {
               if (word->content.type == core::Content::WIDGET_IN_FLOW) {
                  core::Widget *child = word->content.widget;
                  core::Rectangle childArea;
                  if (!core::StackingContextMgr::handledByStackingContextMgr
                          (child) &&
                      child->intersects (this, area, &childArea))
                     child->draw (view, &childArea, context);
               } else {
                  int wordIndex2 = wordIndex;
                  while (wordIndex2 < line->lastWord &&
                         (words->getRef(wordIndex2)->flags
                          & Word::DRAW_AS_ONE_TEXT) &&
                         word->style == words->getRef(wordIndex2 + 1)->style)
                     wordIndex2++;

                  drawWord(line, wordIndex, wordIndex2, view, area,
                           xWidget, yWidgetBase);
                  wordSize = 0;
                  for (int i = wordIndex; i <= wordIndex2; i++)
                     wordSize += words->getRef(i)->size.width;

                  wordIndex = wordIndex2;
                  word = words->getRef(wordIndex);
               }
            }

            if (word->effSpace > 0 && wordIndex < line->lastWord &&
                words->getRef(wordIndex + 1)->content.type !=
                                                        core::Content::BREAK) {
               if (word->spaceStyle->hasBackground ())
                  drawBox (view, word->spaceStyle, area,
                           xWidget + wordSize,
                           yWidgetBase - line->borderAscent, word->effSpace,
                           line->borderAscent + line->borderDescent, false);
               drawSpace (wordIndex, view, area, xWidget + wordSize,
                          yWidgetBase);
            }

         }
      }
      xWidget += wordSize + word->effSpace;
   }

   DBG_OBJ_MSG_END ();

   DBG_OBJ_LEAVE ();
}

/**
 * Find the first line index that includes y, which is given in widget
 * coordinates.
 */
int Textblock::findLineIndex (int y)
{
   return wasAllocated () ?
      findLineIndexWhenAllocated (y) : findLineIndexWhenNotAllocated (y);
}

int Textblock::findLineIndexWhenNotAllocated (int y)
{
   if (lines->size() == 0)
      return -1;
   else
      return
         findLineIndex (y, calcVerticalBorder (getStyle()->padding.top,
                                               getStyle()->borderWidth.top,
                                               getStyle()->marginTop()
                                               + extraSpace.top,
                                               lines->getRef(0)->borderAscent,
                                               lines->getRef(0)->marginAscent));
}

int Textblock::findLineIndexWhenAllocated (int y)
{
   assert (wasAllocated ());
   return findLineIndex (y, allocation.ascent);
}

int Textblock::findLineIndex (int y, int ascent)
{
   DBG_OBJ_ENTER ("events", 0, "findLineIndex", "%d, %d", y, ascent);

   core::Allocation alloc;
   alloc.ascent = ascent; // More is not needed.

   int maxIndex = lines->size () - 1;
   int step, index, low = 0;

   step = (lines->size() + 1) >> 1;
   while ( step > 1 ) {
      index = low + step;
      if (index <= maxIndex && lineYOffsetWidget (index, &alloc) <= y)
         low = index;
      step = (step + 1) >> 1;
   }

   if (low < maxIndex && lineYOffsetWidget (low + 1, &alloc) <= y)
      low++;

   /*
    * This new routine returns the line number between (top) and
    * (top + size.ascent + size.descent + breakSpace): the space
    * _below_ the line is considered part of the line.  Old routine
    * returned line number between (top - previous_line->breakSpace)
    * and (top + size.ascent + size.descent): the space _above_ the
    * line was considered part of the line.  This is important for
    * Dw_page_find_link() --EG
    * That function has now been inlined into Dw_page_motion_notify() --JV
    */

   DBG_OBJ_LEAVE_VAL ("%d", low);
   return low;
}

/**
 * \brief Find the line of word \em wordIndex.
 */
int Textblock::findLineOfWord (int wordIndex)
{
   if (wordIndex < 0 || wordIndex >= words->size () ||
       // Also regard not-yet-existing lines.
       lines->size () <= 0 || wordIndex > lines->getLastRef()->lastWord)
      return -1;

   int high = lines->size () - 1, index, low = 0;

   while (true) {
      index = (low + high) / 2;
      if (wordIndex >= lines->getRef(index)->firstWord) {
         if (wordIndex <= lines->getRef(index)->lastWord)
            return index;
         else
            low = index + 1;
      } else
         high = index - 1;
   }
}

/**
 * \brief Find the paragraph of word \em wordIndex.
 */
int Textblock::findParagraphOfWord (int wordIndex)
{
   int high = paragraphs->size () - 1, index, low = 0;

   if (wordIndex < 0 || wordIndex >= words->size () ||
       // It may be that the paragraphs list is incomplete. But look
       // also at fillParagraphs, where this method is called.
       (paragraphs->size () == 0 ||
        wordIndex > paragraphs->getLastRef()->lastWord))
      return -1;

   while (true) {
      index = (low + high) / 2;
      if (wordIndex >= paragraphs->getRef(index)->firstWord) {
         if (wordIndex <= paragraphs->getRef(index)->lastWord)
            return index;
         else
            low = index + 1;
      } else
         high = index - 1;
   }
}

/**
 * \brief Find the index of the word, or -1.
 */
Textblock::Word *Textblock::findWord (int x, int y, bool *inSpace)
{
   int lineIndex, wordIndex;
   int xCursor, lastXCursor, yWidgetBase;
   Line *line;
   Word *word;

   *inSpace = false;

   if ((lineIndex = findLineIndexWhenAllocated (y)) >= lines->size ())
      return NULL;
   line = lines->getRef (lineIndex);
   yWidgetBase = lineYOffsetWidget (line) + line->borderAscent;
   if (yWidgetBase + line->borderDescent <= y)
      return NULL;

   xCursor = line->textOffset;
   for (wordIndex = line->firstWord; wordIndex <= line->lastWord;wordIndex++) {
      word = words->getRef (wordIndex);
      lastXCursor = xCursor;
      xCursor += word->size.width + word->effSpace;
      if (lastXCursor <= x && xCursor > x) {
         if (x >= xCursor - word->effSpace) {
            if (wordIndex < line->lastWord &&
                (words->getRef(wordIndex + 1)->content.type !=
                 core::Content::BREAK) &&
                y > yWidgetBase - word->spaceStyle->font->ascent &&
                y <= yWidgetBase + word->spaceStyle->font->descent) {
               *inSpace = true;
               return word;
            }
         } else {
            if (y > yWidgetBase - word->size.ascent &&
                y <= yWidgetBase + word->size.descent)
               return word;
         }
         break;
      }
   }

   return NULL;
}

void Textblock::drawLevel (core::View *view, core::Rectangle *area,
                           int level, core::DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "Textblock::drawLevel", "(%d, %d, %d * %d), %s",
                  area->x, area->y, area->width, area->height,
                  stackingLevelText (level));

   switch (level) {
   case SL_IN_FLOW:
      for (int lineIndex = findLineIndexWhenAllocated (area->y);
           lineIndex < lines->size (); lineIndex++) {
         Line *line = lines->getRef (lineIndex);
         if (lineYOffsetWidget (line) >= area->y + area->height)
            break;

         DBG_OBJ_MSGF ("draw", 0, "line %d (of %d)", lineIndex, lines->size ());
         drawLine (line, view, area, context);
      }
      break;

   case SL_OOF_REF:
      // TODO Inefficient. Perhaps store OOF references in separate
      // (much smaller!) list.
      for (int oofmIndex = 0; oofmIndex < NUM_OOFM; oofmIndex++) {
         for (int wordIndex = 0; wordIndex < words->size (); wordIndex++) {
            Word *word = words->getRef (wordIndex);
            if (word->content.type == core::Content::WIDGET_OOF_REF &&
                getOOFMIndex (word->content.widgetReference->widget)
                == oofmIndex &&
                doesWidgetOOFInterruptDrawing (word->content.widget))
               word->content.widget->drawInterruption (view, area, context);
         }
      }
      break;

   default:
      OOFAwareWidget::drawLevel (view, area, level, context);
      break;
   }

   DBG_OBJ_LEAVE ();
}

/**
 * Add a new word (text, widget etc.) to a page.
 */
Textblock::Word *Textblock::addWord (int width, int ascent, int descent,
                                     short flags, core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addWord", "%d * (%d + %d), %d, %p",
                  width, ascent, descent, flags, style);

   if (lineBreakWidth == -1) {
      lineBreakWidth = getAvailWidth (true);
      DBG_OBJ_SET_NUM ("lineBreakWidth", lineBreakWidth);
   }

   words->increase ();
   DBG_OBJ_SET_NUM ("words.size", words->size ());
   int wordNo = words->size () - 1;
   initWord (wordNo);
   fillWord (wordNo, width, ascent, descent, flags, style);

   DBG_OBJ_LEAVE ();
   return words->getRef (wordNo);
}

/**
 * Basic initialization, which is necessary before fillWord.
 */
void Textblock::initWord (int wordNo)
{
   Word *word = words->getRef (wordNo);

   word->style = word->spaceStyle = NULL;
   word->wordImgRenderer = NULL;
   word->spaceImgRenderer = NULL;
}

void Textblock::cleanupWord (int wordNo)
{
   Word *word = words->getRef (wordNo);

   if (word->content.type == core::Content::WIDGET_IN_FLOW)
      delete word->content.widget;
   if (word->content.type == core::Content::WIDGET_OOF_REF)
      delete word->content.widgetReference;
   /** \todo What about texts? */

   removeWordImgRenderer (wordNo);
   removeSpaceImgRenderer (wordNo);

   word->style->unref ();
   word->spaceStyle->unref ();
}

void Textblock::removeWordImgRenderer (int wordNo)
{
   Word *word = words->getRef (wordNo);

   if (word->style && word->wordImgRenderer) {
      word->style->backgroundImage->removeExternalImgRenderer
         (word->wordImgRenderer);
      delete word->wordImgRenderer;
      word->wordImgRenderer = NULL;
   }
}

void Textblock::setWordImgRenderer (int wordNo)
{
   Word *word = words->getRef (wordNo);

   if (word->style->backgroundImage) {
      word->wordImgRenderer = new WordImgRenderer (this, wordNo);
      word->style->backgroundImage->putExternalImgRenderer
         (word->wordImgRenderer);
   } else
      word->wordImgRenderer = NULL;
}

void Textblock::removeSpaceImgRenderer (int wordNo)
{
   Word *word = words->getRef (wordNo);

   if (word->spaceStyle && word->spaceImgRenderer) {
      word->spaceStyle->backgroundImage->removeExternalImgRenderer
         (word->spaceImgRenderer);
      delete word->spaceImgRenderer;
      word->spaceImgRenderer = NULL;
   }
}

void Textblock::setSpaceImgRenderer (int wordNo)
{
   Word *word = words->getRef (wordNo);

   if (word->spaceStyle->backgroundImage) {
      word->spaceImgRenderer = new SpaceImgRenderer (this, wordNo);
      word->spaceStyle->backgroundImage->putExternalImgRenderer
         (word->spaceImgRenderer);
   } else
      word->spaceImgRenderer = NULL;
}

void Textblock::fillWord (int wordNo, int width, int ascent, int descent,
                          short flags, core::style::Style *style)
{
   Word *word = words->getRef (wordNo);

   word->size.width = width;
   word->size.ascent = ascent;
   word->size.descent = descent;
   DBG_SET_WORD_SIZE (wordNo);
   word->origSpace = word->effSpace = 0;
   word->hyphenWidth = 0;
   word->badnessAndPenalty.setPenalty (PENALTY_PROHIBIT_BREAK);
   word->content.space = false;
   word->flags = flags;

   removeWordImgRenderer (wordNo);
   removeSpaceImgRenderer (wordNo);

   word->style = style;
   word->spaceStyle = style;

   setWordImgRenderer (wordNo);
   setSpaceImgRenderer (wordNo);

   style->ref ();
   style->ref ();
}

/*
 * Get the width of a string of text.
 *
 * For "isStart" and "isEnd" see drawText.
 */
int Textblock::textWidth(const char *text, int start, int len,
                         core::style::Style *style, bool isStart, bool isEnd)
{
   int ret = 0;

   if (len > 0) {
      char *str = NULL;

      switch (style->textTransform) {
         case core::style::TEXT_TRANSFORM_NONE:
         default:
            ret = layout->textWidth(style->font, text+start, len);
            break;
         case core::style::TEXT_TRANSFORM_UPPERCASE:
            str = layout->textToUpper(text+start, len);
            ret = layout->textWidth(style->font, str, strlen(str));
            break;
         case core::style::TEXT_TRANSFORM_LOWERCASE:
            str = layout->textToLower(text+start, len);
            ret = layout->textWidth(style->font, str, strlen(str));
            break;
         case core::style::TEXT_TRANSFORM_CAPITALIZE:
            if (isStart) {
               /* \bug No way to know about non-ASCII punctuation. */
               bool initial_seen = false;

               for (int i = 0; i < start; i++)
                  if (!ispunct(text[i]))
                     initial_seen = true;
               if (initial_seen) {
                  ret = layout->textWidth(style->font, text+start, len);
               } else {
                  int after = 0;

                  text += start;
                  while (ispunct(text[after]))
                     after++;
                  if (text[after])
                     after = layout->nextGlyph(text, after);
                  if (after > len)
                     after = len;
                  str = layout->textToUpper(text, after);
                  ret = layout->textWidth(style->font, str, strlen(str)) +
                     layout->textWidth(style->font, text+after, len-after);
               }
            } else
               ret = layout->textWidth(style->font, text+start, len);
            break;
      }

      if (str)
         free(str);
   }

   return ret;
}

/**
 * Calculate the size of a text word.
 *
 * For "isStart" and "isEnd" see textWidth and drawText.
 */
void Textblock::calcTextSize (const char *text, size_t len,
                              core::style::Style *style,
                              core::Requisition *size, bool isStart, bool isEnd)
{
   size->width = textWidth (text, 0, len, style, isStart, isEnd);
   size->ascent = style->font->ascent;
   size->descent = style->font->descent;

   /*
    * For 'normal' line height, just use ascent and descent from font.
    * For absolute/percentage, line height is relative to font size, which
    * is (irritatingly) smaller than ascent+descent.
    */
   if (style->lineHeight != core::style::LENGTH_AUTO) {
      int height, leading;
      float factor = style->font->size;

      factor /= (style->font->ascent + style->font->descent);

      size->ascent = lout::misc::roundInt(size->ascent * factor);
      size->descent = lout::misc::roundInt(size->descent * factor);

      /* TODO: The containing block's line-height property gives a minimum
       * height for the line boxes. (Even when it's set to 'normal', i.e.,
       * AUTO? Apparently.) Once all block elements make Textblocks or
       * something, this can be handled.
       */
      if (core::style::isAbsLength (style->lineHeight))
         height = core::style::absLengthVal(style->lineHeight);
      else
         height =
            core::style::multiplyWithPerLengthRounded (style->font->size,
                                                       style->lineHeight);
      leading = height - style->font->size;

      size->ascent += leading / 2;
      size->descent += leading - (leading / 2);
   }

   /* In case of a sub or super script we increase the word's height and
    * potentially the line's height.
    */
   if (style->valign == core::style::VALIGN_SUB) {
      int requiredDescent = style->font->descent + style->font->ascent / 3;
      size->descent = misc::max (size->descent, requiredDescent);
   } else if (style->valign == core::style::VALIGN_SUPER) {
      int requiredAscent = style->font->ascent + style->font->ascent / 2;
      size->ascent = misc::max (size->ascent, requiredAscent);
   }
}

/**
 * Add a word to the page structure. If it contains dividing
 * characters (hard or soft hyphens, em-dashes, etc.), it is divided.
 */
void Textblock::addText (const char *text, size_t len,
                         core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addText", "..., %d, %p",
                  (int)len, style);

   // Count dividing characters.
   int numParts = 1;

   for (const char *s = text; s; s = nextUtf8Char (s, text + len - s)) {
      int foundDiv = -1;
      for (int j = 0; foundDiv == -1 && j < NUM_DIV_CHARS; j++) {
         int lDiv = strlen (divChars[j].s);
         if (s  <= text + len - lDiv) {
            if (memcmp (s, divChars[j].s, lDiv * sizeof (char)) == 0)
               foundDiv = j;
         }
      }

      if (foundDiv != -1) {
         if (divChars[foundDiv].penaltyIndexLeft != -1)
            numParts ++;
         if (divChars[foundDiv].penaltyIndexRight != -1)
            numParts ++;
      }
   }

   if (numParts == 1) {
      // Simple (and common) case: no dividing characters. May still
      // be hyphenated automatically.
      core::Requisition size;
      calcTextSize (text, len, style, &size, true, true);
      addText0 (text, len,
                Word::CAN_BE_HYPHENATED | Word::WORD_START | Word::WORD_END,
                style, &size);

      //printf ("[%p] %d: added simple word: ", this, words->size() - 1);
      //printWordWithFlags (words->getLastRef());
      //printf ("\n");
   } else {
      PRINTF ("HYPHENATION: '");
      for (size_t i = 0; i < len; i++)
         PUTCHAR(text[i]);
      PRINTF ("', with %d parts\n", numParts);

      // Store hyphen positions.
      int n = 0, totalLenCharRemoved = 0;
      int *partPenaltyIndex = new int[numParts - 1];
      int *partStart = new int[numParts];
      int *partEnd = new int[numParts];
      bool *charRemoved = new bool[numParts - 1];
      bool *canBeHyphenated = new bool[numParts + 1];
      bool *permDivChar = new bool[numParts - 1];
      bool *unbreakableForMinWidth = new bool[numParts - 1];
      canBeHyphenated[0] = canBeHyphenated[numParts] = true;
      partStart[0] = 0;
      partEnd[numParts - 1] = len;

      for (const char *s = text; s; s = nextUtf8Char (s, text + len - s)) {
         int foundDiv = -1;
         for (int j = 0; foundDiv == -1 && j < NUM_DIV_CHARS; j++) {
            int lDiv = strlen (divChars[j].s);
            if (s <= text + len - lDiv) {
               if (memcmp (s, divChars[j].s, lDiv * sizeof (char)) == 0)
                  foundDiv = j;
            }
         }

         if (foundDiv != -1) {
            int lDiv = strlen (divChars[foundDiv].s);

            if (divChars[foundDiv].charRemoved) {
               assert (divChars[foundDiv].penaltyIndexLeft != -1);
               assert (divChars[foundDiv].penaltyIndexRight == -1);

               partPenaltyIndex[n] = divChars[foundDiv].penaltyIndexLeft;
               charRemoved[n] = true;
               permDivChar[n] = false;
               unbreakableForMinWidth[n] =
                  divChars[foundDiv].unbreakableForMinWidth;
               canBeHyphenated[n + 1] = divChars[foundDiv].canBeHyphenated;
               partEnd[n] = s - text;
               partStart[n + 1] = s - text + lDiv;
               n++;
               totalLenCharRemoved += lDiv;
            } else {
               assert (divChars[foundDiv].penaltyIndexLeft != -1 ||
                       divChars[foundDiv].penaltyIndexRight != -1);

               if (divChars[foundDiv].penaltyIndexLeft != -1) {
                  partPenaltyIndex[n] = divChars[foundDiv].penaltyIndexLeft;
                  charRemoved[n] = false;
                  permDivChar[n] = false;
                  unbreakableForMinWidth[n] =
                     divChars[foundDiv].unbreakableForMinWidth;
                  canBeHyphenated[n + 1] = divChars[foundDiv].canBeHyphenated;
                  partEnd[n] = s - text;
                  partStart[n + 1] = s - text;
                  n++;
               }

               if (divChars[foundDiv].penaltyIndexRight != -1) {
                  partPenaltyIndex[n] = divChars[foundDiv].penaltyIndexRight;
                  charRemoved[n] = false;
                  permDivChar[n] = true;
                  unbreakableForMinWidth[n] =
                     divChars[foundDiv].unbreakableForMinWidth;
                  canBeHyphenated[n + 1] = divChars[foundDiv].canBeHyphenated;
                  partEnd[n] = s - text + lDiv;
                  partStart[n + 1] = s - text + lDiv;
                  n++;
               }
            }
         }
      }

      // Get text without removed characters, e. g. hyphens.
      const char *textWithoutHyphens;
      char *textWithoutHyphensBuf = new char[len - totalLenCharRemoved];
      int *breakPosWithoutHyphens;
      int *breakPosWithoutHyphensBuf = new int[numParts - 1];

      if (totalLenCharRemoved == 0) {
         // No removed characters: take original arrays.
         textWithoutHyphens = text;
         // Ends are also break positions, except the last end, which
         // is superfluous, but does not harm (since arrays in C/C++
         // does not have an implicit length).
         breakPosWithoutHyphens = partEnd;
      } else {
         // Copy into special buffers.
         textWithoutHyphens = textWithoutHyphensBuf;
         breakPosWithoutHyphens = breakPosWithoutHyphensBuf;

         int n = 0;
         for (int i = 0; i < numParts; i++) {
            memmove (textWithoutHyphensBuf + n, text + partStart[i],
                     partEnd[i] - partStart[i]);
            n += partEnd[i] - partStart[i];
            if (i < numParts - 1)
               breakPosWithoutHyphensBuf[i] = n;
         }
      }

      PRINTF("H... without hyphens: '");
      for (size_t i = 0; i < len - totalLenCharRemoved; i++)
         PUTCHAR(textWithoutHyphens[i]);
      PRINTF("'\n");

      core::Requisition *wordSize = new core::Requisition[numParts];
      calcTextSizes (textWithoutHyphens, len - totalLenCharRemoved, style,
                     numParts - 1, breakPosWithoutHyphens, wordSize);

      // Finished!
      for (int i = 0; i < numParts; i++) {
         short flags = 0;

         // If this parts adjoins at least one division characters,
         // for which canBeHyphenated is set to false (this is the
         // case for soft hyphens), do not hyphenate.
         if (canBeHyphenated[i] && canBeHyphenated[i + 1])
            flags |= Word::CAN_BE_HYPHENATED;

         if(i < numParts - 1) {
            if (charRemoved[i])
               flags |= Word::DIV_CHAR_AT_EOL;

            if (permDivChar[i])
               flags |= Word::PERM_DIV_CHAR;
            if (unbreakableForMinWidth[i])
               flags |= Word::UNBREAKABLE_FOR_MIN_WIDTH;

            flags |= Word::DRAW_AS_ONE_TEXT;
         }

         if (i == 0)
            flags |= Word::WORD_START;
         if (i == numParts - 1)
            flags |= Word::WORD_END;

         addText0 (text + partStart[i], partEnd[i] - partStart[i],
                   flags, style, &wordSize[i]);

         //printf ("[%p] %d: added word part: ", this, words->size() - 1);
         //printWordWithFlags (words->getLastRef());
         //printf ("\n");

         //PRINTF("H... [%d] '", i);
         //for (int j = partStart[i]; j < partEnd[i]; j++)
         //   PUTCHAR(text[j]);
         //PRINTF("' added\n");

         if(i < numParts - 1) {
            Word *word = words->getLastRef();

            setBreakOption (word, style, penalties[partPenaltyIndex[i]][0],
                            penalties[partPenaltyIndex[i]][1], false);
            DBG_SET_WORD (words->size () - 1);

            if (charRemoved[i])
               // Currently, only unconditional hyphens (UTF-8:
               // "\xe2\x80\x90") can be used. See also drawWord, last
               // section "if (drawHyphen)".
               // Could be extended by adding respective members to
               // DivChar and Word.
               word->hyphenWidth =
                  layout->textWidth (word->style->font, hyphenDrawChar,
                                     strlen (hyphenDrawChar));

            accumulateWordData (words->size() - 1);
            correctLastWordExtremes ();
         }
      }

      delete[] partPenaltyIndex;
      delete[] partStart;
      delete[] partEnd;
      delete[] charRemoved;
      delete[] canBeHyphenated;
      delete[] permDivChar;
      delete[] unbreakableForMinWidth;
      delete[] textWithoutHyphensBuf;
      delete[] breakPosWithoutHyphensBuf;
      delete[] wordSize;
   }

   DBG_OBJ_LEAVE ();
}

void Textblock::calcTextSizes (const char *text, size_t textLen,
                               core::style::Style *style,
                               int numBreaks, int *breakPos,
                               core::Requisition *wordSize)
{
   // The size of the last part is calculated in a simple way.
   int lastStart = breakPos[numBreaks - 1];
   calcTextSize (text + lastStart, textLen - lastStart, style,
                 &wordSize[numBreaks], true, true);

   PRINTF("H... [%d] '", numBreaks);
   for (size_t i = 0; i < textLen - lastStart; i++)
      PUTCHAR(text[i + lastStart]);
   PRINTF("' -> %d\n", wordSize[numBreaks].width);

   // The rest is more complicated. See dw-line-breaking, section
   // "Hyphens".
   for (int i = numBreaks - 1; i >= 0; i--) {
      int start = (i == 0) ? 0 : breakPos[i - 1];
      calcTextSize (text + start, textLen - start, style, &wordSize[i],
                    i == 0, i == numBreaks - 1);

      PRINTF("H... [%d] '", i);
      for (size_t j = 0; j < textLen - start; j++)
         PUTCHAR(text[j + start]);
      PRINTF("' -> %d\n", wordSize[i].width);

      for (int j = i + 1; j < numBreaks + 1; j++) {
         wordSize[i].width -= wordSize[j].width;
         PRINTF("H...    - %d = %d\n", wordSize[j].width, wordSize[i].width);
      }
   }
}

/**
 * Calculate the size of a widget, and return whether it has to be
 * positioned at the top of the line.
 */
bool Textblock::calcSizeOfWidgetInFlow (int wordIndex, Widget *widget,
                                        core::Requisition *size)
{
   DBG_OBJ_ENTER ("resize", 0, "calcSizeOfWidgetInFlow", "%d, %p, ...",
                  wordIndex, widget);

   bool result, firstWordOfLine;

   if (hasListitemValue)
      // For list items, the word #0 at the beginning (bullet, number ...) has
      // to be considered;
      firstWordOfLine = wordIndex == 1 ||
         (wordIndex > 0 &&
          words->getRef(wordIndex - 1)->content.type == core::Content::BREAK);
   else
      firstWordOfLine = wordIndex == 0 ||
         words->getRef(wordIndex - 1)->content.type == core::Content::BREAK;

   DBG_OBJ_MSGF ("resize", 1, "firstWordOfLine = %s",
                 boolToStr (firstWordOfLine));

   if (firstWordOfLine &&
       (widget->getStyle()->display == core::style::DISPLAY_BLOCK ||
        widget->getStyle()->display == core::style::DISPLAY_LIST_ITEM ||
        widget->getStyle()->display == core::style::DISPLAY_TABLE)) {
      // pass positions
      DBG_OBJ_MSG ("resize", 1, "pass position");
      int lastWord = lines->empty () ? -1 : lines->getLastRef()->lastWord;
      assert (wordIndex > lastWord);

      // The position passed to sizeRequest must be equivalent to the position
      // passed later to sizeAllocate. This is more complicated for widgets
      // which are centered or aligned to the right: here, we have to know the
      // width to calculate the horizontal position. Both are calculated in an
      // iterative way; initially, the left border is used (like for other,
      // simpler alignments).
      
      // Since the child widget will regard floats, we do not have to include
      // floats when calculating left and right border.

      // TODO Actually line1OffsetEff should be used instead of line1Offset, but
      // it may not initialized here. Anyway, since ignoreLine1OffsetSometimes
      // is not used, line1OffsetEff is always equal to line1Offset.
      
      int leftBorder = boxOffsetX () + leftInnerPadding
         + (lines->size () == 0 ? line1Offset /* ...Eff, actually */ : 0);
      int rightBorder = boxRestWidth ();
      
      int lastMargin, yLine = yOffsetOfLineToBeCreated (&lastMargin);
      int yRel = yLine - min (lastMargin, widget->getStyle()->marginTop());

      DBG_OBJ_MSGF ("resize", 1,
                    "leftBorder = %d + %d + (%d == 0 ? %d : 0) = %d, "
                    "rightBorder = %d, yRel = %d - min (%d, %d) = %d",
                    boxOffsetX (), leftInnerPadding , lines->size (),
                    line1OffsetEff, leftBorder, rightBorder, yLine, lastMargin,
                    widget->getStyle()->marginTop(), yRel);
            
      core::SizeParams childParams;
      DBG_OBJ_ASSOC_CHILD (&childParams);

      int oldXRel = leftBorder;

      sizeRequestParams.forChild (this, widget, oldXRel, yRel, &childParams);
      widget->sizeRequest (size, childParams.getNumPos (),
                           childParams.getReferences (), childParams.getX (),
                           childParams.getY ());

      DBG_OBJ_MSG_START ();
      
      while (true) {
         DBG_OBJ_MSG_START ();
         
         int xRel;

         switch(widget->getStyle()->textAlign) {
         case core::style::TEXT_ALIGN_LEFT:
         case core::style::TEXT_ALIGN_STRING: // see comment in alignLine()
         case core::style::TEXT_ALIGN_JUSTIFY: // equivalent for only one word
         default: // compiler happiness
            xRel = leftBorder;
            break;
            
         case core::style::TEXT_ALIGN_RIGHT:
            xRel = lineBreakWidth - rightBorder - size->width;
            break;
            
         case core::style::TEXT_ALIGN_CENTER:
            xRel =
               (leftBorder + lineBreakWidth - rightBorder - size->width) / 2;
            break;
         }
         
         // Cf. Textblock::calcTextOffset().
         if (xRel < leftBorder)
            xRel = leftBorder;
      
         DBG_OBJ_MSGF ("resize", 2, "xRel = %d, oldXRel = %d", xRel, oldXRel);

         // Stop when the value of xRel has not changed during the last
         // iteration.
         if (xRel == oldXRel)
            break;
         
         sizeRequestParams.forChild (this, widget, xRel, yRel, &childParams);
         widget->sizeRequest (size, childParams.getNumPos (),
                              childParams.getReferences (), childParams.getX (),
                              childParams.getY ());

         oldXRel = xRel;

         DBG_OBJ_MSG_END ();
      }

      DBG_OBJ_MSG_END ();
      
      result = true;
   } else {
      // do not pass positions (inline elements etc)
      DBG_OBJ_MSG ("resize", 1, "do not pass position");
      widget->sizeRequest (size);
      result = false;
   }

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr (result));
   return result;
}

bool Textblock::findSizeRequestReference (Widget *reference, int *xRef,
                                          int *yRef)
{
   if (reference == this) {
      if (xRef)
         *xRef = 0;
      if (yRef)
         *yRef = 0;
      return true;
   } else
      return sizeRequestParams.findReference (reference, xRef, yRef);
}
   
/**
 * Add a word (without hyphens) to the page structure.
 */
void Textblock::addText0 (const char *text, size_t len, short flags,
                          core::style::Style *style, core::Requisition *size)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addText0",
                  "..., %d, %s:%s:%s:%s:%s:%s:%s:%s, %p, %d * (%d + %d)",
                  (int)len,
                  (flags & Word::CAN_BE_HYPHENATED) ? "h?" : "--",
                  (flags & Word::DIV_CHAR_AT_EOL) ? "de" : "--",
                  (flags & Word::PERM_DIV_CHAR) ? "dp" : "--",
                  (flags & Word::DRAW_AS_ONE_TEXT) ? "t1" : "--",
                  (flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) ? "um" : "--",
                  (flags & Word::WORD_START) ? "st" : "--",
                  (flags & Word::WORD_END) ? "en" : "--",
                  (flags & Word::TOPLEFT_OF_LINE) ? "00" : "--",
                  style, size->width, size->ascent, size->descent);

   //printf("[%p] addText0 ('", this);
   //for (size_t i = 0; i < len; i++)
   //   putchar(text[i]);
   //printf("', ");
   //printWordFlags (flags);
   //printf (", ...)\n");

   Word *word = addWord (size->width, size->ascent, size->descent,
                         flags, style);
   DBG_OBJ_ASSOC_CHILD (style);
   word->content.type = core::Content::TEXT;
   word->content.text = layout->textZone->strndup(text, len);

   DBG_SET_WORD (words->size () - 1);

   // The following debug message may be useful to identify the
   // different textblocks.

   //if (words->size() == 1)
   //   printf ("[%p] first word: '%s'\n", this, text);

   processWord (words->size () - 1);

   DBG_OBJ_LEAVE ();
}

/**
 * Add a widget (word type) to the page.
 */
void Textblock::addWidget (core::Widget *widget, core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addWidget", "%p, %p", widget, style);

   /* We first assign -1 as parent_ref, since the call of widget->size_request
    * will otherwise let this Textblock be rewrapped from the beginning.
    * (parent_ref is actually undefined, but likely has the value 0.) At the,
    * end of this function, the correct value is assigned. */
   widget->parentRef = -1;
   DBG_OBJ_SET_NUM_O (widget, "parentRef", widget->parentRef);

   widget->setStyle (style);

   initOutOfFlowMgrs ();

   if (testWidgetOutOfFlow (widget)) {
      int oofmIndex = getOOFMIndex (widget);
      DBG_OBJ_MSGF ("construct.word", 1, "ouf of flow: oofmIndex = %d (%s)",
                    oofmIndex, OOFM_NAME[oofmIndex]);

      widget->setParent (oofContainer[oofmIndex]);
      widget->setGenerator (this);

      oof::OutOfFlowMgr *oofm = searchOutOfFlowMgr (oofmIndex);
      int oofmSubRef = oofm->addWidgetOOF (widget, this, words->size ());
      widget->parentRef = makeParentRefOOF (oofmIndex, oofmSubRef);

      DBG_OBJ_MSGF ("construct.word", 1, "oofmSubRef = %d => parentRef = %d",
                    oofmSubRef, widget->parentRef);

      core::Requisition size;
      oofm->calcWidgetRefSize (widget, &size);
      Word *word = addWord (size.width, size.ascent, size.descent, 0, style);
      word->content.type = core::Content::WIDGET_OOF_REF;
      word->content.widgetReference = new core::WidgetReference (widget);
      widget->setWidgetReference (word->content.widgetReference);

      // After a out-of-flow reference, breaking is allowed. (This avoids some
      // problems with breaking near float definitions.)
      setBreakOption (word, style, 0, 0, false);
   } else {
      DBG_OBJ_MSG ("construct.word", 1, "in flow");

      widget->setParent (this);

      // TODO Replace (perhaps) later "textblock" by "OOF aware widget".
      if (widget->instanceOf (Textblock::CLASS_ID)) {
         for (int i = 0; i < NUM_OOFM; i++)
            searchOutOfFlowMgr(i)->addWidgetInFlow ((Textblock*)widget, this,
                                                    words->size ());
      }

      core::Requisition size;
      short flags = calcSizeOfWidgetInFlow (words->size (), widget, &size) ?
         Word::TOPLEFT_OF_LINE : 0;
      Word *word =
         addWord (size.width, size.ascent, size.descent, flags, style);
      word->content.type = core::Content::WIDGET_IN_FLOW;
      word->content.widget = widget;
   }

   DBG_SET_WORD (words->size () - 1);

   processWord (words->size () - 1);
   //DBG_OBJ_SET_NUM (word->content.widget, "parent_ref",
   //                 word->content.widget->parent_ref);

   //DEBUG_MSG(DEBUG_REWRAP_LEVEL,
   //          "Assigning parent_ref = %d to added word %d, "
   //          "in page with %d word(s)\n",
   //          lines->size () - 1, words->size() - 1, words->size());

   DBG_OBJ_LEAVE ();
}

/**
 * Add an anchor to the page. "name" is copied, so no dStrdup is necessary for
 * the caller.
 *
 * Return true on success, and false, when this anchor had already been
 * added to the widget tree.
 */
bool Textblock::addAnchor (const char *name, core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addAnchor", "\"%s\", %p", name, style);

   char *copy;
   int y;
   bool result;

   // Since an anchor does not take any space, it is safe to call
   // addAnchor already here.
   if (wasAllocated ()) {
      if (lines->size () == 0)
         y = allocation.y;
      else
         y = allocation.y + lineYOffsetWidget (lines->size () - 1);
      copy = Widget::addAnchor (name, y);
   } else
      copy = Widget::addAnchor (name);

   if (copy == NULL)
      /**
       * \todo It may be necessary for future uses to save the anchor in
       *    some way, e.g. when parts of the widget tree change.
       */
      result = false;
   else {
      Anchor *anchor;

      anchors->increase();
      anchor = anchors->getRef(anchors->size() - 1);
      anchor->name = copy;
      anchor->wordIndex = words->size();
      result =  true;
   }

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(result));
   return result;
}


/**
 * ?
 */
void Textblock::addSpace (core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addSpace", "%p", style);

   int wordIndex = words->size () - 1;
   if (wordIndex >= 0) {
      fillSpace (wordIndex, style);
      DBG_SET_WORD (wordIndex);
      accumulateWordData (wordIndex);
      correctLastWordExtremes ();
   }

   DBG_OBJ_LEAVE ();
}

/**
 * Add a break option (see setBreakOption() for details). Used instead
 * of addSpace for ideographic characters.
 *
 * When "forceBreak" is true, a break is even possible within PRE etc.
 */
void Textblock::addBreakOption (core::style::Style *style, bool forceBreak)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addBreakOption", "%p, %s",
                  style, forceBreak ? "true" : "false");

   int wordIndex = words->size () - 1;
   if (wordIndex >= 0) {
      setBreakOption (words->getRef(wordIndex), style, 0, 0, forceBreak);
      DBG_SET_WORD (wordIndex);
      // Call of accumulateWordData() is not needed here.
      correctLastWordExtremes ();
   }

   DBG_OBJ_LEAVE ();
}

void Textblock::fillSpace (int wordNo, core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "fillSpace", "%d, ...", wordNo);

   DBG_OBJ_MSGF ("construct.word", 1, "style.white-space = %s",
                 style->whiteSpace == core::style::WHITE_SPACE_NORMAL ? "normal"
                 : style->whiteSpace == core::style::WHITE_SPACE_PRE ? "pre"
                 : style->whiteSpace == core::style::WHITE_SPACE_NOWRAP ?
                 "nowrap"
                 : style->whiteSpace == core::style::WHITE_SPACE_PRE_WRAP ?
                 "pre-wrap"
                 : style->whiteSpace == core::style::WHITE_SPACE_PRE_LINE ?
                 "pre-line" : "???");

   // Old comment:
   //
   //     According to
   //     http://www.w3.org/TR/CSS2/text.html#white-space-model: "line
   //     breaking opportunities are determined based on the text
   //     prior to the white space collapsing steps".
   //
   //     So we call addBreakOption () for each Textblock::addSpace ()
   //     call.  This is important e.g. to be able to break between
   //     foo and bar in: <span style="white-space:nowrap">foo </span>
   //     bar
   //
   // TODO: Re-evaluate again.

   Word *word = words->getRef (wordNo);

   // TODO: This line does not work: addBreakOption (word, style);

   if (// Do not override a previously set break penalty:
       !word->content.space &&
       // OOF references are considered specially, and must not have a space:
       word->content.type != core::Content::WIDGET_OOF_REF) {
      setBreakOption (word, style, 0, 0, false);

      word->content.space = true;
      word->origSpace = word->effSpace =
         style->font->spaceWidth + style->wordSpacing;

      removeSpaceImgRenderer (wordNo);

      word->spaceStyle->unref ();
      word->spaceStyle = style;
      style->ref ();

      setSpaceImgRenderer (wordNo);
   }

   DBG_OBJ_LEAVE ();
}

/**
 * Set a break option, if allowed by the style. Called by fillSpace
 * (and so addSpace), but may be called, via addBreakOption(), as an
 * alternative, e. g. for ideographic characters.
 */
void Textblock::setBreakOption (Word *word, core::style::Style *style,
                                int breakPenalty1, int breakPenalty2,
                                bool forceBreak)
{
   DBG_OBJ_ENTER ("construct.word", 0, "setBreakOption", "..., %d, %d, %s",
                  breakPenalty1, breakPenalty2, forceBreak ? "true" : "false");

   // TODO: lineMustBeBroken should be independent of the penalty
   // index? Otherwise, examine the last line.
   if (!word->badnessAndPenalty.lineMustBeBroken(0)) {
      if (forceBreak || isBreakAllowed (style))
         word->badnessAndPenalty.setPenalties (breakPenalty1, breakPenalty2);
      else
         word->badnessAndPenalty.setPenalty (PENALTY_PROHIBIT_BREAK);
   }

   DBG_OBJ_LEAVE ();
}

bool Textblock::isBreakAllowed (core::style::Style *style)
{
   switch (style->whiteSpace) {
   case core::style::WHITE_SPACE_NORMAL:
   case core::style::WHITE_SPACE_PRE_LINE:
   case core::style::WHITE_SPACE_PRE_WRAP:
      return true;

   case core::style::WHITE_SPACE_PRE:
   case core::style::WHITE_SPACE_NOWRAP:
      return false;

   default:
      // compiler happiness
      lout::misc::assertNotReached ();
      return false;
   }
}


/**
 * Cause a paragraph break
 */
void Textblock::addParbreak (int space, core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addParbreak", "%d, %p",
                  space, style);
   DBG_OBJ_MSG ("construct.word", 0,
                "<i>No nesting! Stack trace may be incomplete.</i>");
   DBG_OBJ_LEAVE ();

   Word *word;

   /* A break may not be the first word of a page, or directly after
      the bullet/number (which is the first word) in a list item. (See
      also comment in sizeRequest.) */
   if (words->size () == 0 ||
       (hasListitemValue && words->size () == 1)) {
      /* This is a bit hackish: If a break is added as the
         first/second word of a page, and the parent widget is also a
         Textblock, and there is a break before -- this is the case when
         a widget is used as a text box (lists, blockquotes, list
         items etc) -- then we simply adjust the break before, in a
         way that the space is in any case visible. */
      /* Find the widget where to adjust the breakSpace. (Only
         consider normal flow, no floats etc.) */
      for (Widget *widget = this;
           widget->getParent() != NULL &&
              widget->getParent()->instanceOf (Textblock::CLASS_ID) &&
              !isWidgetOOF (widget);
           widget = widget->getParent ()) {
         Textblock *textblock2 = (Textblock*)widget->getParent ();
         int index = textblock2->hasListitemValue ? 1 : 0;
         bool isfirst = (textblock2->words->getRef(index)->content.type
                         == core::Content::WIDGET_IN_FLOW
                         && textblock2->words->getRef(index)->content.widget
                         == widget);
         if (!isfirst) {
            /* The text block we searched for has been found. */
            Word *word2;
            int lineno = getWidgetInFlowSubRef (widget);

            if (lineno > 0 &&
                (word2 =
                 textblock2->words->getRef(textblock2->lines
                                           ->getRef(lineno - 1)->firstWord)) &&
                word2->content.type == core::Content::BREAK) {
               if (word2->content.breakSpace < space) {
                  word2->content.breakSpace = space;
                  textblock2->queueResize (makeParentRefInFlow (lineno), false);
                  textblock2->mustQueueResize = false;
                  DBG_OBJ_SET_BOOL_O (textblock2, "mustQueueResize",
                                      textblock2->mustQueueResize);
               }
            }
            return;
         }
         /* Otherwise continue to examine parents. */
      }

      /* Return in any case. */
      return;
   }

   /* Another break before? */
   if ((word = words->getRef(words->size () - 1)) &&
       word->content.type == core::Content::BREAK) {
      Line *lastLine = lines->getRef (lines->size () - 1);

      word->content.breakSpace =
         misc::max (word->content.breakSpace, space);
      lastLine->breakSpace =
         misc::max (word->content.breakSpace,
                    lastLine->marginDescent - lastLine->borderDescent,
                    lastLine->breakSpace);
      return;
   }

   word = addWord (0, 0, 0, 0, style);
   DBG_OBJ_ASSOC_CHILD (style);
   word->content.type = core::Content::BREAK;
   word->badnessAndPenalty.setPenalty (PENALTY_FORCE_BREAK);
   word->content.breakSpace = space;

   DBG_SET_WORD (words->size () - 1);

   breakAdded ();
   processWord (words->size () - 1);
}

/*
 * Cause a line break.
 */
void Textblock::addLinebreak (core::style::Style *style)
{
   DBG_OBJ_ENTER ("construct.word", 0, "addLinebreak", "%p", style);

   Word *word;

   if (words->size () == 0 ||
       words->getRef(words->size () - 1)->content.type == core::Content::BREAK)
      // An <BR> in an empty line gets the height of the current font
      // (why would someone else place it here?), ...
      word =
         addWord (0, style->font->ascent, style->font->descent, 0, style);
   else
      // ... otherwise, it has no size (and does not enlarge the line).
      word = addWord (0, 0, 0, 0, style);

   DBG_OBJ_ASSOC_CHILD (style);

   word->content.type = core::Content::BREAK;
   word->badnessAndPenalty.setPenalty (PENALTY_FORCE_BREAK);
   word->content.breakSpace = 0;

   DBG_SET_WORD (words->size () - 1);

   breakAdded ();
   processWord (words->size () - 1);

   DBG_OBJ_LEAVE ();
}

/**
 * Called directly after a (line or paragraph) break has been added.
 */
void Textblock::breakAdded ()
{
   assert (words->size () >= 1);
   assert (words->getRef(words->size () - 1)->content.type
           == core::Content::BREAK);

   // Any space before is removed. It is not used; on the other hand,
   // this snippet (an example from a real-world debugging session)
   // would cause problems:
   //
   // <input style="width: 100%" .../>
   // <button ...>...</button>
   //
   // (Notice the space between <input> and <button>, and also that
   // the HTML parser will insert a BREAK between them.) The <input>
   // would be given the available width ("width: 100%"), but the
   // actual width (Word::totalWidth) would include the space, so that
   // the width of the line is larger than the available width.

   if (words->size () >= 2)
      words->getRef(words->size () - 2)->origSpace =
         words->getRef(words->size () - 2)->effSpace = 0;
}

core::Widget *Textblock::getWidgetAtPointLevel (int x, int y, int level,
                                               core::GettingWidgetAtPointContext
                                                *context)
{
   DBG_OBJ_ENTER ("events", 0, "Textblock::getWidgetAtPointLevel", "%d, %d, %s",
                  x, y, stackingLevelText (level));

   Widget *widgetAtPoint = NULL;

   switch (level) {
   case SL_IN_FLOW:
      {
         int lineIndex = findLineIndexWhenAllocated (y - allocation.y);
      
         if (lineIndex >= 0 && lineIndex < lines->size ()) {
            Line *line = lines->getRef (lineIndex);

            for (int wordIndex = line->lastWord;
                 widgetAtPoint == NULL && wordIndex >= line->firstWord;
                 wordIndex--) {
               Word *word =  words->getRef (wordIndex);
               if (word->content.type == core::Content::WIDGET_IN_FLOW &&
                   !core::StackingContextMgr::handledByStackingContextMgr
                      (word->content.widget))
                   widgetAtPoint =
                      word->content.widget->getWidgetAtPoint (x, y, context);
            }
         }
      }
      break;

   case SL_OOF_REF:
      // TODO Inefficient. Perhaps store OOF references in separate
      // (much smaller!) list.
      for (int oofmIndex = NUM_OOFM; widgetAtPoint == NULL && oofmIndex >= 0;
           oofmIndex--) {
         for (int wordIndex = words->size () - 1;
              widgetAtPoint == NULL && wordIndex >= 0; wordIndex--) {
            Word *word = words->getRef (wordIndex);
            if (word->content.type == core::Content::WIDGET_OOF_REF &&
                getOOFMIndex (word->content.widgetReference->widget)
                == oofmIndex &&
                doesWidgetOOFInterruptDrawing (word->content.widgetReference
                                               ->widget))
               widgetAtPoint = 
                  word->content.widgetReference->widget
                  ->getWidgetAtPointInterrupted (x, y, context);
         }
      }
      break;

   default:
      widgetAtPoint =
         OOFAwareWidget::getWidgetAtPointLevel (x, y, level, context);
      break;
   }

   DBG_OBJ_LEAVE_VAL ("%p", widgetAtPoint);
   return widgetAtPoint;
}

/**
 * This function "hands" the last break of a page "over" to a parent
 * page. This is used for "collapsing spaces".
 */
void Textblock::handOverBreak (core::style::Style *style)
{
   if (lines->size() > 0) {
      Widget *parent;
      Line *lastLine = lines->getRef (lines->size () - 1);

      if (lastLine->breakSpace != 0 && (parent = getParent()) &&
          parent->instanceOf (Textblock::CLASS_ID) &&
          parent->getStyle()->display != core::style::DISPLAY_BLOCK) {
         Textblock *textblock2 = (Textblock*) parent;
         textblock2->addParbreak(lastLine->breakSpace, style);
      }
   }
}

/*
 * Any words added by addWord() are not immediately (queued to
 * be) drawn, instead, this function must be called. This saves some
 * calls to queueResize().
 *
 */
void Textblock::flush ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "flush");

   if (mustQueueResize) {
      DBG_OBJ_MSG ("resize", 0, "mustQueueResize set");

      queueResize (-1, true);
      mustQueueResize = false;
      DBG_OBJ_SET_BOOL ("mustQueueResize", mustQueueResize);
   }

   DBG_OBJ_LEAVE ();
}


// next: Dw_page_find_word

void Textblock::changeLinkColor (int link, int newColor)
{
   for (int lineIndex = 0; lineIndex < lines->size(); lineIndex++) {
      bool changed = false;
      Line *line = lines->getRef (lineIndex);
      int wordIdx;

      for (wordIdx = line->firstWord; wordIdx <= line->lastWord; wordIdx++){
         Word *word = words->getRef(wordIdx);

         if (word->style->x_link == link) {
            core::style::StyleAttrs styleAttrs;

            switch (word->content.type) {
            case core::Content::TEXT:
            {  core::style::Style *old_style = word->style;
               styleAttrs = *old_style;
               styleAttrs.color = core::style::Color::create (layout,
                                                              newColor);
               word->style = core::style::Style::create (&styleAttrs);
               old_style->unref();
               old_style = word->spaceStyle;
               styleAttrs = *old_style;
               styleAttrs.color = core::style::Color::create (layout,
                                                              newColor);
               word->spaceStyle = core::style::Style::create(&styleAttrs);
               old_style->unref();
               break;
            }
            case core::Content::WIDGET_IN_FLOW:
            {  core::Widget *widget = word->content.widget;
               styleAttrs = *widget->getStyle();
               styleAttrs.color = core::style::Color::create (layout,
                                                              newColor);
               styleAttrs.setBorderColor(
                           core::style::Color::create (layout, newColor));
               widget->setStyle(core::style::Style::create (&styleAttrs));
               break;
            }
            default:
               break;
            }
            changed = true;
         }
      }
      if (changed)
         queueDrawArea (0, lineYOffsetWidget(line), allocation.width,
                        line->borderAscent + line->borderDescent);
   }
}

void Textblock::changeWordStyle (int from, int to, core::style::Style *style,
                                 bool includeFirstSpace, bool includeLastSpace)
{
}

void Textblock::queueDrawRange (int index1, int index2)
{
   DBG_OBJ_ENTER ("draw", 0, "queueDrawRange", "%d, %d", index1, index2);

   int from = misc::min (index1, index2);
   int to = misc::max (index1, index2);

   from = misc::min (from, words->size () - 1);
   from = misc::max (from, 0);
   to = misc::min (to, words->size () - 1);
   to = misc::max (to, 0);

   int line1idx = findLineOfWord (from);
   int line2idx = findLineOfWord (to);

   if (line1idx >= 0 && line2idx >= 0) {
      Line *line1 = lines->getRef (line1idx),
           *line2 = lines->getRef (line2idx);
      int y = lineYOffsetWidget (line1) + line1->borderAscent -
              line1->contentAscent;
      int h = lineYOffsetWidget (line2) + line2->borderAscent +
              line2->contentDescent - y;

      queueDrawArea (0, y, allocation.width, h);
   }

   DBG_OBJ_LEAVE ();
}

void Textblock::updateReference (int ref)
{
   DBG_OBJ_ENTER ("resize", 0, "updateReference", "%d", ref);

   // Only `queueResize` when there're words or float clearance
   // (float clearance may change `extraSpace.top`).
   if (words->size () > 0 || getStyle()->clear != core::style::CLEAR_NONE)
      queueResize (ref, false);

   DBG_OBJ_LEAVE ();
}

void Textblock::widgetRefSizeChanged (int externalIndex)
{
   int lineNo = findLineOfWord (externalIndex);
   if (lineNo >= 0 && lineNo < lines->size ())
      queueResize (makeParentRefInFlow (lineNo), true);
}

void Textblock::oofSizeChanged (bool extremesChanged)
{
   DBG_OBJ_ENTER ("resize", 0, "oofSizeChanged", "%s",
                  extremesChanged ? "true" : "false");
   queueResize (-1, extremesChanged);

   // See Textblock::getAvailWidthOfChild(): Extremes changes may become also
   // relevant for the children, under certain conditions:
   if (extremesChanged && !usesMaxGeneratorWidth ())
      containerSizeChanged ();

   DBG_OBJ_LEAVE ();
}

int Textblock::getGeneratorX (int oofmIndex)
{
   DBG_OBJ_ENTER ("resize", 0, "Textblock::getGeneratorX", "%d", oofmIndex);

   int x, xRef;
   if (findSizeRequestReference (oofmIndex, &xRef, NULL))
      x = xRef;
   else {
      // Only called for floats, so this should not happen:
      assertNotReached ();
      x = 0;
   }

   DBG_OBJ_LEAVE_VAL ("%d", x);
   return x;
}

int Textblock::getGeneratorY (int oofmIndex)
{
   DBG_OBJ_ENTER ("resize", 0, "Textblock::getGeneratorY", "%d", oofmIndex);

   int yRef, y;
   if (findSizeRequestReference (oofmIndex, NULL, &yRef))
      y = yRef;
   else {
      // Only called for floats, so this should not happen:
      assertNotReached ();
      y = 0;
   }

   DBG_OBJ_LEAVE_VAL ("%d", y);
   return y;
}

int Textblock::getGeneratorRest (int oofmIndex)
{
   DBG_OBJ_ENTER ("resize", 0, "Textblock::getGeneratorRest", "%d", oofmIndex);

   int xRef, rest;
   OOFAwareWidget *container = oofContainer[oofmIndex];
   
   if (container != NULL && findSizeRequestReference (container, &xRef, NULL))
      rest = container->getGeneratorWidth () - (xRef + getGeneratorWidth ());
   else {
      // Only callend for floats, so this should not happen:
      assertNotReached ();
      rest = 0;
   }

   DBG_OBJ_LEAVE_VAL ("%d", rest);
   return rest;
}

int Textblock::getGeneratorWidth ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "Textblock::getGeneratorWidth");
   
   // Cf. sizeRequestImpl.
   if (usesMaxGeneratorWidth ()) {
      DBG_OBJ_LEAVE_VAL ("%d", lineBreakWidth);
      return lineBreakWidth;
   } else {
      // In some cases (especially when called from sizeRequest for an
      // ancestor), the value is not up to date, since content from children is
      // not yet added to lines. Moreover, this leads to inconsistencies between
      // this widget and ancestors (as in Textblock::getGeneratorRest). For this
      // reason, the children are examined recursively.
      //
      // Test case:
      //
      // <div style="float:left">
      //     <div div style="float:right">float</div>
      //     <div>abcdefghijkl mnopqrstuvwx</div>
      // </div>
                                                                       
      int wChild = 0;
      int firstWordAfterLastLine =
         lines->size() > 0 ? lines->getLastRef()->lastWord + 1 : 0;
      for (int i = firstWordAfterLastLine; i < words->size(); i++) {
         Word *word = words->getRef(i);
         int xRel;
         // We only examine instances of dw::Textblock, since they are relevant
         // for floats, for which this method is only called.
         if(word->content.type == core::Content::WIDGET_IN_FLOW &&
            word->content.widget->instanceOf(Textblock::CLASS_ID)) {
            Textblock *tbChild = (Textblock*)word->content.widget;
            if(tbChild->findSizeRequestReference(this, &xRel, NULL))
               wChild = max(wChild, xRel + tbChild->getGeneratorWidth());
         }
      }               

      DBG_OBJ_MSGF ("resize", 1, "wChild = %d", wChild);
          
      int w0 = lines->size () > 0 ? lines->getLastRef()->maxLineWidth : 0;
      DBG_OBJ_MSGF ("resize", 1, "w0 = %d", w0);
      int wThis = min(w0 + leftInnerPadding + boxDiffWidth (), lineBreakWidth);
      DBG_OBJ_MSGF ("resize", 1, "wThis = min(%d + %d + %d, %d) = %d",
                    w0, leftInnerPadding, boxDiffWidth (), lineBreakWidth,
                    wThis);
      int w = max(wThis, wChild);
      DBG_OBJ_LEAVE_VAL ("max(%d, %d) = %d", wThis, wChild, w);
      return w;
   }
}

int Textblock::getMaxGeneratorWidth ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "Textblock::getMaxGeneratorWidth");
   DBG_OBJ_LEAVE_VAL ("%d", lineBreakWidth);
   return lineBreakWidth;
}

bool Textblock::usesMaxGeneratorWidth ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "usesMaxGeneratorWidth");

   bool result;
   if (treatAsInline) {
      DBG_OBJ_MSG ("resize", 1, "treatAsInline set");
      result = false;
   } else {
      bool toplevel = getParent () == NULL,
         block = getStyle()->display == core::style::DISPLAY_BLOCK,
         vloat = testWidgetFloat (this),
         abspos = testWidgetAbsolutelyPositioned (this),
         fixpos = testWidgetFixedlyPositioned (this);
      DBG_OBJ_MSGF("resize", 1,
                   "toplevel: %s, block: %s, float: %s, abspos: %s, fixpos: %s",
                   boolToStr(toplevel), boolToStr(block), boolToStr(vloat),
                   boolToStr(abspos), boolToStr(fixpos));

      // In detail, this depends on what the respective OOFM does with the
      // child widget:
      result = toplevel || (block && !(vloat || abspos || fixpos));
   }

   DBG_OBJ_LEAVE_VAL ("%s", boolToStr(result));
   return result;
}

bool Textblock::isPossibleOOFContainer (int oofmIndex)
{
   return true;
}

bool Textblock::isPossibleOOFContainerParent (int oofmIndex)
{
   return true;
}

RegardingBorder *Textblock::getWidgetRegardingBorderForLine (Line *line)
{
   return getWidgetRegardingBorderForLine (line->firstWord, line->lastWord);
}

RegardingBorder *Textblock::getWidgetRegardingBorderForLine (int lineNo)
{
   // Can also be used for a line not yet existing.
   int firstWord = lineNo == 0 ? 0 : lines->getRef(lineNo - 1)->lastWord + 1;
   int lastWord = lineNo < lines->size() ?
      lines->getRef(lineNo)->lastWord : words->size() - 1;
   return getWidgetRegardingBorderForLine (firstWord, lastWord);
}

RegardingBorder *Textblock::getWidgetRegardingBorderForLine (int firstWord,
                                                             int lastWord)
{
   DBG_OBJ_ENTER ("resize", 0, "getWidgetRegardingBorderForLine", "%d, %d",
                  firstWord, lastWord);
   DBG_OBJ_MSGF ("resize", 1, "words.size = %d", words->size ());

   RegardingBorder *widgetRegardingBorder = NULL;

   if (firstWord < words->size ()) {
      // Any instance of a subclass of WidgetRegardingBorder is always
      // between two line breaks, and so the first word of the line.
      Word *word = words->getRef (firstWord);

      DBG_MSG_WORD ("resize", 1, "<i>first word:</i> ", firstWord, "");

      if (word->content.type == core::Content::WIDGET_IN_FLOW) {
         Widget *widget = word->content.widget;
         if (widget->instanceOf (RegardingBorder::CLASS_ID) &&
             // Exclude cases where a textblock constitutes a new floats
             // container.
             !isOOFContainer (widget, OOFM_FLOATS))
            widgetRegardingBorder = (RegardingBorder*)widget;
      }
   }

   DBG_OBJ_LEAVE_VAL ("%p", widgetRegardingBorder);
   return widgetRegardingBorder;
}

/**
 * Includes margin, border, and padding.
 */
int Textblock::yOffsetOfLineToBeCreated (int *lastMargin)
{
   // This method does not return an exact result: the position of the
   // new line, which does not yet exist, cannot be calculated, since
   // the top margin of the new line (which collapses either with the
   // top margin of the textblock widget, or the bottom margin of the
   // last line) must be taken into account. However, this method is
   // only called for positioning floats; here, a slight incorrectness
   // does not cause real harm.

   // (Similar applies to the line *height*, which calculated in an
   // iterative way; see wrapWordInFlow. Using the same approach for
   // the *position* is possible, but not worth the increased
   // complexity.)

   DBG_OBJ_ENTER0 ("line.yoffset", 0, "yOffsetOfLineToBeCreated");

   int result;

   if (lines->size () == 0) {
      result = calcVerticalBorder (getStyle()->padding.top,
                                   getStyle()->borderWidth.top + extraSpace.top,
                                   getStyle()->marginTop(), 0, 0);
      if (lastMargin)
         *lastMargin = getStyle()->marginTop();
   } else {
      Line *firstLine = lines->getRef (0), *lastLine = lines->getLastRef ();
      result = calcVerticalBorder (getStyle()->padding.top,
                                   getStyle()->borderWidth.top,
                                   getStyle()->marginTop() + extraSpace.top,
                                   firstLine->borderAscent,
                                   firstLine->marginAscent)
         - firstLine->borderAscent + lastLine->top + lastLine->totalHeight (0);
      if (lastMargin)
         *lastMargin = lastLine->marginDescent - lastLine->borderDescent;
   }

   if (lastMargin)
      DBG_OBJ_LEAVE_VAL ("%d, %d", result, *lastMargin);
   else
      DBG_OBJ_LEAVE_VAL ("%d", result);

   return result;
}

/**
 * Includes margin, border, and padding. Can be used without allocation.
 */
int Textblock::yOffsetOfLineCreated (Line *line)
{
   // Similar applies (regarding exactness) as to yOffsetOfLineToBeCreated.

   DBG_OBJ_ENTER0 ("line.yoffset", 0, "yOffsetOfLineToBeCreated");

   int result;

   Line *firstLine = lines->getRef (0);
   result = calcVerticalBorder (getStyle()->padding.top,
                                getStyle()->borderWidth.top,
                                getStyle()->marginTop() + extraSpace.top,
                                firstLine->borderAscent,
                                firstLine->marginAscent)
      - firstLine->borderAscent + line->top;

   DBG_OBJ_LEAVE_VAL ("%d", result);
   return result;
}

} // namespace dw
