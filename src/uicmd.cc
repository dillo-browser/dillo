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

#include <fltk/draw.h>
#include <fltk/damage.h>
#include <fltk/Widget.h>
#include <fltk/TabGroup.h>
#include <fltk/Tooltip.h>

#include "paths.hh"
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

#define DEFAULT_TAB_LABEL "Dillo"

// Handy macro
#define BW2UI(bw) ((UI*)((bw)->ui))

// Platform idependent part
using namespace dw::core;
// FLTK related
using namespace dw::fltk;


/*
 * Local data
 */
static char *save_dir = NULL;

using namespace fltk;


//----------------------------------------------------------------------------
#define BTN_W 25
#define BTN_H 20

static int btn_x;

/*
 * Adds a tab-close button at the rightmost part
 */
class CustShrinkTabPager : public TabGroupPager {
   bool btn_hl;
   TabGroup *tg;
public:
   int update_positions(
          TabGroup *g, int numchildren, int &selected,
          int &cumulated_width, int &available_width,
          int *tab_pos, int *tab_width);
   virtual int which(TabGroup* g, int m_x,int m_y);
   virtual TabGroupPager* clone() const;
   virtual const char * mode_name() const {return "Shrink";}
   virtual int id() const {return PAGER_SHRINK;}
   virtual int available_width(TabGroup *g) const;
   virtual bool draw_tabs(TabGroup* g, int selected, int* tab_pos,
                          int* tab_width) {
      if (!tg) tg = g;
      if (g->children() > 1) {
         fltk::Rectangle r(btn_x,0,BTN_W,BTN_H);
         setcolor(btn_hl ? 206 : GRAY75);
         fillrect(r);
         if (btn_hl) {
            setcolor(WHITE);
            strokerect(r);
         }
         setcolor(GRAY10);
         //fltk::setfont(fltk::getfont()->bold(), fltk::getsize());
         r.h(r.h()-2);
         drawtext("X", r, ALIGN_CENTER);
         return false;
      } else {
         // WORKAROUND: for http://fltk.org/str.php?L2062
         // By returning true we avoid a call to TabGroup::draw_tab()
         // in TabGroup::draw() in case we don't show the tabs.
         return true;
      }
   }

   void btn_highlight(bool flag) {
      if (btn_hl != flag) {
         btn_hl = flag;
         if (tg)
            tg->redraw(DAMAGE_VALUE);
      }
   };
   bool btn_highlight() { return btn_hl; };

   CustShrinkTabPager() : TabGroupPager() {
      noclip(true);
      btn_hl = false;
      tg = NULL;
   }
};

int CustShrinkTabPager::available_width(TabGroup *g) const
{
   _MSG("CustShrinkTabPager::available_width\n");
   int w = MAX (g->w() - this->slope()-1 - BTN_W, 0);
   btn_x = w + 6;
   return w;
}

int CustShrinkTabPager::which(TabGroup* g, int event_x,int event_y)
{
   int H = g->tab_height();
   if (!H) return -1;
   if (H < 0) {
      if (event_y > g->h() || event_y < g->h()+H) return -1;
   } else {
      if (event_y > H || event_y < 0) return -1;
   }
   if (event_x < 0) return -1;
   int p[128], w[128];
   int selected = g->tab_positions(p, w);
   int d = (event_y-(H>=0?0:g->h()))*slope()/H;
   for (int i=0; i<g->children(); i++) {
      if (event_x < p[i+1]+(i<selected ? slope() - d : d)) return i;
   }
   return -1;
}

/*
 * Prevents tabs from going over the close-tab button.
 * Modified from fltk-2.0.x-r6525.
 */
