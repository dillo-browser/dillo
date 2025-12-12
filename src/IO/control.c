/*
 * File: control.c
 *
 * Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/** @file
 * Dillo remote control socket.
 *
 * Allow other programs to interact with Dillo.
 */

#include "control.h"

#include "config.h"

#if ENABLE_CONTROL_SOCKET

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "iowatch.hh"
#include "uicmd.hh"
#include "capi.h"
#include "dlib/dlib.h"
#include "msg.h"

/* One control fd per process */
static int control_fd = -1;
static char *control_path = NULL;

/* The BrowserWindow that we are currently waiting to finish */
static BrowserWindow *bw_waiting = NULL;
static FILE *bw_waiting_file = NULL;

static void
cmd_load(FILE *f, int fd)
{
   MSG("cmd_load()\n");
   Dstr *dstr = dStr_sized_new(1);
   ssize_t r;
   size_t len = 0;
   do {
      char buf[1];
      r = read(fd, buf, 1);
      MSG("read buf = %zd\n", r);
      if (r < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            MSG_ERR("Control_read_cb %s\n", dStrerror(errno));
            exit(1);
         }
      } else if (r > 0) {
         dStr_append_l(dstr, buf, r);
      }
      len += r;
   } while (r);

   BrowserWindow *bw = a_UIcmd_get_first_active_bw();
   /* Load the given content in the current page */
   DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
   if (url == NULL) {
      fprintf(f, "0\ncannot get current url");
      return;
   }

   a_Cache_entry_inject(url, dstr->str, dstr->len, 0);
   a_UIcmd_repush(bw);

   dStr_free(dstr, 1);

   fprintf(f, "0\n");
}

static void Control_read_cb(int fd, void *data)
{
   _MSG("Control_read_cb called\n");
   char buf[1024];
   const int buf_sz = 1024;
   Dstr *dstr = dStr_sized_new(buf_sz);

   int new_fd = accept(control_fd, NULL, NULL);
   if (new_fd < 0) {
      if (errno != EWOULDBLOCK) {
         MSG("accept failed: %s\n", strerror(errno));
         exit(1);
      }
      /* Nothing? */
      return;
   }

   ssize_t r;
   do {
      r = read(new_fd, buf, 1);
      MSG("read = %zd\n", r);
      if (r < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            MSG_ERR("Control_read_cb %s\n", dStrerror(errno));
            exit(1);
         }
      } else if (r > 0) {
         dStr_append_l(dstr, buf, r);
      }
      if (buf[0] == '\n')
         break;
   } while (r);

   MSG("got msg: %s\n", dstr->str);

   char *cmd = dstr->str;
   for (char *p = cmd; *p != '\0'; p++) {
      if (*p == '\n' || *p == '\r') {
         *p = '\0';
         break;
      }
   }

   FILE *f = fdopen(new_fd, "w");

   if (f == NULL) {
      MSG_ERR("fdopen failed: %s\n", dStrerror(errno));
      exit(1);
   }

   BrowserWindow *bw = a_UIcmd_get_first_active_bw();
   int do_close = 1;

   if (strcmp(cmd, "help") == 0) {
      fprintf(f, "0\n");
      fprintf(f, "Commands return 0 on success or non-zero on error.\n");
      fprintf(f, "Available commands:\n");
      fprintf(f, " ping          Check if dillo replies correctly:\n");
      fprintf(f, " pid           Print PID of selected dillo process\n");
      fprintf(f, " reload        Reload the current tab\n");
      fprintf(f, " ready         Exits with 0 if finished loading, 1 otherwise\n");
      fprintf(f, " open <url>    Open the given url in the current tab\n");
      fprintf(f, " url           Print the url in the current tab\n");
      fprintf(f, " dump          Print the content of the current tab\n");
      fprintf(f, " hdump         Print the HTTP headers of the current tab\n");
      fprintf(f, " load          Replace the content in the current tab by stdin\n");
      fprintf(f, " quit          Close dillo\n");
      fprintf(f, " wait          Wait until the current tab has finished loading\n");
   } else if (strcmp(cmd, "ping") == 0) {
      fprintf(f, "0\npong\n");
   } else if (strcmp(cmd, "pid") == 0) {
      fprintf(f, "0\n%d\n", getpid());
   } else if (strcmp(cmd, "reload") == 0) {
      a_UIcmd_reload_all_active();
      fprintf(f, "0\n");
   } else if (strcmp(cmd, "ready") == 0) {
      fprintf(f, "%d\n", a_UIcmd_has_finished(bw) ? 0 : 1);
   } else if (strncmp(cmd, "open ", 5) == 0) {
      a_UIcmd_open_urlstr(bw, cmd+5);
      fprintf(f, "0\n");
   } else if (strcmp(cmd, "url") == 0) {
      fprintf(f, "0\n%s\n", a_UIcmd_get_location_text(bw));
   } else if (strcmp(cmd, "dump") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      char *buf;
      int size;
      fprintf(f, "0\n");
      if (a_Capi_get_buf(url, &buf, &size)) {
         fwrite(buf, 1, size, f);
      }
      a_Url_free(url);
   } else if (strcmp(cmd, "hdump") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      Dstr *header = a_Cache_get_header(url);
      if (header == NULL) {
         fprintf(f, "1\nCurrent url not cached");
      } else {
         fprintf(f, "0\n");
         fwrite(header->str, 1, header->len, f);
      }
      a_Url_free(url);
   } else if (strcmp(cmd, "load") == 0) {
      cmd_load(f, new_fd);
   } else if (strcmp(cmd, "quit") == 0) {
      /* FIXME: May create confirmation dialog */
      a_UIcmd_close_all_bw(NULL);
      fprintf(f, "0\n");
   } else if (strncmp(cmd, "cmd ", 4) == 0) {
      const char *cmdname = cmd + 4;
      int ret = a_UIcmd_by_name(bw, cmdname);
      fprintf(f, "%d\n", ret == 0 ? 0 : 1);
   } else if (strcmp(cmd, "wait") == 0) {
      if (a_UIcmd_has_finished(bw)) {
         fprintf(f, "0\n");
      } else {
         assert(bw_waiting == NULL);
         bw_waiting = bw;
         bw_waiting_file = f;
         do_close = 0;
      }
   } else {
      fprintf(f, "1\nunknown command: %s\n", cmd);
   }


   dStr_free(dstr, 1);
   if (do_close)
      fclose(f);
   // close(new_fd); // No need as fclose already does
}

