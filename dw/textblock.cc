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


#include "textblock.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <stdio.h>
#include <math.h>

/*
 * Local variables
 */
 /* The tooltip under mouse pointer in current textblock. No ref. hold.
  * (having one per view looks not worth the extra clutter). */
static dw::core::style::Tooltip *hoverTooltip = NULL;



using namespace lout;

namespace dw {

int Textblock::CLASS_ID = -1;

Textblock::Textblock (bool limitTextWidth)
{
   registerName ("dw::Textblock", &CLASS_ID);
   setFlags (BLOCK_LEVEL);
   setFlags (USES_HINTS);
   setButtonSensitive(true);

   hasListitemValue = false;
   innerPadding = 0;
   line1Offset = 0;
   mustQueueResize = false;
   redrawY = 0;
   lastWordDrawn = -1;

   /*
    * The initial sizes of lines and words should not be
    * too high, since this will waste much memory with tables
    * containing many small cells. The few more calls to realloc
    * should not decrease the speed considerably.
    * (Current setting is for minimal memory usage. An interesting fact
    * is that high values decrease speed due to memory handling overhead!)
    * TODO: Some tests would be useful.
    */
   lines = new misc::SimpleVector <Line> (1);
   nonTemporaryLines = 0;
   words = new misc::SimpleVector <Word> (1);
   anchors = new misc::SimpleVector <Anchor> (1);

   //DBG_OBJ_SET_NUM(this, "num_lines", num_lines);

   wrapRef = -1;

   //DBG_OBJ_SET_NUM(this, "last_line_width", last_line_width);
   //DBG_OBJ_SET_NUM(this, "last_line_par_min", last_line_par_min);
   //DBG_OBJ_SET_NUM(this, "last_line_par_max", last_line_par_max);
   //DBG_OBJ_SET_NUM(this, "wrap_ref", wrap_ref);

   hoverLink = -1;

   // random values
   availWidth = 100;
   availAscent = 100;
   availDescent = 0;

   this->limitTextWidth = limitTextWidth;

   for (int layer = 0; layer < core::HIGHLIGHT_NUM_LAYERS; layer++) {
      /* hlStart[layer].index > hlEnd[layer].index means no highlighting */
      hlStart[layer].index = 1;
      hlStart[layer].nChar = 0;
      hlEnd[layer].index = 0;
      hlEnd[layer].nChar = 0;
   }
}

Textblock::~Textblock ()
{
   _MSG("Textblock::~Textblock\n");

   /* make sure not to call a free'd tooltip (very fast overkill) */
   hoverTooltip = NULL;

   for (int i = 0; i < words->size(); i++) {
      Word *word = words->getRef (i);
      if (word->content.type == core::Content::WIDGET)
         delete word->content.widget;
      word->style->unref ();
      word->spaceStyle->unref ();
      word->hyphenStyle->unref ();
   }

   for (int i = 0; i < anchors->size(); i++) {
      Anchor *anchor = anchors->getRef (i);
      /* This also frees the names (see removeAnchor() and related). */
      removeAnchor(anchor->name);
   }

   delete lines;
   delete words;
   delete anchors;

   /* Make sure we don't own widgets anymore. Necessary before call of
      parent class destructor. (???) */
   words = NULL;

   //DBG_OBJ_SET_NUM(this, "num_lines", lines->size ());
}

/**
 * The ascent of a textblock is the ascent of the first line, plus
 * padding/border/margin. This can be used to align the first lines
 * of several textblocks in a horizontal line.
 */
void Textblock::sizeRequestImpl (core::Requisition *requisition)
{
   rewrap ();
   showMissingLines ();

   if (lines->size () > 0) {
      Line *lastLine = lines->getRef (lines->size () - 1);
      requisition->width = lastLine->maxLineWidth;

      PRINTF ("[%p] SIZE_REQUEST: lastLine->maxLineWidth = %d\n",
              this, lastLine->maxLineWidth);

      PRINTF ("[%p] SIZE_REQUEST: lines[0]->boxAscent = %d\n",
              this, lines->getRef(0)->boxAscent);
      PRINTF ("[%p] SIZE_REQUEST: lines[%d]->top = %d\n", 
              this, lines->size () - 1, lastLine->top);
      PRINTF ("[%p] SIZE_REQUEST: lines[%d]->boxAscent = %d\n", 
              this, lines->size () - 1, lastLine->boxAscent);
      PRINTF ("[%p] SIZE_REQUEST: lines[%d]->boxDescent = %d\n", 
              this, lines->size () - 1, lastLine->boxDescent);

      /* Note: the breakSpace of the last line is ignored, so breaks
         at the end of a textblock are not visible. */
      requisition->ascent = lines->getRef(0)->boxAscent;
      requisition->descent = lastLine->top
         + lastLine->boxAscent + lastLine->boxDescent -
         lines->getRef(0)->boxAscent;
   } else {
      requisition->width = 0; // before: lastLineWidth;
      requisition->ascent = 0;
      requisition->descent = 0;
   }

   PRINTF ("[%p] SIZE_REQUEST: inner padding = %d, boxDiffWidth = %d\n",
           this, innerPadding, getStyle()->boxDiffWidth ());

   requisition->width += innerPadding + getStyle()->boxDiffWidth ();
   requisition->ascent += getStyle()->boxOffsetY ();
   requisition->descent += getStyle()->boxRestHeight ();

   if (requisition->width < availWidth)
      requisition->width = availWidth;

   PRINTF ("[%p] SIZE_REQUEST: %d x %d + %d\n", this, requisition->width,
           requisition->ascent, requisition->descent);
}

/**
 * Get the extremes of a word within a textblock.
 */
void Textblock::getWordExtremes (Word *word, core::Extremes *extremes)
{
   if (word->content.type == core::Content::WIDGET) {
      if (word->content.widget->usesHints ())
         word->content.widget->getExtremes (extremes);
      else {
         if (core::style::isPerLength
             (word->content.widget->getStyle()->width)) {
            extremes->minWidth = 0;
            if (word->content.widget->hasContents ())
               extremes->maxWidth = 1000000;
            else
               extremes->maxWidth = 0;
         } else if (core::style::isAbsLength
                    (word->content.widget->getStyle()->width)) {
            /* Fixed lengths are only applied to the content, so we have to
             * add padding, border and margin. */
            extremes->minWidth = extremes->maxWidth =
               core::style::absLengthVal (word->content.widget->getStyle()
                                          ->width)
               + word->style->boxDiffWidth ();
         } else
            word->content.widget->getExtremes (extremes);
      }
   } else {
      extremes->minWidth = word->size.width;
      extremes->maxWidth = word->size.width;
   }
}

void Textblock::getExtremesImpl (core::Extremes *extremes)
{
   core::Extremes wordExtremes;
   Line *line;
   Word *word, *prevWord = NULL;
   int wordIndex, lineIndex;
   int parMax;

   showMissingLines ();

   if (lines->size () == 0) {
      /* empty page */
      extremes->minWidth = 0;
      extremes->maxWidth = 0;

      PRINTF ("GET_EXTREMES: empty (but %d words)\n", words->size());
   } else if (wrapRef == -1) {
      /* no rewrap necessary -> values in lines are up to date */
      line = lines->getRef (lines->size () - 1);
      extremes->minWidth = line->maxParMin;
      extremes->maxWidth = line->maxParMax;

      PRINTF ("GET_EXTREMES: no rewrap => %d, %d\n",
              line->maxParMin, line->maxParMax);
   } else {
      /* Calculate the extremes, based on the values in the line from
         where a rewrap is necessary. */

      PRINTF ("GET_EXTREMES: complex case ...\n");

      if (wrapRef == 0) {
         extremes->minWidth = 0;
         extremes->maxWidth = 0;
         parMax = 0;
      } else {
         line = lines->getRef (wrapRef);
         extremes->minWidth = line->maxParMin;
         extremes->maxWidth = line->maxParMax;
         parMax = line->parMax;
      }

      int prevWordSpace = 0;
      for (lineIndex = wrapRef; lineIndex < lines->size (); lineIndex++) {
         line = lines->getRef (lineIndex);
         int parMin = 0;

         for (wordIndex = line->firstWord; wordIndex <= line->lastWord;
              wordIndex++) {
            word = words->getRef (wordIndex);

            getWordExtremes (word, &wordExtremes);
            if (wordIndex == 0) {
               wordExtremes.minWidth += line1Offset;
               wordExtremes.maxWidth += line1Offset;
            }

            extremes->minWidth = misc::max (extremes->minWidth,
                                            wordExtremes.minWidth);

            if (word->content.type != core::Content::BREAK)
               parMax += prevWordSpace;
            parMax += wordExtremes.maxWidth;

            if (prevWord && !prevWord->badnessAndPenalty.lineMustBeBroken ())
               parMin += prevWordSpace + wordExtremes.minWidth;
            else
               parMin = wordExtremes.minWidth;

            if (extremes->minWidth < parMin)
               extremes->minWidth = parMin;

            prevWordSpace = word->origSpace;
            prevWord = word;
         }

         if ((words->getRef(line->lastWord)->content.type
              == core::Content::BREAK ) ||
             lineIndex == lines->size () - 1 ) {
            extremes->maxWidth = misc::max (extremes->maxWidth, parMax);
            prevWordSpace = 0;
            parMax = 0;
         }
      }

   }

   int diff = innerPadding + getStyle()->boxDiffWidth ();
   extremes->minWidth += diff;
   extremes->maxWidth += diff;
}


void Textblock::sizeAllocateImpl (core::Allocation *allocation)
{
   PRINTF ("SIZE_ALLOCATE: %d, %d, %d x %d + %d\n",
           allocation->x, allocation->y, allocation->width,
           allocation->ascent, allocation->descent);

   int lineIndex, wordIndex;
   Line *line;
   Word *word;
   int xCursor;
   core::Allocation childAllocation;
   core::Allocation *oldChildAllocation;

   if (allocation->width != this->allocation.width) {
      redrawY = 0;
   }

   for (lineIndex = 0; lineIndex < lines->size (); lineIndex++) {
      line = lines->getRef (lineIndex);
      xCursor = lineXOffsetWidget (line);

      for (wordIndex = line->firstWord; wordIndex <= line->lastWord;
           wordIndex++) {
         word = words->getRef (wordIndex);

         if (wordIndex == lastWordDrawn + 1) {
            redrawY = misc::min (redrawY, lineYOffsetWidget (line));
         }

         if (word->content.type == core::Content::WIDGET) {
            /** \todo Justification within the line is done here. */
            childAllocation.x = xCursor + allocation->x;
            /* align=top:
               childAllocation.y = line->top + allocation->y;
            */

            /* align=bottom (base line) */
            /* Commented lines break the n2 and n3 test cases at
             * http://www.dillo.org/test/img/ */
            childAllocation.y =
               lineYOffsetCanvasAllocation (line, allocation)
               + (line->boxAscent - word->size.ascent)
               - word->content.widget->getStyle()->margin.top;
            childAllocation.width = word->size.width;
            childAllocation.ascent = word->size.ascent
               + word->content.widget->getStyle()->margin.top;
            childAllocation.descent = word->size.descent
               + word->content.widget->getStyle()->margin.bottom;

            oldChildAllocation = word->content.widget->getAllocation();

            if (childAllocation.x != oldChildAllocation->x ||
               childAllocation.y != oldChildAllocation->y ||
               childAllocation.width != oldChildAllocation->width) {
               /* The child widget has changed its position or its width
                * so we need to redraw from this line onwards.
                */
               redrawY = misc::min (redrawY, lineYOffsetWidget (line));
               if (word->content.widget->wasAllocated ()) {
                  redrawY = misc::min (redrawY,
                     oldChildAllocation->y - this->allocation.y);
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
               } else {
                  redrawY = misc::min (redrawY, lineYOffsetWidget (line));
               }
            }
            word->content.widget->sizeAllocate (&childAllocation);
         }

         xCursor += (word->size.width + word->effSpace);
      }
   }

   for (int i = 0; i < anchors->size(); i++) {
      Anchor *anchor = anchors->getRef(i);
      int y;

      if (anchor->wordIndex >= words->size()) {
         y = allocation->y + allocation->ascent + allocation->descent;
      } else {
         Line *line = lines->getRef(findLineOfWord (anchor->wordIndex));
         y = lineYOffsetCanvasAllocation (line, allocation);
      }
      changeAnchor (anchor->name, y);
   }
}

void Textblock::resizeDrawImpl ()
{
   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   if (lines->size () > 0) {
      Line *lastLine = lines->getRef (lines->size () - 1);
      /* Remember the last word that has been drawn so we can ensure to
       * draw any new added words (see sizeAllocateImpl()).
       */
      lastWordDrawn = lastLine->lastWord;
   }

   redrawY = getHeight ();
}

void Textblock::markSizeChange (int ref)
{
   markChange (ref);
}

void Textblock::markExtremesChange (int ref)
{
   markChange (ref);
}

/*
 * Implementation for both mark_size_change and mark_extremes_change.
 */
void Textblock::markChange (int ref)
{
   /* By the way: ref == -1 may have two different causes: (i) flush()
      calls "queueResize (-1, true)", when no rewrapping is necessary;
      and (ii) a word may have parentRef == -1 , when it is not yet
      added to a line.  In the latter case, nothing has to be done
      now, but addLine(...) will do everything neccessary. */
   if (ref != -1) {
      if (wrapRef == -1)
         wrapRef = ref;
      else
         wrapRef = misc::min (wrapRef, ref);
   }

   PRINTF ("[%p] MARK_CHANGE (%d) => %d\n", this, ref, wrapRef);
}

void Textblock::setWidth (int width)
{
   /* If limitTextWidth is set to YES, a queueResize() may also be
    * necessary. */
   if (availWidth != width || limitTextWidth) {
      //DEBUG_MSG(DEBUG_REWRAP_LEVEL,
      //          "setWidth: Calling queueResize, "
      //          "in page with %d word(s)\n",
      //          words->size());

      availWidth = width;
      queueResize (0, false);
      mustQueueResize = false;
      redrawY = 0;
   }
}

void Textblock::setAscent (int ascent)
{
   if (availAscent != ascent) {
      //DEBUG_MSG(DEBUG_REWRAP_LEVEL,
      //          "setAscent: Calling queueResize, "
      //          "in page with %d word(s)\n",
      //          words->size());

      availAscent = ascent;
      queueResize (0, false);
      mustQueueResize = false;
   }
}

void Textblock::setDescent (int descent)
{
   if (availDescent != descent) {
      //DEBUG_MSG(DEBUG_REWRAP_LEVEL,
      //          "setDescent: Calling queueResize, "
      //          "in page with %d word(s)\n",
      //          words->size());

      availDescent = descent;
      queueResize (0, false);
      mustQueueResize = false;
   }
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
   core::Iterator *it;
   int wordIndex;
   int charPos = 0, link = -1;
   bool r;

   if (words->size () == 0) {
      wordIndex = -1;
   } else {
      Line *lastLine = lines->getRef (lines->size () - 1);
      int yFirst = lineYOffsetCanvasI (0);
      int yLast = lineYOffsetCanvas (lastLine) + lastLine->boxAscent +
                  lastLine->boxDescent;
      if (event->yCanvas < yFirst) {
         // Above the first line: take the first word.
         wordIndex = 0;
      } else if (event->yCanvas >= yLast) {
         // Below the last line: take the last word.
         wordIndex = words->size () - 1;
         charPos = core::SelectionState::END_OF_WORD;
      } else {
         Line *line = lines->getRef (findLineIndex (event->yWidget));

         // Pointer within the break space?
         if (event->yWidget >
             (lineYOffsetWidget (line) + line->boxAscent + line->boxDescent)) {
            // Choose this break.
            wordIndex = line->lastWord;
            charPos = core::SelectionState::END_OF_WORD;
         } else if (event->xWidget < lineXOffsetWidget (line)) {
            // Left of the first word in the line.
            wordIndex = line->firstWord;
         } else {
            int nextWordStartX = lineXOffsetWidget (line);

            for (wordIndex = line->firstWord;
                 wordIndex <= line->lastWord;
                 wordIndex++) {
               Word *word = words->getRef (wordIndex);
               int wordStartX = nextWordStartX;

               nextWordStartX += word->size.width + word->effSpace;

               if (event->xWidget >= wordStartX &&
                   event->xWidget < nextWordStartX) {
                  // We have found the word.
                  int yWidgetBase = lineYOffsetWidget (line) + line->boxAscent;

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

                        while (1) {
                           int nextCharPos =
                              layout->nextGlyph (word->content.text, charPos);
                           int glyphWidth =
                              textWidth (word->content.text, charPos,
                                         nextCharPos - charPos, word->style);
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
                                                word->style))
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
   it = new TextblockIterator (this, core::Content::SELECTION_CONTENT,
                               wordIndex);
   r = selectionHandleEvent (eventType, it, charPos, link, event);
   it->unref ();
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


/**
 * Calculate the size of a widget within the page.
 * (Subject of change in the near future!)
 */
void Textblock::calcWidgetSize (core::Widget *widget, core::Requisition *size)
{
   core::Requisition requisition;
   int availWidth, availAscent, availDescent;
   core::style::Style *wstyle = widget->getStyle();

   /* We ignore line1_offset[_eff]. */
   availWidth = this->availWidth - getStyle()->boxDiffWidth () - innerPadding;
   availAscent = this->availAscent - getStyle()->boxDiffHeight ();
   availDescent = this->availDescent;

   if (widget->usesHints ()) {
      widget->setWidth (availWidth);
      widget->setAscent (availAscent);
      widget->setDescent (availDescent);
      widget->sizeRequest (size);
   } else {
      if (wstyle->width == core::style::LENGTH_AUTO ||
          wstyle->height == core::style::LENGTH_AUTO)
         widget->sizeRequest (&requisition);

      if (wstyle->width == core::style::LENGTH_AUTO)
         size->width = requisition.width;
      else if (core::style::isAbsLength (wstyle->width))
         /* Fixed lengths are only applied to the content, so we have to
          * add padding, border and margin. */
         size->width = core::style::absLengthVal (wstyle->width)
            + wstyle->boxDiffWidth ();
      else
         size->width = (int) (core::style::perLengthVal (wstyle->width)
                       * availWidth);

      if (wstyle->height == core::style::LENGTH_AUTO) {
         size->ascent = requisition.ascent;
         size->descent = requisition.descent;
      } else if (core::style::isAbsLength (wstyle->height)) {
         /* Fixed lengths are only applied to the content, so we have to
          * add padding, border and margin. */
         size->ascent = core::style::absLengthVal (wstyle->height)
                        + wstyle->boxDiffHeight ();
         size->descent = 0;
      } else {
         double len = core::style::perLengthVal (wstyle->height);
         size->ascent = (int) (len * availAscent);
         size->descent = (int) (len * availDescent);
      }
   }

   /* ascent and descent in words do not contain margins. */
   size->ascent -= wstyle->margin.top;
   size->descent -= wstyle->margin.bottom;
}

/*
 * Draw the decorations on a word.
 */
void Textblock::decorateText(core::View *view, core::style::Style *style,
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
 */
void Textblock::drawText(core::View *view, core::style::Style *style,
                         core::style::Color::Shading shading, int x, int y,
                         const char *text, int start, int len)
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
         {
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
            break;
         }
      }
      view->drawText(style->font, style->color, shading, x, y,
                     str ? str : text + start, str ? strlen(str) : len);
      if (str)
         free(str);
   }
}

/*
 * Draw a word of text. TODO New description;
 */
void Textblock::drawWord (Line *line, int wordIndex1, int wordIndex2,
                          core::View *view, core::Rectangle *area,
                          int xWidget, int yWidgetBase)
{
   core::style::Style *style = words->getRef(wordIndex1)->style;
   bool drawHyphen = wordIndex2 == line->lastWord
      && words->getRef(wordIndex2)->hyphenWidth > 0;

   if (wordIndex1 == wordIndex2 && !drawHyphen) {
      // Simple case, where copying in one buffer is not needed.
      Word *word = words->getRef (wordIndex1);
      drawWord0 (wordIndex1, wordIndex2, word->content.text, word->size.width,
                 style, view, area, xWidget, yWidgetBase);
   } else {
      // Concaternate all words in a new buffer.
      int l = 0, totalWidth = 0;
      for (int i = wordIndex1; i <= wordIndex2; i++) {
         Word *w = words->getRef (i);
         l += strlen (w->content.text);
         totalWidth += w->size.width;
      }
      
      char text[l + (drawHyphen ? 2 : 0) + 1];
      int p = 0;
      for (int i = wordIndex1; i <= wordIndex2; i++) {
         const char * t = words->getRef(i)->content.text;
         strcpy (text + p, t);
         p += strlen (t);
      }

      if(drawHyphen) {
         text[p++] = 0xc2;
         text[p++] = 0xad;
         text[p++] = 0;
      }
      
      drawWord0 (wordIndex1, wordIndex2, text, totalWidth,
                 style, view, area, xWidget, yWidgetBase);
   }
}

/**
 * TODO Comment
 */
void Textblock::drawWord0 (int wordIndex1, int wordIndex2,
                           const char *text, int totalWidth,
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

   drawText (view, style, core::style::Color::SHADING_NORMAL, xWorld,
             yWorldBase, text, 0, strlen (text));

   if (style->textDecoration)
      decorateText(view, style, core::style::Color::SHADING_NORMAL, xWorld,
                   yWorldBase, totalWidth);

   for (int layer = 0; layer < core::HIGHLIGHT_NUM_LAYERS; layer++) {
      if (hlStart[layer].index <= wordIndex1 &&
          hlEnd[layer].index >= wordIndex2) {
         const int wordLen = strlen (text);
         int xStart, width;
         int firstCharIdx = 0;
         int lastCharIdx = wordLen;

         if (wordIndex1 == hlStart[layer].index)
            firstCharIdx = misc::min (hlStart[layer].nChar, wordLen);

         if (wordIndex2 == hlEnd[layer].index)
            lastCharIdx = misc::min (hlEnd[layer].nChar, wordLen);

         xStart = xWorld;
         if (firstCharIdx)
            xStart += textWidth (text, 0, firstCharIdx, style);
         if (firstCharIdx == 0 && lastCharIdx == wordLen)
            width = totalWidth;
         else
            width = textWidth (text, firstCharIdx,
                               lastCharIdx - firstCharIdx, style);
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
                      lastCharIdx - firstCharIdx);

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
void Textblock::drawLine (Line *line, core::View *view, core::Rectangle *area)
{
   int xWidget = lineXOffsetWidget(line);
   int yWidgetBase = lineYOffsetWidget (line) + line->boxAscent;

   for (int wordIndex = line->firstWord;
        wordIndex <= line->lastWord
           /* TODO && xWidget < area->x + area->width*/;
        wordIndex++) {
      Word *word = words->getRef(wordIndex);
      int wordSize = word->size.width;

      if (xWidget + word->size.width + word->effSpace >= area->x) {
         if (word->content.type == core::Content::TEXT ||
             word->content.type == core::Content::WIDGET) {

            if (word->size.width > 0) {
               if (word->content.type == core::Content::WIDGET) {
                  core::Widget *child = word->content.widget;
                  core::Rectangle childArea;

                  if (child->intersects (area, &childArea))
                     child->draw (view, &childArea);
               } else {
                  /* TODO: include in drawWord:
                  if (word->style->hasBackground ()) {
                     drawBox (view, word->style, area, xWidget,
                              yWidgetBase - line->boxAscent, word->size.width,
                              line->boxAscent + line->boxDescent, false);
                  }*/
                  int wordIndex2 = wordIndex;
                  while (wordIndex2 < line->lastWord &&
                         words->getRef(wordIndex2)->hyphenWidth > 0 &&
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
                           xWidget + word->size.width,
                           yWidgetBase - line->boxAscent, word->effSpace,
                           line->boxAscent + line->boxDescent, false);
               drawSpace(wordIndex, view, area, xWidget + word->size.width,
                         yWidgetBase);
            }

         }
      }
      xWidget += wordSize + word->effSpace;
   }
}

/**
 * Find the first line index that includes y, relative to top of widget.
 */
int Textblock::findLineIndex (int y)
{
   int maxIndex = lines->size () - 1;
   int step, index, low = 0;

   step = (lines->size() + 1) >> 1;
   while ( step > 1 ) {
      index = low + step;
      if (index <= maxIndex &&
          lineYOffsetWidgetI (index) <= y)
         low = index;
      step = (step + 1) >> 1;
   }

   if (low < maxIndex && lineYOffsetWidgetI (low + 1) <= y)
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
   return low;
}

/**
 * \brief Find the line of word \em wordIndex.
 */
int Textblock::findLineOfWord (int wordIndex)
{
   int high = lines->size () - 1, index, low = 0;

   if (wordIndex < 0 || wordIndex >= words->size ())
      return -1;

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
 * \brief Find the index of the word, or -1.
 */
Textblock::Word *Textblock::findWord (int x, int y, bool *inSpace)
{
   int lineIndex, wordIndex;
   int xCursor, lastXCursor, yWidgetBase;
   Line *line;
   Word *word;

   *inSpace = false;

   if ((lineIndex = findLineIndex (y)) >= lines->size ())
      return NULL;
   line = lines->getRef (lineIndex);
   yWidgetBase = lineYOffsetWidget (line) + line->boxAscent;
   if (yWidgetBase + line->boxDescent <= y)
      return NULL;

   xCursor = lineXOffsetWidget (line);
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

void Textblock::draw (core::View *view, core::Rectangle *area)
{
   PRINTF ("DRAW: %d, %d, %d x %d\n",
           area->x, area->y, area->width, area->height);

   int lineIndex;
   Line *line;

   drawWidgetBox (view, area, false);

   lineIndex = findLineIndex (area->y);

   for (; lineIndex < lines->size (); lineIndex++) {
      line = lines->getRef (lineIndex);
      if (lineYOffsetWidget (line) >= area->y + area->height)
         break;

      drawLine (line, view, area);
   }
}

/**
 * Add a new word (text, widget etc.) to a page.
 */
Textblock::Word *Textblock::addWord (int width, int ascent, int descent,
                                     core::style::Style *style)
{
   Word *word;

   words->increase ();

   word = words->getRef (words->size() - 1);
   word->size.width = width;
   word->size.ascent = ascent;
   word->size.descent = descent;
   word->origSpace = word->effSpace = word->stretchability =
      word->shrinkability = 0;
   word->hyphenWidth = 0;
   word->badnessAndPenalty.setPenaltyProhibitBreak ();
   word->content.space = false;

   //DBG_OBJ_ARRSET_NUM (this, "words.%d.size.width", words->size() - 1,
   //                    word->size.width);
   //DBG_OBJ_ARRSET_NUM (this, "words.%d.size.descent", words->size() - 1,
   //                    word->size.descent);
   //DBG_OBJ_ARRSET_NUM (this, "words.%d.size.ascent", words->size() - 1,
   //                    word->size.ascent);
   //DBG_OBJ_ARRSET_NUM (this, "words.%d.orig_space", words->size() - 1,
   //                    word->orig_space);
   //DBG_OBJ_ARRSET_NUM (this, "words.%d.effSpace", words->size() - 1,
   //                    word->eff_space);
   //DBG_OBJ_ARRSET_NUM (this, "words.%d.content.space", words->size() - 1,
   //                    word->content.space);

   word->style = style;
   word->spaceStyle = style;
   word->hyphenStyle = style;
   style->ref ();
   style->ref ();
   style->ref ();

   return word;
}

/*
 * Get the width of a string of text.
 */
int Textblock::textWidth(const char *text, int start, int len,
                         core::style::Style *style)
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
         {
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
            break;
         }
      }
      if (str)
         free(str);
   }
   return ret;
}

/**
 * Calculate the size of a text word.
 */
void Textblock::calcTextSize (const char *text, size_t len,
                              core::style::Style *style,
                              core::Requisition *size)
{
   size->width = textWidth (text, 0, len, style);
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
         height = lout::misc::roundInt (
                     core::style::perLengthVal(style->lineHeight) *
                     style->font->size);
      leading = height - style->font->size;

      size->ascent += leading / 2;
      size->descent += leading - (leading / 2);
   }

   /* In case of a sub or super script we increase the word's height and
    * potentially the line's height.
    */
   if (style->valign == core::style::VALIGN_SUB)
      size->descent += (style->font->ascent / 3);
   else if (style->valign == core::style::VALIGN_SUPER)
      size->ascent += (style->font->ascent / 2);
}


/**
 * Add a word to the page structure. If it contains soft hyphens, it is
 * divided.
 */
void Textblock::addText (const char *text, size_t len,
                         core::style::Style *style)
{
   // Count hyphens.
   int numHyphens = 0;
   for (size_t i = 0; i < len - 1; i++)
      // (0xc2, 0xad) is the UTF-8 representation of a soft hyphen (Unicode
      // 0xc2).
      if((unsigned char)text[i] == 0xc2 && (unsigned char)text[i + 1] == 0xad)
         numHyphens++;

   if (numHyphens == 0) {
      // Simple (and often) case: no soft hyphens.
      core::Requisition size;
      calcTextSize (text, len, style, &size);
      addText0 (text, len, style, &size);
   } else {
      PRINTF("HYPHENATION: '");
      for (size_t i = 0; i < len; i++)
         PUTCHAR(text[i]);
      PRINTF("', with %d hyphen(s)\n", numHyphens);

      // Store hyphen positions.
      int n = 0, hyphenPos[numHyphens];
      for (size_t i = 0; i < len - 1; i++)
         if((unsigned char)text[i] == 0xc2 &&
            (unsigned char)text[i + 1] == 0xad)
            hyphenPos[n++] = i;

      // Get text without hyphens. (There are numHyphens + 1 parts in the word,
      // and 2 * numHyphens bytes less, 2 for each hyphen, are needed.)
      char textWithoutHyphens[len - 2 * numHyphens];
      int start = 0; // related to "text"
      for (int i = 0; i < numHyphens + 1; i++) {
         int end = (i == numHyphens) ? len : hyphenPos[i];
         memmove (textWithoutHyphens + start - 2 * i, text + start,
                  end - start);
         start = end + 2;
      }

      PRINTF("H... without hyphens: '");
      for (size_t i = 0; i < len - 2 * numHyphens; i++)
         PUTCHAR(textWithoutHyphens[i]);
      PRINTF("'\n");

      // Calc sizes.
      core::Requisition wordSize[numHyphens + 1];

      // The size of the last part is calculated in a simple way.
      int lastStart = hyphenPos[numHyphens - 1] + 2;
      calcTextSize (text + lastStart, len - lastStart, style,
                    &wordSize[numHyphens]);

      PRINTF("H... [%d] '", numHyphens);
      for (size_t i = 0; i < len - lastStart; i++)
         PUTCHAR(text[i + lastStart]);
      PRINTF("' -> %d\n", wordSize[numHyphens].width);
      
      // The rest is more complicated. TODO Documentation.
      for (int i = numHyphens - 1; i >= 0; i--) {
         int start = (i == 0) ? 0 : hyphenPos[i - 1] - 2 * (i - 1);
         calcTextSize (textWithoutHyphens + start,
                       len - 2 * numHyphens - start, style, &wordSize[i]);

         PRINTF("H... [%d] '", i);
         for (size_t j = 0; j < len - 2 * numHyphens - start; j++)
            PUTCHAR(textWithoutHyphens[j + start]);
         PRINTF("' -> %d\n", wordSize[i].width);

         for (int j = i + 1; j < numHyphens + 1; j++) {
            wordSize[i].width -= wordSize[j].width;
            PRINTF("H...    - %d = %d\n", wordSize[j].width, wordSize[i].width);
         }
      }
 
      // Finished!
      for (int i = 0; i < numHyphens + 1; i++) {
         int start = (i == 0) ? 0 : hyphenPos[i - 1] + 2;
         int end = (i == numHyphens) ? len : hyphenPos[i];
         addText0 (text + start, end - start, style, &wordSize[i]);

         PRINTF("H... [%d] '", i);
         for (int j = start; j < end; j++)
            PUTCHAR(text[j]);
         PRINTF("' added\n");

         if(i < numHyphens) {
            addHyphen (style);
            PRINTF("H... yphen added\n");
         }
      }
   }
}

/**
 * Add a word (without hyphens) to the page structure.
 */
void Textblock::addText0 (const char *text, size_t len,
                          core::style::Style *style, core::Requisition *size)
{
   Word *word;

   word = addWord (size->width, size->ascent, size->descent, style);
   word->content.type = core::Content::TEXT;
   word->content.text = layout->textZone->strndup(text, len);

   //DBG_OBJ_ARRSET_STR (page, "words.%d.content.text", words->size() - 1,
   //                    word->content.text);

   wordWrap (words->size () - 1, false);
}

/**
 * Add a widget (word type) to the page.
 */
void Textblock::addWidget (core::Widget *widget, core::style::Style *style)
{
   Word *word;
   core::Requisition size;

   /* We first assign -1 as parent_ref, since the call of widget->size_request
    * will otherwise let this Textblock be rewrapped from the beginning.
    * (parent_ref is actually undefined, but likely has the value 0.) At the,
    * end of this function, the correct value is assigned. */
   widget->parentRef = -1;

   PRINTF ("%p becomes child of %p\n", widget, this);
   
   widget->setParent (this);
   widget->setStyle (style);

   calcWidgetSize (widget, &size);
   word = addWord (size.width, size.ascent, size.descent, style);

   word->content.type = core::Content::WIDGET;
   word->content.widget = widget;

   //DBG_OBJ_ARRSET_PTR (page, "words.%d.content.widget", words->size() - 1,
   //                    word->content.widget);

   wordWrap (words->size () - 1, false);
   //DBG_OBJ_SET_NUM (word->content.widget, "parent_ref",
   //                 word->content.widget->parent_ref);

   //DEBUG_MSG(DEBUG_REWRAP_LEVEL,
   //          "Assigning parent_ref = %d to added word %d, "
   //          "in page with %d word(s)\n",
   //          lines->size () - 1, words->size() - 1, words->size());
}

/**
 * Add an anchor to the page. "name" is copied, so no strdup is necessary for
 * the caller.
 *
 * Return true on success, and false, when this anchor had already been
 * added to the widget tree.
 */
bool Textblock::addAnchor (const char *name, core::style::Style *style)
{
   char *copy;
   int y;

   // Since an anchor does not take any space, it is safe to call
   // addAnchor already here.
   if (wasAllocated ()) {
      if (lines->size () == 0)
         y = allocation.y;
      else
         y = allocation.y + lineYOffsetWidgetI (lines->size () - 1);
      copy = Widget::addAnchor (name, y);
   } else
      copy = Widget::addAnchor (name);

   if (copy == NULL)
      /**
       * \todo It may be necessary for future uses to save the anchor in
       *    some way, e.g. when parts of the widget tree change.
       */
      return false;
   else {
      Anchor *anchor;

      anchors->increase();
      anchor = anchors->getRef(anchors->size() - 1);
      anchor->name = copy;
      anchor->wordIndex = words->size();
      return true;
   }
}


/**
 * ?
 */
void Textblock::addSpace (core::style::Style *style)
{
   int wordIndex = words->size () - 1;

   if (wordIndex >= 0) {
      Word *word = words->getRef(wordIndex);

      // According to http://www.w3.org/TR/CSS2/text.html#white-space-model:
      // "line breaking opportunities are determined based on the text prior
      //  to the white space collapsing steps".
      // So we call addBreakOption () for each Textblock::addSpace () call.
      // This is important e.g. to be able to break between foo and bar in:
      // <span style="white-space:nowrap">foo </span> bar
      addBreakOption (style);

      if (!word->content.space) {
         word->badnessAndPenalty.setPenalty (0);
         word->content.space = true;
         word->effSpace = word->origSpace = style->font->spaceWidth +
                                            style->wordSpacing;
         word->stretchability = word->origSpace / 2;
         if(style->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
            word->shrinkability = word->origSpace / 3;
         else
            word->shrinkability = 0;

         //DBG_OBJ_ARRSET_NUM (this, "words.%d.origSpace", wordIndex,
         //                    word->origSpace);
         //DBG_OBJ_ARRSET_NUM (this, "words.%d.effSpace", wordIndex,
         //                    word->effSpace);
         //DBG_OBJ_ARRSET_NUM (this, "words.%d.content.space", wordIndex,
         //                    word->content.space);

         word->spaceStyle->unref ();
         word->spaceStyle = style;
         style->ref ();

         accumulateWordData (wordIndex);
      }
   }
}

void Textblock::addHyphen (core::style::Style *style)
{
   int wordIndex = words->size () - 1;

   if (wordIndex >= 0) {
      Word *word = words->getRef(wordIndex);
 
      word->badnessAndPenalty.setPenalty (HYPHEN_BREAK);
      //word->penalty = 0;
      // TODO Optimize? Like spaces?
      word->hyphenWidth = layout->textWidth (style->font, "\xc2\xad", 2);

      word->hyphenStyle->unref ();
      word->hyphenStyle = style;
      style->ref ();

      accumulateWordData (wordIndex);
   }
}

/**
 * Cause a paragraph break
 */
void Textblock::addParbreak (int space, core::style::Style *style)
{
   Word *word;

   /* A break may not be the first word of a page, or directly after
      the bullet/number (which is the first word) in a list item. (See
      also comment in Dw_page_size_request.) */
   if (words->size () == 0 ||
       (hasListitemValue && words->size () == 1)) {
      /* This is a bit hackish: If a break is added as the
         first/second word of a page, and the parent widget is also a
         Textblock, and there is a break before -- this is the case when
         a widget is used as a text box (lists, blockquotes, list
         items etc) -- then we simply adjust the break before, in a
         way that the space is in any case visible. */
      Widget *widget;

      /* Find the widget where to adjust the breakSpace. */
      for (widget = this;
           widget->getParent() &&
              widget->getParent()->instanceOf (Textblock::CLASS_ID);
           widget = widget->getParent ()) {
         Textblock *textblock2 = (Textblock*)widget->getParent ();
         int index = textblock2->hasListitemValue ? 1 : 0;
         bool isfirst = (textblock2->words->getRef(index)->content.type
                         == core::Content::WIDGET
                         && textblock2->words->getRef(index)->content.widget
                         == widget);
         if (!isfirst) {
            /* The page we searched for has been found. */
            Word *word2;
            int lineno = widget->parentRef;

            if (lineno > 0 &&
                (word2 =
                 textblock2->words->getRef(textblock2->lines
                                           ->getRef(lineno - 1)->firstWord)) &&
                word2->content.type == core::Content::BREAK) {
               if (word2->content.breakSpace < space) {
                  word2->content.breakSpace = space;
                  textblock2->queueResize (lineno, false);
                  textblock2->mustQueueResize = false;
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
                    lastLine->marginDescent - lastLine->boxDescent,
                    lastLine->breakSpace);
      return;
   }

   word = addWord (0, 0, 0, style);
   word->content.type = core::Content::BREAK;
   word->badnessAndPenalty.setPenaltyForceBreak ();
   word->content.breakSpace = space;
   wordWrap (words->size () - 1, false);
}

/*
 * Cause a line break.
 */
void Textblock::addLinebreak (core::style::Style *style)
{
   Word *word;

   if (words->size () == 0 ||
       words->getRef(words->size () - 1)->content.type == core::Content::BREAK)
      // An <BR> in an empty line gets the height of the current font
      // (why would someone else place it here?), ...
      word = addWord (0, style->font->ascent, style->font->descent, style);
   else
      // ... otherwise, it has no size (and does not enlarge the line).
      word = addWord (0, 0, 0, style);

   word->content.type = core::Content::BREAK;
   word->badnessAndPenalty.setPenaltyForceBreak ();
   word->content.breakSpace = 0;
   wordWrap (words->size () - 1, false);
}


/**
 * \brief Search recursively through widget.
 *
 * This is an optimized version of the general
 * dw::core::Widget::getWidgetAtPoint method.
 */
core::Widget  *Textblock::getWidgetAtPoint(int x, int y, int level)
{
   int lineIndex, wordIndex;
   Line *line;

   if (x < allocation.x ||
       y < allocation.y ||
       x > allocation.x + allocation.width ||
       y > allocation.y + getHeight ()) {
      return NULL;
   }

   lineIndex = findLineIndex (y - allocation.y);

   if (lineIndex < 0 || lineIndex >= lines->size ()) {
      return this;
   }

   line = lines->getRef (lineIndex);

   for (wordIndex = line->firstWord; wordIndex <= line->lastWord;wordIndex++) {
      Word *word =  words->getRef (wordIndex);

      if (word->content.type == core::Content::WIDGET) {
         core::Widget * childAtPoint;
         childAtPoint = word->content.widget->getWidgetAtPoint (x, y,
                                                                level + 1);
         if (childAtPoint) {
            return childAtPoint;
         }
      }
   }

   return this;
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
          parent->instanceOf (Textblock::CLASS_ID)) {
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
   PRINTF ("[%p] FLUSH => %s (parentRef = %d)\n",
           this, mustQueueResize ? "true" : "false", parentRef);

   if (mustQueueResize) {
      queueResize (-1, true);
      mustQueueResize = false;
   }
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
               word->style = core::style::Style::create (layout, &styleAttrs);
               old_style->unref();
               old_style = word->spaceStyle;
               styleAttrs = *old_style;
               styleAttrs.color = core::style::Color::create (layout,
                                                              newColor);
               word->spaceStyle =
                               core::style::Style::create(layout, &styleAttrs);
               old_style->unref();
               break;
            }
            case core::Content::WIDGET:
            {  core::Widget *widget = word->content.widget;
               styleAttrs = *widget->getStyle();
               styleAttrs.color = core::style::Color::create (layout,
                                                              newColor);
               styleAttrs.setBorderColor(
                           core::style::Color::create (layout, newColor));
               widget->setStyle(
                             core::style::Style::create (layout, &styleAttrs));
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
                        line->boxAscent + line->boxDescent);
   }
}

void Textblock::changeWordStyle (int from, int to, core::style::Style *style,
                                 bool includeFirstSpace, bool includeLastSpace)
{
}

// ----------------------------------------------------------------------

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

int Textblock::TextblockIterator::compareTo(misc::Comparable *other)
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

   textblock->queueDrawRange (index1, index2);
}

void Textblock::TextblockIterator::unhighlight (int direction,
                                                core::HighlightLayer layer)
{
   Textblock *textblock = (Textblock*)getWidget();
   int index1 = index, index2 = index;

   if (textblock->hlStart[layer].index > textblock->hlEnd[layer].index)
      return;

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

   textblock->queueDrawRange (index1, index2);
}

void Textblock::queueDrawRange (int index1, int index2)
{
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
      int y = lineYOffsetWidget (line1) + line1->boxAscent -
              line1->contentAscent;
      int h = lineYOffsetWidget (line2) + line2->boxAscent +
              line2->contentDescent - y;

      queueDrawArea (0, y, allocation.width, h);
   }
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
                                             word->style);
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
                                  word->style);
      }
   }
   allocation->ascent = word->size.ascent;
   allocation->descent = word->size.descent;
}

} // namespace dw