int CustShrinkTabPager::update_positions(
       TabGroup *g, int numchildren, int &selected,
       int &cumulated_width, int &available_width,
       int *tab_pos, int *tab_width)
{
   available_width-=BTN_W;

   // uh oh, they are too big, we must move them:
   // special case when the selected tab itself is too big, make it fill
   // cumulated_width:
   int i;

   if (tab_width[selected] >= available_width) {
      tab_width[selected] = available_width;
      for (i = 0; i <= selected; i++)
         tab_pos[i] = 0;
      for (i = selected + 1; i <= numchildren; i++)
         tab_pos[i] = available_width;
      return selected;
   }

   int w2[128];

   for (i = 0; i < numchildren; i++)
      w2[i] = tab_width[i];
   i = numchildren - 1;
   int j = 0;

   int minsize = 5;

   bool right = true;

   while (cumulated_width > available_width) {
      int n;                    // which one to shrink

      if (j < selected && (!right || i <= selected)) {  // shrink a left one
         n = j++;
         right = true;
      } else if (i > selected) {        // shrink a right one
         n = i--;
         right = false;
      } else {                  // no more space, start making them zero
         minsize = 0;
         i = numchildren - 1;
         j = 0;
         right = true;
         continue;
      }
      cumulated_width -= w2[n] - minsize;
      w2[n] = minsize;
      if (cumulated_width < available_width) {
         w2[n] = available_width - cumulated_width + minsize;
         cumulated_width = available_width;
         break;
      }
   }
   // re-sum the positions:
   cumulated_width = 0;
   for (i = 0; i < numchildren; i++) {
      cumulated_width += w2[i];
      tab_pos[i+1] = cumulated_width;
   }
   return selected;
}

TabGroupPager* CustShrinkTabPager::clone() const {
   return new CustShrinkTabPager(*this);
}

//----------------------------------------------------------------------------

/*
 * For custom handling of keyboard
 */
class CustTabGroup : public fltk::TabGroup {
  Tooltip *toolTip;
  bool tooltipEnabled;
  bool buttonPushed;
public:
   CustTabGroup (int x, int y, int ww, int wh, const char *lbl=0) :
      TabGroup(x,y,ww,wh,lbl) {
         // The parameter pager is cloned, so free it.
         CustShrinkTabPager *cp = new CustShrinkTabPager();
         this->pager(cp);
         delete cp;
         toolTip = new Tooltip;
         tooltipEnabled = false;
         buttonPushed = false;
      };
      ~CustTabGroup() { delete toolTip; }
   int handle(int e) {
      // Don't focus with arrow keys
      _MSG("CustTabGroup::handle %d\n", e);
      fltk::Rectangle r(btn_x,0,BTN_W,BTN_H);
      if (e == KEY) {
         int k = event_key();
         // We're only interested in some flags
         unsigned modifier = event_state() & (SHIFT | CTRL | ALT);
         if (k == UpKey || k == DownKey || k == TabKey) {
            return 0;
         } else if (k == LeftKey || k == RightKey) {
            if (modifier == SHIFT) {
               int i = value();
               if (k == LeftKey) {i = i ? i-1 : children()-1;}
               else {i++; if (i >= children()) i = 0;}
               selected_child(child(i));
               return 1;
            }
            // Avoid focus change.
            return 0;
         }
      } else if (e == MOVE) {
         CustShrinkTabPager *cstp = (CustShrinkTabPager *) pager();
         if (event_inside(r) && children() > 1) {
            /* We're inside the button area */
            cstp->btn_highlight(true);
            /* Prepare the tooltip for pop-up */
            tooltipEnabled = true;
            /* We use parent() if available because we are returning 0.
             * Returning without having TabGroup processing makes the
             * popup event never reach 'this', but it reaches parent() */
            toolTip->enter(parent() ? parent() : this, r, "Close current Tab");

            return 0;              // Change focus
         } else {
            cstp->btn_highlight(false);

            /* Hide the tooltip or enable it again.*/
            if (tooltipEnabled) {
               tooltipEnabled = false;
               toolTip->exit();
            } else {
               toolTip->enable();
            }
         }
      } else if (e == PUSH && event_inside(r) &&
                 event_button() == 1 && children() > 1) {
         buttonPushed = true;
         return 1;                 /* non-zero */
      } else if (e == RELEASE) {
         if (event_inside(r) && event_button() == 1 &&
             children() > 1 && buttonPushed) {
            a_UIcmd_close_bw(a_UIcmd_get_bw_by_widget(selected_child()));
         } else {
            CustShrinkTabPager *cstp = (CustShrinkTabPager *) pager();
            cstp->btn_highlight(false);
         }
         buttonPushed = false;
      } else if (e == DRAG) {
         /* Ignore this event */
         return 1;
      }
      return TabGroup::handle(e);
   }