void a_Control_notify_finish(BrowserWindow *bw)
{
   if (bw_waiting == NULL || bw_waiting != bw)
      return;

   MSG("a_Control_notify_finish matched\n");

   assert(bw_waiting_file);

   FILE *f = bw_waiting_file;

   fprintf(f, "0\n");
   fclose(f);

   bw_waiting = NULL;
   bw_waiting_file = NULL;
}

int a_Control_init(void)
{
   int fd;
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      MSG("cannot create control socket: %s\n", strerror(errno));
      exit(1);
      return -1;
   }

   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

#define LEN ((int) sizeof(addr.sun_path))

   char ctldir[LEN];
   if (snprintf(ctldir, LEN, "%s/.dillo/ctl", dGethomedir()) >= LEN) {
      MSG("path too long\n");
      return -1;
   }

   /* Only the user should have access, otherwise other users could control the
    * browser remotely */
   if (mkdir(ctldir, 0700) != 0) {
      if (errno != EEXIST) {
         MSG("cannot create ctl dir: %s\n", strerror(errno));
         return -1;
      }
   }

   if (snprintf(addr.sun_path, LEN, "%s/%d", ctldir, (int) getpid()) >= LEN) {
      MSG("path too long\n");
      return -1;
   }

   int slen = sizeof(addr);

   fcntl(fd, F_SETFL, O_NONBLOCK);

   int on = 1;
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      MSG("setsockopt failed: %s\n", strerror(errno));
      exit(-1);
   }

   if (ioctl(fd, FIONBIO, (char *)&on) != 0) {
      MSG("ioctl failed: %s\n", strerror(errno));
      exit(1);
   }

   if (bind(fd, (struct sockaddr *) &addr, slen) != 0) {
      MSG("cannot bind control socket: %s\n", strerror(errno));
      exit(1);
      return -1;
   }

   if (listen(fd, 32) != 0) {
      MSG("cannot listen control socket: %s\n", strerror(errno));
      exit(1);
   }

   control_fd = fd;
   control_path = dStrdup(addr.sun_path);

   a_IOwatch_add_fd(control_fd, DIO_READ, Control_read_cb, NULL);

   return 0;
}

int a_Control_free(void)
{
   /* Nothing to do */
   if (control_fd == -1)
      return 0;

   assert(control_path);

   a_IOwatch_remove_fd(control_fd, DIO_READ);
   if (close(control_fd) != 0) {
      MSG_ERR("close ctl socket failed\n");
      return -1;
   }
   if (unlink(control_path) != 0) {
      MSG_ERR("unlink ctl socket failed\n");
      return -1;
   }
   dFree(control_path);
   return 0;
}

#else

int a_Control_init(void) { return 0; }
void a_Control_notify_finish(BrowserWindow *bw) { (void) bw; }
int a_Control_free(void) { return 0; }

#endif
