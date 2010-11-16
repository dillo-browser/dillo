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
 * A FLTK2-based GUI for the downloads dpi (dillo plugin).
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

#include <fltk/run.h>
#include <fltk/Window.h>
#include <fltk/Widget.h>
#include <fltk/damage.h>
#include <fltk/Box.h>
#include <fltk/draw.h>
#include <fltk/HighlightButton.h>
#include <fltk/PackedGroup.h>
#include <fltk/ScrollGroup.h>
#include <fltk/ask.h>
#include <fltk/file_chooser.h>

#include "dpiutil.h"
#include "../dpip/dpip.h"

using namespace fltk;

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[downloads dpi]: " __VA_ARGS__)

/*
 * Internal types
 */
typedef enum {
   DL_NEWFILE,
   DL_CONTINUE,
   DL_RENAME,
   DL_OVERWRITE,
   DL_ABORT
} DLAction;

/*
 * Class declarations
 */

// ProgressBar widget --------------------------------------------------------

// class FL_API ProgressBar : public Widget {
class ProgressBar : public Widget {
protected:
  double mMin;
  double mMax;
  double mPresent;
  double mStep;
  bool mShowPct, mShowMsg;
  char mMsg[64];
  Color mTextColor;
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
  void text_color(Color col)    { mTextColor = col; }
  Color text_color()      { return mTextColor; }
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
   int log_len, log_max, log_state;
   char *log_text;
   time_t init_time;
   char **dl_argv;
   time_t twosec_time, onesec_time;
   int twosec_bytesize, onesec_bytesize;
   int init_bytesize, curr_bytesize, total_bytesize;
   int DataDone, LogDone, ForkDone, UpdatesDone, WidgetDone;
   int WgetStatus;

   int gw, gh;
   Group *group;
   ProgressBar *prBar;
   HighlightButton *prButton;
   Widget *prTitle, *prGot, *prSize, *prRate, *pr_Rate, *prETA, *prETAt;

public:
   DLItem(const char *full_filename, const char *url, DLAction action);
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
   Widget *get_widget() { return group; }
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
   Window *mWin;
   ScrollGroup *mScroll;
   PackedGroup *mPG;

public:
   DLWin(int ww, int wh);
   void add(const char *full_filename, const char *url, DLAction action);
   void del(int n_item);
   int num();
   int num_running();
   void listen(int req_fd);
   void show() { mWin->show(); }
   void hide() { mWin->hide(); }
   void abort_all();
   DLAction check_filename(char **p_dl_dest);
};


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
:  Widget(x, y, w, h, lbl)
{
   mMin = mPresent = 0;
   mMax = 100;
   mShowPct = true;
   mShowMsg = false;
   box(DOWN_BOX);
   selection_color(BLUE);
   color(WHITE);
   textcolor(RED);
}

void ProgressBar::draw()
{
   drawstyle(style(), flags());
   if (damage() & DAMAGE_ALL)
      draw_box();
   Rectangle r(w(), h());
   box()->inset(r);
   if (mPresent > mMax)
      mPresent = mMax;
   if (mPresent < mMin)
      mPresent = mMin;
   double pct = (mPresent - mMin) / mMax;

   if (vertical()) {
      int barHeight = int (r.h() * pct + .5);
      r.y(r.y() + r.h() - barHeight);
      r.h(barHeight);
   } else {
      r.w(int (r.w() * pct + .5));
   }

   setcolor(selection_color());

   if (mShowPct) {
      fillrect(r);
   } else {
      Rectangle r2(int (r.w() * pct), 0, int (w() * .1), h());
      push_clip(r2);
       fillrect(r);
      pop_clip();
   }

   if (mShowMsg) {
      setcolor(textcolor());
      setfont(this->labelfont(), this->labelsize());
      drawtext(mMsg, Rectangle(w(), h()), ALIGN_CENTER);
   } else if (mShowPct) {
      char buffer[30];
      sprintf(buffer, "%d%%", int (pct * 100 + .5));
      setcolor(textcolor());
      setfont(this->labelfont(), this->labelsize());
      drawtext(buffer, Rectangle(w(), h()), ALIGN_CENTER);
   }
}


