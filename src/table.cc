/*
 * File: table.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "table.hh"
#include "html_common.hh"

#include "dw/style.hh"
#include "dw/textblock.hh"
#include "dw/table.hh"

#include "prefs.h"
#include "msg.h"
#include "css.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;

/*
 * Forward declarations
 */

static void Html_tag_open_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize,
                                     dw::core::style::TextAlignType text_align);
static void Html_tag_content_table_cell(DilloHtml *html,
                                        const char *tag, int tagsize);

/*
 * <TABLE>
 */
void Html_tag_open_table(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   int32_t border = -1, cellspacing = -1, cellpadding = -1, bgcolor = -1;
   CssLength cssLength;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "border")))
      border = isdigit(attrbuf[0]) ? strtol (attrbuf, NULL, 10) : 1;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellspacing"))) {
      cellspacing = strtol (attrbuf, NULL, 10);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<table> cellspacing attribute is obsolete.");
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellpadding"))) {
      cellpadding = strtol (attrbuf, NULL, 10);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<table> cellpadding attribute is obsolete.");
   }

   if (border != -1) {
      cssLength = CSS_CREATE_LENGTH (border, CSS_LENGTH_TYPE_PX);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_STYLE,
                                        CSS_TYPE_ENUM, BORDER_OUTSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                        CSS_TYPE_ENUM, BORDER_OUTSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_OUTSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_OUTSET);
   }

   if (cellspacing != -1) {
      cssLength = CSS_CREATE_LENGTH (cellspacing, CSS_LENGTH_TYPE_PX);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_SPACING,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width"))) {
      html->styleEngine->setNonCssHint (CSS_PROPERTY_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE,
                                        a_Html_parse_length (html, attrbuf));
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<table> width attribute is obsolete.");
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrAsciiCasecmp (attrbuf, "left") == 0)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_TEXT_ALIGN,
                                           CSS_TYPE_ENUM, TEXT_ALIGN_LEFT);
      else if (dStrAsciiCasecmp (attrbuf, "right") == 0)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_TEXT_ALIGN,
                                           CSS_TYPE_ENUM, TEXT_ALIGN_RIGHT);
      else if (dStrAsciiCasecmp (attrbuf, "center") == 0)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_TEXT_ALIGN,
                                           CSS_TYPE_ENUM, TEXT_ALIGN_CENTER);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<table> align attribute is obsolete.");
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      bgcolor = a_Html_color_parse(html, attrbuf, -1);
      if (bgcolor != -1)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BACKGROUND_COLOR,
                                           CSS_TYPE_COLOR, bgcolor);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<table> bgcolor attribute is obsolete.");
   }

   html->style (); // evaluate now, so we can build non-css hints for the cells

   /* The style for the cells */
   html->styleEngine->clearNonCssHints ();
   if (border > 0) {
      cssLength = CSS_CREATE_LENGTH (1, CSS_LENGTH_TYPE_PX);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_STYLE,
                                        CSS_TYPE_ENUM, BORDER_INSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                        CSS_TYPE_ENUM, BORDER_INSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_INSET);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_INSET);
   }

   if (cellpadding != -1) {
      cssLength = CSS_CREATE_LENGTH (cellpadding, CSS_LENGTH_TYPE_PX);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_PADDING_TOP,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_PADDING_BOTTOM,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_PADDING_LEFT,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_PADDING_RIGHT,
                                        CSS_TYPE_LENGTH_PERCENTAGE, cssLength);
   }

}
void Html_tag_content_table(DilloHtml *html, const char *tag, int tagsize)
{
   dw::core::Widget *table;

   HT2TB(html)->addParbreak (0, html->wordStyle ());
   table = new dw::Table(prefs.limit_text_width);
   HT2TB(html)->addWidget (table, html->style ());
   HT2TB(html)->addParbreak (0, html->wordStyle ());

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TOP;
   S_TOP(html)->table_border_mode = DILLO_HTML_TABLE_BORDER_SEPARATE;
   S_TOP(html)->cell_text_align_set = FALSE;
   S_TOP(html)->table = table;

}

/*
 * <TR>
 */
