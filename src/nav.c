/*
 * File: nav.c
 *
 * Copyright (C) 2000-2006 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/* Support for a navigation stack */

#include <stdio.h>
#include <sys/stat.h>
#include "msg.h"
#include "list.h"
#include "nav.h"
#include "history.h"
#include "web.hh"
#include "uicmd.hh"
#include "dialog.hh"
#include "prefs.h"
#include "capi.h"

//#define DEBUG_LEVEL 3
#include "debug.h"

/*
 * Forward declarations
 */
static void Nav_reload(BrowserWindow *bw);


/*
 * Free memory used by this module
 */
void a_Nav_free(BrowserWindow *bw)
{
   a_Nav_cancel_expect(bw);
   dFree(bw->nav_stack);
}


/* Navigation stack methods ------------------------------------------------ */

/*
 * Return current nav_stack pointer [0 based; -1 = empty]
 */
int a_Nav_stack_ptr(BrowserWindow *bw)
{
   return bw->nav_stack_ptr;
}

/*
 * Move the nav_stack pointer
 */
static void Nav_stack_move_ptr(BrowserWindow *bw, int offset)
{
   int nptr;

   dReturn_if_fail (bw != NULL);
   if (offset != 0) {
      nptr = bw->nav_stack_ptr + offset;
      dReturn_if_fail (nptr >= 0 && nptr < bw->nav_stack_size);
      bw->nav_stack_ptr = nptr;
   }
}

/*
 * Return size of nav_stack [1 based]
 */
int a_Nav_stack_size(BrowserWindow *bw)
{
   return bw->nav_stack_size;
}

/*
 * Add an URL-index in the navigation stack.
 */
static void Nav_stack_add(BrowserWindow *bw, int idx)
{
   dReturn_if_fail (bw != NULL);

   ++bw->nav_stack_ptr;
   if (bw->nav_stack_ptr == bw->nav_stack_size) {
      a_List_add(bw->nav_stack, bw->nav_stack_size, bw->nav_stack_size_max);
      ++bw->nav_stack_size;
   } else {
      bw->nav_stack_size = bw->nav_stack_ptr + 1;
   }
   bw->nav_stack[bw->nav_stack_ptr] = idx;
}

/*
 * Remove an URL-index from the navigation stack.
 */
static void Nav_stack_remove(BrowserWindow *bw, int idx)
{
   int sz = a_Nav_stack_size(bw);

   dReturn_if_fail (bw != NULL && idx >=0 && idx < sz);

   for (  ; idx < sz - 1; ++idx)
      bw->nav_stack[idx] = bw->nav_stack[idx + 1];
   if (bw->nav_stack_ptr == --bw->nav_stack_size)
      --bw->nav_stack_ptr;
}

/*
 * Remove equal adyacent URLs at the top of the stack.
 * (It may happen with redirections)
 */
static void Nav_stack_clean(BrowserWindow *bw)
{
   int i;

   dReturn_if_fail (bw != NULL);

   if ((i = a_Nav_stack_size(bw)) >= 2 &&
       bw->nav_stack[i-2] == bw->nav_stack[i-1])
         Nav_stack_remove(bw, i - 1);
}


/* General methods --------------------------------------------------------- */

/*
 * Create a DilloWeb structure for 'url' and ask the cache to send it back.
 *  - Also set a few things related to the browser window.
 * This function requests the page's root-URL; images and related stuff
 * are fetched directly by the HTML module.
 */
