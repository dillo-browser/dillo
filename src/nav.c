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
typedef struct {
   int url_idx;
   int posx, posy;
} nav_stack_item;



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
 * Remove equal adjacent URLs at the top of the stack.
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
static void Nav_open_url(BrowserWindow *bw, const DilloUrl *url,
                         const DilloUrl *requester, int offset)
{
   const DilloUrl *old_url;
   bool_t MustLoad, ForceReload, IgnoreScroll;
   int x, y, idx, ClientKey;
   DilloWeb *Web;

   MSG("Nav_open_url: new url='%s'\n", URL_STR_(url));

   ForceReload = (URL_FLAGS(url) & (URL_E2EQuery + URL_ReloadFromCache)) != 0;
   IgnoreScroll = (URL_FLAGS(url) & URL_IgnoreScroll) != 0;

   /* Get the url of the current page */
   idx = a_Nav_stack_ptr(bw);
   old_url = a_History_get_url(NAV_UIDX(bw, idx));
   _MSG("Nav_open_url:  old_url='%s' idx=%d\n", URL_STR(old_url), idx);
   /* Record current scrolling position */
   if (old_url && !IgnoreScroll) {
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

      Web = a_Web_new(bw, url, requester);
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
   if (a_Bw_expecting(bw)) {
      a_Bw_cancel_expect(bw);
      a_UIcmd_set_buttons_sens(bw);
   }
   if (bw->meta_refresh_status > 0)
      --bw->meta_refresh_status;
}

/*
 * Cancel the expect if 'url' matches.
 */
void a_Nav_cancel_expect_if_eq(BrowserWindow *bw, const DilloUrl *url)
{
   if (a_Url_cmp(url, a_Bw_expected_url(bw)) == 0)
      a_Nav_cancel_expect(bw);
}

/*
 * We have an answer! Set things accordingly.
 * This function is called for root URLs only.
 * Beware: this function is much more complex than it looks
 *         because URLs with repush pass twice through it.
 */
void a_Nav_expect_done(BrowserWindow *bw)
{
   int m, url_idx, posx, posy, reload, repush, e2equery, goto_old_scroll=TRUE;
   DilloUrl *url;
   char *fragment = NULL;

   dReturn_if_fail(bw != NULL);

   if (a_Bw_expecting(bw)) {
      url = a_Url_dup(a_Bw_expected_url(bw));
      reload = (URL_FLAGS(url) & URL_ReloadPage);
      repush = (URL_FLAGS(url) & URL_ReloadFromCache);
      e2equery = (URL_FLAGS(url) & URL_E2EQuery);
      fragment = a_Url_decode_hex_str(URL_FRAGMENT_(url));

      /* Unset E2EQuery, ReloadPage, ReloadFromCache and IgnoreScroll
       * before adding this url to history */
      m = URL_E2EQuery|URL_ReloadPage|URL_ReloadFromCache|URL_IgnoreScroll;
      a_Url_set_flags(url, URL_FLAGS(url) & ~m);
      url_idx = a_History_add_url(url);
      a_Url_free(url);

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
      /* Scroll to where we were in this page */
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
void a_Nav_push(BrowserWindow *bw, const DilloUrl *url,
                                   const DilloUrl *requester)
{
   const DilloUrl *e_url;
   dReturn_if_fail (bw != NULL);

   e_url = a_Bw_expected_url(bw);
   if (e_url && !a_Url_cmp(e_url, url) &&
       !strcmp(URL_FRAGMENT(e_url),URL_FRAGMENT(url))) {
      /* we're already expecting that url (most probably a double-click) */
      return;
   }
   a_Nav_cancel_expect(bw);
   a_Bw_expect(bw, url);
   Nav_open_url(bw, url, requester, 0);
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
      a_Bw_expect(bw, url);
      Nav_open_url(bw, url, NULL, 0);
      a_Url_free(url);
   }
}

static void Nav_repush_callback(void *data)
{
   _MSG(">>>> Nav_repush_callback <<<<\n");
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
   MSG(">>>> a_Nav_repush <<<<\n");
   a_Timeout_add(0.0, Nav_repush_callback, (void*)bw);
}

/*
 * This one does a_Nav_redirection0's job.
 */
static void Nav_redirection0_callback(void *data)
{
   BrowserWindow *bw = (BrowserWindow *)data;
   const DilloUrl *referer_url = a_History_get_url(NAV_TOP_UIDX(bw));
   _MSG(">>>> Nav_redirection0_callback <<<<\n");

   if (bw->meta_refresh_status == 2) {
      Nav_stack_move_ptr(bw, -1);
      a_Nav_push(bw, bw->meta_refresh_url, referer_url);
   }
   a_Url_free(bw->meta_refresh_url);
   bw->meta_refresh_url = NULL;
   bw->meta_refresh_status = 0;
   a_Timeout_remove();
}

/*
 * Handle a zero-delay URL redirection given by META
 */
void a_Nav_redirection0(BrowserWindow *bw, const DilloUrl *new_url)
{
   dReturn_if_fail (bw != NULL);
   _MSG(">>>> a_Nav_redirection0 <<<<\n");

   a_Url_free(bw->meta_refresh_url);
   bw->meta_refresh_url = a_Url_dup(new_url);
   a_Url_set_flags(bw->meta_refresh_url,
                   URL_FLAGS(new_url)|URL_E2EQuery|URL_IgnoreScroll);
   bw->meta_refresh_status = 2;
   a_Timeout_add(0.0, Nav_redirection0_callback, (void*)bw);
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
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), NULL, -1);
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
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), NULL, +1);
   }
}

