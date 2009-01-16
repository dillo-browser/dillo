/*
 * File: nav.c
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
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
#include "nav.h"
#include "history.h"
#include "web.hh"
#include "uicmd.hh"
#include "dialog.hh"
#include "prefs.h"
#include "capi.h"
#include "timeout.hh"

/*
 * For back and forward navigation, each bw keeps an url index,
 * and its scroll position.
 */
typedef struct _nav_stack_item nav_stack_item;
struct _nav_stack_item
{
   int url_idx;
   int posx, posy;
};



/*
 * Free memory used by this module
 * TODO: this may be removed or called by a_Bw_free().
  *      Currently is not called from anywhere.
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
 * Return the url index of i-th element in the stack. [-1 = Error]
 */
int a_Nav_get_uidx(BrowserWindow *bw, int i)
{
   nav_stack_item *nsi = dList_nth_data (bw->nav_stack, i);
   return (nsi) ? nsi->url_idx : -1;
}

/*
 * Return the url index of the top element in the stack.
 */
int a_Nav_get_top_uidx(BrowserWindow *bw)
{
   nav_stack_item *nsi;

   nsi = dList_nth_data (bw->nav_stack, a_Nav_stack_ptr(bw));
   return (nsi) ? nsi->url_idx : -1;
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
      dReturn_if_fail (nptr >= 0 && nptr < a_Nav_stack_size(bw));
      bw->nav_stack_ptr = nptr;
   }
}

/*
 * Return size of nav_stack [1 based]
 */
int a_Nav_stack_size(BrowserWindow *bw)
{
   return dList_length(bw->nav_stack);
}

/*
 * Truncate the navigation stack including 'pos' and upwards.
 */
static void Nav_stack_truncate(BrowserWindow *bw, int pos)
{
   void *data;

   dReturn_if_fail (bw != NULL && pos >= 0);

   while (pos < dList_length(bw->nav_stack)) {
      data = dList_nth_data(bw->nav_stack, pos);
      dList_remove_fast (bw->nav_stack, data);
      dFree(data);
   }
}

/*
 * Insert a nav_stack_item into the stack at a given position.
 */
static void Nav_stack_append(BrowserWindow *bw, int url_idx)
{
   nav_stack_item *nsi;

   dReturn_if_fail (bw != NULL);

   /* Insert the new element */
   nsi = dNew(nav_stack_item, 1);
   nsi->url_idx = url_idx;
   nsi->posx = 0;
   nsi->posy = 0;
   dList_append (bw->nav_stack, nsi);
}

/*
 * Get the scrolling position of the current page.
 */
static void Nav_get_scroll_pos(BrowserWindow *bw, int *posx, int *posy)
{
   nav_stack_item *nsi;

   if ((nsi = dList_nth_data (bw->nav_stack, a_Nav_stack_ptr(bw)))) {
      *posx = nsi->posx;
      *posy = nsi->posy;
   } else {
      *posx = *posy = 0;
   }
}

/*
 * Save the scrolling position of the current page.
 */
static void Nav_save_scroll_pos(BrowserWindow *bw, int idx, int posx, int posy)
{
   nav_stack_item *nsi;

   if ((nsi = dList_nth_data (bw->nav_stack, idx))) {
      nsi->posx = posx;
      nsi->posy = posy;
   }
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
       NAV_UIDX(bw,i - 2) == NAV_UIDX(bw,i - 1)) {
      void *data = dList_nth_data (bw->nav_stack, i - 1);
      dList_remove_fast (bw->nav_stack, data);
      dFree(data);
      if (bw->nav_stack_ptr >= a_Nav_stack_size(bw))
         bw->nav_stack_ptr = a_Nav_stack_size(bw) - 1;
   }
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
   DilloUrl *old_url;
   bool_t MustLoad, ForceReload, Repush;
   int x, y, idx, ClientKey;
   DilloWeb *Web;

   MSG("Nav_open_url: new url='%s'\n", URL_STR_(url));

   Repush = (URL_FLAGS(url) & URL_ReloadFromCache) != 0;
   ForceReload = (URL_FLAGS(url) & (URL_E2EQuery + URL_ReloadFromCache)) != 0;

   /* Get the url of the current page */
   idx = a_Nav_stack_ptr(bw);
   old_url = a_History_get_url(NAV_UIDX(bw, idx));
   _MSG("Nav_open_url:  old_url='%s' idx=%d\n", URL_STR(old_url), idx);
   /* Record current scrolling position */
   if (Repush) {
      /* Don't change scroll position */
   } else if (old_url) {
      a_UIcmd_get_scroll_xy(bw, &x, &y);
      Nav_save_scroll_pos(bw, idx, x, y);
      _MSG("Nav_open_url:  saved scroll of '%s' at x=%d y=%d\n",
          URL_STR(old_url), x, y);
   }

   /* Update navigation-stack-pointer (offset may be zero) */
   Nav_stack_move_ptr(bw, offset);

   /* Page must be reloaded, if old and new url (considering anchor) differ */
   MustLoad = ForceReload || !old_url;
   if (old_url){
      MustLoad |= (a_Url_cmp(old_url, url) ||
                   strcmp(URL_FRAGMENT(old_url), URL_FRAGMENT(url)));
   }

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
 * This function is called for root URLs only.
 * Beware: this function is much more complex than it looks
 *         because URLs with repush pass twice through it.
 */
