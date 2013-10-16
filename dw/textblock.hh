#ifndef __DW_TEXTBLOCK_HH__
#define __DW_TEXTBLOCK_HH__

#include <limits.h>

#include "core.hh"
#include "../lout/misc.hh"

// These were used when improved line breaking and hyphenation were
// implemented. Should be cleaned up; perhaps reactivate RTFL again.
#define PRINTF(fmt, ...)
#define PUTCHAR(ch)

namespace dw {

/**
 * \brief A Widget for rendering text blocks, i.e. paragraphs or sequences
 *    of paragraphs.
 *
 * <div style="border: 2px solid #ffff00; margin-top: 0.5em;
 * margin-bottom: 0.5em; padding: 0.5em 1em;
 * background-color: #ffffe0"><b>Info:</b> The recent changes (line
 * breaking and hyphenation) have not yet been incorporated into this
 * documentation. See \ref dw-line-breaking.</div>
 *
 * <h3>Signals</h3>
 *
 * dw::Textblock uses the signals defined in
 * dw::core::Layout::LinkReceiver, related to links. The coordinates are
 * always -1.
 *
 *
 * <h3>Collapsing Spaces</h3>
 *
 * The idea behind this is that every paragraph has a specific vertical
 * space around and that they are combined to one space, according to
 * rules stated below. A paragraph consists either of the lines between
 * two paragraph breaks within a dw::Textblock, or of a dw::Textblock
 * within a dw::Textblock, in a single line; the latter is used for
 * indented boxes and list items.
 *
 * The rules:
 *
 * <ol>
 * <li> If a paragraph is following by another, the space between them is the
 *      maximum of both box spaces:
 *
 *      \image html dw-textblock-collapsing-spaces-1-1.png
 *
 *       are combined like this:
 *
 *       \image html dw-textblock-collapsing-spaces-1-2.png
 *
 * <li> a) If one paragraph is the first paragraph within another, the upper
 *      space of these paragraphs collapse. b) The analogue is the case for the
 *      last box:
 *
 *      \image html dw-textblock-collapsing-spaces-2-1.png
 *
 *      If B and C are put into A, the result is:
 *
 *      \image html dw-textblock-collapsing-spaces-2-2.png
 * </ol>
 *
 * For achieving this, there are some features of dw::Textblock:
 *
 * <ul>
 * <li> Consequent breaks are automatically combined, according to
 *      rule 1. See the code of dw::Textblock::addParBreak for details.
 *
 * <li> If a break is added as the first word of the dw::Textblock within
 *      another dw::Textblock, collapsing according to rule 2a is done
 *      automatically. See the code of dw::Textblock::addParBreak.
 *
 *  <li> To collapse spaces according to rule 2b,
 *        dw::Textblock::addParBreak::handOverBreak must be called for
 *        the \em inner widget. The HTML parser does this in
 *        Html_eventually_pop_dw.
 * </ul>
 *
 *
 * <h3>Collapsing Margins</h3>
 *
 * Collapsing margins, as defined in the CSS2 specification, are,
 * supported in addition to collapsing spaces. Also, spaces and margins
 * collapse themselves. I.e., the space between two paragraphs is the
 * maximum of the space calculated as described in "Collapsing Spaces"
 * and the space calculated according to the rules for collapsing margins.
 *
 * (This is an intermediate hybrid state, collapsing spaces are used in
 * the current version of dillo, while I implemented collapsing margins
 * for the CSS prototype and integrated it already into the main trunk. For
 * a pure CSS-based dillo, collapsing spaces will not be needed anymore, and
 * may be removed for simplicity.)
 *
 *
 * <h3>Some Internals</h3>
 *
 * There are 4 lists, dw::Textblock::words, dw::Textblock::paragraphs,
 * dw::Textblock::lines, and dw::Textblock::anchors. The word list is
 * quite static; only new words may be added. A word is either text, a
 * widget, or a break.
 *
 * Lines refer to the word list (first and last). They are completely
 * redundant, i.e., they can be rebuilt from the words. Lines can be
 * rewrapped either completely or partially (see "Incremental Resizing"
 * below). For the latter purpose, several values are accumulated in the
 * lines. See dw::Textblock::Line for details.
 *
 * A recent change was the introduction of the paragraphs list, which
 * works quite similar, is also redundant, but is used to calculate
 * the extremes, not the size.
 *
 * Anchors associate the anchor name with the index of the next word at
 * the point of the anchor.
 *
 * <h4>Incremental Resizing</h4>
 *
 * dw::Textblock makes use of incremental resizing as described in \ref
 * dw-widget-sizes. The parentRef is, for children of a dw::Textblock, simply
 * the number of the line.
 *
 * Generally, there are three cases which may change the size of the
 * widget:
 *
 * <ul>
 * <li> The available size of the widget has changed, e.g., because the
 *      user has changed the size of the browser window. In this case,
 *      it is necessary to rewrap all the lines.
 *
 * <li> A child widget has changed its size. In this case, only a rewrap
 *      down from the line where this widget is located is necessary.
 *
 *      (This case is very important for tables. Tables are quite at the
 *      bottom, so that a partial rewrap is relevant. Otherwise, tables
 *      change their size quite often, so that this is necessary for a
 *      fast, non-blocking rendering)
 *
 * <li> A word (or widget, break etc.) is added to the text block. This
 *      makes it possible to reuse the old size by simply adjusting the
 *      current width and height, so no rewrapping is necessary.
 * </ul>
 *
 * The state of the size calculation is stored in wrapRef within
 * dw::Textblock, which has the value -1 if no rewrapping of lines
 * necessary, or otherwise the line from which a rewrap is necessary.
 *
 */
class Textblock: public core::Widget
{
private:
   /**
    * This class encapsulates the badness/penalty calculation, and so
    * (i) makes changes (hopefully) simpler, and (ii) hides the
    * integer arithmetic (floating point arithmetic avoided for
    * performance reasons). Unfortunately, the value range of the
    * badness is not well defined, so fiddling with the penalties is a
    * bit difficult.
    */
   
