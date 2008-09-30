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

/* Undefine if you want to unroll tables. For instance for PDAs */
#define USE_TABLES

#define dillo_dbg_rendering 0

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;

/*
 * Forward declarations 
 */

static void Html_tag_open_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize,
                                     dw::core::style::TextAlignType text_align);

/*
 * <TABLE>
 */
void Html_tag_open_table(DilloHtml *html, const char *tag, int tagsize)
{
#ifdef USE_TABLES
   dw::core::Widget *table;
   dw::core::style::StyleAttrs style_attrs;
   dw::core::style::Style *cell_style, *old_style;
   const char *attrbuf;
   int32_t border = 0, cellspacing = 1, cellpadding = 2, bgcolor;
#endif

   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);

#ifdef USE_TABLES
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "border")))
      border = isdigit(attrbuf[0]) ? strtol (attrbuf, NULL, 10) : 1;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellspacing")))
      cellspacing = strtol (attrbuf, NULL, 10);
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellpadding")))
      cellpadding = strtol (attrbuf, NULL, 10);

   /* The style for the table */
   style_attrs = *S_TOP(html)->style;

   /* When dillo was started with the --debug-rendering option, there
    * is always a border around the table. */
   if (dillo_dbg_rendering)
      style_attrs.borderWidth.setVal (MIN (border, 1));
   else
      style_attrs.borderWidth.setVal (border);

   style_attrs.setBorderColor (
      Color::createShaded(HT2LT(html), S_TOP(html)->current_bg_color));
   style_attrs.setBorderStyle (BORDER_OUTSET);
   style_attrs.hBorderSpacing = cellspacing;
   style_attrs.vBorderSpacing = cellspacing;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width")))
      style_attrs.width = a_Html_parse_length (html, attrbuf);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrcasecmp (attrbuf, "left") == 0)
         style_attrs.textAlign = dw::core::style::TEXT_ALIGN_LEFT;
      else if (dStrcasecmp (attrbuf, "right") == 0)
         style_attrs.textAlign = dw::core::style::TEXT_ALIGN_RIGHT;
      else if (dStrcasecmp (attrbuf, "center") == 0)
         style_attrs.textAlign = dw::core::style::TEXT_ALIGN_CENTER;
   }

   if (!prefs.force_my_colors &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      bgcolor = a_Html_color_parse(html, attrbuf, -1);
      if (bgcolor != -1) {
         if (bgcolor == 0xffffff && !prefs.allow_white_bg)
            bgcolor = prefs.bg_color;
         S_TOP(html)->current_bg_color = bgcolor;
         style_attrs.backgroundColor =
            Color::createShaded (HT2LT(html), bgcolor);
      }
   }

   /* The style for the cells */
   cell_style = Style::create (HT2LT(html), &style_attrs);
   style_attrs = *S_TOP(html)->style;
   /* When dillo was started with the --debug-rendering option, there
    * is always a border around the cells. */
   if (dillo_dbg_rendering)
      style_attrs.borderWidth.setVal (1);
   else
      style_attrs.borderWidth.setVal (border ? 1 : 0);
   style_attrs.padding.setVal(cellpadding);
   style_attrs.setBorderColor (cell_style->borderColor.top);
   style_attrs.setBorderStyle (BORDER_INSET);

   old_style = S_TOP(html)->table_cell_style;
   S_TOP(html)->table_cell_style =
      Style::create (HT2LT(html), &style_attrs);
   if (old_style)
      old_style->unref ();

   table = new dw::Table(prefs.limit_text_width);
   DW2TB(html->dw)->addWidget (table, cell_style);
   cell_style->unref ();

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TOP;
   S_TOP(html)->cell_text_align_set = FALSE;
   S_TOP(html)->table = table;
#endif
}

/*
 * <TR>
 */
void Html_tag_open_tr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   dw::core::style::StyleAttrs style_attrs;
   dw::core::style::Style *style, *old_style;
   int32_t bgcolor = -1;
   bool new_style = false;