static void Nav_open_url(BrowserWindow *bw, const DilloUrl *url, int offset)
{
   DilloUrl *old_url = NULL;
   bool_t MustLoad;
   int x, y, ClientKey;
   DilloWeb *Web;
   char *loc_text;
   bool_t ForceReload = (URL_FLAGS(url) & URL_E2EReload);

   MSG("Nav_open_url: Url=>%s<\n", URL_STR_(url));

   /* Get the url of the current page */
   if (a_Nav_stack_ptr(bw) != -1)
      old_url = a_History_get_url(NAV_TOP(bw));

   /* Record current scrolling position
    * (the strcmp check is necessary because of redirections) */
   loc_text = a_UIcmd_get_location_text(bw);
   if (old_url && !strcmp(URL_STR(old_url), loc_text)) {
      a_UIcmd_get_scroll_xy(bw, &x, &y);
      _MSG("NAV: ScrollPosXY: x=%d y=%d\n",x,y);
      a_Url_set_pos(old_url, x, y);
   }

   /* Update navigation-stack-pointer (offset may be zero) */
   Nav_stack_move_ptr(bw, offset);

   /* Page must be reloaded, if old and new url (without anchor) differ */
   MustLoad = ForceReload || !old_url;
   if (old_url){
      MustLoad |= (a_Url_cmp(old_url, url) != 0);
      MustLoad |= strcmp(URL_STR(old_url), a_UIcmd_get_location_text(bw));
   }
   dFree(loc_text);

   if (MustLoad) {
      a_Bw_stop_clients(bw, BW_Root + BW_Img);
      a_Bw_cleanup(bw);

      // a_Menu_pagemarks_new(bw);

      Web = a_Web_new(url);
      Web->bw = bw;
      Web->flags |= WEB_RootUrl;
      if ((ClientKey = a_Capi_open_url(Web, NULL, NULL)) != 0) {
         a_Bw_add_client(bw, ClientKey, 1);
         a_Bw_add_url(bw, url);
      }
   }

   /* Jump to #anchor position */
   if (URL_FRAGMENT_(url)) {
      /* todo: push on stack */
      char *pf = a_Url_decode_hex_str(URL_FRAGMENT_(url));
      //a_Dw_render_layout_set_anchor(bw->render_layout, pf);
      dFree(pf);
   }
}

/*
 * Cancel the last expected url if present. The responsibility
 * for actually aborting the data stream remains with the caller.
 */
void a_Nav_cancel_expect(BrowserWindow *bw)
{
   if (bw->nav_expecting) {
      if (bw->nav_expect_url) {
         a_Url_free(bw->nav_expect_url);
         bw->nav_expect_url = NULL;
      }
      bw->nav_expecting = FALSE;
   }
}

/*
 * We have an answer! Set things accordingly.
 */
void a_Nav_expect_done(BrowserWindow *bw)
{
   int idx;
   DilloUrl *url;

   dReturn_if_fail(bw != NULL);

   if (bw->nav_expecting) {
      url = bw->nav_expect_url;
      /* unset E2EReload before adding this url to history */
      a_Url_set_flags(url, URL_FLAGS(url) & ~URL_E2EReload);
      idx = a_History_add_url(url);
      Nav_stack_add(bw, idx);

      a_Url_free(url);
      bw->nav_expect_url = NULL;
      bw->nav_expecting = FALSE;
   }
   Nav_stack_clean(bw);
   a_UIcmd_set_buttons_sens(bw);
}

/*
 * Make 'url' the current browsed page (upon data arrival)
 * - Set bw to expect the URL data
 * - Ask the cache to feed back the requested URL (via Nav_open_url)
 */
void a_Nav_push(BrowserWindow *bw, const DilloUrl *url)
{
   dReturn_if_fail (bw != NULL);

   if (bw->nav_expecting && !a_Url_cmp(bw->nav_expect_url, url) &&
       !strcmp(URL_FRAGMENT(bw->nav_expect_url),URL_FRAGMENT(url))) {
      /* we're already expecting that url (most probably a double-click) */
      return;
   }
   a_Nav_cancel_expect(bw);
   bw->nav_expect_url = a_Url_dup(url);
   bw->nav_expecting = TRUE;
   Nav_open_url(bw, url, 0);
}

/*
 * Same as a_Nav_push() but in a new window.
 */
void a_Nav_push_nw(BrowserWindow *bw, const DilloUrl *url)
{
   int w, h;
   BrowserWindow *newbw;

   a_UIcmd_get_wh(bw, &w, &h);
   newbw = a_UIcmd_browser_window_new(w, h);
   a_Nav_push(newbw, url);
}

/*
 * Wraps a_Nav_push to match 'DwPage->link' function type
 */
void a_Nav_vpush(void *vbw, const DilloUrl *url)
{
   a_Nav_push(vbw, url);
}