   enum {
      PENALTY_FORCE_BREAK = INT_MIN,
      PENALTY_PROHIBIT_BREAK = INT_MAX
   };

   class BadnessAndPenalty
   {
   private:
      enum { NOT_STRETCHABLE, QUITE_LOOSE, BADNESS_VALUE, TOO_TIGHT }
         badnessState;
      int ratio; // ratio is only defined when badness is defined
      int badness, penalty[2];
      
      // For debugging: define DEBUG for more informations in print().
#ifdef DEBUG
      int totalWidth, idealWidth, totalStretchability, totalShrinkability;
#endif

      // "Infinity levels" are used to represent very large numbers,
      // including "quasi-infinite" numbers. A couple of infinity
      // level and number can be mathematically represented as
      //
      //    number * N ^ (infinity level)
      //
      // where N is a number which is large enough. Practically,
      // infinity levels are used to circumvent limited ranges for
      // integer numbers.

      // Here, all infinity levels have got special meanings.
      enum {
         INF_VALUE = 0,        /* simple values */
         INF_LARGE,            /* large values, like QUITE_LOOSE */
         INF_NOT_STRETCHABLE,  /* reserved for NOT_STRECTHABLE */
         INF_TOO_TIGHT,        /* used for lines, which are too tight */
         INF_PENALTIES,        /* used for penalties */
         INF_MAX = INF_PENALTIES

         // That INF_PENALTIES is the last value means that an
         // infinite penalty (breaking is prohibited) makes a break
         // not possible at all, so that pre-formatted text
         // etc. works.
      };

      void setSinglePenalty (int index, int penalty);
      int badnessValue (int infLevel);
      int penaltyValue (int index, int infLevel);
      
   public:
      void calcBadness (int totalWidth, int idealWidth,
                        int totalStretchability, int totalShrinkability);
      inline void setPenalty (int penalty) { setPenalties (penalty, penalty); }
      void setPenalties (int penalty1, int penalty2);

      bool lineLoose ();
      bool lineTight ();
      bool lineTooTight ();
      bool lineMustBeBroken (int penaltyIndex);
      bool lineCanBeBroken (int penaltyIndex);
      int compareTo (int penaltyIndex, BadnessAndPenalty *other);

      void print ();
   };

   enum { PENALTY_HYPHEN, PENALTY_EM_DASH_LEFT, PENALTY_EM_DASH_RIGHT,
          PENALTY_NUM };
   enum { NUM_DIV_CHARS = 4 };