/*
 * Redirect the browser to the HOME page!
 */
void a_Nav_home(BrowserWindow *bw)
{
   a_Nav_push(bw, prefs.home, NULL);
}

/*
 * This one does a_Nav_reload's job!
 */
static void Nav_reload_callback(void *data)
{
   BrowserWindow *bw = data;
   const DilloUrl *h_url;
   DilloUrl *r_url;
   int choice, confirmed = 1;

   a_Nav_cancel_expect(bw);
   if (a_Nav_stack_size(bw)) {
      h_url = a_History_get_url(NAV_TOP_UIDX(bw));
      if (dStrAsciiCasecmp(URL_SCHEME(h_url), "dpi") == 0 &&
          strncmp(URL_PATH(h_url), "/vsource/", 9) == 0) {
         /* allow reload for view source dpi */
         confirmed = 1;
      } else if (URL_FLAGS(h_url) & URL_Post) {
         /* Attempt to repost data, let's confirm... */
         choice = a_Dialog_choice("Dillo: Repost form?",
                                  "Repost form data?",
                                  "No", "Yes", "Cancel", NULL);
         confirmed = (choice == 2);  /* "Yes" */
      }

      if (confirmed) {
         r_url = a_Url_dup(h_url);
         /* Mark URL as reload to differentiate from push */
         a_Url_set_flags(r_url, URL_FLAGS(r_url) | URL_ReloadPage);
         /* Let's make reload be end-to-end */
         a_Url_set_flags(r_url, URL_FLAGS(r_url) | URL_E2EQuery);
         /* This is an explicit reload, so clear the SpamSafe flag */
         a_Url_set_flags(r_url, URL_FLAGS(r_url) & ~URL_SpamSafe);
         a_Bw_expect(bw, r_url);
         Nav_open_url(bw, r_url, NULL, 0);
         a_Url_free(r_url);
      }
   }
}

/*
 * Implement the RELOAD button functionality.
 * (Currently it only reloads the page, not its images)
 * Note: the timeout lets CCC operations end before making the request.
 */
void a_Nav_reload(BrowserWindow *bw)
{
   dReturn_if_fail (bw != NULL);
   a_Timeout_add(0.0, Nav_reload_callback, (void*)bw);
}

/*
 * Jump to a URL in the Navigation stack.
 */
void a_Nav_jump(BrowserWindow *bw, int offset, int new_bw)
{
   int idx = a_Nav_stack_ptr(bw) + offset;

   if (new_bw) {
      a_UIcmd_open_url_nw(bw, a_History_get_url(NAV_UIDX(bw,idx)));
   } else {
      a_Nav_cancel_expect(bw);
      Nav_open_url(bw, a_History_get_url(NAV_UIDX(bw,idx)), NULL, offset);
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
   DilloWeb *Web = a_Web_new(bw, url, NULL);
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
   a_Capi_unref_buf(Url);
}

/*
 * Wrapper for a_Capi_get_content_type().
 */
const char *a_Nav_get_content_type(const DilloUrl *url)
{
   return a_Capi_get_content_type(url);
}

/*
 * Wrapper for a_Capi_set_vsource_url().
 */
void a_Nav_set_vsource_url(const DilloUrl *Url)
{
   a_Capi_set_vsource_url(Url);
}