void Html_tag_open_tr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   int32_t bgcolor = -1;

   html->styleEngine->inheritNonCssHints ();

   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      _MSG("Invalid HTML syntax: <tr> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1)
            html->styleEngine->setNonCssHint (CSS_PROPERTY_BACKGROUND_COLOR,
                                              CSS_TYPE_COLOR, bgcolor);
         if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
            BUG_MSG("<tr> bgcolor attribute is obsolete.");
      }

      if (a_Html_get_attr (html, tag, tagsize, "align")) {
         S_TOP(html)->cell_text_align_set = TRUE;
         a_Html_tag_set_align_attr (html, tag, tagsize);
      }

      html->styleEngine->inheritBackgroundColor ();

      if (bgcolor != -1) {
         html->styleEngine->setNonCssHint(CSS_PROPERTY_BACKGROUND_COLOR,
                                          CSS_TYPE_COLOR, bgcolor);
      }
      a_Html_tag_set_valign_attr (html, tag, tagsize);
      break;
   default:
      break;
   }
}

void Html_tag_content_tr(DilloHtml *html, const char *tag, int tagsize)
{
   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      return;
   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      ((dw::Table*)S_TOP(html)->table)->addRow (html->style ());
   default:
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TR;
}

/*
 * <TD>
 */
void Html_tag_open_td(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_open_table_cell (html, tag, tagsize,
                             dw::core::style::TEXT_ALIGN_LEFT);
}

void Html_tag_content_td(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_content_table_cell (html, tag, tagsize);
}

/*
 * <TH>
 */
void Html_tag_open_th(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_open_table_cell (html, tag, tagsize,
                             dw::core::style::TEXT_ALIGN_CENTER);
}

void Html_tag_content_th(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_content_table_cell (html, tag, tagsize);
}

/*
 * Utilities
 */

/*
 * The table border model is stored in the table's stack item
 */
static int Html_table_get_border_model(DilloHtml *html)
{
   static int i_TABLE = -1;
   if (i_TABLE == -1)
      i_TABLE = a_Html_tag_index("table");

   int s_idx = html->stack->size();
   while (--s_idx > 0 && html->stack->getRef(s_idx)->tag_idx != i_TABLE)
      ;
   return html->stack->getRef(s_idx)->table_border_mode;
}

/*
 * Set current table's border model
 */
static void Html_table_set_border_model(DilloHtml *html,
                                        DilloHtmlTableBorderMode mode)
{
   int s_idx = html->stack->size(), i_TABLE = a_Html_tag_index("table");

   while (--s_idx > 0 && html->stack->getRef(s_idx)->tag_idx != i_TABLE) ;
   if (s_idx > 0)
      html->stack->getRef(s_idx)->table_border_mode = mode;
}

/* WORKAROUND: collapsing border model requires moving rendering code from
 *             the cell to the table, and making table-code aware of each
 *             cell style.
 * This workaround mimics collapsing model within separate model. This is not
 * a complete emulation but should be enough for most cases.
 */
static void Html_set_collapsing_border_model(DilloHtml *html, Widget *col_tb)
{
   dw::core::style::Style *collapseStyle, *tableStyle;
   dw::core::style::StyleAttrs collapseCellAttrs, collapseTableAttrs;
   int borderWidth, marginWidth;

   tableStyle = ((dw::Table*)S_TOP(html)->table)->getStyle ();
   borderWidth = html->style ()->borderWidth.top;
   marginWidth = tableStyle->margin.top;

   collapseCellAttrs = *(html->style ());
   collapseCellAttrs.margin.setVal (0);
   collapseCellAttrs.borderWidth.left = 0;
   collapseCellAttrs.borderWidth.top = 0;
   collapseCellAttrs.borderWidth.right = borderWidth;
   collapseCellAttrs.borderWidth.bottom = borderWidth;
   collapseCellAttrs.hBorderSpacing = 0;
   collapseCellAttrs.vBorderSpacing = 0;
   collapseStyle = Style::create(&collapseCellAttrs);
   col_tb->setStyle (collapseStyle);

   if (Html_table_get_border_model(html) != DILLO_HTML_TABLE_BORDER_COLLAPSE) {
      Html_table_set_border_model(html, DILLO_HTML_TABLE_BORDER_COLLAPSE);
      collapseTableAttrs = *tableStyle;
      collapseTableAttrs.margin.setVal (marginWidth);
      collapseTableAttrs.borderWidth.left = borderWidth;
      collapseTableAttrs.borderWidth.top = borderWidth;
      collapseTableAttrs.borderWidth.right = 0;
      collapseTableAttrs.borderWidth.bottom = 0;
      collapseTableAttrs.hBorderSpacing = 0;
      collapseTableAttrs.vBorderSpacing = 0;
      collapseTableAttrs.borderColor = collapseCellAttrs.borderColor;
      collapseTableAttrs.borderStyle = collapseCellAttrs.borderStyle;
      /* CSS2 17.6.2: table does not have padding (in collapsing mode) */
      collapseTableAttrs.padding.setVal (0);
      collapseStyle = Style::create(&collapseTableAttrs);
      ((dw::Table*)S_TOP(html)->table)->setStyle (collapseStyle);
   }
}

