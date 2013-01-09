/*
 * File: downloads.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * A FLTK-based GUI for the downloads dpi (dillo plugin).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

#include "dpiutil.h"
#include "../dpip/dpip.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[downloads dpi]: " __VA_ARGS__)

/*
 * Class declarations
 */

// ProgressBar widget --------------------------------------------------------

class ProgressBar : public Fl_Box {
protected:
   double mMin;
   double mMax;
   double mPresent;
   double mStep;
   bool mShowPct, mShowMsg;
   char mMsg[64];
   Fl_Color mTextColor;
   void draw();
public:
   ProgressBar(int x, int y, int w, int h, const char *lbl = 0);
   void range(double min, double max, double step = 1)  {
      mMin = min; mMax = max; mStep = step;
   };
   void step(double step)        { mPresent += step; redraw(); };
   void move(double step);
   double minimum()        { return mMin; }
   double maximum()        { return mMax; }
   void minimum(double nm) { mMin = nm; };
   void maximum(double nm) { mMax = nm; };
   double position  ()     { return mPresent; }
   double step()           { return mStep; }
   void position(double pos)     { mPresent = pos; redraw(); }
   void showtext(bool st)        { mShowPct = st; }
   void message(char *msg) { mShowMsg = true; strncpy(mMsg,msg,63); redraw(); }
   bool showtext()               { return mShowPct; }
   void text_color(Fl_Color col) { mTextColor = col; }
   Fl_Color text_color()   { return mTextColor; }
};

// Download-item class -------------------------------------------------------

class DLItem {
   enum {
      ST_newline, ST_number, ST_discard, ST_copy
   };

   pid_t mPid;
   int LogPipe[2];
   char *shortname, *fullname;
   char *target_dir;
   size_t log_len, log_max;
   int log_state;
   char *log_text;
   time_t init_time;
   char **dl_argv;
   time_t twosec_time, onesec_time;
   int twosec_bytesize, onesec_bytesize;
   int init_bytesize, curr_bytesize, total_bytesize;
   int DataDone, LogDone, ForkDone, UpdatesDone, WidgetDone;
   int WgetStatus;

   int gw, gh;
   Fl_Group *group;
   ProgressBar *prBar;
   Fl_Button *prButton;
   Fl_Widget *prTitle, *prGot, *prSize, *prRate, *pr_Rate, *prETA, *prETAt;

public:
   DLItem(const char *full_filename, const char *url);
   ~DLItem();
   void child_init();
   void father_init();
   void update_size(int new_sz);
   void log_text_add(const char *buf, ssize_t st);
   void log_text_show();
   void abort_dl();
   void prButton_cb();
   pid_t pid() { return mPid; }
   void pid(pid_t p) { mPid = p; }
   void child_finished(int status);
   void status_msg(const char *msg) { prBar->message((char*)msg); }
   Fl_Widget *get_widget() { return group; }
   int widget_done() { return WidgetDone; }
   void widget_done(int val) { WidgetDone = val; }
   int updates_done() { return UpdatesDone; }
   void updates_done(int val) { UpdatesDone = val; }
   int fork_done() { return ForkDone; }
   void fork_done(int val) { ForkDone = val; }
   int log_done() { return LogDone; }
   void log_done(int val) { LogDone = val; }
   int wget_status() { return WgetStatus; }
   void wget_status(int val) { WgetStatus = val; }
   void update_prSize(int newsize);
   void update();
};

// DLItem List ---------------------------------------------------------------

/// BUG: make dynamic
class DLItemList {
   DLItem *mList[32];
   int mNum, mMax;

public:
   DLItemList() { mNum = 0; mMax = 32; }
   ~DLItemList() { }
   int num() { return mNum; }
   void add(DLItem *i) { if (mNum < mMax) mList[mNum++] = i; }
   DLItem *get(int n) { return (n >= 0 && n < mNum) ? mList[n] : NULL; }
   void del(int n) { if (n >= 0 && n < mNum) mList[n] = mList[--mNum]; }
};

