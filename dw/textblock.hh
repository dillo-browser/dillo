#ifndef __DW_TEXTBLOCK_HH__
#define __DW_TEXTBLOCK_HH__

#include "core.hh"
#include "../lout/misc.hh"

namespace dw {

/**
 * \brief A Widget for rendering text blocks, i.e. paragraphs or sequences
 *    of paragraphs.
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
 * There are 3 lists, dw::Textblock::words, dw::Textblock::lines, and
 * dw::Textblock::anchors. The word list is quite static; only new words
 * may be added. A word is either text, a widget, or a break.
 *
 * Lines refer to the word list (first and last). They are completely
 * redundant, i.e., they can be rebuilt from the words. Lines can be
 * rewrapped either completely or partially (see "Incremental Resizing"
 * below). For the latter purpose, several values are accumulated in the
 * lines. See dw::Textblock::Line for details.
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
protected:
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

      /* The following members contain accumulated values, from the top
       * down to the line before. */
      int maxLineWidth; /* maximum of all line widths */
      int maxWordMin;   /* maximum of all word minima */
      int maxParMax;    /* maximum of all paragraph maxima */
      int parMin;       /* the minimal total width down from the last
                         * paragraph start, to the *beginning* of the
                         * line */
      int parMax;       /* the maximal total width down from the last
                         * paragraph start, to the *beginning* of the
                         * line */
   };

   struct Word
   {
      /* TODO: perhaps add a xLeft? */
      core::Requisition size;
      /* Space after the word, only if it's not a break: */
      short origSpace; /* from font, set by addSpace */
      short effSpace;  /* effective space, set by wordWrap,
                        * used for drawing etc. */
      core::Content content;

      core::style::Style *style;
      core::style::Style *spaceStyle; /* initially the same as of the word,
                                         later set by a_Dw_page_add_space */
   };

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
      int compareTo(lout::misc::Comparable *other);

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

   bool limitTextWidth; /* from preferences */

   int redrawY;
   int lastWordDrawn;

   /* These values are set by set_... */
   int availWidth, availAscent, availDescent;

   int lastLineWidth;
   int lastLineParMin;
   int lastLineParMax;
   int wrapRef;  /* [0 based] */

   lout::misc::SimpleVector <Line> *lines;
   lout::misc::SimpleVector <Word> *words;
   lout::misc::SimpleVector <Anchor> *anchors;

   struct {int index, nChar;}
      hlStart[core::HIGHLIGHT_NUM_LAYERS], hlEnd[core::HIGHLIGHT_NUM_LAYERS];

   int hoverLink;  /* The link under the mouse pointer */


   void queueDrawRange (int index1, int index2);
   void getWordExtremes (Word *word, core::Extremes *extremes);
   void markChange (int ref);
   void justifyLine (Line *line, int availWidth);
   Line *addLine (int wordInd, bool newPar);
   void calcWidgetSize (core::Widget *widget, core::Requisition *size);
   void rewrap ();
   void decorateText(core::View *view, core::style::Style *style,
                     core::style::Color::Shading shading,
                     int x, int yBase, int width);
   void drawText(int wordIndex, core::View *view, core::Rectangle *area,
                 int xWidget, int yWidgetBase);
   void drawSpace(int wordIndex, core::View *view, core::Rectangle *area,
                  int xWidget, int yWidgetBase);
   void drawLine (Line *line, core::View *view, core::Rectangle *area);
   int findLineIndex (int y);
   int findLineOfWord (int wordIndex);
   Word *findWord (int x, int y, bool *inSpace);

   Word *addWord (int width, int ascent, int descent,
                  core::style::Style *style);
   void calcTextSize (const char *text, size_t len, core::style::Style *style,
                      core::Requisition *size);


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
         (line == lines->getRef (0) ? line1OffsetEff : 0);
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

   bool sendSelectionEvent (core::SelectionState::EventType eventType,
                            core::MousePositionEvent *event);

   virtual void wordWrap(int wordIndex);

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

public:
   static int CLASS_ID;

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
   void addSpace(core::style::Style *style);
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
