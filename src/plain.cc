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

typedef struct _DilloPlainED DilloPlainED;

struct _DilloPlainED {
   class PlainEventReceiver: public dw::core::Widget::EventReceiver
   {
   private:
      DilloPlainED *ed;

   public:
      inline PlainEventReceiver (DilloPlainED *ed) { this->ed = ed; }

      bool buttonPress(dw::core::Widget *widget, dw::core::EventButton *event);
   };

   // Since DilloPlain is a struct, not a class, a simple
   // "PlainEventReceiver eventReceiver" (see signal documentation) would not
   // work, therefore the pointer.
   PlainEventReceiver *eventReceiver;

   BrowserWindow *bw;
   DilloUrl *url;
};

typedef struct _DilloPlain {
   Widget *dw;
   DilloPlainED *eventdata;
   size_t Start_Ofs;    /* Offset of where to start reading next */
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
 * Create the plain event-data structure (analog to linkblock in HTML).
 */
static DilloPlainED *Plain_ed_new(BrowserWindow *bw, const DilloUrl *url)
{
   DilloPlainED *plain_ed = dNew(DilloPlainED, 1);

   plain_ed->eventReceiver = new DilloPlainED::PlainEventReceiver (plain_ed);
   plain_ed->bw = bw;
   plain_ed->url = a_Url_dup(url);

   return plain_ed;
}

/*
 * Free memory used by the eventdata structure
 */
static void Plain_ed_free(void *ed)
{
   DilloPlainED *plain_ed = (DilloPlainED *)ed;

   delete plain_ed->eventReceiver;
   a_Url_free(plain_ed->url);

   dFree(plain_ed);
}

/*
 * Receive the mouse button press event
 */
bool DilloPlainED::PlainEventReceiver::buttonPress (Widget *widget,
                                                    EventButton *event)
{
   if (event->button == 3) {
      a_UIcmd_page_popup(ed->bw, ed->url, NULL);
      return true;
   }
   return false;
}

/*
 * Create and initialize a new DilloPlain structure.
 */
static DilloPlain *Plain_new(BrowserWindow *bw, const DilloUrl *url)
{
   DilloPlain *plain;
   Textblock *textblock;
   style::StyleAttrs styleAttrs;
   style::FontAttrs fontAttrs;

   plain = dNew(DilloPlain, 1);
   plain->state = ST_SeekingEol;
   plain->Start_Ofs = 0;
   plain->bw = bw;
   textblock = new Textblock (false);
   plain->dw = (Widget*) textblock;

   // BUG: event receiver is never freed.
   plain->eventdata = Plain_ed_new(bw, url);

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
   plain->widgetStyle = style::Style::create (layout, &styleAttrs);

   /* The context menu */
   textblock->connectEvent (plain->eventdata->eventReceiver);

   return plain;
}

/*
 * Set callback function and callback data for "text/" MIME major-type.
 */
void *a_Plain_text(const char *type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloPlain *plain = Plain_new(web->bw, web->url);

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
      textblock->addText(a_Misc_expand_tabs(data), plain->widgetStyle);
      textblock->addParbreak(0, plain->widgetStyle);
      dFree(data);
      plain->Start_Ofs += len;
   }

   if (plain->bw)
      a_UIcmd_set_page_prog(plain->bw, plain->Start_Ofs, 1);
}
