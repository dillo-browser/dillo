/*
 * File: uicmd.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Functions/Methods for commands triggered from the UI


#include <stdio.h>
#include <stdarg.h>
#include <math.h>       /* for rint */
#include <fltk/Widget.h>
#include <fltk/TabGroup.h>

#include "dir.h"
#include "ui.hh"
#include "uicmd.hh"
#include "timeout.hh"
#include "menu.hh"
#include "dialog.hh"
#include "bookmark.h"
#include "history.h"
#include "msg.h"
#include "prefs.h"

#include "dw/fltkviewport.hh"

#include "nav.h"

// Handy macro
#define BW2UI(bw) ((UI*)(bw->ui))

// Platform idependent part
using namespace dw::core;
// FLTK related
using namespace dw::fltk;


/*
 * Local data
 */
static char *save_dir = NULL;

using namespace fltk;

//
// For custom handling of keyboard
//
class CustTabGroup : public fltk::TabGroup {
public:
   CustTabGroup (int x, int y, int ww, int wh, const char *lbl=0) :
      TabGroup(x,y,ww,wh,lbl) {};
   int handle(int e) {
      // Don't focus with arrow keys
      printf("CustTabGroup::handle %d\n", e);
      int k = event_key();
      // We're only interested in some flags
      unsigned modifier = event_state() & (SHIFT | CTRL | ALT);
      if (e == KEY) {
         if (k == UpKey || k == DownKey || k == TabKey) {
            return 0;
         } else if (k == LeftKey || k == RightKey) {
            if (modifier == SHIFT) {
               int i = value();
               if (k == LeftKey) {i = i ? i-1 : children()-1;}
               else {i++; if (i >= children()) i = 0;}
               if (value(i)) do_callback();
               return 1;
            }
            return 0;
         }
      }
      return TabGroup::handle(e);
   }
};


/*
 * Create a new UI and its associated BrowserWindow data structure.
 * Use style from v_ui. If non-NULL it must be of type UI*.
 */
BrowserWindow *a_UIcmd_browser_window_new(int ww, int wh, const void *v_ui)
{
   static TabGroup *DilloTabs = NULL;
   BrowserWindow *new_bw = NULL;

   if (ww <= 0 || wh <= 0) {
      // Set default geometry from dillorc.
      ww = prefs.width;
      wh = prefs.height;
   }

   if (!DilloTabs) {
       {Window *o = new Window(ww, wh);
        o->shortcut(0); // Ignore Escape
        o->clear_double_buffer();
        DilloTabs = new CustTabGroup(0, 0, ww, wh);
        DilloTabs->selection_color(156);
        //DilloTabs->clear_tab_to_focus();
        o->add(DilloTabs);
       }
       wh -= 20;
   }

   fltk::Group* o = new fltk::Group(0, 20, ww, wh, "Label1");
   o->clear_tab_to_focus();
   o->clear_click_to_focus();
   DilloTabs->add(o);

   // Create and set the UI
   UI *new_ui = new UI(ww, wh, "", (UI*) v_ui);
   new_ui->set_status("http://www.dillo.org/");
   new_ui->tabs(DilloTabs);
   //new_ui->set_location("http://dillo.org/");
   //new_ui->customize(12);

   o->add(new_ui);
   DilloTabs->resizable(o);
   DilloTabs->window()->resizable(new_ui);
   DilloTabs->window()->show();

   if (v_ui == NULL && prefs.xpos >= 0 && prefs.ypos >= 0) {
      // position the first window according to preferences
      fltk::Rectangle r;
      new_ui->window()->borders(&r);
      // borders() gives x and y border sizes as negative values
      new_ui->window()->position(prefs.xpos - r.x(), prefs.ypos - r.y());
   }

   // Now create the Dw render layout and viewport
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   FltkViewport *viewport = new FltkViewport (0, 0, 1, 1);
   
   layout->attachView (viewport);
   new_ui->set_render_layout(*viewport);

   viewport->setScrollStep((int) rint(14.0 * prefs.font_factor));

   // Now, create a new browser window structure
   new_bw = a_Bw_new(ww, wh, 0);

   // Store new_bw for callback data inside UI
   new_ui->vbw(new_bw);

   // Reference the UI from the bw
   new_bw->ui = (void *)new_ui;
   // Copy the layout pointer into the bw data
   new_bw->render_layout = (void*)layout;

   //new_ui->show();

   return new_bw;
}

