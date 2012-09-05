#include "textblock.hh"
#include "hyphenator.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <stdio.h>
#include <math.h>

using namespace lout;

namespace dw {

int Textblock::BadnessAndPenalty::badnessValue (int infLevel)
{
   switch (badnessState) {
   case NOT_STRETCHABLE:
      return infLevel == INF_NOT_STRETCHABLE ? 1 : 0;

   case QUITE_LOOSE:
      return infLevel == INF_LARGE ? 1 : 0;

   case BADNESS_VALUE:
      return infLevel == INF_VALUE ? badness : 0;

   case TOO_TIGHT:
      return infLevel == INF_TOO_TIGHT ? 1 : 0;
   }

   // compiler happiness
   lout::misc::assertNotReached ();
   return 0;
}

int Textblock::BadnessAndPenalty::penaltyValue (int infLevel)
{
   switch (penaltyState) {
   case FORCE_BREAK:
      return infLevel == INF_PENALTIES ? -1 : 0;

   case PROHIBIT_BREAK:
      return infLevel == INF_PENALTIES ? 1 : 0;

   case PENALTY_VALUE:
      return  infLevel == INF_VALUE ? penalty : 0;
   }

   // compiler happiness
   lout::misc::assertNotReached ();
   return 0;
}

void Textblock::BadnessAndPenalty::calcBadness (int totalWidth, int idealWidth,
                                                int totalStretchability,
                                                int totalShrinkability)
{
   this->totalWidth = totalWidth;
   this->idealWidth = idealWidth;
   this->totalStretchability = totalStretchability;
   this->totalShrinkability = totalShrinkability;

   ratio = 0; // because this is used in print()

   if (totalWidth == idealWidth) {
      badnessState = BADNESS_VALUE;
      badness = 0;
   } else if (totalWidth < idealWidth) {
      if (totalStretchability == 0)
         badnessState = NOT_STRETCHABLE;
      else {
         ratio = 100 * (idealWidth - totalWidth) / totalStretchability;
         if (ratio > 1024)
            badnessState = QUITE_LOOSE;
         else {
            badnessState = BADNESS_VALUE;
            badness = ratio * ratio * ratio;
         }
      }
   } else { // if (word->totalWidth > availWidth)
      if (totalShrinkability == 0)
         badnessState = TOO_TIGHT;
      else {
         // ratio is negative here
         ratio = 100 * (idealWidth - totalWidth) / totalShrinkability;
         if (ratio <= - 100)
            badnessState = TOO_TIGHT;
         else {
            badnessState = BADNESS_VALUE;
            badness = - ratio * ratio * ratio;
         }
      }
   }
}

void Textblock::BadnessAndPenalty::setPenalty (int penalty)
{
   this->penalty = penalty;
   penaltyState = PENALTY_VALUE;
}

void Textblock::BadnessAndPenalty::setPenaltyProhibitBreak ()
{
   penaltyState = PROHIBIT_BREAK;
}

void Textblock::BadnessAndPenalty::setPenaltyForceBreak ()
{
   penaltyState = FORCE_BREAK;
}

bool Textblock::BadnessAndPenalty::lineLoose ()
{
   return
      badnessState == NOT_STRETCHABLE || badnessState == QUITE_LOOSE ||
      (badnessState == BADNESS_VALUE && ratio > 0);
}

bool Textblock::BadnessAndPenalty::lineTight ()
{
   return
      badnessState == TOO_TIGHT || (badnessState == BADNESS_VALUE && ratio < 0);
}

bool Textblock::BadnessAndPenalty::lineTooTight ()
{
   return badnessState == TOO_TIGHT;
}


bool Textblock::BadnessAndPenalty::lineMustBeBroken ()
{
   return penaltyState == FORCE_BREAK;
}

bool Textblock::BadnessAndPenalty::lineCanBeBroken ()
{
   return penaltyState != PROHIBIT_BREAK;
}

int Textblock::BadnessAndPenalty::compareTo (BadnessAndPenalty *other)
{
   for (int l = INF_MAX; l >= 0; l--) {
      int thisValue = badnessValue (l) + penaltyValue (l);
      int otherValue = other->badnessValue (l) + other->penaltyValue (l);

      if (thisValue != otherValue)
         return thisValue - otherValue;
   }
   
   return 0;
}

void Textblock::BadnessAndPenalty::print ()
{
   switch (badnessState) {
   case NOT_STRETCHABLE:
      printf ("not stretchable");
      break;

   case TOO_TIGHT:
      printf ("too tight");
      break;

   case QUITE_LOOSE:
      printf ("quite loose (ratio = %d)", ratio);
      break;

   case BADNESS_VALUE:
      printf ("%d", badness);
      break;
   }

   printf (" [%d + %d - %d vs. %d] + ",
           totalWidth, totalStretchability, totalShrinkability, idealWidth);

   switch (penaltyState) {
   case FORCE_BREAK:
      printf ("-inf");
      break;

   case PROHIBIT_BREAK:
      printf ("inf");
      break;

   case PENALTY_VALUE:
      printf ("%d", penalty);
      break;
   }
}

void Textblock::printWord (Word *word)
{
   switch(word->content.type) {
   case core::Content::TEXT:
      printf ("\"%s\"", word->content.text);
      break;
   case core::Content::WIDGET:
      printf ("<widget: %p>\n", word->content.widget);
      break;
   case core::Content::BREAK:
      printf ("<break>\n");
      break;
   default:
      printf ("<?>\n");
      break;              
   }
                 
   printf (" [%d / %d + %d - %d => %d + %d - %d] => ",
           word->size.width, word->origSpace, word->stretchability,
           word->shrinkability, word->totalWidth, word->totalStretchability,
           word->totalShrinkability);
   word->badnessAndPenalty.print ();
}

/*
 * ...
 *
 * diff ...
 */
void Textblock::justifyLine (Line *line, int diff)
{
   /* To avoid rounding errors, the calculation is based on accumulated
    * values. */

   if (diff > 0) {
      int stretchabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         stretchabilitySum += words->getRef(i)->stretchability;

      if (stretchabilitySum > 0) {
         int stretchabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            stretchabilityCum += word->stretchability;
            int spaceDiff =
               stretchabilityCum * diff / stretchabilitySum - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            PRINTF ("         %d (of %d): diff = %d\n", i, words->size (),
                    spaceDiff);

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   } else if (diff < 0) {
      int shrinkabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         shrinkabilitySum += words->getRef(i)->shrinkability;

      if (shrinkabilitySum > 0) {
         int shrinkabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            shrinkabilityCum += word->shrinkability;
            int spaceDiff =
               shrinkabilityCum * diff / shrinkabilitySum - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   }
}


Textblock::Line *Textblock::addLine (int firstWord, int lastWord,
                                     bool temporary)
{
   PRINTF ("[%p] ADD_LINE (%d, %d) => %d\n",
           this, firstWord, lastWord, lines->size ());

   Word *lastWordOfLine = words->getRef(lastWord);
   // Word::totalWidth includes the hyphen (which is what we want here).
   int lineWidth = lastWordOfLine->totalWidth;
   int maxOfMinWidth, sumOfMaxWidth;
   accumulateWordExtremes (firstWord, lastWord, &maxOfMinWidth,
                           &sumOfMaxWidth);

   PRINTF ("   words[%d]->totalWidth = %d\n", lastWord,
           lastWordOfLine->totalWidth);

   PRINTF ("[%p] ##### LINE ADDED: %d, from %d to %d #####\n",
           this, lines->size (), firstWord, lastWord);

   lines->increase ();
   if(!temporary) {
      // If the last line was temporary, this will be temporary, too, even
      // if not requested.
      if (lines->size () == 1 || nonTemporaryLines == lines->size () -1)
         nonTemporaryLines = lines->size ();
   }

   PRINTF ("nonTemporaryLines = %d\n", nonTemporaryLines);

   int lineIndex = lines->size () - 1;
   Line *line = lines->getRef (lineIndex);

   line->firstWord = firstWord;
   line->lastWord = lastWord;
   line->boxAscent = line->contentAscent = 0;
   line->boxDescent = line->contentDescent = 0;
   line->marginDescent = 0;
   line->breakSpace = 0;
   line->leftOffset = 0;

   alignLine (lineIndex);
   for (int i = line->firstWord; i < line->lastWord; i++) {
      Word *word = words->getRef (i);
      lineWidth += (word->effSpace - word->origSpace);
   }

   int lastMaxParMax; // maxParMax of the last line
   
   if (lines->size () == 1) {
      // first line
      line->top = 0;

      line->maxLineWidth = lineWidth;
      line->maxParMin = maxOfMinWidth;
      line->parMax = sumOfMaxWidth;

      lastMaxParMax = 0;
   } else {
      Line *prevLine = lines->getRef (lines->size () - 2);

      line->top = prevLine->top + prevLine->boxAscent +
         prevLine->boxDescent + prevLine->breakSpace;

      line->maxLineWidth = misc::max (lineWidth, prevLine->maxLineWidth);
      line->maxParMin = misc::max (maxOfMinWidth, prevLine->maxParMin);

      Word *lastWordOfPrevLine = words->getRef (prevLine->lastWord);
      if (lastWordOfPrevLine->badnessAndPenalty.lineMustBeBroken ())
         // This line starts a new paragraph.
         line->parMax = sumOfMaxWidth;
      else
         // This line continues the paragraph from prevLine.
         line->parMax = prevLine->parMax + sumOfMaxWidth;

      lastMaxParMax = prevLine->maxParMax;
   }

   // "maxParMax" is only set, when this line is the last line of the
   // paragraph. 
   Word *lastWordOfThisLine = words->getRef (line->lastWord);
   if (lastWordOfThisLine->badnessAndPenalty.lineMustBeBroken ())
      // Paragraph ends here.
      line->maxParMax =
         misc::max (lastMaxParMax,
                    // parMax includes the last space, which we ignore here
                    line->parMax - lastWordOfThisLine->origSpace
                    + lastWordOfThisLine->hyphenWidth);
   else
      // Paragraph continues: simply copy the last value of "maxParMax".
      line->maxParMax = lastMaxParMax;
   
   for(int i = line->firstWord; i <= line->lastWord; i++)
      accumulateWordForLine (lineIndex, i);

   PRINTF ("   line[%d].top = %d\n", lines->size () - 1, line->top);
   PRINTF ("   line[%d].boxAscent = %d\n", lines->size () - 1, line->boxAscent);
   PRINTF ("   line[%d].boxDescent = %d\n",
           lines->size () - 1, line->boxDescent);
   PRINTF ("   line[%d].contentAscent = %d\n", lines->size () - 1,
           line->contentAscent);
   PRINTF ("   line[%d].contentDescent = %d\n",
           lines->size () - 1, line->contentDescent);

   PRINTF ("   line[%d].maxLineWidth = %d\n",
           lines->size () - 1, line->maxLineWidth);
   PRINTF ("   line[%d].maxParMin = %d\n",
           lines->size () - 1, line->maxParMin);
   PRINTF ("   line[%d].maxParMax = %d\n",
           lines->size () - 1, line->maxParMax);
   PRINTF ("   line[%d].parMax = %d\n", lines->size () - 1, line->parMax);

   mustQueueResize = true;

   return line;
}

void Textblock::accumulateWordExtremes (int firstWord, int lastWord,
                                        int *maxOfMinWidth, int *sumOfMaxWidth)
{
   int parMin = 0;
   *maxOfMinWidth = *sumOfMaxWidth = 0;

   for (int i = firstWord; i <= lastWord; i++) {
      Word *word = words->getRef (i);
      bool atLastWord = i == lastWord;

      core::Extremes extremes;
      getWordExtremes (word, &extremes);

      // Minimum: between two *possible* breaks (or at the end).
      // TODO This is redundant to getExtremesImpl().
      if (word->badnessAndPenalty.lineCanBeBroken () || atLastWord) {
         parMin += extremes.minWidth + word->hyphenWidth;
         *maxOfMinWidth = misc::max (*maxOfMinWidth, parMin);
         parMin = 0;
      } else
         // Shrinkability could be considered, but really does not play a
         // role.
         parMin += extremes.minWidth + word->origSpace;

      //printf ("[%p] after word: ", this);
      //printWord (word);
      //printf ("\n");

      //printf ("[%p] (%d / %d) => parMin = %d, maxOfMinWidth = %d\n",
      //        this, extremes.minWidth, extremes.maxWidth, parMin,
      //        *maxOfMinWidth);

      *sumOfMaxWidth += (extremes.maxWidth + word->origSpace);
      // Notice that the last space is added. See also: Line::parMax.
   }
}

/*
 * This method is called in two cases: (i) when a word is added
 * (ii) when a page has to be (partially) rewrapped. It does word wrap,
 * and adds new lines if necessary.
 */
void Textblock::wordWrap (int wordIndex, bool wrapAll)
{
   PRINTF ("[%p] WORD_WRAP (%d, %s)\n",
           this, wordIndex, wrapAll ? "true" : "false");

   Word *word;
   //core::Extremes wordExtremes;

   if (!wrapAll)
      removeTemporaryLines ();

   initLine1Offset (wordIndex);

   word = words->getRef (wordIndex);
   word->effSpace = word->origSpace;

   accumulateWordData (wordIndex);

   bool newLine;
   do {
      bool tempNewLine = false;
      int firstIndex =
         lines->size() == 0 ? 0 : lines->getLastRef()->lastWord + 1;
      int searchUntil;

      if (wrapAll && wordIndex >= firstIndex && wordIndex == words->size() -1) {
         newLine = true;
         searchUntil = wordIndex;
         tempNewLine = true;
         PRINTF ("   NEW LINE: last word\n");
      } else if (wordIndex >= firstIndex &&
          word->badnessAndPenalty.lineMustBeBroken ()) {
         newLine = true;
         searchUntil = wordIndex;
         PRINTF ("   NEW LINE: forced break\n");
      } else if (wordIndex > firstIndex &&
                 word->badnessAndPenalty.lineTooTight () &&
                 words->getRef(wordIndex- 1)
                 ->badnessAndPenalty.lineCanBeBroken ()) {
         // TODO Comment the last condition (also below where the minimun is
         // searched for)
         newLine = true;
         searchUntil = wordIndex - 1;
         PRINTF ("   NEW LINE: line too tight\n");
      } else
         newLine = false;

      if(!newLine && !wrapAll)
         // No new line is added. "mustQueueResize" must,
         // nevertheless, be set, so that flush() will call
         // queueResize(), and later sizeRequestImpl() is called,
         // which then calls showMissingLines(), which eventually
         // calls this method again, with wrapAll == true, so that
         // newLine is calculated as "true".
         mustQueueResize = true;

      if(newLine) {
         accumulateWordData (wordIndex);
         int wordIndexEnd = wordIndex;

         bool lineAdded;
         do {
            PRINTF ("   searching from %d to %d\n", firstIndex, searchUntil);

            int breakPos = -1;
            for (int i = firstIndex; i <= searchUntil; i++) {
               Word *w = words->getRef(i);
               
               //printf ("      %d (of %d): ", i, words->size ());
               //printWord (w);
               //printf ("\n");
               
               // TODO: is this condition needed:
               // if(w->badnessAndPenalty.lineCanBeBroken ()) ?
               
               if (breakPos == -1 ||
                   w->badnessAndPenalty.compareTo
                   (&words->getRef(breakPos)->badnessAndPenalty) <= 0)
                  // "<=" instead of "<" in the next lines tends to result in
                  // more words per line -- theoretically. Practically, the
                  // case "==" will never occur.
                  breakPos = i;
            }

            PRINTF ("      breakPos = %d\n", breakPos);
            
            if (wrapAll && searchUntil == words->size () - 1) {
               // Since no break and no space is added, the last word
               // will have a penalty of inf. Actually, it should be -inf,
               // since it is the last word. However, since more words may
               // follow, the penalty is not changesd, but here, the search
               // is corrected (maybe only temporary).
               Word *lastWord = words->getRef (searchUntil);
               BadnessAndPenalty correctedBap = lastWord->badnessAndPenalty;
               correctedBap.setPenaltyForceBreak ();
               if (correctedBap.compareTo
                   (&words->getRef(breakPos)->badnessAndPenalty) <= 0) {
                  breakPos = searchUntil;
                  PRINTF ("      corrected: breakPos = %d\n", breakPos);
               }
            }

            int hyphenatedWord = -1;
            Word *word1 = words->getRef(breakPos);
            PRINTF ("[%p] line (broken at word %d): ", this, breakPos);
            //word1->badnessAndPenalty.print ();
            PRINTF ("\n");

            if (word1->badnessAndPenalty.lineTight () &&
                word1->canBeHyphenated &&
                word1->style->x_lang[0] &&
                word1->content.type == core::Content::TEXT &&
                Hyphenator::isHyphenationCandidate (word1->content.text))
               hyphenatedWord = breakPos;
            
            if (word1->badnessAndPenalty.lineLoose () &&
                breakPos + 1 < words->size ()) {
               Word *word2 = words->getRef(breakPos + 1);
               if (word2->canBeHyphenated &&
                   word2->style->x_lang[0] &&
                   word2->content.type == core::Content::TEXT  &&
                   Hyphenator::isHyphenationCandidate (word2->content.text))
                  hyphenatedWord = breakPos + 1;
            }

            PRINTF ("[%p] breakPos = %d, hyphenatedWord = %d\n",
                    this, breakPos, hyphenatedWord);

            if(hyphenatedWord == -1) {
               addLine (firstIndex, breakPos, tempNewLine);
               PRINTF ("[%p]    new line %d (%s), from %d to %d\n",
                       this, lines->size() - 1,
                       tempNewLine ? "temporally" : "permanently",
                       firstIndex, breakPos);
               lineAdded = true;
            } else {
               // TODO hyphenateWord() should return weather something has
               // changed at all. So that a second run, with
               // !word->canBeHyphenated, is unneccessary.
               // TODO Update: for this, searchUntil == 0 should be checked.
               PRINTF ("[%p] old searchUntil = %d ...\n", this, searchUntil);
               int n = hyphenateWord (hyphenatedWord);
               searchUntil += n;
               if (hyphenatedWord >= wordIndex)
                  wordIndexEnd += n;
               PRINTF ("[%p] -> new searchUntil = %d ...\n", this, searchUntil);
               lineAdded = false;
            }
            
            PRINTF ("[%p]       accumulating again from %d to %d\n",
                    this, breakPos + 1, wordIndexEnd);
            for(int i = breakPos + 1; i <= wordIndexEnd; i++)
               accumulateWordData (i);

         } while(!lineAdded);
      }
   } while (newLine);
}

int Textblock::hyphenateWord (int wordIndex)
{
   Word *hyphenatedWord = words->getRef(wordIndex);
   char lang[3] = { hyphenatedWord->style->x_lang[0], 
                    hyphenatedWord->style->x_lang[1], 0 };
   Hyphenator *hyphenator =
      Hyphenator::getHyphenator (layout->getPlatform (), lang);
   PRINTF ("[%p]    considering to hyphenate word %d, '%s', in language '%s'\n",
           this, wordIndex, words->getRef(wordIndex)->content.text, lang);
   int numBreaks;
   int *breakPos =
      hyphenator->hyphenateWord (hyphenatedWord->content.text, &numBreaks);

   if (numBreaks > 0) {
      Word origWord = *hyphenatedWord;

      core::Requisition wordSize[numBreaks + 1];
      calcTextSizes (origWord.content.text, strlen (origWord.content.text),
                     origWord.style, numBreaks, breakPos, wordSize);
      
      PRINTF ("[%p]       %d words ...\n", this, words->size ());
      words->insert (wordIndex, numBreaks);
      PRINTF ("[%p]       ... => %d words\n", this, words->size ());

      // Adjust anchor indexes.
      for (int i = 0; i < anchors->size (); i++) {
         Anchor *anchor = anchors->getRef (i);
         if (anchor->wordIndex > wordIndex)
            anchor->wordIndex += numBreaks;
      }
      
      for (int i = 0; i < numBreaks + 1; i++) {
         Word *w = words->getRef (wordIndex + i);

         fillWord (w, wordSize[i].width, wordSize[i].ascent,
                   wordSize[i].descent, false, origWord.style);

         // TODO There should be a method fillText0.
         w->content.type = core::Content::TEXT;

         int start = (i == 0 ? 0 : breakPos[i - 1]);
         int end = (i == numBreaks ?
                    strlen (origWord.content.text) : breakPos[i]);
         w->content.text =
            layout->textZone->strndup (origWord.content.text + start,
                                       end - start);
         PRINTF ("      [%d] -> '%s'\n", wordIndex + i, w->content.text);

         // Note: there are numBreaks + 1 word parts.
         if (i < numBreaks) {
            // TODO There should be a method fillHyphen.
            w->badnessAndPenalty.setPenalty (HYPHEN_BREAK);
            w->hyphenWidth =
               layout->textWidth (origWord.style->font, "\xc2\xad", 2);
            PRINTF ("      [%d] + hyphen\n", wordIndex + i);
         } else {
            if (origWord.content.space) {
               fillSpace (w, origWord.spaceStyle);
               PRINTF ("      [%d] + space\n", wordIndex + i);
            } else {
               PRINTF ("      [%d] + nothing\n", wordIndex + i);
            }
         }

         accumulateWordData (wordIndex + i);
      }

      PRINTF ("   finished\n");
      
      //delete origword->content.text; TODO: Via textZone?
      origWord.style->unref ();
      origWord.spaceStyle->unref ();

      delete breakPos;
   } else {
      words->getRef(wordIndex)->canBeHyphenated = false;
   }

   return numBreaks;
}

void Textblock::accumulateWordForLine (int lineIndex, int wordIndex)
{
   Line *line = lines->getRef (lineIndex);
   Word *word = words->getRef (wordIndex);

   PRINTF ("      %d + %d / %d + %d\n", line->boxAscent, line->boxDescent,
           word->size.ascent, word->size.descent);

   line->boxAscent = misc::max (line->boxAscent, word->size.ascent);
   line->boxDescent = misc::max (line->boxDescent, word->size.descent);

   int len = word->style->font->ascent;
   if (word->style->valign == core::style::VALIGN_SUPER)
      len += len / 2;
   line->contentAscent = misc::max (line->contentAscent, len);
         
   len = word->style->font->descent;
   if (word->style->valign == core::style::VALIGN_SUB)
      len += word->style->font->ascent / 3;
   line->contentDescent = misc::max (line->contentDescent, len);

   if (word->content.type == core::Content::WIDGET) {
      int collapseMarginTop = 0;

      line->marginDescent =
         misc::max (line->marginDescent,
                    word->size.descent +
                    word->content.widget->getStyle()->margin.bottom);

      if (lines->size () == 1 &&
          word->content.widget->blockLevel () &&
          getStyle ()->borderWidth.top == 0 &&
          getStyle ()->padding.top == 0) {
         // collapse top margins of parent element and its first child
         // see: http://www.w3.org/TR/CSS21/box.html#collapsing-margins
         collapseMarginTop = getStyle ()->margin.top;
      }

      line->boxAscent =
            misc::max (line->boxAscent,
                       word->size.ascent,
                       word->size.ascent
                       + word->content.widget->getStyle()->margin.top
                       - collapseMarginTop);

      word->content.widget->parentRef = lineIndex;
   } else {
      line->marginDescent =
         misc::max (line->marginDescent, line->boxDescent);

      if (word->content.type == core::Content::BREAK)
         line->breakSpace =
            misc::max (word->content.breakSpace,
                       line->marginDescent - line->boxDescent,
                       line->breakSpace);
   }
}

void Textblock::accumulateWordData (int wordIndex)
{
   // Typically, the word in question is in the last line; in any case
   // quite at the end of the text, so that linear search is actually
   // the fastest option.
   int lineIndex = lines->size ();
   while (lineIndex > 0 && wordIndex <= lines->getRef(lineIndex - 1)->lastWord)
      lineIndex--;

   int firstWordOfLine;
   if (lineIndex == 0)
      firstWordOfLine = 0;
   else
      firstWordOfLine = lines->getRef(lineIndex - 1)->lastWord + 1;

   Word *word = words->getRef (wordIndex);
   PRINTF ("[%p] ACCUMULATE_WORD_DATA (%d); lineIndex = %d: ...\n",
           this, wordIndex, lineIndex);

   int availWidth = calcAvailWidth (lineIndex);

   PRINTF ("      (%s existing line %d starts with word %d)\n",
           lineIndex < lines->size () ? "already" : "not yet",
           lineIndex, firstWordOfLine);

   if (wordIndex == firstWordOfLine) {
      // first word of the (not neccessarily yet existing) line
      word->totalWidth = word->size.width + word->hyphenWidth;
      word->totalStretchability = 0;
      word->totalShrinkability = 0;
   } else {
      Word *prevWord = words->getRef (wordIndex - 1);

      word->totalWidth = prevWord->totalWidth
         + prevWord->origSpace - prevWord->hyphenWidth
         + word->size.width + word->hyphenWidth;
      word->totalStretchability =
         prevWord->totalStretchability + prevWord->stretchability;
      word->totalShrinkability =
         prevWord->totalShrinkability + prevWord->shrinkability;
   }

   word->badnessAndPenalty.calcBadness (word->totalWidth, availWidth,
                                        word->totalStretchability,
                                        word->totalShrinkability);

   //printf ("      => ");
   //printWord (word);
   //printf ("\n");
}

int Textblock::calcAvailWidth (int lineIndex)
{
   int availWidth =
      this->availWidth - getStyle()->boxDiffWidth() - innerPadding;
   if (limitTextWidth &&
       layout->getUsesViewport () &&
       availWidth > layout->getWidthViewport () - 10)
      availWidth = layout->getWidthViewport () - 10;
   if (lineIndex == 0)
      availWidth -= line1OffsetEff;

   //PRINTF("[%p] CALC_AVAIL_WIDTH => %d - %d - %d = %d\n",
   //       this, this->availWidth, getStyle()->boxDiffWidth(), innerPadding,
   //       availWidth);

   return availWidth;
}

void Textblock::initLine1Offset (int wordIndex)
{
   Word *word = words->getRef (wordIndex);

   /* Test whether line1Offset can be used. */
   if (wordIndex == 0) {
      if (ignoreLine1OffsetSometimes &&
          line1Offset + word->size.width > availWidth) {
         line1OffsetEff = 0;
      } else {
         int indent = 0;

         if (word->content.type == core::Content::WIDGET &&
             word->content.widget->blockLevel() == true) {
            /* don't use text-indent when nesting blocks */
         } else {
            if (core::style::isPerLength(getStyle()->textIndent)) {
               indent = misc::roundInt(this->availWidth *
                        core::style::perLengthVal (getStyle()->textIndent));
            } else {
               indent = core::style::absLengthVal (getStyle()->textIndent);
            }
         }
         line1OffsetEff = line1Offset + indent;
      }
   }
}

/**
 * Align the line.
 *
 * \todo Use block's style instead once paragraphs become proper blocks.
 */
void Textblock::alignLine (int lineIndex)
{
   Line *line = lines->getRef (lineIndex);
   int availWidth = calcAvailWidth (lineIndex);
   Word *firstWord = words->getRef (line->firstWord);
   Word *lastWord = words->getRef (line->lastWord);

   for (int i = line->firstWord; i < line->lastWord; i++)
      words->getRef(i)->origSpace = words->getRef(i)->effSpace;

   if (firstWord->content.type != core::Content::BREAK) {
      switch (firstWord->style->textAlign) {
      case core::style::TEXT_ALIGN_LEFT:
      case core::style::TEXT_ALIGN_STRING:   /* handled elsewhere (in the
                                              * future)? */
         line->leftOffset = 0;
         break;
      case core::style::TEXT_ALIGN_JUSTIFY:  /* see some lines above */
         line->leftOffset = 0;
         if(lastWord->content.type != core::Content::BREAK &&
            line->lastWord != words->size () - 1) {
            PRINTF ("      justifyLine => %d vs. %d\n",
                    lastWord->totalWidth, availWidth);
            justifyLine (line, availWidth - lastWord->totalWidth);
         }
         break;
      case core::style::TEXT_ALIGN_RIGHT:
         line->leftOffset = availWidth - lastWord->totalWidth;
         break;
      case core::style::TEXT_ALIGN_CENTER:
         line->leftOffset = (availWidth - lastWord->totalWidth) / 2;
         break;
      default:
         /* compiler happiness */
         line->leftOffset = 0;
      }

      /* For large lines (images etc), which do not fit into the viewport: */
      if (line->leftOffset < 0)
         line->leftOffset = 0;
   }
}

/**
 * Rewrap the page from the line from which this is necessary.
 * There are basically two times we'll want to do this:
 * either when the viewport is resized, or when the size changes on one
 * of the child widgets.
 */
void Textblock::rewrap ()
{
   PRINTF ("[%p] REWRAP: wrapRef = %d\n", this, wrapRef);

   if (wrapRef == -1)
      /* page does not have to be rewrapped */
      return;

   /* All lines up from wrapRef will be rebuild from the word list,
    * the line list up from this position is rebuild. */
   lines->setSize (wrapRef);
   nonTemporaryLines = misc::min (nonTemporaryLines, wrapRef);

   int firstWord;
   if (lines->size () > 0)
      firstWord = lines->getLastRef()->lastWord + 1;
   else
      firstWord = 0;

   for (int i = firstWord; i < words->size (); i++) {
      Word *word = words->getRef (i);
         
      if (word->content.type == core::Content::WIDGET)
         calcWidgetSize (word->content.widget, &word->size);
      
      wordWrap (i, false);
      
      if (word->content.type == core::Content::WIDGET) {
         word->content.widget->parentRef = lines->size () - 1;
      }
   }

   /* Next time, the page will not have to be rewrapped. */
   wrapRef = -1;
}

void Textblock::showMissingLines ()
{
   int firstWordToWrap = lines->size () > 0 ?
      lines->getRef(lines->size () - 1)->lastWord + 1 : 0;
   PRINTF ("[%p] SHOW_MISSING_LINES: wrap from %d to %d\n",
           this, firstWordToWrap, words->size () - 1);
   for (int i = firstWordToWrap; i < words->size (); i++)
      wordWrap (i, true);
}


void Textblock::removeTemporaryLines ()
{
   lines->setSize (nonTemporaryLines);
}

} // namespace dw