// DLWin ---------------------------------------------------------------------

class DLWin {
   DLItemList *mDList;
   Fl_Window *mWin;
   Fl_Scroll *mScroll;
   Fl_Pack *mPG;

public:
   DLWin(int ww, int wh);
   void add(const char *full_filename, const char *url);
   void del(int n_item);
   int num();
   int num_running();
   void listen(int req_fd);
   void show() { mWin->show(); }
   void hide() { mWin->hide(); }
   void abort_all();
};

/*
 * FLTK cannot be dissuaded from interpreting '@' in a tooltip
 * as indicating a symbol unless we escape it.
 */
static char *escape_tooltip(const char *buf, ssize_t len)
{
   if (len < 0)
      len = 0;

   char *ret = (char *) malloc(2 * len + 1);
   char *dest = ret;

   while (len-- > 0) {
      if (*buf == '@')
         *dest++ = *buf;
      *dest++ = *buf++;
   }
   *dest = '\0';

   return ret;
}


/*
 * Global variables
 */

// SIGCHLD mask
sigset_t mask_sigchld;

// SIGCHLD flag
volatile sig_atomic_t caught_sigchld = 0;

// The download window object
static class DLWin *dl_win = NULL;



// ProgressBar widget --------------------------------------------------------

void ProgressBar::move(double step)
{
   mPresent += step;
   if (mPresent > mMax)
      mPresent = mMin;
   redraw();
}

ProgressBar::ProgressBar(int x, int y, int w, int h, const char *lbl)
:  Fl_Box(x, y, w, h, lbl)
{
   mMin = mPresent = 0;
   mMax = 100;
   mShowPct = true;
   mShowMsg = false;
   box(FL_THIN_UP_BOX);
   color(FL_WHITE);
}

void ProgressBar::draw()
{
   struct Rectangle {
      int x, y, w, h;
   };

   //drawstyle(style(), flags());
   draw_box();
   Rectangle r = {x(), y(), w(), h()};
   if (mPresent > mMax)
      mPresent = mMax;
   if (mPresent < mMin)
      mPresent = mMin;
   double pct = (mPresent - mMin) / mMax;

   r.w = r.w * pct + .5;
   fl_rectf(r.x, r.y, r.w, r.h, FL_BLUE);

   if (mShowMsg) {
      fl_color(FL_RED);
      fl_font(this->labelfont(), this->labelsize());
      fl_draw(mMsg, x(), y(), w(), h(), FL_ALIGN_CENTER);
   } else if (mShowPct) {
      char buffer[30];
      sprintf(buffer, "%d%%", int (pct * 100 + .5));
      fl_color(FL_RED);
      fl_font(this->labelfont(), this->labelsize());
      fl_draw(buffer, x(), y(), w(), h(), FL_ALIGN_CENTER);
   }
}


// Download-item class -------------------------------------------------------

static void prButton_scb(Fl_Widget *, void *cb_data)
{
   DLItem *i = (DLItem *)cb_data;

   i->prButton_cb();
}

