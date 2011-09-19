/*
 * File: bw.c
 *
 * Copyright (C) 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/* Data structures for each browser window */


#include "bw.h"
#include "msg.h"
#include "list.h"
#include "capi.h"
#include "uicmd.hh"


/*
 * Local Data
 */
/* A list of working browser windows */
static BrowserWindow **bws;
static int num_bws, num_bws_max;


/*
 * Initialize global data
 */
void a_Bw_init(void)
{
   num_bws = 0;
   num_bws_max = 16;
   bws = NULL;
}

/*
 * Create a new browser window and return it.
 * (the new window is stored in browser_window[])
 */
BrowserWindow *a_Bw_new()
{
   BrowserWindow *bw;

   /* We use dNew0() to zero the memory */
   bw = dNew0(BrowserWindow, 1);
   a_List_add(bws, num_bws, num_bws_max);
   bws[num_bws++] = bw;

   /* Initialize nav_stack */
   bw->nav_stack = dList_new(8);
   bw->nav_stack_ptr = -1;

   /* Init expect */
   bw->nav_expect_url = NULL;

   bw->redirect_level = 0;
   bw->meta_refresh_status = 0;
   bw->meta_refresh_url = NULL;

   bw->RootClients = dList_new(8);
   bw->ImageClients = dList_new(8);
   bw->NumImages = 0;
   bw->NumImagesGot = 0;
   bw->NumPendingStyleSheets = 0;
   bw->PageUrls = dList_new(8);
   bw->Docs = dList_new(8);

   bw->num_page_bugs = 0;
   bw->page_bugs = dStr_new("");

   /* now that the bw is made, let's customize it.. */
   //Interface_browser_window_customize(bw);

   return bw;
}

/*
 * Free resources associated to a bw.
 */
void a_Bw_free(BrowserWindow *bw)
{
   int i, j;

   for (i = 0; i < num_bws; i++) {
      if (bws[i] == bw) {
         a_List_remove(bws, i, num_bws);

         dList_free(bw->RootClients);
         dList_free(bw->ImageClients);
         dList_free(bw->Docs);

         a_Url_free(bw->nav_expect_url);
         for (j = 0; j < dList_length(bw->PageUrls); ++j)
            a_Url_free(dList_nth_data(bw->PageUrls, j));
         dList_free(bw->PageUrls);

         for (j = 0; j < dList_length(bw->nav_stack); ++j)
            dFree(dList_nth_data(bw->nav_stack, j));
         dList_free(bw->nav_stack);

         a_Url_free(bw->meta_refresh_url);

         dStr_free(bw->page_bugs, 1);
         dFree(bw);
         break;
      }
   }
}

/*- Clients ----------------------------------------------------------------*/
/*
 * Add a reference to a cache-client. It is kept int this bw's list.
 * This helps us keep track of which are active in the window so that it's
 * possible to abort/stop them.
 * (Root: Flag, whether a Root URL or not)
 *
 * TODO: Make NumImages count different images.
 */
void a_Bw_add_client(BrowserWindow *bw, int Key, int Root)
{
   dReturn_if_fail ( bw != NULL );

   if (Root) {
      dList_append(bw->RootClients, INT2VOIDP(Key));
   } else {
      dList_append(bw->ImageClients, INT2VOIDP(Key));
      bw->NumImages++;
      /* --Images progress-bar stuff-- */
      a_UIcmd_set_img_prog(bw, bw->NumImagesGot, bw->NumImages, 1);
   }
   if (dList_length(bw->RootClients) + dList_length(bw->ImageClients) == 1)
      a_UIcmd_set_buttons_sens(bw);
}

/*
 * Remove the cache-client from the bw's list
 * (client can be a image or a html page)
 * Return: 0 if found, 1 otherwise.
 */
int a_Bw_remove_client(BrowserWindow *bw, int ClientKey)
{
   void *data;

   if ((data = dList_find(bw->RootClients, INT2VOIDP(ClientKey)))) {
      dList_remove_fast(bw->RootClients, data);
   } else if ((data = dList_find(bw->ImageClients, INT2VOIDP(ClientKey)))) {
      dList_remove_fast(bw->ImageClients, data);
      ++bw->NumImagesGot;
   }
   return data ? 0 : 1;
}

/*
 * Close a cache-client upon successful retrieval.
 * Remove the cache-client from the bw list and update the meters.
 * (client can be a image or a html page)
 */
void a_Bw_close_client(BrowserWindow *bw, int ClientKey)
{
   if (a_Bw_remove_client(bw, ClientKey) == 0) {
      a_UIcmd_set_img_prog(bw, bw->NumImagesGot, bw->NumImages, 1);
      if (bw->NumImagesGot == bw->NumImages)
         a_UIcmd_set_img_prog(bw, 0, 0, 0);
      if (dList_length(bw->RootClients) == 0)
         a_UIcmd_set_buttons_sens(bw);
   }
}

