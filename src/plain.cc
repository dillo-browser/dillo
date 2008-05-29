/*
 * File: plain.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
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

#include "msg.h"
#include "prefs.h"
#include "cache.h"
#include "bw.h"
#include "web.hh"
#include "misc.h"
#include "decode.h"

#include "uicmd.hh"

#include "dw/core.hh"
#include "dw/textblock.hh"

// Dw to Textblock
#define DW2TB(dw)  ((Textblock*)dw)

using namespace dw;
using namespace dw::core;


class DilloPlain {
private:
   class PlainEventReceiver: public dw::core::Widget::EventReceiver {
   public:
      DilloPlain *plain;
      bool buttonPress(dw::core::Widget *widget, dw::core::EventButton *event);
   };
   PlainEventReceiver plainReceiver;

public:
   BrowserWindow *bw;
   DilloUrl *url;

   Decode *decoder;
   size_t Buf_Consumed;
   char *content_type, *charset;

   Widget *dw;
   style::Style *widgetStyle;
   int state;

   DilloPlain(BrowserWindow *bw, const DilloUrl *url,
              const char *content_type);
   ~DilloPlain();

   void write(void *Buf, uint_t BufSize, int Eof);
};

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
static void Plain_callback(int Op, CacheClient_t *Client);
void a_Plain_free(void *data);


/*
 * Diplain constructor.
 */
DilloPlain::DilloPlain(BrowserWindow *p_bw, const DilloUrl *p_url,
                       const char *content_type)
{
   style::StyleAttrs styleAttrs;
   style::FontAttrs fontAttrs;

   /* Init event receiver */
   plainReceiver.plain = this;

   /* Init internal variables */
   bw = p_bw;
   url = a_Url_dup(p_url);
   dw = new Textblock (prefs.limit_text_width);
   state = ST_SeekingEol;

   MSG("PLAIN content type: %s\n", content_type);
   this->content_type = dStrdup(content_type);
   /* get charset */
   a_Misc_parse_content_type(content_type, NULL, NULL, &charset);
   /* Initiallize the charset decoder */
   decoder = a_Decode_charset_init(charset);
   Buf_Consumed = 0;

   /* Create the font and attribute for the page. */
   fontAttrs.name = prefs.fw_fontname;
   fontAttrs.size = (int) rint(12.0 * prefs.font_factor);
   fontAttrs.weight = 400;
   fontAttrs.style = style::FONT_STYLE_NORMAL;

   Layout *layout = (Layout*)bw->render_layout;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.font = style::Font::create (layout, &fontAttrs);
   styleAttrs.color = style::Color::createSimple (layout, prefs.text_color);
   styleAttrs.backgroundColor = 
      style::Color::createSimple (layout, prefs.bg_color);
   widgetStyle = style::Style::create (layout, &styleAttrs);

   /* The context menu */
   DW2TB(dw)->connectEvent (&plainReceiver);

   /* Hook destructor to the dw delete call */
   dw->setDeleteCallback(a_Plain_free, this);
}

/*
 * Free memory used by the DilloPlain class.
 */
DilloPlain::~DilloPlain()
{
   MSG("::~DilloPlain()\n");
   a_Url_free(url);
   a_Decode_free(decoder);
   dFree(content_type);
   dFree(charset);
   widgetStyle->unref();
}

/*
 * Receive the mouse button press event
 */
bool DilloPlain::PlainEventReceiver::buttonPress (Widget *widget,
                                                  EventButton *event)
{
   _MSG("DilloPlain::PlainEventReceiver::buttonPress\n");

   if (event->button == 3) {
      a_UIcmd_page_popup(plain->bw, plain->url, NULL, 1);
      return true;
   }
   return false;
}

/*
 * Here we parse plain text and put it into the page structure.
 * (This function is called by Plain_callback whenever there's new data)
 */
void DilloPlain::write(void *Buf, uint_t BufSize, int Eof)
{
   char *Start;
   char *data;
   Dstr *new_text = NULL;
   uint_t i, len, MaxBytes;

   _MSG(" DilloPlain::write Buf=%p, BufSize=%d Buf_Consumed=%d Eof=%d\n",
       Buf, BufSize, Buf_Consumed, Eof);

   char *str = (char*)Buf + Buf_Consumed;
   int str_len = BufSize - Buf_Consumed;

   /* decode to target charset (UTF-8) */
   if (decoder) {
      new_text = a_Decode_process(decoder, str, str_len);
      str = new_text->str;
      str_len = new_text->len;
   }

   Start = str;
   MaxBytes = str_len;
   i = len = 0;
   while ( i < MaxBytes ) {
      switch ( state ) {
      case ST_SeekingEol:
         if (Start[i] == '\n' || Start[i] == '\r')
            state = ST_Eol;
         else {
            ++i; ++len;
         }
         break;
      case ST_Eol:
         data = dStrndup(Start + i - len, len);
         DW2TB(dw)->addText(a_Misc_expand_tabs(data), widgetStyle);
         DW2TB(dw)->addParbreak(0, widgetStyle);
         dFree(data);
         if (Start[i] == '\r' && Start[i + 1] == '\n') ++i;
         if (i < MaxBytes) ++i;
         state = ST_SeekingEol;
         len = 0;
         break;
      }
   }
   if (Eof && len) {
      data = dStrndup(Start + i - len, len);
      DW2TB(dw)->addText(a_Misc_expand_tabs(data), widgetStyle);
      DW2TB(dw)->addParbreak(0, widgetStyle);
      dFree(data);
      len = 0;
   }
   Buf_Consumed = BufSize - len;
   dStr_free(new_text, 1);

   DW2TB(dw)->flush(Eof ? true : false);
   if (bw)
      a_UIcmd_set_page_prog(bw, Buf_Consumed, 1);
}

/*
 * Set callback function and callback data for "text/" MIME major-type.
 */
void *a_Plain_text(const char *Type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloPlain *plain = new DilloPlain(web->bw, web->url, Type);

   *Call = (CA_Callback_t)Plain_callback;
   *Data = (void*)plain;

   return (void*)plain->dw;
}

void a_Plain_free(void *data)
{
   MSG("a_Plain_free! %p\n", data);
   delete ((DilloPlain *)data);
}

/*
 * This function is a cache client
 */
static void Plain_callback(int Op, CacheClient_t *Client)
{
   DilloPlain *plain = (DilloPlain*)Client->CbData;

   if (Op) {
      /* Do the last line: */
      plain->write(Client->Buf, Client->BufSize, 1);
      /* remove this client from our active list */
      a_Bw_close_client(plain->bw, Client->Key);
      /* set progress bar insensitive */
      a_UIcmd_set_page_prog(plain->bw, 0, 0);
   } else {
      plain->write(Client->Buf, Client->BufSize, 0);
   }
}