#ifdef USE_TABLES
   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      _MSG("Invalid HTML syntax: <tr> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      style = NULL;

      if (!prefs.force_my_colors &&
          (attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            style_attrs = *S_TOP(html)->style;
            style_attrs.backgroundColor =
               Color::createShaded (HT2LT(html), bgcolor);
            style = Style::create (HT2LT(html), &style_attrs);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      ((dw::Table*)S_TOP(html)->table)->addRow (style);
      if (style)
         style->unref ();

      if (a_Html_get_attr (html, tag, tagsize, "align")) {
         S_TOP(html)->cell_text_align_set = TRUE;
         a_Html_tag_set_align_attr (html, tag, tagsize);
      }

      style_attrs = *S_TOP(html)->table_cell_style;
      if (bgcolor != -1) {
         style_attrs.backgroundColor =Color::createShaded(HT2LT(html),bgcolor);
         new_style = true;
      }
      if (a_Html_tag_set_valign_attr (html, tag, tagsize, &style_attrs))
         new_style = true;
      if (new_style) {
         old_style = S_TOP(html)->table_cell_style;
         S_TOP(html)->table_cell_style =
            Style::create (HT2LT(html), &style_attrs);
         old_style->unref ();
      }
      break;
   default:
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TR;
#else
   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);
#endif
}

/*
 * <TD>
 */
void Html_tag_open_td(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_open_table_cell (html, tag, tagsize,
                             dw::core::style::TEXT_ALIGN_LEFT);
}

/*
 * <TH>
 */
void Html_tag_open_th(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_set_top_font(html, NULL, 0, 1, 1);
   Html_tag_open_table_cell (html, tag, tagsize,
                             dw::core::style::TEXT_ALIGN_CENTER);
}

/*
 * Utilities 
 */

/*
 * used by <TD> and <TH>
 */
static void Html_tag_open_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize,
                                     dw::core::style::TextAlignType text_align)
{
#ifdef USE_TABLES
   Widget *col_tb;
   int colspan = 1, rowspan = 1;
   const char *attrbuf;
   dw::core::style::StyleAttrs style_attrs;
   dw::core::style::Style *style, *old_style;
   int32_t bgcolor;
   bool_t new_style;

   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      BUG_MSG("<td> or <th> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
      BUG_MSG("<td> or <th> outside <tr>\n");
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

      /* text style */
      old_style = S_TOP(html)->style;
      style_attrs = *old_style;
      if (!S_TOP(html)->cell_text_align_set)
         style_attrs.textAlign = text_align;
      if (a_Html_get_attr(html, tag, tagsize, "nowrap"))
         style_attrs.whiteSpace = WHITE_SPACE_NOWRAP;
      else
         style_attrs.whiteSpace = WHITE_SPACE_NORMAL;

      S_TOP(html)->style =
         Style::create (HT2LT(html), &style_attrs);
      old_style->unref ();
      a_Html_tag_set_align_attr (html, tag, tagsize);

      /* cell style */
      style_attrs = *S_TOP(html)->table_cell_style;
      new_style = FALSE;

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width"))) {
         style_attrs.width = a_Html_parse_length (html, attrbuf);
         new_style = TRUE;
      }

      if (a_Html_tag_set_valign_attr (html, tag, tagsize, &style_attrs))
         new_style = TRUE;

      if (!prefs.force_my_colors &&
          (attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            new_style = TRUE;
            style_attrs.backgroundColor =
               Color::createShaded (HT2LT(html), bgcolor);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      if (S_TOP(html)->style->textAlign
          == TEXT_ALIGN_STRING)
         col_tb = new dw::TableCell (((dw::Table*)S_TOP(html)->table)->getCellRef (),
                                     prefs.limit_text_width);
      else
         col_tb = new Textblock (prefs.limit_text_width);

      if (new_style) {
         style = dw::core::style::Style::create (HT2LT(html), &style_attrs);
         col_tb->setStyle (style);
         style->unref ();
      } else
         col_tb->setStyle (S_TOP(html)->table_cell_style);

      ((dw::Table*)S_TOP(html)->table)->addCell (col_tb, colspan, rowspan);
      S_TOP(html)->textblock = html->dw = col_tb;

      /* Handle it when the user clicks on a link */
      html->connectSignals(col_tb);
      break;

   default:
      /* compiler happiness */
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TD;
#endif
}
