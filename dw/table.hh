#ifndef __DW_TABLE_HH__
#define __DW_TABLE_HH__

#include "core.hh"
#include "tablecell.hh"
#include "../lout/misc.hh"

namespace dw {

/**
 * \brief A Widget for rendering tables.
 *
 * <h3>Introduction</h3>
 *
 * The dw::Table widget is used to render HTML tables.
 *
 * Each cell is itself a separate widget. Any widget may be used, however, in
 * dillo, only instances of dw::Textblock and dw::TableCell are used as
 * children of dw::Table.
 *
 *
 * <h3>Sizes</h3>
 *
 * <h4>General</h4>
 *
 * The following diagram shows the dependencies between the different
 * functions, which are related to size calculation. Click on the boxes
 * for more information.
 *
 * \dot
 * digraph G {
 *    node [shape=record, fontname=Helvetica, fontsize=10, color="#c0c0c0"];
 *    edge [arrowhead="open", arrowtail="none", labelfontname=Helvetica,
 *          labelfontsize=10, color="#404040", labelfontcolor="#000080",
 *          fontname=Helvetica, fontsize=10];
 *    fontname=Helvetica; fontsize=10;
 *
 *    sizeRequestImpl [color="#0000ff", URL="\ref dw::Table::sizeRequestImpl"];
 *    sizeAllocateImpl [color="#0000ff",
 *                      URL="\ref dw::Table::sizeAllocateImpl"];
 *    getExtremesImpl [color="#0000ff", URL="\ref dw::Table::getExtremesImpl"];
 *
 *    subgraph cluster_sizes {
 *       style="dashed"; color="#8080c0";
 *       calcCellSizes [URL="\ref dw::Table::calcCellSizes"];
 *       forceCalcCellSizes [URL="\ref dw::Table::forceCalcCellSizes"];
 *    }
 *
 *    subgraph cluster_extremes {
 *       style="dashed"; color="#8080c0";
 *       calcColumnExtremes [URL="\ref dw::Table::calcColumnExtremes"];
 *      forceCalcColumnExtremes[URL="\ref dw::Table::forceCalcColumnExtremes"];
 *    }
 *
 *    sizeRequestImpl -> forceCalcCellSizes [label="[B]"];
 *    sizeAllocateImpl -> calcCellSizes [label="[A]"];
 *    getExtremesImpl -> forceCalcColumnExtremes [label="[B]"];
 *
 *    forceCalcCellSizes -> calcColumnExtremes;
 *
 *    calcCellSizes -> forceCalcCellSizes [style="dashed", label="[C]"];
 *    calcColumnExtremes -> forceCalcColumnExtremes [style="dashed",
 *                                                   label="[C]"];
 * }
 * \enddot
 *
 * [A] In this case, the new calculation is \em not forced, but only
 * done, when necessary.
 *
 * [B] In this case, the new calculation is allways necessary, since [C]
 * is the case.
 *
 * [C] Whether this function is called, depends on NEEDS_RESIZE /
 * EXTREMES_CHANGED.
 *
 *
 * <h4>Apportionment</h4>
 *
 * \sa\ref rounding-errors
 *
 * Given two array \f$e_{i,\min}\f$ and \f$e_{i,\max}\f$, which
 * represent the column minima and maxima, and a total width \f$W\f$, \em
 * apportionment means to calculate column widths \f$w_{i}\f$, with
 *
 * \f[e_{i,\min} \le w_{i} \le e_{i,\max}\f]
 *
 * and
 *
 * \f[\sum w_{i} = W\f]
 *
 * There are different algorithms for apportionment, a simple one is
 * recommended in the HTML 4.0.1 specification
 * (http://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.5.2.2):
 *
 * \f[w_{i} = e_{i,\min} +
 *    {e_{i,\max} - e_{i,\min}\over\sum e_{i,\max} - \sum e_{i,\min}}
 *    (W - \sum e_{i,\min})\f]
 *
 * This one is used currently, but another one will be used soon, which is
 * described below. The rest of this chapter is independent of the exact
 * apportionment algorithm.
 *
 * When referring to the apportionment function, we will call it
 * \f$a_i (W, (e_{i,\min}), (e_{i,\min}))\f$ and write
 * something like this:
 *
 * \f[w_{i} = a_i (W, (e_{i,\min}), (e_{i,\max})) \f]
 *
 * It is implemented by dw::Table::apportion.
 *
 * <h4>Column Extremes</h4>
 *
 * \sa\ref rounding-errors
 *
 * The sizes, which all other sizes depend on, are column extremes, which
 * define, how wide a column may be at min and at max. They are
 * calculated in the following way:
 *
 * <ol>
 * <li> First, only cells with colspan = 1 are regarded:
 *      \f[ e_{\hbox{base},i,\min} = \max \{ e_{\hbox{cell},i,j,\min} \} \f]
 *      \f[ e_{\hbox{base},i,\max} = \max \{ e_{\hbox{cell},i,j,\max} \} \f]
 *      only for cells \f$(i, j)\f$ with colspan = 1.
 *
 * <li> Then,
 *      \f$e_{\hbox{span},i,\min}\f$ (but not \f$e_{\hbox{span},i,\max}\f$)
 *      are calculated from cells with colspan > 1. (In the following formulas,
 *      the cell at \f$(i_1, j)\f$ always span from \f$i_1\f$ to \f$i_2\f$.)
 *      If the minimal width of the column exceeds the sum of the column minima
 *      calculated in the last step:
 *      \f[e_{\hbox{cell},i_1,j,\min} >
 *         \sum_{i=i_1}^{i=i_2} e_{\hbox{base},i,\min}\f]
 *      then the minimal width of this cell is apportioned to the columns:
 *
 *      <ul>
 *      <li> If the minimal width of this cell also exceeds the sum of the
 *           column maxima:
 *        \f[e_{\hbox{cell},i_1,j,\min} >
 *           \sum_{i=i_1}^{i=i_2} e_{\hbox{base},i,\max}\f]
 *           then \f$e_{\hbox{cell},i_1,j,\min}\f$ is apportioned in a simple
 *           way:
 *        \f[e_{\hbox{span},i,j,\min} =
 *              e_{\hbox{base},i,\max}
 *                 {e_{\hbox{span},i,j,\min} \over
 *                  \sum_{i=i_1}^{i=i_2} e_{\hbox{base},i,\max}}\f]
 *      <li> Otherwise, the apportionment function is used:
 *        \f[e_{\hbox{span},i,j,\min} =
 *           a_i (e_{\hbox{cell},i_1,j,\min},
 *                (e_{\hbox{cell},i_1,j,\min} \ldots
 *                    e_{\hbox{cell},i_2,j,\min}),
 *                (e_{\hbox{cell},i_1,j,\max} \ldots
 *                    e_{\hbox{cell},i_2,j,\max}))\f]
 *      </ul>
 *
 *      After this, \f$e_{\hbox{span},i,\min}\f$ is then the maximum of all
 *      \f$e_{\hbox{span},i,j,\min}\f$.
 *
 * <li> Finally, the maximum of both is used.
 *      \f[ e_{i,\min} =
 *         \max \{ e_{\hbox{base},i,\min}, e_{\hbox{span},i,\min} \} \f]
 *      \f[ e_{i,\max} =
 *         \max \{ e_{\hbox{base},i,\max}, e_{i,\min} \} \f]
 *      For the maxima, there is no \f$e_{\hbox{span},i,\max}\f$, but it has to
 *      be assured, that the maximum is always greater than or equal to the
 *      minimum.
 *
 * </ol>
 *
 * Generally, if absolute widths are specified, they are, instead of the
 * results of dw::core::Widget::getExtremes, taken for the minimal and
 * maximal width of a cell (minus the box difference, i.e. the difference
 * between content size and widget size). If the content width
 * specification is smaller than the minimal content width of the widget
 * (determined by dw::core::Widget::getExtremes), the latter is used
 * instead.
 *
 * If percentage widths are specified, they are also collected, as column
 * maxima. A similar method as for the extremes is used, for cells with
 * colspan > 1:
 *
 * \f[w_{\hbox{span},i,j,\%} =
 *    a_i (w_{\hbox{cell},i_1,j,\%},
 *      (e_{\hbox{cell},i_1,j,\min} \ldots e_{\hbox{cell},i_2,j,\min}),
 *      (e_{\hbox{cell},i_1,j,\max} \ldots e_{\hbox{cell},i_2,j,\max}))\f]
 *
 * <h4>Cell Sizes</h4>
 *
 * <h5>Determining the Width of the Table</h5>
 *
 * The total width is
 *
 * <ul>
 * <li> the specified absolute width of the table, when given, or
 * <li> the available width (set by dw::Table::setWidth) times the specified
 *      percentage width of t(at max 100%), if the latter is given, or
 * <li> otherwise the available width.
 * </ul>
 *
 * In any case, it is corrected, if it is less than the minimal width
 * (but not if it is greater than the maximal width).
 *
 * \bug The parentheses is not fully clear, look at the old code.
 *
 * Details on differences because of styles are omitted. Below, this
 * total width is called \f$W\f$.
 *
 * <h5>Evaluating percentages</h5>
 *
 * The following algorithms are used to solve collisions between
 * different size specifications (absolute and percentage). Generally,
 * inherent sizes and specified absolute sizes are preferred.
 *
 * <ol>
 * <li> First, calculate the sum of the minimal widths, for columns, where
 *      no percentage width has been specified. The difference to the total
 *      width is at max available to the columns with percentage width
 *      specifications:
 *      \f[W_{\hbox{columns}_\%,\hbox{available}} = W - \sum e_{i,\min}\f]
 *      with only those columns \f$i\f$ with no percentage width specification.
 *
 * <li> Then, calculate the sum of the widths, which the columns with
 *      percentage width specification would allocate, when fully adhering to
 *      them:
 *      \f[W_{\hbox{columns}_\%,\hbox{best}} = W \sum w_{i,\%}\f]
 *      with only those columns \f$i\f$ with a percentage width specification.
 *
 * <li> Two cases are distinguished:
 *
 *      <ul>
 *      <li> \f$W_{\hbox{columns}_\%,\hbox{available}} \ge
 *             W_{\hbox{columns}_\%,\hbox{best}}\f$: In this case, the
 *           percentage widths can be used without any modification, by
 *           setting the extremes:
 *           \f[e_{i,\min} = e_{i,\max} = W w_{i,\%}\f]
 *           for only those columns \f$i\f$ with a percentage width
 *           specification.
 *
 *      <li> \f$W_{\hbox{columns}_\%,\hbox{available}} <
 *             W_{\hbox{columns}_\%,\hbox{best}}\f$: In this case, the widths
 *           for these columns must be cut down:
 *           \f[e_{i,\min} = e_{i,\max} =
 *              w_{i,\%}
 *              {W_{\hbox{columns}_\%,\hbox{available}} \over
 *               w_{\hbox{total},\%}}\f]
 *           with
 *           \f[w_{\hbox{total},\%} = \sum w_{i,\%}\f]
 *           in both cases for only those columns \f$i\f$ with a percentage
 *           width specification.
 *      </ul>
 * </ol>
 *
 * (\f$e_{i,\min}\f$ and \f$e_{i,\max}\f$ are set \em temporarily here,
 * the notation should be a bit clearer.)
 *
 *
 * <h5>Column Widths</h5>
 *
 * The column widths are now simply calculated by applying the
 * apportionment function.
 *
 *
 * <h5>Row Heights</h5>
 *
 * ...
 *
 * <h3>Alternative Apportionment Algorithm</h3>
 *
 * The algorithm described here tends to result in more homogeneous column
 * widths.
 *
 * The following rule leads to well-defined \f$w_{i}\f$: All columns
 * \f$i\f$ have have the same width \f$w\f$, except:
 * <ul>
 * <li> \f$w < e_{i,\min}\f$, or
 * <li> \f$w > e_{i,\max}\f$.
 * </ul>
 *
 * Furthermore, \f$w\f$ is
 * <ul>
 * <li> less than all \f$e_{i,\min}\f$ of columns not having \f$w\f$ as
 *      width, and
 * <li> greater than all \f$e_{i,\min}\f$ of columns not having \f$w\f$ as
 *      width.
 * </ul>
 *
 * Of course, \f$\sum w_{i} = W\f$ must be the case.
 *
 * Based on an initial value \f$w = {W\over n}\f$, \f$w\f$ can iteratively
 * adjusted, based on these rules.
 *
 *
 * <h3>Borders, Paddings, Spacing</h3>
 *
 * Currently, DwTable supports only the separated borders model (see CSS
 * specification). Borders, paddings, spacing is done by creating
 * dw::core::style::Style structures with values equivalent to following CSS:
 *
 * <pre>
 * TABLE {
 *   border:           outset \em table-border;
 *   border-collapse:  separate;
 *   border-spacing:   \em table-cellspacing;
 *   background-color: \em table-bgcolor;
 * }
 *
 * TD TH {
 *   border:           inset \em table-border;
 *   padding:          \em table-cellspacing;
 *   background-color: \em td/th-bgcolor;
 * }
 * </pre>
 *
 * Here, \em foo-bar refers to the attribute \em bar of the tag \em foo foo.
 * Look at the HTML parser for more details.
 */
class Table: public core::Widget
{
private:

   struct Child
   {
      enum {
         CELL,       // cell starts here
         SPAN_SPACE  // part of a spanning cell
      } type;
      union {
         struct {
            core::Widget *widget;
            int colspanOrig, colspanEff, rowspan;
         } cell;
         struct {
            int startCol, startRow;  // where the cell starts
         } spanSpace;
      };
   };

   class TableIterator: public core::Iterator
   {
   private:
      int index;

   public:
      TableIterator (Table *table, core::Content::Type mask, bool atEnd);
      TableIterator (Table *table, core::Content::Type mask, int index);

      lout::object::Object *clone();
      int compareTo(lout::object::Comparable *other);

      bool next ();
      bool prev ();
      void highlight (int start, int end, core::HighlightLayer layer);
      void unhighlight (int direction, core::HighlightLayer layer);
      void getAllocation (int start, int end, core::Allocation *allocation);
   };

   friend class TableIterator;

   bool limitTextWidth, rowClosed;
   int availWidth, availAscent, availDescent;  // set by set...

   int numRows, numCols, curRow, curCol;
   lout::misc::SimpleVector<Child*> *children;

   int redrawX, redrawY;

   /**
    * \brief The extremes of all columns.
    */
   lout::misc::SimpleVector<core::Extremes> *colExtremes;