DLItem::DLItem(const char *full_filename, const char *url)
{
   struct stat ss;
   const char *p;
   char *esc_url;

   if (pipe(LogPipe) < 0) {
      MSG("pipe, %s\n", dStrerror(errno));
      return;
   }
   /* Set FD to background */
   fcntl(LogPipe[0], F_SETFL,
         O_NONBLOCK | fcntl(LogPipe[0], F_GETFL));

   fullname = dStrdup(full_filename);
   p = strrchr(fullname, '/');
   shortname = (p) ? dStrdup(p + 1) : dStrdup("??");
   p = strrchr(full_filename, '/');
   target_dir= p ? dStrndup(full_filename,p-full_filename+1) : dStrdup("??");

   log_len = 0;
   log_max = 0;
   log_state = ST_newline;
   log_text = NULL;
   onesec_bytesize = twosec_bytesize = curr_bytesize = init_bytesize = 0;
   total_bytesize = -1;

   // Init value. Reset later, upon the first data bytes arrival
   init_time = time(NULL);

   twosec_time = onesec_time = init_time;

   // BUG:? test a URL with ' inside.
   /* escape "'" character for the shell. Is it necessary? */
   esc_url = Escape_uri_str(url, "'");
   /* avoid malicious SMTP relaying with FTP urls */
   if (dStrnAsciiCasecmp(esc_url, "ftp:/", 5) == 0)
      Filter_smtp_hack(esc_url);
   dl_argv = new char*[8];
   int i = 0;
   dl_argv[i++] = (char*)"wget";
   if (stat(fullname, &ss) == 0)
      init_bytesize = (int)ss.st_size;
   dl_argv[i++] = (char*)"-c";
   dl_argv[i++] = (char*)"--load-cookies";
   dl_argv[i++] = dStrconcat(dGethomedir(), "/.dillo/cookies.txt", NULL);
   dl_argv[i++] = (char*)"-O";
   dl_argv[i++] = fullname;
   dl_argv[i++] = esc_url;
   dl_argv[i++] = NULL;

   DataDone = 0;
   LogDone = 0;
   UpdatesDone = 0;
   ForkDone = 0;
   WidgetDone = 0;
   WgetStatus = -1;

   gw = 400, gh = 70;
   group = new Fl_Group(0,0,gw,gh);
   group->begin();
   prTitle = new Fl_Box(24, 7, 290, 23);
   prTitle->box(FL_RSHADOW_BOX);
   prTitle->color(FL_WHITE);
   prTitle->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CLIP);
   prTitle->copy_label(shortname);
   // Attach this 'log_text' to the tooltip
   log_text_add("Target File: ", 13);
   log_text_add(fullname, strlen(fullname));
   log_text_add("\n\n", 2);

   prBar = new ProgressBar(24, 40, 92, 20);
   prBar->box(FL_THIN_UP_BOX);
   prBar->tooltip("Progress Status");

   int ix = 122, iy = 37, iw = 50, ih = 14;
   Fl_Widget *o = new Fl_Box(ix,iy,iw,ih, "Got");
   o->box(FL_RFLAT_BOX);
   o->color(FL_DARK2);
   o->labelsize(12);
   o->tooltip("Downloaded Size");
   prGot = new Fl_Box(ix,iy+14,iw,ih, "0KB");
   prGot->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
   prGot->labelcolor(FL_BLUE);
   prGot->labelsize(12);
   prGot->box(FL_NO_BOX);

   ix += iw;
   o = new Fl_Box(ix,iy,iw,ih, "Size");
   o->box(FL_RFLAT_BOX);
   o->color(FL_DARK2);
   o->labelsize(12);
   o->tooltip("Total Size");
   prSize = new Fl_Box(ix,iy+14,iw,ih, "??");
   prSize->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
   prSize->labelsize(12);
   prSize->box(FL_NO_BOX);

   ix += iw;
   o = new Fl_Box(ix,iy,iw,ih, "Rate");
   o->box(FL_RFLAT_BOX);
   o->color(FL_DARK2);
   o->labelsize(12);
   o->tooltip("Current transfer Rate (KBytes/sec)");
   prRate = new Fl_Box(ix,iy+14,iw,ih, "??");
   prRate->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
   prRate->labelsize(12);
   prRate->box(FL_NO_BOX);

   ix += iw;
   o = new Fl_Box(ix,iy,iw,ih, "~Rate");
   o->box(FL_RFLAT_BOX);
   o->color(FL_DARK2);
   o->labelsize(12);
   o->tooltip("Average transfer Rate (KBytes/sec)");
   pr_Rate = new Fl_Box(ix,iy+14,iw,ih, "??");
   pr_Rate->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
   pr_Rate->labelsize(12);
   pr_Rate->box(FL_NO_BOX);

   ix += iw;
   prETAt = o = new Fl_Box(ix,iy,iw,ih, "ETA");
   o->box(FL_RFLAT_BOX);
   o->color(FL_DARK2);
   o->labelsize(12);
   o->tooltip("Estimated Time of Arrival");
   prETA = new Fl_Box(ix,iy+14,iw,ih, "??");
   prETA->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
   prETA->labelsize(12);
   prETA->box(FL_NO_BOX);

   prButton = new Fl_Button(326, 9, 44, 19, "Stop");
   prButton->tooltip("Stop this transfer");
   prButton->box(FL_UP_BOX);
   prButton->clear_visible_focus();
   prButton->callback(prButton_scb, this);

   group->box(FL_ROUNDED_BOX);
   group->end();
}

