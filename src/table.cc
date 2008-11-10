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
   CssPropertyList props, *table_cell_props;
   const char *attrbuf;
   int32_t border = -1, cellspacing = -1, cellpadding = -1, bgcolor = -1;
#endif

   DW2TB(html->dw)->addParbreak (0, html->styleEngine->style ());

#ifdef USE_TABLES
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "border")))
      border = isdigit(attrbuf[0]) ? strtol (attrbuf, NULL, 10) : 1;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellspacing")))
      cellspacing = strtol (attrbuf, NULL, 10);
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cellpadding")))
      cellpadding = strtol (attrbuf, NULL, 10);

   if (border != -1)
      props.set (CssProperty::CSS_PROPERTY_BORDER_WIDTH, border);
   if (cellspacing != -1) {
      props.set (CssProperty::CSS_PROPERTY_BORDER_SPACING_HORIZONTAL, cellspacing);
      props.set (CssProperty::CSS_PROPERTY_BORDER_SPACING_VERTICAL, cellspacing);
   }

   /* When dillo was started with the --debug-rendering option, there
    * is always a border around the table. */
   if (dillo_dbg_rendering && border < 1)
      props.set (CssProperty::CSS_PROPERTY_BORDER_WIDTH, 1);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width")))
      props.set (CssProperty::CSS_PROPERTY_WIDTH,
         a_Html_parse_length (html, attrbuf));

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrcasecmp (attrbuf, "left") == 0)
         props.set (CssProperty::CSS_PROPERTY_TEXT_ALIGN, TEXT_ALIGN_LEFT);
      else if (dStrcasecmp (attrbuf, "right") == 0)
         props.set (CssProperty::CSS_PROPERTY_TEXT_ALIGN, TEXT_ALIGN_RIGHT);
      else if (dStrcasecmp (attrbuf, "center") == 0)
         props.set (CssProperty::CSS_PROPERTY_TEXT_ALIGN, TEXT_ALIGN_CENTER);
   }

   /** \todo figure out how to implement shaded colors with CSS */
   props.set (CssProperty::CSS_PROPERTY_BORDER_COLOR, 0x000000);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      bgcolor = a_Html_color_parse(html, attrbuf, -1);
      if (bgcolor != -1) {
         if (bgcolor == 0xffffff && !prefs.allow_white_bg)
            bgcolor = prefs.bg_color;
         S_TOP(html)->current_bg_color = bgcolor;
         props.set (CssProperty::CSS_PROPERTY_BACKGROUND_COLOR, bgcolor);
      }
   }

   html->styleEngine->setNonCssProperties (&props);

   /* The style for the cells */
   table_cell_props = new CssPropertyList ();
   table_cell_props->ref ();
   if (border != -1)
      table_cell_props->set (CssProperty::CSS_PROPERTY_BORDER_WIDTH, border);
   if (dillo_dbg_rendering && border < 1)
      table_cell_props->set (CssProperty::CSS_PROPERTY_BORDER_WIDTH, 1);
   table_cell_props->set (CssProperty::CSS_PROPERTY_PADDING, cellpadding);
   /** \todo figure out how to implement shaded colors with CSS */
   table_cell_props->set (CssProperty::CSS_PROPERTY_BORDER_COLOR, 0x000000);

   if (S_TOP(html)->table_cell_props)
      S_TOP(html)->table_cell_props->unref ();

   S_TOP(html)->table_cell_props = table_cell_props;

   table = new dw::Table(prefs.limit_text_width);
   DW2TB(html->dw)->addWidget (table, html->styleEngine->style ());

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
   int32_t bgcolor = -1;
   bool new_style = false;
   CssPropertyList props, *table_cell_props;

#ifdef USE_TABLES
   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      _MSG("Invalid HTML syntax: <tr> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;
            props.set (CssProperty::CSS_PROPERTY_BACKGROUND_COLOR, bgcolor);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      html->styleEngine->setNonCssProperties (&props);

      ((dw::Table*)S_TOP(html)->table)->addRow (html->styleEngine->style ());

      if (a_Html_get_attr (html, tag, tagsize, "align")) {
         S_TOP(html)->cell_text_align_set = TRUE;
         a_Html_tag_set_align_attr (html, &props, tag, tagsize);
         html->styleEngine->setNonCssProperties (&props);
      }

      table_cell_props = new CssPropertyList (*S_TOP(html)->table_cell_props);
      if (bgcolor != -1) {
         table_cell_props->set (CssProperty::CSS_PROPERTY_BACKGROUND_COLOR, bgcolor);
         new_style = true;
      }
      if (a_Html_tag_set_valign_attr (html, tag, tagsize, table_cell_props))
         new_style = true;
      if (new_style) {
         S_TOP(html)->table_cell_props->unref ();
         S_TOP(html)->table_cell_props = table_cell_props;
         S_TOP(html)->table_cell_props->ref ();
      } else {
         delete table_cell_props;
      }
      break;
   default:
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TR;
#else
   DW2TB(html->dw)->addParbreak (0, html->styleEngine->style ());
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
   int32_t bgcolor;
   bool_t new_style;
   CssPropertyList props (*S_TOP(html)->table_cell_props);

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
      if (!S_TOP(html)->cell_text_align_set) {
         props.set (CssProperty::CSS_PROPERTY_TEXT_ALIGN, text_align);
      }
      if (a_Html_get_attr(html, tag, tagsize, "nowrap"))
         props.set (CssProperty::CSS_PROPERTY_WHITE_SPACE, WHITE_SPACE_NOWRAP);
      else
         props.set (CssProperty::CSS_PROPERTY_WHITE_SPACE, WHITE_SPACE_NORMAL);

      a_Html_tag_set_align_attr (html, &props, tag, tagsize);

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "width"))) {
         props.set (CssProperty::CSS_PROPERTY_WIDTH,
            a_Html_parse_length (html, attrbuf));
      }

      if (a_Html_tag_set_valign_attr (html, tag, tagsize, &props))
         new_style = TRUE;

      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            props.set (CssProperty::CSS_PROPERTY_BACKGROUND_COLOR, bgcolor);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      html->styleEngine->setNonCssProperties (&props);

      if (html->styleEngine->style ()->textAlign
          == TEXT_ALIGN_STRING)
         col_tb = new dw::TableCell (((dw::Table*)S_TOP(html)->table)->getCellRef (),
                                     prefs.limit_text_width);
      else
         col_tb = new Textblock (prefs.limit_text_width);

      col_tb->setStyle (html->styleEngine->style ());

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
