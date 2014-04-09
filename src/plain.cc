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
   class PlainLinkReceiver: public dw::core::Layout::LinkReceiver {
   public:
      DilloPlain *plain;
      bool press(dw::core::Widget *widget, int link, int img, int x, int y,
                 dw::core::EventButton *event);
   };
   PlainLinkReceiver plainReceiver;

   void addLine(char *Buf, uint_t BufSize);

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

   Layout *layout = (Layout*) bw->render_layout;
   // TODO (1x) No URL?
   StyleEngine styleEngine (layout, NULL, NULL);

   styleEngine.startElement ("body", bw);
   styleEngine.startElement ("pre", bw);
   widgetStyle = styleEngine.wordStyle (bw);
   widgetStyle->ref ();

   /* The context menu */
   layout->connectLink (&plainReceiver);

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
bool DilloPlain::PlainLinkReceiver::press (Widget *widget, int, int, int, int,
                                           EventButton *event)
{
   _MSG("DilloPlain::PlainLinkReceiver::buttonPress\n");

   if (event->button == 3) {
      a_UIcmd_page_popup(plain->bw, FALSE, NULL);
      return true;
   }
   return false;
}

void DilloPlain::addLine(char *Buf, uint_t BufSize)
{
   int len;
   char buf[129];
   char *end = Buf + BufSize;

   if (BufSize > 0) {
      // Limit word length to avoid X11 coordinate
      // overflow with extremely long lines.
      while ((len = a_Misc_expand_tabs(&Buf, end, buf, sizeof(buf) - 1))) {
         assert ((uint_t)len < sizeof(buf));
         buf[len] = '\0';
         DW2TB(dw)->addText(buf, len, widgetStyle);
      }
   } else {
      // Add dummy word for empty lines - otherwise the parbreak is ignored.
      DW2TB(dw)->addText("", 0, widgetStyle);
   }

   DW2TB(dw)->addParbreak(0, widgetStyle);
}

/*
 * Here we parse plain text and put it into the page structure.
 * (This function is called by Plain_callback whenever there's new data)
 */
void DilloPlain::write(void *Buf, uint_t BufSize, int Eof)
{
   char *Start;
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
         addLine(Start + i - len, len);
         if (Start[i] == '\r' && Start[i + 1] == '\n') ++i;
         if (i < MaxBytes) ++i;
         state = ST_SeekingEol;
         len = 0;
         break;
      }
   }
   Start_Ofs += i - len;
   if (Eof && len) {
      addLine(Start + i - len, len);
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

