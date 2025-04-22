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
#include "dlib/dlib.h"
#include "msg.h"

/* One control fd per process */
static int control_fd = -1;
static char *control_path = NULL;

/* The BrowserWindow that we are currently waiting to finish */
static BrowserWindow *bw_waiting = NULL;
static FILE *bw_waiting_file = NULL;

static void Control_read_cb(int fd, void *data)
{
   MSG("Control_read_cb called\n");
   int st;
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

   do {
      st = read(new_fd, buf, buf_sz);
      MSG("read = %d\n", st);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            MSG_ERR("Control_read_cb %s\n", dStrerror(errno));
            exit(1);
         }
      } else if (st > 0) {
         dStr_append_l(dstr, buf, st);
      }
   } while (st == buf_sz);

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

   if (strcmp(cmd, "ping") == 0) {
      fprintf(f, "pong\n");
   } else if (strcmp(cmd, "reload") == 0) {
      a_UIcmd_reload_all_active();
      fprintf(f, "ok\n");
   } else if (strcmp(cmd, "has_finished") == 0) {
      fprintf(f, "%d\n", a_UIcmd_has_finished(bw));
   } else if (strncmp(cmd, "open ", 5) == 0) {
      a_UIcmd_open_urlstr(bw, cmd+5);
      fprintf(f, "ok\n");
   } else if (strcmp(cmd, "quit") == 0) {
      /* FIXME: May create confirmation dialog */
      a_UIcmd_close_all_bw(NULL);
      fprintf(f, "ok\n");
   } else if (strcmp(cmd, "wait") == 0) {
      if (a_UIcmd_has_finished(bw)) {
         fprintf(f, "done\n");
      } else {
         assert(bw_waiting == NULL);
         bw_waiting = bw;
         bw_waiting_file = f;
         do_close = 0;
      }
   } else {
      fprintf(f, "?\n");
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

   fprintf(f, "done\n");
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