/*
 * Close one browser window
 */
void a_UIcmd_close_bw(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;
   UI *ui = BW2UI(bw);
   Layout *layout = (Layout*)bw->render_layout;

   MSG("a_UIcmd_close_bw\n");
   a_Bw_stop_clients(bw, BW_Root + BW_Img + Bw_Force);
   delete(layout);
   if (ui->tabs()) {
      ui->tabs()->remove(ui);
      if (ui->tabs()->value() != -1)
         ui->tabs()->selected_child()->take_focus();
      else
         ui->tabs()->window()->hide();
   }
   delete(ui);
   a_Bw_free(bw);
}

/*
 * Close all the browser windows
 */
void a_UIcmd_close_all_bw()
{
   BrowserWindow *bw;

   while ((bw = a_Bw_get()))
      a_UIcmd_close_bw((void*)bw);
}

/*
 * Open a new URL in the given browser window.
 *
 * our custom "file:" URIs are normalized here too.
 */
void a_UIcmd_open_urlstr(void *vbw, const char *urlstr)
{
   char *new_urlstr;
   DilloUrl *url;
   int ch;
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if (urlstr && *urlstr) {
      /* Filter URL string */
      new_urlstr = a_Url_string_strip_delimiters(urlstr);

      if (!dStrncasecmp(new_urlstr, "file:", 5)) {
         /* file URI */
         ch = new_urlstr[5];
         if (!ch || ch == '.') {
            url = a_Url_new(a_Dir_get_owd(), "file:", 0, 0, 0);
         } else if (ch == '~') {
            url = a_Url_new(dGethomedir(), "file:", 0, 0, 0);
         } else {
            url = a_Url_new(new_urlstr, "file:", 0, 0, 0);
         }

      } else {
         /* common case */
         url = a_Url_new(new_urlstr, NULL, 0, 0, 0);
      }
      dFree(new_urlstr);

      if (url) {
         a_Nav_push(bw, url);
         a_Url_free(url);
      }
   }

   /* let the rendered area have focus */
   //gtk_widget_grab_focus(GTK_BIN(bw->render_main_scroll)->child);
}

/*
 * Open a new URL in the given browser window
 */
void a_UIcmd_open_url(BrowserWindow *bw, const DilloUrl *url)
{
   a_Nav_push(bw, url);
}

/*
 * Open a new URL in the given browser window
 */
void a_UIcmd_open_url_nw(BrowserWindow *bw, const DilloUrl *url)
{
   a_Nav_push_nw(bw, url);
}

/*
 * Send the browser back to previous page
 */
void a_UIcmd_back(void *vbw)
{
   a_Nav_back((BrowserWindow*)vbw);
}

/*
 * Popup the navigation menu of the Back button
 */
void a_UIcmd_back_popup(void *vbw)
{
   a_Menu_history_popup((BrowserWindow*)vbw, -1);
}

/*
 * Send the browser to next page in the history list
 */
void a_UIcmd_forw(void *vbw)
{
   a_Nav_forw((BrowserWindow*)vbw);
}

/*
 * Popup the navigation menu of the Forward button
 */
void a_UIcmd_forw_popup(void *vbw)
{
   a_Menu_history_popup((BrowserWindow*)vbw, 1);
}

/*
 * Send the browser to home URL
 */
void a_UIcmd_home(void *vbw)
{
   a_Nav_home((BrowserWindow*)vbw);
}

/*
 * Reload current URL
 */
void a_UIcmd_reload(void *vbw)
{
   a_Nav_reload((BrowserWindow*)vbw);
}

/*
 * Return a suitable filename for a given URL path.
 */
static char *UIcmd_make_save_filename(const char *pathstr)
{
   size_t MaxLen = 64;
   char *FileName, *name;
   const char *dir = a_UIcmd_get_save_dir();

   if ((name = strrchr(pathstr, '/'))) {
      if (strlen(++name) > MaxLen) {
         name = name + strlen(name) - MaxLen;
      }
      FileName = dStrconcat(dir ? dir : "", name, NULL);
   } else {
      FileName = dStrconcat(dir ? dir : "", pathstr, NULL);
   }
   return FileName;
}

/*
 * Get the default directory for saving files.
 */
const char *a_UIcmd_get_save_dir()
{
   return save_dir;
}

/*
 * Set the default directory for saving files.
 */