DLItem::~DLItem()
{
   free(shortname);
   dFree(fullname);
   dFree(target_dir);
   free(log_text);
   int idx = (strcmp(dl_argv[1], "-c")) ? 2 : 3;
   dFree(dl_argv[idx]);
   dFree(dl_argv[idx+3]);
   delete [] dl_argv;

   delete(group);
}

/*
 * Abort a running download
 */
void DLItem::abort_dl()
{
   if (!log_done()) {
      dClose(LogPipe[0]);
      Fl::remove_fd(LogPipe[0]);
      log_done(1);
      // Stop wget
      if (!fork_done())
         kill(pid(), SIGTERM);
   }
   widget_done(1);
}

void DLItem::prButton_cb()
{
   prButton->deactivate();
   abort_dl();
}

void DLItem::child_init()
{
   dClose(0); // stdin
   dClose(1); // stdout
   dClose(LogPipe[0]);
   dup2(LogPipe[1], 2); // stderr
   // set the locale to C for log parsing
   setenv("LC_ALL", "C", 1);
   // start wget
   execvp(dl_argv[0], dl_argv);
}

/*
 * Update displayed size
 */
void DLItem::update_prSize(int newsize)
{
   char num[64];

   if (newsize > 1024 * 1024)
      snprintf(num, 64, "%.1fMB", (float)newsize / (1024*1024));
   else
      snprintf(num, 64, "%.0fKB", (float)newsize / 1024);
   prSize->copy_label(num);
}

void DLItem::log_text_add(const char *buf, ssize_t st)
{
   const char *p;
   char *esc_str, *q, *d, num[64];
   size_t esc_len;

   // WORKAROUND: We have to escape '@' in FLTK tooltips.
   esc_str = escape_tooltip(buf, st);
   esc_len = strlen(esc_str);

   // Make room...
   if (log_len + esc_len >= log_max) {
      log_max = log_len + esc_len + 1024;
      log_text = (char *) dRealloc (log_text, log_max);
      log_text[log_len] = 0;
      prTitle->tooltip(log_text);
   }

   // FSM to remove wget's "dot-progress" (i.e. "^ " || "^[0-9]+K")
   q = log_text + log_len;
   for (p = esc_str; (size_t)(p - esc_str) < esc_len; ++p) {
      switch (log_state) {
      case ST_newline:
         if (*p == ' ') {
            log_state = ST_discard;
         } else if (isdigit(*p)) {
            *q++ = *p; log_state = ST_number;
         } else if (*p == '\n') {
            *q++ = *p;
         } else {
            *q++ = *p; log_state = ST_copy;
         }
         break;
      case ST_number:
         if (isdigit(*q++ = *p)) {
            // keep here
         } else if (*p == 'K') {
            for (--q; isdigit(q[-1]); --q) ; log_state = ST_discard;
         } else {
            log_state = ST_copy;
         }
         break;
      case ST_discard:
         if (*p == '\n')
            log_state = ST_newline;
         break;
      case ST_copy:
         if ((*q++ = *p) == '\n')
            log_state = ST_newline;
         break;
      }
   }
   *q = 0;
   log_len = strlen(log_text);

   free(esc_str);

   // Now scan for the length of the file
   if (total_bytesize == -1) {
      p = strstr(log_text, "\nLength: ");
      if (p && isdigit(p[9]) && strchr(p + 9, ' ')) {
         for (p += 9, d = &num[0]; *p != ' '; ++p)
            if (isdigit(*p))
               *d++ = *p;
         *d = 0;
         total_bytesize = strtol (num, NULL, 10);
         // Update displayed size
         update_prSize(total_bytesize);

         // WORKAROUND: For unknown reasons a redraw is needed here for some
         //             machines --jcid
         group->redraw();
      }
   }

   // Show we're connecting...
   if (curr_bytesize == 0) {
      prTitle->copy_label("Connecting...");
   }
}