   typedef struct
   {
      const char *s;
      bool charRemoved, canBeHyphenated, unbreakableForMinWidth;
      int penaltyIndexLeft, penaltyIndexRight;
   } DivChar;

   static DivChar divChars[NUM_DIV_CHARS];

   static const char *hyphenDrawChar;

protected:
   /**
    * \brief Implementation used for words.
    */
   class WordImgRenderer:
      public core::style::StyleImage::ExternalWidgetImgRenderer
   {
   protected:
      Textblock *textblock;
      int wordNo, xWordWidget, lineNo;
      bool dataSet;

   public:
      WordImgRenderer (Textblock *textblock, int wordNo);
      ~WordImgRenderer ();
      
      void setData (int xWordWidget, int lineNo);

      bool readyToDraw ();
      void getBgArea (int *x, int *y, int *width, int *height);
      void getRefArea (int *xRef, int *yRef, int *widthRef, int *heightRef);
      core::style::Style *getStyle ();
      void draw (int x, int y, int width, int height);

      virtual void print ();
   };

   class SpaceImgRenderer: public WordImgRenderer
   {
   public:
      inline SpaceImgRenderer (Textblock *textblock, int wordNo) :
         WordImgRenderer (textblock, wordNo) { }

      void getBgArea (int *x, int *y, int *width, int *height);
      core::style::Style *getStyle ();

      void print ();
   };

   struct Paragraph
   {
      int firstWord;    /* first word's index in word vector */
      int lastWord;     /* last word's index in word vector */

      /*
       * General remark: all values include the last hyphen width, but
       * not the last space; these values are, however corrected, when
       * another word is added.
       *
       * Also, as opposed to lines, paragraphs are created with the
       * first, not the last word, so these values change when new
       * words are added.
       */

      int parMin;       /* The sum of all word minima (plus spaces,
                           hyphen width etc.) since the last possible
                           break within this paragraph. */
      int parMax;       /* The sum of all word maxima in this
                         * paragraph (plus spaces, hyphen width
                         * etc.). */

      int maxParMin;    /* Maximum of all paragraph minima (value of
                         * "parMin), including this paragraph. */
      int maxParMax;    /* Maximum of all paragraph maxima (value of
                         * "parMax"), including this paragraph. */
   };

   struct Line
   {
      int firstWord;    /* first word's index in word vector */
      int lastWord;     /* last word's index in word vector */

      /* "top" is always relative to the top of the first line, i.e.
       * page->lines[0].top is always 0. */
      int top, boxAscent, boxDescent, contentAscent, contentDescent,
          breakSpace, leftOffset;

      /* This is similar to descent, but includes the bottom margins of the
       * widgets within this line. */
      int marginDescent;

      /* Maximum of all line widths, including this line. Does not
       * include the last space, but the last hyphen width. Please
       * notice a change: until recently (before hyphenation and
       * changed line breaking), the values were accumulated up to the
       * last line, not this line.*/
      int maxLineWidth;
   };

   struct Word
   {
      enum {
         /** Can be hyphenated automatically. (Cleared after
          * hyphenation.) */
         CAN_BE_HYPHENATED         = 1 << 0,
         /** Must be drawn with a hyphen, when at the end of the line. */
         DIV_CHAR_AT_EOL           = 1 << 1,
         /** Is or ends with a "division character", which is part of
          * the word. */
         PERM_DIV_CHAR             = 1 << 2,
         /** This word must be drawn, together with the following
          * word(s), by only one call of View::drawText(), to get
          * kerning, ligatures etc. right. The last of the words drawn
          * as one text does *not* have this flag set. */
         DRAW_AS_ONE_TEXT          = 1 << 3,
         /* When calculating the minimal width (as part of extremes),
          * do not consider this word as breakable. This flag is
          * ignored when the line is actually broken.  */
         UNBREAKABLE_FOR_MIN_WIDTH = 1 << 4,
         /* If a word represents a "real" text word, or (after
          * hyphenation) the first part of a "real" text word, this
          * flag is set. Plays a role for text transformation. */
         WORD_START                = 1 << 5,
         /* If a word represents a "real" text word, or (after
          * hyphenation) the last part of a "real" text word, this
          * flag is set. Analogue to WORD_START.  */
         WORD_END                  = 1 << 6
      };