/*
 * Adjust style for separate border model.
 * (Dw uses this model internally).
 */
static void Html_set_separate_border_model(DilloHtml *html, Widget *col_tb)
{
   dw::core::style::Style *separateStyle;
   dw::core::style::StyleAttrs separateCellAttrs;

   separateCellAttrs = *(html->style ());
   /* CSS2 17.5: Internal table elements do not have margins */
   separateCellAttrs.margin.setVal (0);
   separateStyle = Style::create(&separateCellAttrs);
   col_tb->setStyle (separateStyle);
}

/*
 * used by <TD> and <TH>
 */
static void Html_tag_open_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize,
                                     dw::core::style::TextAlignType text_align)
{
   const char *attrbuf;
   int32_t bgcolor;

   html->styleEngine->inheritNonCssHints ();

   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
      /* a_Dw_table_add_cell takes care that dillo does not crash. */
      /* continues */
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      /* text style */
      if (!S_TOP(html)->cell_text_align_set) {
         html->styleEngine->setNonCssHint (CSS_PROPERTY_TEXT_ALIGN,
                                           CSS_TYPE_ENUM, text_align);
      }
      if (a_Html_get_attr(html, tag, tagsize, "nowrap")) {
         if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
            BUG_MSG("<t%c> nowrap attribute is obsolete.",
               (tagsize >=3 && (D_ASCII_TOLOWER(tag[2]) == 'd')) ? 'd' : 'h');
         html->styleEngine->setNonCssHint(CSS_PROPERTY_WHITE_SPACE,
                                          CSS_TYPE_ENUM, WHITE_SPACE_NOWRAP);
      }

      a_Html_tag_set_align_attr (html, tag, tagsize);

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width"))) {
         html->styleEngine->setNonCssHint (CSS_PROPERTY_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE,
                                           a_Html_parse_length (html, attrbuf));
         if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
            BUG_MSG("<t%c> width attribute is obsolete.",
               (tagsize >=3 && (D_ASCII_TOLOWER(tag[2]) == 'd')) ? 'd' : 'h');
      }

      a_Html_tag_set_valign_attr (html, tag, tagsize);

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1)
            html->styleEngine->setNonCssHint (CSS_PROPERTY_BACKGROUND_COLOR,
                                              CSS_TYPE_COLOR, bgcolor);
         if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
            BUG_MSG("<t%c> bgcolor attribute is obsolete.",
               (tagsize >=3 && (D_ASCII_TOLOWER(tag[2]) == 'd')) ? 'd' : 'h');
      }

   default:
      /* compiler happiness */
      break;
   }
}

static void Html_tag_content_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize)
{
   int colspan = 1, rowspan = 1;
   const char *attrbuf;
   Widget *col_tb;

   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      BUG_MSG("<t%c> outside <table>.",
              (tagsize >=3 && (D_ASCII_TOLOWER(tag[2]) == 'd')) ? 'd' : 'h');
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
      BUG_MSG("<t%c> outside <tr>.",
              (tagsize >=3 && (D_ASCII_TOLOWER(tag[2]) == 'd')) ? 'd' : 'h');
      /* a_Dw_table_add_cell takes care that dillo does not crash. */
      /* continues */
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "colspan"))) {
         char *invalid;
         colspan = strtol(attrbuf, &invalid, 10);
         if ((colspan < 0) || (attrbuf == invalid))
            colspan = 1;
      }
      /* TODO: check errors? */
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "rowspan")))
         rowspan = MAX(1, strtol (attrbuf, NULL, 10));
      if (html->style ()->textAlign
          == TEXT_ALIGN_STRING)
         col_tb = new dw::TableCell (
                     ((dw::Table*)S_TOP(html)->table)->getCellRef (),
                     prefs.limit_text_width);
      else
         col_tb = new Textblock (prefs.limit_text_width);

      if (html->style()->borderCollapse == BORDER_MODEL_COLLAPSE){
         Html_set_collapsing_border_model(html, col_tb);
      } else {
         Html_set_separate_border_model(html, col_tb);
      }

      ((dw::Table*)S_TOP(html)->table)->addCell (col_tb, colspan, rowspan);
      S_TOP(html)->textblock = html->dw = col_tb;
      break;

   default:
      /* compiler happiness */
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TD;
}