///
void DLItem::log_text_show()
{
   MSG("\nStored Log:\n%s", log_text);
}

void DLItem::update_size(int new_sz)
{
   char buf[64];

   if (curr_bytesize == 0 && new_sz) {
      // Start the timer with the first bytes got
      init_time = time(NULL);
      // Update the title
      prTitle->copy_label(shortname);
      // WORKAROUND: For unknown reasons a redraw is needed here for some
      //             machines --jcid
      group->redraw();
   }

   curr_bytesize = new_sz;
   if (curr_bytesize > 1024 * 1024)
      snprintf(buf, 64, "%.1fMB", (float)curr_bytesize / (1024*1024));
   else
      snprintf(buf, 64, "%.0fKB", (float)curr_bytesize / 1024);
   prGot->copy_label(buf);
   if (total_bytesize == -1) {
      prBar->showtext(false);
      prBar->move(1);
   } else {
      prBar->showtext(true);
      double pos = 100.0;
      if (total_bytesize > 0)
         pos *= (double)curr_bytesize / total_bytesize;
      prBar->position(pos);
   }
}

static void read_log_cb(int fd_in, void *data)
{
   DLItem *dl_item = (DLItem *)data;
   const int BufLen = 4096;
   char Buf[BufLen];
   ssize_t st;

   do {
      st = read(fd_in, Buf, BufLen);
      if (st < 0) {
         if (errno == EAGAIN) {
            break;
         }
         perror("read, ");
         break;
      } else if (st == 0) {
         dClose(fd_in);
         Fl::remove_fd(fd_in, 1);
         dl_item->log_done(1);
         break;
      } else {
         dl_item->log_text_add(Buf, st);
      }
   } while (1);
}

void DLItem::father_init()
{
   dClose(LogPipe[1]);
   Fl::add_fd(LogPipe[0], 1, read_log_cb, this); // Read

   // Start the timer after the child is running.
   // (this makes a big difference with wget)
   //init_time = time(NULL);
}

/*
 * Our wget exited, let's check its status and update the panel.
 */
void DLItem::child_finished(int status)
{
   wget_status(status);

   if (status == 0) {
      prButton->label("Done");
      prButton->tooltip("Close this information panel");
   } else {
      prButton->label("Close");
      prButton->tooltip("Close this information panel");
      status_msg("ABORTED");
      if (curr_bytesize == 0) {
         // Update the title
         prTitle->copy_label(shortname);
      }
   }
   prButton->activate();
   prButton->redraw();
   MSG("wget status %d\n", status);
}

/*
 * Convert seconds into human readable [hour]:[min]:[sec] string.
 */
static void secs2timestr(int et, char *str)
{
   int eh, em, es;

   eh = et / 3600; em = (et % 3600) / 60; es = et % 60;
   if (eh == 0) {
      if (em == 0)
         snprintf(str, 8, "%ds", es);
      else
         snprintf(str, 8, "%dm%ds", em, es);
   } else {
      snprintf(str, 8, "%dh%dm", eh, em);
   }
}

/*
 * Update Got, Rate, ~Rate and ETA
 */