   void remove (Widget *w) {
      TabGroup::remove (w);
      /* fixup resizable in case we just removed it */
      if (resizable () == w) {
         if (children () > 0)
            resizable (child (children () - 1));
         else
            resizable (NULL);
      }

      if (children () < 2)
         hideLabels ();
   }

   void add (Widget *w) {
      TabGroup::add (w);
      if (children () > 1)
         showLabels ();
   }

   void hideLabels() {
      for (int i = children () - 1; i >= 0; i--)
         child(i)->resize(x(), y(), w(), h());
   }

   void showLabels() {
      for (int i = children () - 1; i >= 0; i--)
         child(i)->resize(x(), y() + 20, w(), h() - 20);
   }
};

//----------------------------------------------------------------------------

static void win_cb (fltk::Widget *w, void *cb_data) {
   CustTabGroup *tabs;
   int choice = 0;

   tabs = BW2UI((BrowserWindow*) cb_data)->tabs();
   if (tabs->children () > 1)
      choice = a_Dialog_choice3 ("Window contains more than one tab.",
                                 "Close all tabs", "Cancel", NULL);
   if (choice == 0)
      while (tabs->children())
         a_UIcmd_close_bw(a_UIcmd_get_bw_by_widget(tabs->child(0)));
}

/*
 * Given a UI or UI child widget, return its bw.
 */
BrowserWindow *a_UIcmd_get_bw_by_widget(void *v_wid)
{
   BrowserWindow *bw;
   for (int i = 0; i < a_Bw_num(); ++i) {
      bw = a_Bw_get(i);
      if (((fltk::Widget*)bw->ui)->contains((fltk::Widget*)v_wid))
         return bw;
   }
   return NULL;
}

/*
 * FLTK regards SHIFT + {LeftKey, Right} as navigation keys.
 * Special handling is required to override it. Here we route
 * these events directly to the recipient.
 * TODO: focus is not remembered correctly.
 */
void a_UIcmd_send_event_to_tabs_by_wid(int e, void *v_wid)
{
   BrowserWindow *bw = a_UIcmd_get_bw_by_widget(v_wid);
   UI *ui = (UI*)bw->ui;
   if (ui->tabs())
      ui->tabs()->handle(e);
}

/*
 * Create a new UI and its associated BrowserWindow data structure.
 * Use style from v_ui. If non-NULL it must be of type UI*.
 */