   /**
    * \brief The widths of all columns.
    */
   lout::misc::SimpleVector<int> *colWidths;

   /**
    * Row cumulative height array: cumHeight->size() is numRows + 1,
    * cumHeight->get(0) is 0, cumHeight->get(numRows) is the total table
    * height.
    */
   lout::misc::SimpleVector<int> *cumHeight;
   /**
    * If a Cell has rowspan > 1, it goes into this array
    */
   lout::misc::SimpleVector<int> *rowSpanCells;
   /**
    * If a Cell has colspan > 1, it goes into this array
    */
   lout::misc::SimpleVector<int> *colSpanCells;
   lout::misc::SimpleVector<int> *baseline;

   lout::misc::SimpleVector<core::style::Style*> *rowStyle;

   /**
    * hasColPercent becomes true when any cell specifies a percentage width.
    */
   int hasColPercent;
   lout::misc::SimpleVector<core::style::Length> *colPercents;

   inline bool childDefined(int n)
   {
      return n < children->size() && children->get(n) != NULL &&
         children->get(n)->type != Child::SPAN_SPACE;
   }

   void reallocChildren (int newNumCols, int newNumRows);

   void calcCellSizes ();
   void forceCalcCellSizes ();
   void apportionRowSpan ();

   void calcColumnExtremes ();
   void forceCalcColumnExtremes ();