      /* TODO: perhaps add a xLeft? */
      core::Requisition size;
      /* Space after the word, only if it's not a break: */
      short origSpace; /* from font, set by addSpace */
      short effSpace;  /* effective space, set by wordWrap,
                        * used for drawing etc. */
      short hyphenWidth; /* Additional width, when a word is part
                          * (except the last part) of a hyphenationed
                          * word. Has to be added to the width, when
                          * this is the last word of the line, and
                          * "hyphenWidth > 0" is also used to decide
                          * whether to draw a hyphen. */
      short flags;
      core::Content content;

      // accumulated values, relative to the beginning of the line
      int totalWidth;          /* The sum of all word widths; plus all
                                  spaces, excluding the one of this
                                  word; plus the hyphen width of this
                                  word (but of course, no hyphen
                                  widths of previous words. In other
                                  words: the value compared to the
                                  ideal width of the line, if the line
                                  would be broken after this word. */
      int maxAscent, maxDescent;
      int totalSpaceStretchability; // includes all *before* current word
      int totalSpaceShrinkability;  // includes all *before* current word
      BadnessAndPenalty badnessAndPenalty; /* when line is broken after this
                                            * word */

      core::style::Style *style;
      core::style::Style *spaceStyle; /* initially the same as of the word,
                                         later set by a_Dw_page_add_space */

      // These two are used rarely, so there is perhaps a way to store
      // them which is consuming less memory.
      WordImgRenderer *wordImgRenderer;
      SpaceImgRenderer *spaceImgRenderer;
   };

   void printWordShort (Word *word);
   void printWordFlags (short flags);
   void printWordWithFlags (Word *word);
   void printWord (Word *word);

   struct Anchor
   {
      char *name;
      int wordIndex;
   };

   class TextblockIterator: public core::Iterator
   {
   private:
      int index;

   public:
      TextblockIterator (Textblock *textblock, core::Content::Type mask,
                         bool atEnd);
      TextblockIterator (Textblock *textblock, core::Content::Type mask,
                         int index);

      lout::object::Object *clone();
      int compareTo(lout::object::Comparable *other);

      bool next ();
      bool prev ();
      void highlight (int start, int end, core::HighlightLayer layer);
      void unhighlight (int direction, core::HighlightLayer layer);
      void getAllocation (int start, int end, core::Allocation *allocation);
   };

   friend class TextblockIterator;

   /* These fields provide some ad-hoc-functionality, used by sub-classes. */
   bool hasListitemValue; /* If true, the first word of the page is treated
                          specially (search in source). */
   int innerPadding;    /* This is an additional padding on the left side
                            (used by ListItem). */
   int line1Offset;     /* This is an additional offset of the first line.
                           May be negative (shift to left) or positive
                           (shift to right). */
   int line1OffsetEff; /* The "effective" value of line1_offset, may
                          differ from line1_offset when
                          ignoreLine1OffsetSometimes is set to true. */

   /* The following is really hackish: It is used for DwTableCell (see
    * comment in dw_table_cell.c), to avoid too wide table columns. If
    * set to true, it has following effects:
    *
    *  (i) line1_offset is ignored in calculating the minimal width
    *      (which is used by DwTable!), and
    * (ii) line1_offset is ignored (line1_offset_eff is set to 0),
    *      when line1_offset plus the width of the first word is
    *      greater than the the available witdh.
    *
    * \todo Eliminate all these ad-hoc features by a new, simpler and
    *       more elegant design. ;-)
    */
   bool ignoreLine1OffsetSometimes;

   bool mustQueueResize;

   /**
    * The penalties for hyphens and other, multiplied by 100. So, 100
    * means 1.0. INT_MAX and INT_MIN are also allowed. See
    * dw::Textblock::BadnessAndPenalty::setPenalty for more
    * details. Set from preferences.
    */
   static int penalties[PENALTY_NUM][2];

   /**
    * ...
    */
   static int stretchabilityFactor;

   bool limitTextWidth; /* from preferences */

   int redrawY;
   int lastWordDrawn;

   /* These values are set by set_... */
   int availWidth, availAscent, availDescent;

   int wrapRefLines, wrapRefParagraphs;  /* [0 based] */