// Download-item class -------------------------------------------------------

static void prButton_scb(Widget *, void *cb_data)
{
   DLItem *i = (DLItem *)cb_data;

   i->prButton_cb();
}

DLItem::DLItem(const char *full_filename, const char *url, DLAction action)
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

   // BUG:? test a URL with ' inside.
   /* escape "'" character for the shell. Is it necessary? */
   esc_url = Escape_uri_str(url, "'");
   /* avoid malicious SMTP relaying with FTP urls */
   if (dStrncasecmp(esc_url, "ftp:/", 5) == 0)
      Filter_smtp_hack(esc_url);
   dl_argv = new char*[8];
   int i = 0;
   dl_argv[i++] = (char*)"wget";
   if (action == DL_CONTINUE) {
      if (stat(fullname, &ss) == 0)
         init_bytesize = (int)ss.st_size;
      dl_argv[i++] = (char*)"-c";
   }
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
   group = new Group(0,0,gw,gh);
   group->begin();
    prTitle = new Widget(24, 7, 290, 23, shortname);
    prTitle->box(RSHADOW_BOX);
    prTitle->align(ALIGN_LEFT|ALIGN_INSIDE|ALIGN_CLIP);
    prTitle->set_flag (RAW_LABEL);
    // Attach this 'log_text' to the tooltip
    log_text_add("Target File: ", 13);
    log_text_add(fullname, strlen(fullname));
    log_text_add("\n\n", 2);

    prBar = new ProgressBar(24, 40, 92, 20);
    prBar->box(BORDER_BOX); // ENGRAVED_BOX
    prBar->tooltip("Progress Status");

    int ix = 122, iy = 36, iw = 50, ih = 14;
    Widget *o = new Widget(ix,iy,iw,ih, "Got");
    o->box(RFLAT_BOX);
    o->color((Color)0xc0c0c000);
    o->tooltip("Downloaded Size");
    prGot = new Widget(ix,iy+14,iw,ih, "0KB");
    prGot->align(ALIGN_CENTER|ALIGN_INSIDE);
    prGot->labelcolor((Color)0x6c6cbd00);
    prGot->box(NO_BOX);

    ix += iw;
    o = new Widget(ix,iy,iw,ih, "Size");
    o->box(RFLAT_BOX);
    o->color((Color)0xc0c0c000);
    o->tooltip("Total Size");
    prSize = new Widget(ix,iy+14,iw,ih, "??");
    prSize->align(ALIGN_CENTER|ALIGN_INSIDE);
    prSize->box(NO_BOX);

    ix += iw;
    o = new Widget(ix,iy,iw,ih, "Rate");
    o->box(RFLAT_BOX);
    o->color((Color)0xc0c0c000);
    o->tooltip("Current transfer Rate (KBytes/sec)");
    prRate = new Widget(ix,iy+14,iw,ih, "??");
    prRate->align(ALIGN_CENTER|ALIGN_INSIDE);
    prRate->box(NO_BOX);

    ix += iw;
    o = new Widget(ix,iy,iw,ih, "~Rate");
    o->box(RFLAT_BOX);
    o->color((Color)0xc0c0c000);
    o->tooltip("Average transfer Rate (KBytes/sec)");
    pr_Rate = new Widget(ix,iy+14,iw,ih, "??");
    pr_Rate->align(ALIGN_CENTER|ALIGN_INSIDE);
    pr_Rate->box(NO_BOX);

    ix += iw;
    prETAt = o = new Widget(ix,iy,iw,ih, "ETA");
    o->box(RFLAT_BOX);
    o->color((Color)0xc0c0c000);
    o->tooltip("Estimated Time of Arrival");
    prETA = new Widget(ix,iy+14,iw,ih, "??");
    prETA->align(ALIGN_CENTER|ALIGN_INSIDE);
    prETA->box(NO_BOX);

    //ix += 50;
    //prButton = new HighlightButton(ix, 41, 38, 19, "Stop");
    prButton = new HighlightButton(328, 9, 38, 19, "Stop");
    prButton->tooltip("Stop this transfer");
    prButton->box(UP_BOX);
    prButton->clear_tab_to_focus();
    prButton->callback(prButton_scb, this);

   group->box(ROUND_UP_BOX);
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
   delete(dl_argv);

   delete(group);
}