void DLItem::update()
{
   struct stat ss;
   time_t curr_time;
   float csec, tsec, rate, _rate = 0;
   char str[64];
   int et;

   if (updates_done())
      return;

   /* Update curr_size */
   if (stat(fullname, &ss) == -1) {
      MSG("stat, %s\n", dStrerror(errno));
      return;
   }
   update_size((int)ss.st_size);

   /* Get current time */
   time(&curr_time);
   csec = (float) (curr_time - init_time);

   /* Rate */
   if (csec >= 2) {
      tsec = (float) (curr_time - twosec_time);
      rate = ((float)(curr_bytesize-twosec_bytesize) / 1024) / tsec;
      snprintf(str, 64, (rate < 100) ? "%.1fK/s" : "%.0fK/s", rate);
      prRate->copy_label(str);
   }
   /* ~Rate */
   if (csec >= 1) {
      _rate = ((float)(curr_bytesize-init_bytesize) / 1024) / csec;
      snprintf(str, 64, (_rate < 100) ? "%.1fK/s" : "%.0fK/s", _rate);
      pr_Rate->copy_label(str);
   }

   /* ETA */
   if (fork_done()) {
      updates_done(1); // Last update
      prETAt->label("Time");
      prETAt->tooltip("Download Time");
      prETAt->redraw();
      secs2timestr((int)csec, str);
      prETA->copy_label(str);
      if (total_bytesize == -1) {
         update_prSize(curr_bytesize);
         if (wget_status() == 0)
            status_msg("Done");
      }
   } else {
      if (_rate > 0 && total_bytesize > 0 && curr_bytesize > 0) {
         et = (int)((total_bytesize-curr_bytesize) / (_rate * 1024));
         secs2timestr(et, str);
         prETA->copy_label(str);
      }
   }

   /* Update one and two secs ago times and bytesizes */
   twosec_time = onesec_time;
   onesec_time = curr_time;
   twosec_bytesize = onesec_bytesize;
   onesec_bytesize = curr_bytesize;
}

// SIGCHLD -------------------------------------------------------------------

/*! SIGCHLD handler
 */
static void raw_sigchld(int)
{
   caught_sigchld = 1;
}

/*! Establish SIGCHLD handler */
static void est_sigchld(void)
{
   struct sigaction sigact;
   sigset_t set;

   (void) sigemptyset(&set);
   sigact.sa_handler = raw_sigchld;
   sigact.sa_mask = set;
   sigact.sa_flags = SA_NOCLDSTOP;
   if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
      perror("sigaction");
      exit(1);
   }
}

/*
 * Timeout function to check wget's exit status.
 */
static void cleanup_cb(void *data)
{
   DLItemList *list = (DLItemList *)data;

   sigprocmask(SIG_BLOCK, &mask_sigchld, NULL);
   if (caught_sigchld) {
      /* Handle SIGCHLD */
      int i, status;
      for (i = 0; i < list->num(); ++i) {
         if (!list->get(i)->fork_done() &&
             waitpid(list->get(i)->pid(), &status, WNOHANG) > 0) {
            list->get(i)->child_finished(status);
            list->get(i)->fork_done(1);
         }
      }
      caught_sigchld = 0;
   }
   sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);

   Fl::repeat_timeout(1.0,cleanup_cb,data);
}

/*
 * Timeout function to update the widget indicators,
 * also remove widgets marked "done".
 */
static void update_cb(void *data)
{
   static int cb_used = 0;

   DLItemList *list = (DLItemList *)data;

   /* Update the widgets and remove the ones marked as done */
   for (int i = 0; i < list->num(); ++i) {
      if (!list->get(i)->widget_done()) {
         list->get(i)->update();
      } else if (list->get(i)->fork_done()) {
         // widget_done and fork_done avoid a race condition.
         dl_win->del(i); --i;
      }
      cb_used = 1;
   }

   if (cb_used && list->num() == 0)
      exit(0);

   Fl::repeat_timeout(1.0,update_cb,data);
}


// DLWin ---------------------------------------------------------------------

/*
 * Callback function for the request socket.
 * Read the request, parse and start a new download.
 */