/*
 * Stop the active clients of this bw's top page.
 * Note: rendering stops, but the cache continues to be fed.
 */
void a_Bw_stop_clients(BrowserWindow *bw, int flags)
{
   void *data;

   if (flags & BW_Root) {
      /* Remove root clients */
      while ((data = dList_nth_data(bw->RootClients, 0))) {
         a_Capi_stop_client(VOIDP2INT(data), (flags & BW_Force));
         dList_remove_fast(bw->RootClients, data);
      }
   }

   if (flags & BW_Img) {
      /* Remove image clients */
      while ((data = dList_nth_data(bw->ImageClients, 0))) {
         a_Capi_stop_client(VOIDP2INT(data), (flags & BW_Force));
         dList_remove_fast(bw->ImageClients, data);
      }
   }
}

/*- Page -------------------------------------------------------------------*/
/*
 * Add an URL to the browser window's list.
 * This helps us keep track of page-requested URLs so that it's
 * possible to stop, abort and reload them.
 */
void a_Bw_add_url(BrowserWindow *bw, const DilloUrl *Url)
{
   dReturn_if_fail ( bw != NULL && Url != NULL );

   if (!dList_find_custom(bw->PageUrls, Url, (dCompareFunc)a_Url_cmp)) {
      dList_append(bw->PageUrls, a_Url_dup(Url));
   }
}

/*
 * Add a document to the browser window's list.
 */
void a_Bw_add_doc(BrowserWindow *bw, void *vdoc)
{
   dReturn_if_fail ( bw != NULL && vdoc != NULL);

   dList_append(bw->Docs, vdoc);
}

/*
 * Get current document.
 */
void *a_Bw_get_current_doc(BrowserWindow *bw)
{
   void *doc = NULL;
   int len = dList_length(bw->Docs);

   if (len == 1)
      doc = dList_nth_data(bw->Docs, 0);
   else if (len > 1)
      MSG("a_Bw_get_current_doc() multiple docs not implemented\n");

   return doc;
}

/*
 * Get document by URL.
 *
 * This is currently used by popup menus that need to ensure that the
 * page has not changed while the menu was popped up.
 */
void *a_Bw_get_url_doc(BrowserWindow *bw, const DilloUrl *url)
{
   void *doc = NULL;

   if (url && dList_find_custom(bw->PageUrls, url, (dCompareFunc)a_Url_cmp)) {
      doc = a_Bw_get_current_doc(bw);
   }
   return doc;
}

/*
 * Remove a document from the bw's list
 */
void a_Bw_remove_doc(BrowserWindow *bw, void *vdoc)
{
   void *data;

   if ((data = dList_find(bw->Docs, vdoc))) {
      dList_remove_fast(bw->Docs, data);
   }
}

/*- Cleanup ----------------------------------------------------------------*/
/*
 * Empty RootClients, ImageClients and PageUrls lists and
 * reset progress bar data.
 */
void a_Bw_cleanup(BrowserWindow *bw)
{
   void *data;

   /* Remove root clients */
   while ((data = dList_nth_data(bw->RootClients, 0))) {
      dList_remove_fast(bw->RootClients, data);
   }
   /* Remove image clients */
   while ((data = dList_nth_data(bw->ImageClients, 0))) {
      dList_remove_fast(bw->ImageClients, data);
   }
   /* Remove PageUrls */
   while ((data = dList_nth_data(bw->PageUrls, 0))) {
      a_Url_free(data);
      dList_remove_fast(bw->PageUrls, data);
   }

   /* Zero image-progress data */
   bw->NumImages = 0;
   bw->NumImagesGot = 0;

   /* Zero stylesheet counter */
   bw->NumPendingStyleSheets = 0;
}

/*--------------------------------------------------------------------------*/

int a_Bw_num()
{
   return num_bws;
}

/*
 * Return a bw by index
 */
BrowserWindow *a_Bw_get(int i)
{
   if (i >= 0 && i < num_bws)
      return bws[i];
   return NULL;
}

/* expect API ------------------------------------------------------------- */

void a_Bw_expect(BrowserWindow *bw, const DilloUrl *url)
{
   a_Url_free(bw->nav_expect_url);
   bw->nav_expect_url = a_Url_dup(url);
}

void a_Bw_cancel_expect(BrowserWindow *bw)
{
   a_Url_free(bw->nav_expect_url);
   bw->nav_expect_url = NULL;
}

bool_t a_Bw_expecting(BrowserWindow *bw)
{
   return (bw->nav_expect_url != NULL);
}

const DilloUrl *a_Bw_expected_url(BrowserWindow *bw)
{
   return bw->nav_expect_url;
}