   lout::misc::SimpleVector <Line> *lines;
   lout::misc::SimpleVector <Paragraph> *paragraphs;
   int nonTemporaryLines;
   lout::misc::NotSoSimpleVector <Word> *words;
   lout::misc::SimpleVector <Anchor> *anchors;

   struct {int index, nChar;}
      hlStart[core::HIGHLIGHT_NUM_LAYERS], hlEnd[core::HIGHLIGHT_NUM_LAYERS];

   int hoverLink;  /* The link under the mouse pointer */


   void queueDrawRange (int index1, int index2);
   void getWordExtremes (Word *word, core::Extremes *extremes);
   void justifyLine (Line *line, int diff);
   Line *addLine (int firstWord, int lastWord, bool temporary);
   void calcWidgetSize (core::Widget *widget, core::Requisition *size);
   void rewrap ();
   void fillParagraphs ();
   void showMissingLines ();
   void removeTemporaryLines ();

   void decorateText (core::View *view, core::style::Style *style,
                      core::style::Color::Shading shading,
                      int x, int yBase, int width);
   void drawText (core::View *view, core::style::Style *style,
                  core::style::Color::Shading shading, int x, int y,
                  const char *text, int start, int len, bool isStart,
                  bool isEnd);
   void drawWord (Line *line, int wordIndex1, int wordIndex2, core::View *view,
                  core::Rectangle *area, int xWidget, int yWidgetBase);
   void drawWord0 (int wordIndex1, int wordIndex2,
                   const char *text, int totalWidth, bool drawHyphen,
                   core::style::Style *style, core::View *view,
                   core::Rectangle *area, int xWidget, int yWidgetBase);
   void drawSpace (int wordIndex, core::View *view, core::Rectangle *area,
                   int xWidget, int yWidgetBase);
   void drawLine (Line *line, core::View *view, core::Rectangle *area);
   int findLineIndex (int y);
   int findLineOfWord (int wordIndex);
   int findParagraphOfWord (int wordIndex);
   Word *findWord (int x, int y, bool *inSpace);

   Word *addWord (int width, int ascent, int descent, short flags,
                  core::style::Style *style);
   void initWord (int wordNo);
   void removeWordImgRenderer (int wordNo);
   void setWordImgRenderer (int wordNo);
   void removeSpaceImgRenderer (int wordNo);
   void setSpaceImgRenderer (int wordNo);
   void fillWord (int wordNo, int width, int ascent, int descent,
                  short flags, core::style::Style *style);
   void fillSpace (int wordNo, core::style::Style *style);
   void setBreakOption (Word *word, core::style::Style *style,
                        int breakPenalty1, int breakPenalty2, bool forceBreak);
   bool isBreakAllowed (Word *word);
   int textWidth (const char *text, int start, int len,
                  core::style::Style *style, bool isStart, bool isEnd);
   void calcTextSize (const char *text, size_t len, core::style::Style *style,
                      core::Requisition *size, bool isStart, bool isEnd);

   /**
    * \brief Returns the x offset (the indentation plus any offset needed for
    *    centering or right justification) for the line.
    *
    * The offset returned is relative to the page *content* (i.e. without
    * border etc.).
    */
   inline int lineXOffsetContents (Line *line)
   {
      return innerPadding + line->leftOffset +
         (line == lines->getFirstRef() ? line1OffsetEff : 0);
   }

   /**
    * \brief Like lineXOffset, but relative to the allocation (i.e.
    *    including border etc.).
    */
   inline int lineXOffsetWidget (Line *line)
   {
      return lineXOffsetContents (line) + getStyle()->boxOffsetX ();
   }

   inline int lineYOffsetWidgetAllocation (Line *line,
                                           core::Allocation *allocation)
   {
      return line->top + (allocation->ascent - lines->getRef(0)->boxAscent);
   }

   inline int lineYOffsetWidget (Line *line)
   {
      return lineYOffsetWidgetAllocation (line, &allocation);
   }

   /**
    * Like lineYOffsetCanvas, but with the allocation as parameter.
    */
   inline int lineYOffsetCanvasAllocation (Line *line,
                                           core::Allocation *allocation)
   {
      return allocation->y + lineYOffsetWidgetAllocation(line, allocation);
   }

   /**
    * Returns the y offset (within the canvas) of a line.
    */
   inline int lineYOffsetCanvas (Line *line)
   {
      return lineYOffsetCanvasAllocation(line, &allocation);
   }