/*
 * Send the browser back to previous page
 */
void a_Nav_back(BrowserWindow *bw)
{
   int idx = a_Nav_stack_ptr(bw);

   a_Nav_cancel_expect(bw);
   if (--idx >= 0){
      a_UIcmd_set_msg(bw, "");
      Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)), -1);
   }
}

/*
 * Send the browser to next page in the history list
 */
void a_Nav_forw(BrowserWindow *bw)
{
   int idx = a_Nav_stack_ptr(bw);

   a_Nav_cancel_expect(bw);
   if (++idx < a_Nav_stack_size(bw)) {
      a_UIcmd_set_msg(bw, "");
      Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)), +1);
   }
}

/*
 * Redirect the browser to the HOME page!
 */
void a_Nav_home(BrowserWindow *bw)
{
   a_Nav_push(bw, prefs.home);
}

/*
 * This one does a_Nav_reload's job!
 */
static void Nav_reload(BrowserWindow *bw)
{
   DilloUrl *url, *ReqURL;

   a_Nav_cancel_expect(bw);
   if (a_Nav_stack_size(bw)) {
      url = a_History_get_url(NAV_TOP(bw));
      ReqURL = a_Url_dup(a_History_get_url(NAV_TOP(bw)));
      /* Let's make reload be end-to-end */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) | URL_E2EReload);
      /* This is an explicit reload, so clear the SpamSafe flag */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) & ~URL_SpamSafe);
      Nav_open_url(bw, ReqURL, 0);
      a_Url_free(ReqURL);
   }
}

/*
 * Implement the RELOAD button functionality.
 * (Currently it only reloads the page, not its images)
 */
void a_Nav_reload(BrowserWindow *bw)
{
   DilloUrl *url;
   int choice;

   a_Nav_cancel_expect(bw);
   if (a_Nav_stack_size(bw)) {
      url = a_History_get_url(NAV_TOP(bw));
      if (URL_FLAGS(url) & URL_Post) {
         /* Attempt to repost data, let's confirm... */
         choice = a_Dialog_choice3("Repost form data?",
                                   "Yes", "*No", "Cancel");
         if (choice == 0) { /* "Yes" */
            DEBUG_MSG(3, "Nav_reload_confirmed\n");
            Nav_reload(bw);
         }

      } else {
         Nav_reload(bw);
      }
   }
}

/*
 * Jump to a URL in the Navigation stack.
 */
void a_Nav_jump(BrowserWindow *bw, int offset, int new_bw)
{
   int idx = a_Nav_stack_ptr(bw) + offset;

   if (new_bw) {
      a_Nav_push_nw(bw, a_History_get_url(NAV_IDX(bw,idx)));
   } else {
      a_Nav_cancel_expect(bw);
      Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)), offset);
      a_UIcmd_set_buttons_sens(bw);
   }
}


/* Specific methods -------------------------------------------------------- */

/*
 * Receive data from the cache and save it to a local file
 */
static void Nav_save_cb(int Op, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;
   int Bytes;

   if (Op){
      struct stat st;

      fflush(Web->stream);
      fstat(fileno(Web->stream), &st);
      fclose(Web->stream);
      a_UIcmd_set_msg(Web->bw, "File saved (%d Bytes)", st.st_size);
   } else {
      if ((Bytes = Client->BufSize - Web->SavedBytes) > 0) {
         Bytes = fwrite(Client->Buf + Web->SavedBytes, 1, Bytes, Web->stream);
         Web->SavedBytes += Bytes;
      }
   }
}

/*
 * Save a URL (from cache or from the net).
 */
void a_Nav_save_url(BrowserWindow *bw,
                    const DilloUrl *url, const char *filename)
{
   DilloWeb *Web = a_Web_new(url);
   Web->bw = bw;
   Web->filename = dStrdup(filename); 
   Web->flags |= WEB_Download;
   /* todo: keep track of this client */
   a_Capi_open_url(Web, Nav_save_cb, Web);
}

/*
 * Wrapper for a_Capi_get_buf.
 */
int a_Nav_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   return a_Capi_get_buf(Url, PBuf, BufSize);
}