static void read_req_cb(int req_fd, void *)
{
   struct sockaddr_un clnt_addr;
   int sock_fd;
   socklen_t csz;
   Dsh *sh = NULL;
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *dl_dest = NULL;

   /* Initialize the value-result parameter */
   csz = sizeof(struct sockaddr_un);
   /* accept the request */
   do {
      sock_fd = accept(req_fd, (struct sockaddr *) &clnt_addr, &csz);
   } while (sock_fd == -1 && errno == EINTR);
   if (sock_fd == -1) {
      MSG("accept, %s fd=%d\n", dStrerror(errno), req_fd);
      return;
   }

   /* create a sock handler */
   sh = a_Dpip_dsh_new(sock_fd, sock_fd, 8*1024);

   /* Authenticate our client... */
   if (!(dpip_tag = a_Dpip_dsh_read_token(sh, 1)) ||
       a_Dpip_check_auth(dpip_tag) < 0) {
      MSG("can't authenticate request: %s fd=%d\n", dStrerror(errno), sock_fd);
      a_Dpip_dsh_close(sh);
      goto end;
   }
   dFree(dpip_tag);

   /* Read request */
   if (!(dpip_tag = a_Dpip_dsh_read_token(sh, 1))) {
      MSG("can't read request: %s fd=%d\n", dStrerror(errno), sock_fd);
      a_Dpip_dsh_close(sh);
      goto end;
   }
   a_Dpip_dsh_close(sh);
   _MSG("Received tag={%s}\n", dpip_tag);

   if ((cmd = a_Dpip_get_attr(dpip_tag, "cmd")) == NULL) {
      MSG("Failed to parse 'cmd' in {%s}\n", dpip_tag);
      goto end;
   }
   if (strcmp(cmd, "DpiBye") == 0) {
      MSG("got DpiBye, ignoring...\n");
      goto end;
   }
   if (strcmp(cmd, "download") != 0) {
      MSG("unknown command: '%s'. Aborting.\n", cmd);
      goto end;
   }
   if (!(url = a_Dpip_get_attr(dpip_tag, "url"))){
      MSG("Failed to parse 'url' in {%s}\n", dpip_tag);
      goto end;
   }
   if (!(dl_dest = a_Dpip_get_attr(dpip_tag, "destination"))){
      MSG("Failed to parse 'destination' in {%s}\n", dpip_tag);
      goto end;
   }
   dl_win->add(dl_dest, url);

end:
   dFree(cmd);
   dFree(url);
   dFree(dl_dest);
   dFree(dpip_tag);
   a_Dpip_dsh_free(sh);
}

/*
 * Callback for close window request (WM or EscapeKey press)
 */
static void dlwin_esc_cb(Fl_Widget *, void *)
{
   const char *msg = "There are running downloads.\n"
                     "ABORT them and EXIT anyway?";

   if (dl_win && dl_win->num_running() > 0) {
      fl_message_title("Dillo Downloads: Abort downloads?");
      int ch = fl_choice("%s", "Cancel", "*No", "Yes", msg);
      if (ch == 0 || ch == 1)
         return;
   }

   /* abort each download properly */
   dl_win->abort_all();
}

/*
 * Add a new download request to the main window and
 * fork a child to do the job.
 */
void DLWin::add(const char *full_filename, const char *url)
{
   DLItem *dl_item = new DLItem(full_filename, url);
   mDList->add(dl_item);
   mPG->insert(*dl_item->get_widget(), 0);

   _MSG("Child index = %d\n", mPG->find(dl_item->get_widget()));

   // Start the child process
   pid_t f_pid = fork();
   if (f_pid == 0) {
      /* child */
      dl_item->child_init();
      _exit(EXIT_FAILURE);
   } else if (f_pid < 0) {
      perror("fork, ");
      exit(1);
   } else {
      /* father */
      dl_item->get_widget()->show();
      dl_win->show();
      dl_item->pid(f_pid);
      dl_item->father_init();
   }
}

/*
 * Delete a download request from the main window.
 */
void DLWin::del(int n_item)
{
   DLItem *dl_item = mDList->get(n_item);

   // Remove the widget from the packed group
   mPG->remove(dl_item->get_widget());
   mScroll->redraw();
   mDList->del(n_item);
   delete(dl_item);
}