/*
 * Abort a running download
 */
void DLItem::abort_dl()
{
   if (!log_done()) {
      close(LogPipe[0]);
      remove_fd(LogPipe[0]);
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
   close(0); // stdin
   close(1); // stdout
   close(LogPipe[0]);
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
   prSize->redraw_label();
}

void DLItem::log_text_add(const char *buf, ssize_t st)
{
   const char *p;
   char *q, *d, num[64];

   // Make room...
   if (log_len + st >= log_max) {
      log_max = log_len + st + 1024;
      log_text = (char *) realloc (log_text, log_max);
      log_text[log_len] = 0;
      prTitle->tooltip(log_text);
   }

   // FSM to remove wget's "dot-progress" (i.e. "^ " || "^[0-9]+K")
   q = log_text + log_len;
   for (p = buf; (p - buf) < st; ++p) {
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
      }
   }

   // Show we're connecting...
   if (curr_bytesize == 0) {
      prTitle->label("Connecting...");
      prTitle->redraw_label();
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
      prTitle->label(shortname);
      prTitle->redraw_label();
   }

   curr_bytesize = new_sz;
   if (curr_bytesize > 1024 * 1024)
      snprintf(buf, 64, "%.1fMB", (float)curr_bytesize / (1024*1024));
   else
      snprintf(buf, 64, "%.0fKB", (float)curr_bytesize / 1024);
   prGot->copy_label(buf);
   prGot->redraw_label();
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
   int ret = -1;

   do {
      st = read(fd_in, Buf, BufLen);
      if (st < 0) {
         if (errno == EAGAIN) {
            ret = 1;
            break;
         }
         perror("read, ");
         break;
      } else if (st == 0) {
         close(fd_in);
         remove_fd(fd_in, 1);
         dl_item->log_done(1);
         ret = 0;
         break;
      } else {
         dl_item->log_text_add(Buf, st);
      }
   } while (1);
}

void DLItem::father_init()
{
   close(LogPipe[1]);
   add_fd(LogPipe[0], 1, read_log_cb, this); // Read

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
         prTitle->label(shortname);
         prTitle->redraw_label();
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
      prRate->redraw_label();
   }
   /* ~Rate */
   if (csec >= 1) {
      _rate = ((float)(curr_bytesize-init_bytesize) / 1024) / csec;
      snprintf(str, 64, (_rate < 100) ? "%.1fK/s" : "%.0fK/s", _rate);
      pr_Rate->copy_label(str);
      pr_Rate->redraw_label();
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
   prETA->redraw_label();

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

   repeat_timeout(1.0,cleanup_cb,data);
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

   repeat_timeout(1.0,update_cb,data);
}


// DLWin ---------------------------------------------------------------------

/*
 * Make a new name and place it in 'dl_dest'.
 */
static void make_new_name(char **dl_dest, const char *url)
{
   Dstr *gstr = dStr_new(*dl_dest);
   int idx = gstr->len;

   if (gstr->str[idx - 1] != '/'){
      dStr_append_c(gstr, '/');
      ++idx;
   }

   /* Use a mangled url as name */
   dStr_append(gstr, url);
   for (   ; idx < gstr->len; ++idx)
      if (!isalnum(gstr->str[idx]))
         gstr->str[idx] = '_';

   /* free memory */
   dFree(*dl_dest);
   *dl_dest = gstr->str;
   dStr_free(gstr, FALSE);
}

/*
 * Callback function for the request socket.
 * Read the request, parse and start a new download.
 */
