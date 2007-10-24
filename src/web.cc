/*
 * File: web.cc
 *
 * Copyright 2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>      /* for rint */

#include "msg.h"
#include "nav.h"

#include "uicmd.hh"

#include "IO/IO.h"
#include "IO/mime.h"

#include "dw/core.hh"
#include "prefs.h"
#include "web.hh"

#define DEBUG_LEVEL 5
#include "debug.h"

// Platform idependent part
using namespace dw::core;


/*
 * Local data
 */
static Dlist *ValidWebs;      /* Active web structures list; it holds
                               * pointers to DilloWeb structures. */

/*
 * Initialize local data
 */
void a_Web_init(void)
{
   ValidWebs = dList_new(32);
}

/*
 * Given the MIME content type, and a fd to read it from,
 * this function connects the proper MIME viewer to it.
 * Return value:
 *    1 on success (and Call and Data properly set).
 *   -1 for unhandled MIME types (and Call and Data untouched).
 */
int a_Web_dispatch_by_type (const char *Type, DilloWeb *Web,
                            CA_Callback_t *Call, void **Data)
{
   Widget *dw = NULL;
   style::StyleAttrs styleAttrs;
   style::Style *widgetStyle;
   style::FontAttrs fontAttrs;

   DEBUG_MSG(1, "a_Web_dispatch_by_type\n");

   dReturn_val_if_fail(Web->bw != NULL, -1);

   // get the Layout object from the bw structure.
   Layout *layout = (Layout*)Web->bw->render_layout;

   if (Web->flags & WEB_RootUrl) {
      /* We have RootUrl! */
      dw = (Widget*) a_Mime_set_viewer(Type, Web, Call, Data);
      if (dw == NULL)
         return -1;

      /* Set a style for the widget */
      fontAttrs.name = "Bitstream Charter";
      fontAttrs.size = (int) rint(12.0 * prefs.font_factor);
      fontAttrs.weight = 400;
      fontAttrs.style = style::FONT_STYLE_NORMAL;

      styleAttrs.initValues ();
      styleAttrs.margin.setVal (5);
      styleAttrs.font = style::Font::create (layout, &fontAttrs);
      styleAttrs.color = style::Color::createSimple (layout, 0xff0000);
      styleAttrs.backgroundColor = 
         style::Color::createSimple (layout, prefs.bg_color);
      widgetStyle = style::Style::create (layout, &styleAttrs);
      dw->setStyle (widgetStyle);
      widgetStyle->unref ();

      /* This method frees the old dw if any */
      layout->setWidget(dw);

      /* Clear the title bar for pages without a <TITLE> tag */
      a_UIcmd_set_page_title(Web->bw, "");
      a_UIcmd_set_location_text(Web->bw, URL_STR(Web->url));
      /* Reset both progress bars */
      a_UIcmd_set_page_prog(Web->bw, 0, 2);
      a_UIcmd_set_img_prog(Web->bw, 0, 0, 2);
      /* Reset the bug meter */
      a_UIcmd_set_bug_prog(Web->bw, 0);

      /* Let the Nav module know... */
      a_Nav_expect_done(Web->bw);

   } else {
      /* A non-RootUrl. At this moment we only handle image-children */
      if (!dStrncasecmp(Type, "image/", 6))
         dw = (Widget*) a_Mime_set_viewer(Type, Web, Call, Data);
   }

   if (!dw) {
      MSG_HTTP("unhandled MIME type: \"%s\"\n", Type);
   }
   return (dw ? 1 : -1);
}


/*
 * Allocate and set safe values for a DilloWeb structure
 */
DilloWeb* a_Web_new(const DilloUrl *url)
{
   DilloWeb *web= dNew(DilloWeb, 1);

   _MSG(" a_Web_new: ValidWebs ==> %d\n", dList_length(ValidWebs));
   web->url = a_Url_dup(url);
   web->bw = NULL;
   web->flags = 0;
   web->Image = NULL;
   web->filename = NULL;
   web->stream  = NULL;
   web->SavedBytes = 0;

   dList_append(ValidWebs, (void *)web);
   return web;
}

/*
 * Validate a DilloWeb pointer
 */
int a_Web_valid(DilloWeb *web)
{
   return (dList_find(ValidWebs, web) != NULL);
}

/*
 * Deallocate a DilloWeb structure
 */
void a_Web_free(DilloWeb *web)
{
   if (!web) return;
   if (web->url)
      a_Url_free(web->url);
   if (web->Image)
      a_Image_unref(web->Image);
   dFree(web->filename);
   dList_remove(ValidWebs, (void *)web);
   dFree(web);
}