   inline int lineYOffsetWidgetI (int lineIndex)
   {
      return lineYOffsetWidget (lines->getRef (lineIndex));
   }

   inline int lineYOffsetCanvasI (int lineIndex)
   {
      return lineYOffsetCanvas (lines->getRef (lineIndex));
   }
   
   inline int calcPenaltyIndexForNewLine ()
   {
      if (lines->size() == 0)
         return 0;
      else
         return
            (words->getRef(lines->getLastRef()->lastWord)->flags &
             (Word::DIV_CHAR_AT_EOL | Word::PERM_DIV_CHAR)) ? 1 : 0;
   }

   bool sendSelectionEvent (core::SelectionState::EventType eventType,
                            core::MousePositionEvent *event);

   void accumulateWordExtremes (int firstWord, int lastWord,
                                int *maxOfMinWidth, int *sumOfMaxWidth);
   void processWord (int wordIndex);
   virtual bool wordWrap (int wordIndex, bool wrapAll);
   int searchMinBap (int firstWord, int lastWordm, int penaltyIndex,
                     bool correctAtEnd);
   int considerHyphenation (int firstIndex, int breakPos);
   bool isHyphenationCandidate (Word *word);

   void handleWordExtremes (int wordIndex);
   void correctLastWordExtremes ();

   static int getSpaceShrinkability(struct Word *word);
   static int getSpaceStretchability(struct Word *word);
   static int getLineShrinkability(Word *lastWord);
   static int getLineStretchability(Word *lastWord);
   int hyphenateWord (int wordIndex);
   void accumulateWordForLine (int lineIndex, int wordIndex);
   void accumulateWordData (int wordIndex);
   int calcAvailWidth (int lineIndex);
   void initLine1Offset (int wordIndex);
   void alignLine (int lineIndex);

   void sizeRequestImpl (core::Requisition *requisition);
   void getExtremesImpl (core::Extremes *extremes);
   void sizeAllocateImpl (core::Allocation *allocation);
   void resizeDrawImpl ();

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   void setWidth (int width);
   void setAscent (int ascent);
   void setDescent (int descent);
   void draw (core::View *view, core::Rectangle *area);

   bool buttonPressImpl (core::EventButton *event);
   bool buttonReleaseImpl (core::EventButton *event);
   bool motionNotifyImpl (core::EventMotion *event);
   void enterNotifyImpl (core::EventCrossing *event);
   void leaveNotifyImpl (core::EventCrossing *event);

   void removeChild (Widget *child);

   void addText0 (const char *text, size_t len, short flags,
                  core::style::Style *style, core::Requisition *size);
   void calcTextSizes (const char *text, size_t textLen,
                       core::style::Style *style,
                       int numBreaks, int *breakPos,
                       core::Requisition *wordSize);

public:
   static int CLASS_ID;

   static void setPenaltyHyphen (int penaltyHyphen);
   static void setPenaltyHyphen2 (int penaltyHyphen2);
   static void setPenaltyEmDashLeft (int penaltyLeftEmDash);
   static void setPenaltyEmDashRight (int penaltyRightEmDash);
   static void setPenaltyEmDashRight2 (int penaltyRightEmDash2);
   static void setStretchabilityFactor (int stretchabilityFactor);

   Textblock(bool limitTextWidth);
   ~Textblock();

   core::Iterator *iterator (core::Content::Type mask, bool atEnd);

   void flush ();

   void addText (const char *text, size_t len, core::style::Style *style);
   inline void addText (const char *text, core::style::Style *style)
   {
      addText (text, strlen(text), style);
   }
   void addWidget (core::Widget *widget, core::style::Style *style);
   bool addAnchor (const char *name, core::style::Style *style);
   void addSpace (core::style::Style *style);
   void addBreakOption (core::style::Style *style, bool forceBreak);
   void addParbreak (int space, core::style::Style *style);
   void addLinebreak (core::style::Style *style);

   core::Widget *getWidgetAtPoint (int x, int y, int level);
   void handOverBreak (core::style::Style *style);
   void changeLinkColor (int link, int newColor);
   void changeWordStyle (int from, int to, core::style::Style *style,
                         bool includeFirstSpace, bool includeLastSpace);
};

} // namespace dw

#endif // __DW_TEXTBLOCK_HH__