static void read_req_cb(int req_fd, void *)
{
   struct sockaddr_un clnt_addr;
   int sock_fd;
   socklen_t csz;
   struct stat sb;
   Dsh *sh = NULL;
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *dl_dest = NULL;
   DLAction action = DL_ABORT; /* compiler happiness */

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
   /* 'dl_dest' may be a directory */
   if (stat(dl_dest, &sb) == 0 && S_ISDIR(sb.st_mode)) {
      make_new_name(&dl_dest, url);
   }
   action = dl_win->check_filename(&dl_dest);
   if (action != DL_ABORT) {
      // Start the whole thing whithin FLTK.
      dl_win->add(dl_dest, url, action);
   } else if (dl_win->num() == 0) {
      exit(0);
   }

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
static void dlwin_esc_cb(Widget *, void *)
{
   const char *msg = "There are running downloads.\n"
                     "ABORT them and EXIT anyway?";

   if (dl_win && dl_win->num_running() > 0) {
      int ch = fltk::choice(msg, "Yes", "*No", "Cancel");
      if (ch != 0)
         return;
   }

   /* abort each download properly */
   dl_win->abort_all();
}

/*
 * Add a new download request to the main window and
 * fork a child to do the job.
 */
void DLWin::add(const char *full_filename, const char *url, DLAction action)
{
   DLItem *dl_item = new DLItem(full_filename, url, action);
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
      dl_win->show();
      dl_item->pid(f_pid);
      dl_item->father_init();
   }
}

/*
 * Decide what to do when the filename already exists.
 * (renaming takes place here when necessary)
 */
DLAction DLWin::check_filename(char **p_fullname)
{
   struct stat ss;
   Dstr *ds;
   int ch;
   DLAction ret = DL_ABORT;

   if (stat(*p_fullname, &ss) == -1)
      return DL_NEWFILE;

   ds = dStr_sized_new(128);
   dStr_sprintf(ds,
                "The file:\n  %s (%d Bytes)\nalready exists. What do we do?",
                *p_fullname, (int)ss.st_size);
   ch = fltk::choice(ds->str, "Rename", "Continue", "Abort");
   dStr_free(ds, 1);
   MSG("Choice %d\n", ch);
   if (ch == 0) {
      const char *p;
      p = fltk::file_chooser("Enter a new name:", NULL, *p_fullname);
      if (p) {
         dFree(*p_fullname);
         *p_fullname = dStrdup(p);
         ret = check_filename(p_fullname);
      }
   } else if (ch == 1) {
      ret = DL_CONTINUE;
   }
   return ret;
}

/*
 * Delete a download request from the main window.
 */
void DLWin::del(int n_item)
{
   DLItem *dl_item = mDList->get(n_item);

   // Remove the widget from the packed group
   mPG->remove(dl_item->get_widget());
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
   add_fd(req_fd, 1, read_req_cb, NULL); // Read
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
 * Create the main window and an empty list of requests.
 */
DLWin::DLWin(int ww, int wh) {

   // Init an empty list for the download requests
   mDList = new DLItemList();

   // Create the empty main window
   mWin = new Window(ww, wh, "Downloads:");
   mWin->begin();
    mScroll = new ScrollGroup(0,0,ww,wh);
    mScroll->begin();
     mPG = new PackedGroup(0,0,ww,wh);
     mPG->end();
     //mPG->spacing(10);
    mScroll->end();
    mScroll->type(ScrollGroup::VERTICAL);
   mWin->end();
   mWin->resizable(mScroll);
   mWin->callback(dlwin_esc_cb, NULL);
   mWin->show();

   // Set SIGCHLD handlers
   sigemptyset(&mask_sigchld);
   sigaddset(&mask_sigchld, SIGCHLD);
   est_sigchld();

   // Set the cleanup timeout
   add_timeout(1.0, cleanup_cb, mDList);
   // Set the update timeout
   add_timeout(1.0, update_cb, mDList);
}


// ---------------------------------------------------------------------------



//int main(int argc, char **argv)
int main()
{
   int ww = 420, wh = 85;

   lock();

   // Create the download window
   dl_win = new DLWin(ww, wh);

   // Start listening to the request socket
   dl_win->listen(STDIN_FILENO);

   MSG("started...\n");

   return run();
}