void a_UIcmd_set_save_dir(const char *dir)
{
   char *p;

   if (dir && (p = strrchr(dir, '/'))) {
      dFree(save_dir);
      // assert a trailing '/'
      save_dir = dStrconcat(dir, (p[1] != 0) ? "/" : "", NULL);
   }
}

/*
 * Save current URL
 */
void a_UIcmd_save(void *vbw)
{
   const char *name;
   char *SuggestedName, *urlstr;
   DilloUrl *url;

   a_UIcmd_set_save_dir(prefs.save_dir);

   urlstr = a_UIcmd_get_location_text((BrowserWindow*)vbw);
   url = a_Url_new(urlstr, NULL, 0, 0, 0);
   SuggestedName = UIcmd_make_save_filename(URL_PATH(url));
   name = a_Dialog_save_file("Save Page as File", NULL, SuggestedName);
   MSG("a_UIcmd_save: %s\n", name);
   dFree(SuggestedName);
   dFree(urlstr);

   if (name) {
      a_Nav_save_url((BrowserWindow*)vbw, url, name);
   }

   a_Url_free(url);
}

/*
 * Select a file
 */
const char *a_UIcmd_select_file()
{
   return a_Dialog_select_file("Select a File", NULL, NULL);
}

/*
 * Stop network activity on this bw.
 * The stop button was pressed: stop page (and images) downloads.
 */
void a_UIcmd_stop(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow *)vbw;

   MSG("a_UIcmd_stop()\n");
   a_Bw_stop_clients(bw, BW_Root + BW_Img + Bw_Force);
   a_UIcmd_set_buttons_sens(bw);
}

/*
 * Open URL with dialog chooser
 */
void a_UIcmd_open_file(void *vbw)
{
   char *name;
   DilloUrl *url;

   name = a_Dialog_open_file("Open File", NULL, "");

   if (name) {
      url = a_Url_new(name, "file:", 0, 0, 0);
      a_Nav_push((BrowserWindow*)vbw, url);
      a_Url_free(url);
      dFree(name);
   }
}

/*
 * Returns a newly allocated string holding a search url generated from
 * a string of keywords (separarated by blanks) and prefs.search_url.
 * The search string is urlencoded.
 */
static char *UIcmd_make_search_str(const char *str)
{
   char *keys = a_Url_encode_hex_str(str), *c = prefs.search_url;
   Dstr *ds = dStr_sized_new(128);
   char *search_url;

   for (; *c; c++) {
      if (*c == '%')
         switch(*++c) {
         case 's':
            dStr_append(ds, keys); break;;
         case '%':
            dStr_append_c(ds, '%'); break;;
         case 0:
            MSG_WARN("search_url ends with '%%'\n"); c--; break;;
         default:
            MSG_WARN("illegal specifier '%%%c' in search_url\n", *c);
         }
      else
         dStr_append_c(ds, *c);
   }
   dFree(keys);

   search_url = ds->str;
   dStr_free(ds, 0);
   return search_url;
}

/*
 * Get a query from a dialog and open it
 */
void a_UIcmd_search_dialog(void *vbw)
{
   const char *query, *url_str;

   if ((query = a_Dialog_input("Search the Web:"))) {
      url_str = UIcmd_make_search_str(query);
      a_UIcmd_open_urlstr(vbw, url_str);
   }
}

/*
 * Get password for user
 */
const char *a_UIcmd_get_passwd(const char *user)
{
   const char *passwd;
   char *prompt = dStrconcat("Password for user \"", user, "\"", NULL);
   passwd = a_Dialog_passwd(prompt);
   dFree(prompt);
   return passwd;
}

/*
 * Save link URL
 */
void a_UIcmd_save_link(BrowserWindow *bw, const DilloUrl *url)
{
   const char *name;
   char *SuggestedName;

   a_UIcmd_set_save_dir(prefs.save_dir);

   SuggestedName = UIcmd_make_save_filename(URL_STR(url));
   name = a_Dialog_save_file("Save Link as File", NULL, SuggestedName);
   MSG("a_UIcmd_save_link: %s\n", name);
   dFree(SuggestedName);

   if (name) {
      a_Nav_save_url(bw, url, name);
   }
}

/*
 * Request the bookmarks page
 */
void a_UIcmd_book(void *vbw)
{
   DilloUrl *url = a_Url_new("dpi:/bm/", NULL, 0, 0, 0);
   a_Nav_push((BrowserWindow*)vbw, url);
   a_Url_free(url);
}

/*
 * Add a bookmark for a certain URL
 */