   void apportion2 (int totalWidth, int forceTotalWidth);
   void apportion_percentages2 (int totalWidth, int forceTotalWidth);

   void setCumHeight (int row, int value)
   {
      if (value != cumHeight->get (row)) {
         redrawY = lout::misc::min ( redrawY, value );
         cumHeight->set (row, value);
      }
   }

   inline void setColWidth (int col, int value)
   {
      if (value != colWidths->get (col)) {
         redrawX = lout::misc::min (redrawX, value);
         colWidths->set (col, value);
      }
   }

protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void getExtremesImpl (core::Extremes *extremes);
   void sizeAllocateImpl (core::Allocation *allocation);
   void resizeDrawImpl ();

   void setWidth (int width);
   void setAscent (int ascent);
   void setDescent (int descent);
   void draw (core::View *view, core::Rectangle *area);

   //bool buttonPressImpl (core::EventButton *event);
   //bool buttonReleaseImpl (core::EventButton *event);
   //bool motionNotifyImpl (core::EventMotion *event);

   void removeChild (Widget *child);

public:
   static int CLASS_ID;

   Table(bool limitTextWidth);
   ~Table();

   core::Iterator *iterator (core::Content::Type mask, bool atEnd);

   void addCell (Widget *widget, int colspan, int rowspan);
   void addRow (core::style::Style *style);
   TableCell *getCellRef ();
};

} // namespace dw

#endif // __DW_TABLE_HH__
