/*
 * File: web.cc
 *
 * Copyright 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "msg.h"
#include "nav.h"

#include "uicmd.hh"

#include "IO/IO.h"
#include "IO/mime.h"

#include "dw/core.hh"
#include "styleengine.hh"
#include "web.hh"

// Platform independent part
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

   _MSG("a_Web_dispatch_by_type\n");

   dReturn_val_if_fail(Web->bw != NULL, -1);

   Layout *layout = (Layout*)Web->bw->render_layout;

   Viewer_t viewer = a_Mime_get_viewer(Type);
   if (viewer == NULL)
      return -1;

   if (Web->flags & WEB_RootUrl) {
      /* We have RootUrl! */

      style::Color *bgColor = style::Color::create (layout, prefs.bg_color);
      Web->bgColor = bgColor->getColor ();
      layout->setBgColor (bgColor);
      layout->setBgImage (NULL, style::BACKGROUND_REPEAT,
                          style::BACKGROUND_ATTACHMENT_SCROLL,
                          style::createPerLength (0),
                          style::createPerLength (0));

      /* Set a style for the widget */
      StyleEngine styleEngine (layout, Web->url, Web->url);
      styleEngine.startElement ("body", Web->bw);

      dw = (Widget*) viewer(Type, Web, Call, Data);
      if (dw == NULL)
         return -1;

      dw->setStyle (styleEngine.style (Web->bw));

      /* This method frees the old dw if any */
      layout->setWidget(dw);

      /* Set the page title with the bare filename (e.g. for images),
       * HTML pages with a <TITLE> tag will overwrite it later */
      const char *p = strrchr(URL_STR(Web->url), '/');
      a_UIcmd_set_page_title(Web->bw, p ? p+1 : "");

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
      if (!dStrnAsciiCasecmp(Type, "image/", 6)) {
         dw = (Widget*) viewer(Type, Web, Call, Data);
      } else {
         MSG_HTTP("'%s' cannot be displayed as image; has media type '%s'\n",
                  URL_STR(Web->url), Type);
      }
   }
   return (dw ? 1 : -1);
}


/*
 * Allocate and set safe values for a DilloWeb structure
 */
DilloWeb* a_Web_new(BrowserWindow *bw, const DilloUrl *url,
                    const DilloUrl *requester)
{
   DilloWeb *web= dNew(DilloWeb, 1);

   _MSG(" a_Web_new: ValidWebs ==> %d\n", dList_length(ValidWebs));
   web->url = a_Url_dup(url);
   web->requester = a_Url_dup(requester);
   web->bw = bw;
   web->flags = 0;
   web->Image = NULL;
   web->filename = NULL;
   web->stream  = NULL;
   web->SavedBytes = 0;
   web->bgColor = 0x000000; /* Dummy value will be overwritten
                             * in a_Web_dispatch_by_type. */
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
   a_Url_free(web->url);
   a_Url_free(web->requester);
   a_Image_unref(web->Image);
   dFree(web->filename);
   dList_remove(ValidWebs, (void *)web);
   _MSG("a_Web_free: ValidWebs=%d\n", dList_length(ValidWebs));
   dFree(web);
}