void a_UIcmd_add_bookmark(BrowserWindow *bw, const DilloUrl *url)
{
   a_Bookmarks_add(bw, url);
}


/*
 * Popup the page menu
 */
void a_UIcmd_page_popup(void *vbw, const DilloUrl *url,
                        const char *bugs_txt, bool_t unloaded_imgs)
{
   a_Menu_page_popup((BrowserWindow*)vbw, url, bugs_txt, unloaded_imgs);
}

/*
 * Popup the link menu
 */
void a_UIcmd_link_popup(void *vbw, const DilloUrl *url)
{
   a_Menu_link_popup((BrowserWindow*)vbw, url);
}

/*
 * Pop up the image menu
 */
void a_UIcmd_image_popup(void *vbw, const DilloUrl *url, DilloUrl *link_url)
{
   a_Menu_image_popup((BrowserWindow*)vbw, url, link_url);
}

/*
 * Copy url string to paste buffer
 */
void a_UIcmd_copy_urlstr(BrowserWindow *bw, const char *urlstr)
{
   Layout *layout = (Layout*)bw->render_layout;
   layout->copySelection(urlstr);
}

/*
 * Show a text window with the URL's source
 */
void a_UIcmd_view_page_source(const DilloUrl *url)
{
   char *buf;
   int buf_size;

   if (a_Nav_get_buf(url, &buf, &buf_size)) {
      void *vWindow = a_Dialog_make_text_window(buf, "View Page source");
      a_Nav_unref_buf(url);
      a_Dialog_show_text_window(vWindow);
   }
}

/*
 * Show a text window with the URL's source
 */
void a_UIcmd_view_page_bugs(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;

   if (bw->num_page_bugs > 0) {
      void *vWindow = a_Dialog_make_text_window(bw->page_bugs->str,
                                                "Detected HTML errors");
      a_Dialog_show_text_window(vWindow);
   } else {
      a_Dialog_msg("Zero detected HTML errors!");
   }
}

/*
 * Popup the bug meter menu
 */
void a_UIcmd_bugmeter_popup(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;

   a_Menu_bugmeter_popup(bw, a_History_get_url(NAV_TOP_UIDX(bw)));
}

/*
 * Make a list of URL indexes for the history popup
 * based on direction (-1 = back, 1 = forward)
 */
int *a_UIcmd_get_history(BrowserWindow *bw, int direction)
{
   int i, j, n;
   int *hlist;

   // Count the number of URLs
   i = a_Nav_stack_ptr(bw) + direction;
   for (n = 0 ; i >= 0 && i < a_Nav_stack_size(bw); i+=direction)
      ++n;
   hlist = dNew(int, n + 1);

   // Fill the list
   i = a_Nav_stack_ptr(bw) + direction;
   for (j = 0 ; i >= 0 && i < a_Nav_stack_size(bw); i+=direction, j += 1) {
      hlist[j] = NAV_UIDX(bw,i);
   }
   hlist[j] = -1;

   return hlist;
}

/*
 * Jump to a certain URL in the navigation stack.
 */
void a_UIcmd_nav_jump(BrowserWindow *bw, int offset, int new_bw)
{
   a_Nav_jump(bw, offset, new_bw);
}

// UI binding functions -------------------------------------------------------

/*
 * Return browser window width and height
 */
void a_UIcmd_get_wh(BrowserWindow *bw, int *w, int *h)
{
   *w = BW2UI(bw)->w();
   *h = BW2UI(bw)->h();
   _MSG("a_UIcmd_wh: w=%d, h=%d\n", *w, *h);
}

/*
 * Get the scroll position (x, y offset pair)
 */
void a_UIcmd_get_scroll_xy(BrowserWindow *bw, int *x, int *y)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout) {
     *x = layout->getScrollPosX();
     *y = layout->getScrollPosY();
   }
}

/*
 * Set the scroll position ({x, y} offset pair)
 */
void a_UIcmd_set_scroll_xy(BrowserWindow *bw, int x, int y)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout) {
      layout->scrollTo(HPOS_LEFT, VPOS_TOP, x, y, 0, 0);
   }
}

/*
 * Set the scroll position by fragment (from URL)
 */
void a_UIcmd_set_scroll_by_fragment(BrowserWindow *bw, const char *f)
{
   Layout *layout = (Layout*)bw->render_layout;

   if (layout && f) {
      layout->setAnchor(f);
   }
}