BrowserWindow *a_UIcmd_browser_window_new(int ww, int wh, const void *vbw)
{
   BrowserWindow *old_bw = (BrowserWindow*)vbw;
   BrowserWindow *new_bw = NULL;

   if (ww <= 0 || wh <= 0) {
      // Set default geometry from dillorc.
      ww = prefs.width;
      wh = prefs.height;
   }

   Window *win = new Window(ww, wh);
   win->shortcut(0); // Ignore Escape
   if (prefs.buffered_drawing != 2)
      win->clear_double_buffer();
   win->set_flag(RAW_LABEL);
   CustTabGroup *DilloTabs = new CustTabGroup(0, 0, ww, wh);
   DilloTabs->clear_tab_to_focus();
   DilloTabs->selection_color(156);
   win->add(DilloTabs);

   // Create and set the UI
   UI *new_ui = new UI(0, 0, ww, wh, DEFAULT_TAB_LABEL,
                       old_bw ? BW2UI(old_bw) : NULL);
   new_ui->set_status("http://www.dillo.org/");
   new_ui->tabs(DilloTabs);

   DilloTabs->add(new_ui);
   DilloTabs->resizable(new_ui);
   DilloTabs->window()->resizable(new_ui);
   DilloTabs->window()->show();

   if (old_bw == NULL && prefs.xpos >= 0 && prefs.ypos >= 0) {
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
   if (prefs.buffered_drawing == 1)
      viewport->setBufferedDrawing (true);
   else
      viewport->setBufferedDrawing (false);

   layout->attachView (viewport);
   new_ui->set_render_layout(*viewport);

   viewport->setScrollStep((int) rint(14.0 * prefs.font_factor));

   // Now, create a new browser window structure
   new_bw = a_Bw_new();

   // Reference the UI from the bw
   new_bw->ui = (void *)new_ui;
   // Copy the layout pointer into the bw data
   new_bw->render_layout = (void*)layout;

   win->callback(win_cb, new_bw);

   return new_bw;
}

/*
 * Create a new Tab.
 * i.e the new UI and its associated BrowserWindow data structure.
 */
static BrowserWindow *UIcmd_tab_new(const void *vbw)
{
   _MSG(" UIcmd_tab_new vbw=%p\n", vbw);

   dReturn_val_if_fail (vbw != NULL, NULL);

   BrowserWindow *new_bw = NULL;
   BrowserWindow *old_bw = (BrowserWindow*)vbw;
   UI *ui = BW2UI(old_bw);

   // WORKAROUND: limit the number of tabs because of a fltk bug
   if (ui->tabs()->children() >= 127)
      return a_UIcmd_browser_window_new(ui->window()->w(), ui->window()->h(),
                                        vbw);

   // Create and set the UI
   UI *new_ui = new UI(0, 0, ui->w(), ui->h(), DEFAULT_TAB_LABEL, ui);
   new_ui->tabs(ui->tabs());

   new_ui->tabs()->add(new_ui);
   new_ui->tabs()->resizable(new_ui);
   new_ui->tabs()->window()->resizable(new_ui);
   new_ui->tabs()->window()->show();

   // Now create the Dw render layout and viewport
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   FltkViewport *viewport = new FltkViewport (0, 0, 1, 1);

   layout->attachView (viewport);
   new_ui->set_render_layout(*viewport);

   viewport->setScrollStep((int) rint(14.0 * prefs.font_factor));

   // Now, create a new browser window structure
   new_bw = a_Bw_new();

   // Reference the UI from the bw
   new_bw->ui = (void *)new_ui;
   // Copy the layout pointer into the bw data
   new_bw->render_layout = (void*)layout;

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
      ui->tabs()->value(ui->tabs()->children() - 1);
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
void a_UIcmd_close_all_bw(void *)
{
   BrowserWindow *bw;
   int choice = 0;

   if (a_Bw_num() > 1)
      choice = a_Dialog_choice3 ("More than one open tab or Window.",
         "Close all tabs and windows", "Cancel", NULL);
   if (choice == 0)
      while ((bw = a_Bw_get(0)))
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
            url = a_Url_new(Paths::getOldWorkingDir(), "file:");
         } else if (ch == '~') {
            url = a_Url_new(dGethomedir(), "file:");
         } else {
            url = a_Url_new(new_urlstr, "file:");
         }

      } else {
         /* common case */
         url = a_Url_new(new_urlstr, NULL);
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
 * Open a new URL in a new tab in the same browser window
 */
void a_UIcmd_open_url_nt(void *vbw, const DilloUrl *url, int focus)
{
   BrowserWindow *new_bw = UIcmd_tab_new(vbw);
   if (url)
      a_Nav_push(new_bw, url);
   if (focus) {
      BW2UI(new_bw)->tabs()->selected_child(BW2UI(new_bw));
      BW2UI(new_bw)->tabs()->selected_child()->take_focus();
   }
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
 * Repush current URL
 */
void a_UIcmd_repush(void *vbw)
{
   a_Nav_repush((BrowserWindow*)vbw);
}

/*
 * Zero-delay URL redirection.
 */
void a_UIcmd_redirection0(void *vbw, const DilloUrl *url)
{
   a_Nav_redirection0((BrowserWindow*)vbw, url);
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
   char *SuggestedName;
   BrowserWindow *bw = (BrowserWindow *)vbw;
   const DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));

   if (url) {
      a_UIcmd_set_save_dir(prefs.save_dir);
      SuggestedName = UIcmd_make_save_filename(URL_PATH(url));
      name = a_Dialog_save_file("Save Page as File", NULL, SuggestedName);
      MSG("a_UIcmd_save: %s\n", name);
      dFree(SuggestedName);

      if (name) {
         a_Nav_save_url(bw, url, name);
      }
   }
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
   a_Nav_cancel_expect(bw);
   a_Bw_stop_clients(bw, BW_Root + BW_Img + Bw_Force);
   a_UIcmd_set_buttons_sens(bw);
}

/*
 * Popup the tools menu
 */
void a_UIcmd_tools(void *vbw, void *v_wid)
{
   a_Menu_tools_popup((BrowserWindow*)vbw, v_wid);
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
      url = a_Url_new(name, "file:");
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
   DilloUrl *url = a_Url_new("dpi:/bm/", NULL);
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
void a_UIcmd_page_popup(void *vbw, bool_t has_bugs, void *v_cssUrls)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   DilloUrl *url = a_History_get_url(NAV_TOP_UIDX(bw));
   a_Menu_page_popup(bw, url, has_bugs, v_cssUrls);
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
void a_UIcmd_image_popup(void *vbw, const DilloUrl *url, bool_t loaded_img,
                         DilloUrl *page_url, DilloUrl *link_url)
{
   a_Menu_image_popup((BrowserWindow*)vbw, url, loaded_img, page_url,link_url);
}

/*
 * Pop up the form menu
 */
void a_UIcmd_form_popup(void *vbw, const DilloUrl *url, void *vform,
                        bool_t showing_hiddens)
{
   a_Menu_form_popup((BrowserWindow*)vbw, url, vform, showing_hiddens);
}

/*
 * Pop up the file menu
 */
void a_UIcmd_file_popup(void *vbw, void *v_wid)
{
   a_Menu_file_popup((BrowserWindow*)vbw, v_wid);
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
void a_UIcmd_set_buttons_sens(BrowserWindow *bw)
{
   int sens;

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
}

/*
 * Keep track of mouse pointer over a link.
 */
void a_UIcmd_set_pointer_on_link(BrowserWindow *bw, int flag)
{
   BW2UI(bw)->pointerOnLink(flag);
}

/*
 * Is the mouse pointer over a link?
 */
int a_UIcmd_pointer_on_link(BrowserWindow *bw)
{
   return BW2UI(bw)->pointerOnLink();
}

/*
 * Toggle control panel (aka. fullscreen)
 */
void a_UIcmd_fullscreen_toggle(BrowserWindow *bw)
{
   BW2UI(bw)->fullscreen_toggle();
}

/*
 * Search for next/previous occurrence of key.
 */
void a_UIcmd_findtext_search(BrowserWindow *bw, const char *key,
                             int case_sens, int backwards)
{
   Layout *l = (Layout *)bw->render_layout;

   switch (l->search(key, case_sens, backwards)) {
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

/*
 * Focus the location bar.
 */
void a_UIcmd_focus_location(void *vbw)
{
   BrowserWindow *bw = (BrowserWindow*)vbw;
   BW2UI(bw)->focus_location();
}