/*
 * Return number of entries
 */
int DLWin::num()
{
   return mDList->num();
}

/*
 * Return number of running downloads
 */
int DLWin::num_running()
{
   int i, nr;

   for (i = nr = 0; i < mDList->num(); ++i)
      if (!mDList->get(i)->fork_done())
         ++nr;
   return nr;
}

/*
 * Set a callback function for the request socket
 */
void DLWin::listen(int req_fd)
{
   Fl::add_fd(req_fd, 1, read_req_cb, NULL); // Read
}

/*
 * Abort each download properly, and let the main cycle exit
 */
void DLWin::abort_all()
{
   for (int i = 0; i < mDList->num(); ++i)
      mDList->get(i)->abort_dl();
}

/*
 * A Scroll class that resizes its resizable widget to its width.
 * see http://seriss.com/people/erco/fltk/#ScrollableWidgetBrowser
 */
class DlScroll : public Fl_Scroll
{
public:
  void resize(int x_, int y_, int w_, int h_)
  {
    Fl_Scroll::resize(x_, y_, w_, h_);
    Fl_Widget *resizable_ = resizable();
    int sb_size =
       resizable_->h() <= h() ? 0 :
       scrollbar_size() ? scrollbar_size() :
       Fl::scrollbar_size();
    if (resizable_)
      resizable_->resize(resizable_->x(),
                         resizable_->y(),
                         w() - sb_size,
                         resizable_->h());
  }
  DlScroll(int x, int y, int w, int h, const char *l = 0)
    : Fl_Scroll(x, y, w, h, l)
  {
  }
};

/*
 * Create the main window and an empty list of requests.
 */
DLWin::DLWin(int ww, int wh) {

   // Init an empty list for the download requests
   mDList = new DLItemList();

   // Create the empty main window
   mWin = new Fl_Window(ww, wh, "Dillo Downloads");
   mWin->begin();
   mScroll = new DlScroll(0,0,ww,wh);
   mScroll->begin();
   mPG = new Fl_Pack(0,0,ww-18,wh);
   mPG->end();
   mScroll->end();
   mScroll->type(Fl_Scroll::VERTICAL);
   mScroll->resizable(mPG);
   mWin->end();
   mWin->resizable(mScroll);
   mWin->callback(dlwin_esc_cb, NULL);
   mWin->show();

   // Set SIGCHLD handlers
   sigemptyset(&mask_sigchld);
   sigaddset(&mask_sigchld, SIGCHLD);
   est_sigchld();

   fl_message_title_default("Dillo Downloads: Message");

   // Set the cleanup timeout
   Fl::add_timeout(1.0, cleanup_cb, mDList);
   // Set the update timeout
   Fl::add_timeout(1.0, update_cb, mDList);
}


// ---------------------------------------------------------------------------

/*
 * Set FL_NORMAL_LABEL to interpret neither symbols (@) nor shortcuts (&)
 */
static void custLabelDraw(const Fl_Label* o, int X, int Y, int W, int H,
                          Fl_Align align)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 0;
   fl_font(o->font, o->size);
   fl_color((Fl_Color)o->color);
   fl_draw(o->value, X, Y, W, H, align, o->image, interpret_symbols);
}

static void custLabelMeasure(const Fl_Label* o, int& W, int& H)
{
   const int interpret_symbols = 0;

   fl_draw_shortcut = 0;
   fl_font(o->font, o->size);
   fl_measure(o->value, W, H, interpret_symbols);
}



//int main(int argc, char **argv)
int main()
{
   int ww = 420, wh = 85;

   Fl::lock();

   // Disable '@' and '&' interpretation in normal labels.
   Fl::set_labeltype(FL_NORMAL_LABEL, custLabelDraw, custLabelMeasure);

   Fl::scheme(NULL);

   // Create the download window
   dl_win = new DLWin(ww, wh);

   // Start listening to the request socket
   dl_win->listen(STDIN_FILENO);

   MSG("started...\n");

   return Fl::run();
}

