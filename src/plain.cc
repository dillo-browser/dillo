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

#include "msg.h"
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

   Widget *dw;
   style::Style *widgetStyle;
   size_t Start_Ofs;    /* Offset of where to start reading next */
   int state;

   DilloPlain(BrowserWindow *bw, const DilloUrl *url);
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
DilloPlain::DilloPlain(BrowserWindow *p_bw, const DilloUrl *p_url)
{
   Textblock *textblock;
   style::StyleAttrs styleAttrs;
   style::FontAttrs fontAttrs;

   /* init event receiver */
   plainReceiver.plain = this;

   /* Init internal variables */
   bw = p_bw;
   url = a_Url_dup(p_url);
   textblock = new Textblock (false);
   dw = (Widget*) textblock;
   Start_Ofs = 0;
   state = ST_SeekingEol;

   /* Create the font and attribute for the page. */
   fontAttrs.name = "Courier";
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
   textblock->connectEvent (&plainReceiver);

   /* Hook destructor to the dw delete call */
   dw->setDeleteCallback(a_Plain_free, this);
}

/*
 * Free memory used by the DilloPlain class.
 */
DilloPlain::~DilloPlain()
{
   a_Url_free(url);
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
      a_UIcmd_page_popup(plain->bw, plain->url, NULL);
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
   Textblock *textblock = (Textblock*)dw;
   char *Start;
   char *data;
   uint_t i, len, MaxBytes;

   Start = (char*)Buf + Start_Ofs;
   MaxBytes = BufSize - Start_Ofs;
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
         textblock->addText(a_Misc_expand_tabs(data), widgetStyle);
         textblock->addParbreak(0, widgetStyle);
         dFree(data);
         if (Start[i] == '\r' && Start[i + 1] == '\n') ++i;
         if (i < MaxBytes) ++i;
         state = ST_SeekingEol;
         len = 0;
         break;
      }
   }
   Start_Ofs += i - len;
   if (Eof && len) {
      data = dStrndup(Start + i - len, len);
      textblock->addText(a_Misc_expand_tabs(data), widgetStyle);
      textblock->addParbreak(0, widgetStyle);
      dFree(data);
      Start_Ofs += len;
   }

   textblock->flush();

   if (bw)
      a_UIcmd_set_page_prog(bw, Start_Ofs, 1);
}

/*
 * Set callback function and callback data for "text/" MIME major-type.
 */
void *a_Plain_text(const char *type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloPlain *plain = new DilloPlain(web->bw, web->url);

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
      if (plain->Start_Ofs < Client->BufSize)
         plain->write(Client->Buf, Client->BufSize, 1);
      /* remove this client from our active list */
      a_Bw_close_client(plain->bw, Client->Key);
      /* set progress bar insensitive */
      a_UIcmd_set_page_prog(plain->bw, 0, 0);
   } else {
      plain->write(Client->Buf, Client->BufSize, 0);
   }
}

