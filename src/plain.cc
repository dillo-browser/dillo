/*
 * File: plain.cc
 *
 * Copyright (C) 2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Module for decoding a text/plain object into a dw widget.
 */

#include <string.h>     /* for memcpy and memmove */
#include <math.h>       /* for rint() */

#include "prefs.h"
#include "cache.h"
#include "bw.h"
#include "web.hh"
#include "misc.h"
#include "history.h"
#include "nav.h"
#include "menu.hh"

#include "uicmd.hh"

#include "dw/core.hh"
#include "dw/textblock.hh"

using namespace dw;
using namespace dw::core;


typedef struct _DilloPlain {
   //DwWidget *dw;
   Widget *dw;
   size_t Start_Ofs;    /* Offset of where to start reading next */
   //DwStyle *style;
   style::Style *widgetStyle;
   BrowserWindow *bw;
   int state;
} DilloPlain;

/* FSM states */
enum {
   ST_SeekingEol,
   ST_Eol,
   ST_Eof
};

/*
 * Exported function with C linkage.
 */
extern "C" {
void *a_Plain_text(const char *type, void *P, CA_Callback_t *Call,void **Data);
}

/*
 * Forward declarations
 */
static void Plain_write(DilloPlain *plain, void *Buf, uint_t BufSize, int Eof);
static void Plain_callback(int Op, CacheClient_t *Client);



/*
 * Popup the page menu ("button_press_event" callback of the viewport)
 */
//static int Plain_page_menu(GtkWidget *viewport, GdkEventButton *event,
//                         BrowserWindow *bw)
//{
//   if (event->button == 3) {
//      a_Menu_popup_set_url(bw, a_History_get_url(NAV_TOP_UIDX(bw)));
//      gtk_menu_popup(GTK_MENU(bw->menu_popup.over_page), NULL, NULL,
//                     NULL, NULL, event->button, event->time);
//      return TRUE;
//   } else
//      return FALSE;
//}

/*
 * Create and initialize a new DilloPlain structure.
 */
static DilloPlain *Plain_new(BrowserWindow *bw)
{
   DilloPlain *plain;
   //DwPage *page;
   //DwStyle style_attrs;
   //DwStyleFont font_attrs;
   Textblock *textblock;
   style::StyleAttrs styleAttrs;
   style::FontAttrs fontAttrs;

   plain = dNew(DilloPlain, 1);
   plain->state = ST_SeekingEol;
   plain->Start_Ofs = 0;
   plain->bw = bw;
   //plain->dw = a_Dw_page_new();
   //page = (DwPage *) plain->dw;
   textblock = new Textblock (false);
   plain->dw = (Widget*) textblock;

   /* Create the font and attribute for the page. */
   //font_attrs.name = prefs.fw_fontname;
   //font_attrs.size = rint(12.0 * prefs.font_factor);
   //font_attrs.weight = 400;
   //font_attrs.style = DW_STYLE_FONT_STYLE_NORMAL;
   fontAttrs.name = "Courier";
   fontAttrs.size = (int) rint(12.0 * prefs.font_factor);
   fontAttrs.weight = 400;
   fontAttrs.style = style::FONT_STYLE_NORMAL;

   //a_Dw_style_init_values (&style_attrs);
   //style_attrs.font =
   //   a_Dw_style_font_new (plain->bw->render_layout, &font_attrs);
   //style_attrs.color =
   //   a_Dw_style_color_new (plain->bw->render_layout, prefs.text_color);
   //plain->style = a_Dw_style_new (plain->bw->render_layout, &style_attrs);
   //a_Dw_widget_set_style (plain->dw, plain->style);
   Layout *layout = (Layout*)bw->render_layout;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.font = style::Font::create (layout, &fontAttrs);
   styleAttrs.color = style::Color::createSimple (layout, prefs.text_color);
   styleAttrs.backgroundColor = 
      style::Color::createSimple (layout, prefs.bg_color);
   plain->widgetStyle = style::Style::create (layout, &styleAttrs);

   /* The context menu */
// gtk_signal_connect_while_alive
//    (GTK_OBJECT(GTK_BIN(plain->bw->render_main_scroll)->child),
//     "button_press_event", GTK_SIGNAL_FUNC(Plain_page_menu),
//     plain->bw, GTK_OBJECT (page));

   return plain;
}

/*
 * Set callback function and callback data for "text/" MIME major-type.
 */
void *a_Plain_text(const char *type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloPlain *plain = Plain_new(web->bw);

   *Call = (CA_Callback_t)Plain_callback;
   *Data = (void*)plain;

   return (void*)plain->dw;
}

/*
 * This function is a cache client
 */
static void Plain_callback(int Op, CacheClient_t *Client)
{
   DilloPlain *plain = (DilloPlain*)Client->CbData;
   Textblock *textblock = (Textblock*)plain->dw;

   if (Op) {
      /* Do the last line: */
      if (plain->Start_Ofs < Client->BufSize)
         Plain_write(plain, Client->Buf, Client->BufSize, 1);
      /* remove this client from our active list */
      a_Bw_close_client(plain->bw, Client->Key);
      /* set progress bar insensitive */
      a_UIcmd_set_page_prog(plain->bw, 0, 0);

      //a_Dw_style_unref (plain->bw->render_layout, plain->style);
      plain->widgetStyle->unref();
      dFree(plain);
   } else {
      Plain_write(plain, Client->Buf, Client->BufSize, 0);
   }

   textblock->flush();
}

/*
 * Here we parse plain text and put it into the page structure.
 * (This function is called by Plain_callback whenever there's new data)
 */
static void Plain_write(DilloPlain *plain, void *Buf, uint_t BufSize, int Eof)
{
   //DwPage *page = (DwPage *)plain->dw;
   Textblock *textblock = (Textblock*)plain->dw;
   char *Start;
   char *data;
   uint_t i, len, MaxBytes;

   Start = (char*)Buf + plain->Start_Ofs;
   MaxBytes = BufSize - plain->Start_Ofs;
   i = len = 0;
   while ( i < MaxBytes ) {
      switch ( plain->state ) {
      case ST_SeekingEol:
         if (Start[i] == '\n' || Start[i] == '\r')
            plain->state = ST_Eol;
         else {
            ++i; ++len;
         }
         break;
      case ST_Eol:
         data = dStrndup(Start + i - len, len);
         //a_Dw_page_add_text(page, a_Misc_expand_tabs(data), plain->style);
         //a_Dw_page_add_parbreak(page, 0, plain->style);
         textblock->addText(a_Misc_expand_tabs(data), plain->widgetStyle);
         textblock->addParbreak(0, plain->widgetStyle);
         dFree(data);
         if (Start[i] == '\r' && Start[i + 1] == '\n') ++i;
         if (i < MaxBytes) ++i;
         plain->state = ST_SeekingEol;
         len = 0;
         break;
      }
   }
   plain->Start_Ofs += i - len;
   if (Eof && len) {
      data = dStrndup(Start + i - len, len);
      //a_Dw_page_add_text(page, a_Misc_expand_tabs(data), plain->style);
      //a_Dw_page_add_parbreak(page, 0, plain->style);
      textblock->addText(a_Misc_expand_tabs(data), plain->widgetStyle);
      textblock->addParbreak(0, plain->widgetStyle);
      dFree(data);
      plain->Start_Ofs += len;
   }

   if (plain->bw)
      a_UIcmd_set_page_prog(plain->bw, plain->Start_Ofs, 1);
}
