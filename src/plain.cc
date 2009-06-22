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

#include "msg.h"
#include "prefs.h"
#include "cache.h"
#include "bw.h"
#include "web.hh"
#include "misc.h"
#include "styleengine.hh"

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

   Widget *dw;
   style::Style *widgetStyle;
   size_t Start_Ofs;    /* Offset of where to start reading next */
   int state;

   DilloPlain(BrowserWindow *bw);
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
DilloPlain::DilloPlain(BrowserWindow *p_bw)
{
   /* Init event receiver */
   plainReceiver.plain = this;

   /* Init internal variables */
   bw = p_bw;
   dw = new Textblock (prefs.limit_text_width);
   Start_Ofs = 0;
   state = ST_SeekingEol;

   StyleEngine styleEngine ((Layout*)bw->render_layout);

   styleEngine.startElement ("body");
   styleEngine.startElement ("pre");
   widgetStyle = styleEngine.wordStyle ();
   widgetStyle->ref ();

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
   _MSG("::~DilloPlain()\n");
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
      a_UIcmd_page_popup(plain->bw, FALSE, NULL);
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
   uint_t i, len, MaxBytes;

   _MSG("DilloPlain::write Eof=%d\n", Eof);

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
         data = a_Misc_expand_tabs(Start + i - len, len);
         DW2TB(dw)->addText(data, widgetStyle);
         DW2TB(dw)->addParbreak(0, widgetStyle);
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
      data = a_Misc_expand_tabs(Start + i - len, len);
      DW2TB(dw)->addText(data, widgetStyle);
      DW2TB(dw)->addParbreak(0, widgetStyle);
      dFree(data);
      Start_Ofs += len;
   }

   DW2TB(dw)->flush();
}

/*
 * Set callback function and callback data for "text/" MIME major-type.
 */
void *a_Plain_text(const char *type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloPlain *plain = new DilloPlain(web->bw);

   *Call = (CA_Callback_t)Plain_callback;
   *Data = (void*)plain;

   return (void*)plain->dw;
}

void a_Plain_free(void *data)
{
   _MSG("a_Plain_free! %p\n", data);
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
   } else {
      plain->write(Client->Buf, Client->BufSize, 0);
   }
}