/*
 * Get location's text
 */
char *a_UIcmd_get_location_text(BrowserWindow *bw)
{
   return dStrdup(BW2UI(bw)->get_location());
}

/*
 * Set location's text
 */
void a_UIcmd_set_location_text(void *vbw, const char *text)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   BW2UI(bw)->set_location(text);
}

/*
 * Set the page progress bar
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void a_UIcmd_set_page_prog(BrowserWindow *bw, size_t nbytes, int cmd)
{
   BW2UI(bw)->set_page_prog(nbytes, cmd);
}

/*
 * Set the images progress bar
 * cmd: 0 Deactivate, 1 Update, 2 Clear
 */
void a_UIcmd_set_img_prog(BrowserWindow *bw, int n_img, int t_img, int cmd)
{
   BW2UI(bw)->set_img_prog(n_img, t_img, cmd);
#if 0
   if (!cmd)
      a_UIcmd_close_bw(bw);
#endif
}

/*
 * Set the bug meter progress label
 */
void a_UIcmd_set_bug_prog(BrowserWindow *bw, int n_bug)
{
   BW2UI(bw)->set_bug_prog(n_bug);
}

/*
 * Set the page title.
 * now it goes to the window titlebar (maybe to TAB label in the future).
 */
void a_UIcmd_set_page_title(BrowserWindow *bw, const char *label)
{
   BW2UI(bw)->set_page_title(label);
}

/*
 * Set a printf-like status string on the bottom of the dillo window.
 * Beware: The safe way to set an arbitrary string is
 *         a_UIcmd_set_msg(bw, "%s", str)
 */
void a_UIcmd_set_msg(BrowserWindow *bw, const char *format, ...)
{
   va_list argp;
   Dstr *ds = dStr_sized_new(128);
   va_start(argp, format);
   dStr_vsprintf(ds, format, argp);
   va_end(argp);
   BW2UI(bw)->set_status(ds->str);
   dStr_free(ds, 1);
}

/*
 * Set the sensitivity of back/forw/stop buttons.
 */
static void a_UIcmd_set_buttons_sens_cb(void *vbw)
{
   int sens;
   BrowserWindow *bw = (BrowserWindow*)vbw;

   // Stop
   sens = (dList_length(bw->ImageClients) || dList_length(bw->RootClients));
   BW2UI(bw)->button_set_sens(UI_STOP, sens);
   // Back
   sens = (a_Nav_stack_ptr(bw) > 0);
   BW2UI(bw)->button_set_sens(UI_BACK, sens);
   // Forward
   sens = (a_Nav_stack_ptr(bw) < a_Nav_stack_size(bw) - 1 &&
           !bw->nav_expecting);
   BW2UI(bw)->button_set_sens(UI_FORW, sens);
  
   bw->sens_idle_up = 0;
}


/*
 * Set the timeout function for button sensitivity
 */
void a_UIcmd_set_buttons_sens(BrowserWindow *bw)
{
   if (bw->sens_idle_up == 0) {
      a_Timeout_add(0.0, a_UIcmd_set_buttons_sens_cb, bw);
      bw->sens_idle_up = 1;
   }
}

/*
 * Toggle control panel (aka. fullscreen)
 */
void a_UIcmd_fullscreen_toggle(BrowserWindow *bw)
{
   BW2UI(bw)->fullscreen_toggle();
}

/*
 * Search for next occurrence of key.
 */
void a_UIcmd_findtext_search(BrowserWindow *bw, const char *key, int case_sens)
{
   Layout *l = (Layout *)bw->render_layout;
   
   switch (l->search(key, case_sens)) {
      case FindtextState::RESTART:
         a_UIcmd_set_msg(bw, "No further occurrences of \"%s\". "
                             "Restarting from the top.", key);
         break;
      case FindtextState::NOT_FOUND:
         a_UIcmd_set_msg(bw, "\"%s\" not found.", key);
         break;
      case FindtextState::SUCCESS:
      default:
         a_UIcmd_set_msg(bw, "");
   }
}

/*
 * Reset text search state.
 */
void a_UIcmd_findtext_reset(BrowserWindow *bw)
{
   Layout *l = (Layout *)bw->render_layout;
   l->resetSearch();

   a_UIcmd_set_msg(bw, "");
}

/*
 * Focus the rendered area.
 */
void a_UIcmd_focus_main_area(BrowserWindow *bw)
{
   BW2UI(bw)->focus_main();
}