void a_Nav_expect_done(BrowserWindow *bw)
{
   int url_idx, posx, posy, reload, repush, e2equery, goto_old_scroll = TRUE;
   DilloUrl *url;
   char *fragment = NULL;

   dReturn_if_fail(bw != NULL);

   if (bw->nav_expecting) {
      url = bw->nav_expect_url;
      reload = (URL_FLAGS(url) & URL_ReloadPage);
      repush = (URL_FLAGS(url) & URL_ReloadFromCache);
      e2equery = (URL_FLAGS(url) & URL_E2EQuery);
      fragment = a_Url_decode_hex_str(URL_FRAGMENT_(url));

      /* Unset E2EQuery, ReloadPage and ReloadFromCache
       * before adding this url to history */
      a_Url_set_flags(url, URL_FLAGS(url) & ~URL_E2EQuery);
      a_Url_set_flags(url, URL_FLAGS(url) & ~URL_ReloadPage);
      a_Url_set_flags(url, URL_FLAGS(url) & ~URL_ReloadFromCache);
      url_idx = a_History_add_url(url);

      if (repush) {
         MSG("a_Nav_expect_done: repush!\n");
      } else if (reload) {
         MSG("a_Nav_expect_done: reload!\n");
      } else {
         Nav_stack_truncate(bw, a_Nav_stack_ptr(bw) + 1);
         Nav_stack_append(bw, url_idx);
         Nav_stack_move_ptr(bw, 1);
      }

      if (fragment) {
         goto_old_scroll = FALSE;
         if (repush) {
            Nav_get_scroll_pos(bw, &posx, &posy);
            if (posx || posy)
               goto_old_scroll = TRUE;
         } else if (e2equery) {
            /* Reset scroll, so repush goes to fragment in the next pass */
            Nav_save_scroll_pos(bw, a_Nav_stack_ptr(bw), 0, 0);
         }
      }
      a_Nav_cancel_expect(bw);
   }

   if (goto_old_scroll) {
      /* Scroll to were we were in this page */
      Nav_get_scroll_pos(bw, &posx, &posy);
      a_UIcmd_set_scroll_xy(bw, posx, posy);
      _MSG("Nav: expect_done scrolling to x=%d y=%d\n", posx, posy);
   } else if (fragment) {
      /* Scroll to fragment */
      a_UIcmd_set_scroll_by_fragment(bw, fragment);
   } else {
      /* Scroll to origin */
      a_UIcmd_set_scroll_xy(bw, 0, 0);
   }

   dFree(fragment);
   Nav_stack_clean(bw);
   a_UIcmd_set_buttons_sens(bw);
   _MSG("Nav: a_Nav_expect_done\n");
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
 * This one does a_Nav_repush's job.
 */
static void Nav_repush(BrowserWindow *bw)
{
   DilloUrl *url;

   a_Nav_cancel_expect(bw);
   if (a_Nav_stack_size(bw)) {
      url = a_Url_dup(a_History_get_url(NAV_TOP_UIDX(bw)));
      /* Let's make reload be from Cache */
      a_Url_set_flags(url, URL_FLAGS(url) | URL_ReloadFromCache);
      bw->nav_expect_url = a_Url_dup(url);
      bw->nav_expecting = TRUE;
      Nav_open_url(bw, url, 0);
      a_Url_free(url);
   }
}

static void Nav_repush_callback(void *data)
{
   Nav_repush(data);
   a_Timeout_remove();
}

/*
 * Repush current URL: not an end-to-end reload but from cache.
 * - Currently used to switch to a charset decoder given by the META element.
 * - Delayed to let dillo finish the call flow into a known state.
 *
 * There's no need to stop the parser before calling this function:
 * When the timeout activates, a_Bw_stop_clients will stop the data feed.
 */
void a_Nav_repush(BrowserWindow *bw)
{
   dReturn_if_fail (bw != NULL);
   MSG(">>> a_Nav_repush <<<<\n");
   a_Timeout_add(0.0, Nav_repush_callback, (void*)bw);
}

/*
 * Same as a_Nav_push() but in a new window.
 */
void a_Nav_push_nw(BrowserWindow *bw, const DilloUrl *url)
{
   int w, h;
   BrowserWindow *newbw;

   a_UIcmd_get_wh(bw, &w, &h);
   newbw = a_UIcmd_browser_window_new(w, h, bw);
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
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), -1);
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
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), +1);
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
   DilloUrl *ReqURL;

   a_Nav_cancel_expect(bw);
   if (a_Nav_stack_size(bw)) {
      ReqURL = a_Url_dup(a_History_get_url(NAV_TOP_UIDX(bw)));
      /* Mark URL as reload to differentiate from push */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) | URL_ReloadPage);
      /* Let's make reload be end-to-end */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) | URL_E2EQuery);
      /* This is an explicit reload, so clear the SpamSafe flag */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) & ~URL_SpamSafe);
      bw->nav_expect_url = ReqURL;
      bw->nav_expecting = TRUE;
      Nav_open_url(bw, ReqURL, 0);
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
      url = a_History_get_url(NAV_TOP_UIDX(bw));
      if (URL_FLAGS(url) & URL_Post) {
         /* Attempt to repost data, let's confirm... */
         choice = a_Dialog_choice3("Repost form data?",
                                   "Yes", "*No", "Cancel");
         if (choice == 0) { /* "Yes" */
            _MSG("Nav_reload_confirmed\n");
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
      a_Nav_push_nw(bw, a_History_get_url(NAV_UIDX(bw,idx)));
   } else {
      a_Nav_cancel_expect(bw);
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), offset);
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
   /* TODO: keep track of this client */
   a_Capi_open_url(Web, Nav_save_cb, Web);
}

/*
 * Wrapper for a_Capi_get_buf.
 */
int a_Nav_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize)
{
   return a_Capi_get_buf(Url, PBuf, BufSize);
}

/*
 * Wrapper for a_Capi_unref_buf().
 */
void a_Nav_unref_buf(const DilloUrl *Url)
{
   return a_Capi_unref_buf(Url);
}
