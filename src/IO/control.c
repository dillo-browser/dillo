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
#include "timeout.hh"

enum control_req_stage {
   REQ_READ_COMMAND,
   REQ_READ_BODY,
   REQ_PREPARE_REPLY,
   REQ_SWITCH_WRITE,
   REQ_WRITE_REPLY,
   REQ_CLOSE,
};

struct control_req {
   int fd;
   enum control_req_stage stage;
   Dstr *cmd;
   Dstr *body;
   Dstr *reply;
   size_t reply_offset;
};

/* One control fd per process */
static int control_fd = -1;
static char *control_path = NULL;

/* The BrowserWindow that we are currently waiting to finish */
static BrowserWindow *bw_waiting = NULL;
static struct control_req *bw_waiting_req = NULL;

static void handle_wait_timeout(void *data);

/* Read the given non-blocking fd, read available data into output.
 * Returns:
 *    0:  when the end of file has been reached.
 *   +1:  if there is more data to be read.
 *   <0: other unexpected error.
 */
static int
Control_read_fd(int fd, Dstr *output)
{
   ssize_t r;
   do {
      char buf[BUFSIZ];
      r = read(fd, buf, BUFSIZ);
      _MSG("read buf = %zd\n", r);
      if (r < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Reached end of available data, try later */
            return +1;
         } else {
            MSG_ERR("Control_read_fd: read failed: %s\n", dStrerror(errno));
            return -1;
         }
      } else if (r > 0) {
         dStr_append_l(output, buf, r);
      }
   } while (r);

   return 0;
}

static int
Control_write_fd(int fd, Dstr *input, size_t *ptr_offset)
{
   size_t offset = *ptr_offset;
   ssize_t w;

   while ((size_t) input->len > offset) {
      const char *src = input->str + offset;
      size_t left = input->len - offset;
      w = write(fd, src, left);
      if (w < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Cannot write more data, return error */
            *ptr_offset = offset;
            return +1;
         } else {
            MSG_ERR("Control_write_fd: write failed: %s\n", dStrerror(errno));
            return -1;
         }
      } else if (w > 0) {
         offset += w;
      }
   }

   /* Save offset just in case */
   *ptr_offset = offset;

   return 0;
}

/* Create new request structure */
static struct control_req *Control_req_new(int fd)
{
   struct control_req *req = dMalloc0(sizeof(struct control_req));
   req->fd = fd;
   req->cmd = dStr_sized_new(128);
   req->body = dStr_sized_new(1);
   req->reply = dStr_sized_new(64);
   req->stage = REQ_READ_COMMAND;

   return req;
}

static void Control_req_free(struct control_req *req)
{
   dStr_free(req->cmd, 1);
   dStr_free(req->body, 1);
   dStr_free(req->reply, 1);
   dFree(req);
}

static void Control_req_read_cmd(struct control_req *req)
{
   ssize_t r;
   int err = 0;
   do {
      /* Read characters one by one */
      char buf[1];
      r = read(req->fd, buf, 1);
      if (r < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Reached end of available data for now, try later */
            return;
         } else {
            MSG_ERR("Control_req_read_cmd: read failed: %s\n", dStrerror(errno));
            err = 1;
            break;
         }
      } else if (r == 1) {
         dStr_append_l(req->cmd, buf, 1);
      }
      if (buf[0] == '\n')
         break;
   } while (r);

   if (err || req->cmd->len == 0) {
      /* Problem reading command or empty (may be caused by a reset
       * connection). Go straight to close the request. */
      req->stage = REQ_CLOSE;
   } else {
      /* Got command, strip new line and line feed */
      char *cmd = req->cmd->str;
      for (char *p = cmd; *p != '\0'; p++) {
         if (*p == '\n' || *p == '\r') {
            *p = '\0';
            break;
         }
      }

      /* Only determine if we need to read the body or not */
      if (!strcmp(cmd, "load") || !strcmp(cmd, "rawload"))
         req->stage = REQ_READ_BODY;
      else
         req->stage = REQ_PREPARE_REPLY;
   }
}

static void Control_req_prepare_reply(struct control_req *req)
{
   int done = 1;
   const char *cmd = req->cmd->str;
   Dstr *r = req->reply;
   BrowserWindow *bw = a_UIcmd_get_first_active_bw();

   if (strcmp(cmd, "help") == 0) {
      dStr_sprintfa(r, "0\n");
      dStr_sprintfa(r, "Commands return 0 on success or non-zero on error.\n");
      dStr_sprintfa(r, "Available commands:\n");
      dStr_sprintfa(r, " ping          Check if dillo replies correctly:\n");
      dStr_sprintfa(r, " pid           Print PID of selected dillo process\n");
      dStr_sprintfa(r, " reload        Reload the current tab\n");
      dStr_sprintfa(r, " ready         Exits with 0 if finished loading, 1 otherwise\n");
      dStr_sprintfa(r, " open URL      Open the given URL in the current tab\n");
      dStr_sprintfa(r, " url           Print the url in the current tab\n");
      dStr_sprintfa(r, " dump          Print the content of the current tab\n");
      dStr_sprintfa(r, " hdump         Print the HTTP headers of the current tab\n");
      dStr_sprintfa(r, " load          Replace the content in the current tab by stdin\n");
      dStr_sprintfa(r, " rawload       Replace the HTTP headers and content of the current\n");
      dStr_sprintfa(r, "               tab by stdin\n");
      dStr_sprintfa(r, " quit          Close dillo\n");
      dStr_sprintfa(r, " wait [T]      Wait until the current tab has finished loading\n");
      dStr_sprintfa(r, "               at most T seconds (default 60.0). Wait forever with\n");
      dStr_sprintfa(r, "               T set to 0.\n");
   } else if (strcmp(cmd, "ping") == 0) {
      dStr_sprintfa(r, "0\npong\n");
   } else if (strcmp(cmd, "pid") == 0) {
      dStr_sprintfa(r, "0\n%d\n", getpid());
   } else if (strcmp(cmd, "reload") == 0) {
      a_UIcmd_reload_all_active();
      dStr_sprintfa(r, "0\n");
   } else if (strcmp(cmd, "ready") == 0) {
      dStr_sprintfa(r, "%d\n", a_UIcmd_has_finished(bw) ? 0 : 1);
   } else if (strncmp(cmd, "open ", 5) == 0) {
      a_UIcmd_open_urlstr(bw, cmd+5);
      dStr_sprintfa(r, "0\n");
   } else if (strcmp(cmd, "url") == 0) {
      dStr_sprintfa(r, "0\n%s\n", a_UIcmd_get_location_text(bw));
   } else if (strcmp(cmd, "dump") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      char *buf;
      int size;
      dStr_sprintfa(r, "0\n");
      if (a_Capi_get_buf(url, &buf, &size))
         dStr_append_l(r, buf, size);
      a_Url_free(url);
   } else if (strcmp(cmd, "hdump") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      Dstr *header = a_Cache_get_header(url);
      if (header == NULL) {
         dStr_sprintfa(r, "1\nCurrent url not cached");
      } else {
         dStr_sprintfa(r, "0\n");
         dStr_append_l(r, header->str, header->len);
      }
      a_Url_free(url);
   } else if (strcmp(cmd, "load") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      if (url) {
         a_Cache_entry_inject(url, req->body->str, req->body->len,
               CA_GotHeader | CA_GotLength);
         a_UIcmd_repush(bw);
         dStr_sprintfa(r, "0\n");
         a_Url_free(url);
      } else {
         dStr_sprintfa(r, "1\ncannot get current url");
      }
   } else if (strcmp(cmd, "rawload") == 0) {
      DilloUrl *url = a_Url_new(a_UIcmd_get_location_text(bw), NULL);
      if (url) {
         a_Cache_entry_inject(url, req->body->str, req->body->len, 0);
         a_UIcmd_repush(bw);
         dStr_sprintfa(r, "0\n");
         a_Url_free(url);
      } else {
         dStr_sprintfa(r, "1\ncannot get current url");
      }
   } else if (strcmp(cmd, "quit") == 0) {
      a_UIcmd_close_all_bw((void *)1);
      dStr_sprintfa(r, "0\n");
   } else if (strncmp(cmd, "cmd ", 4) == 0) {
      const char *cmdname = cmd + 4;
      int ret = a_UIcmd_by_name(bw, cmdname);
      dStr_sprintfa(r, "%d\n", ret == 0 ? 0 : 1);
   } else if (strcmp(cmd, "wait") == 0 || strncmp(cmd, "wait ", 5) == 0) {
      float timeout = 60.0f; /* 1 minute by default */
      /* Contains timeout argument? */
      if (cmd[4] == ' ')
         timeout = atof(&cmd[5]);
      if (a_UIcmd_has_finished(bw)) {
         dStr_sprintfa(r, "0\n");
      } else {
         if (bw_waiting) {
            dStr_sprintfa(r, "1\nalready waiting\n");
         } else {
            bw_waiting = bw;
            bw_waiting_req = req;
            done = 0;
            /* If timeout is 0 or negative, wait forever */
            if (timeout > 0.0f)
               a_Timeout_add(timeout, handle_wait_timeout, NULL);
         }
      }
   } else {
      dStr_sprintfa(r, "1\nunknown command: %s\n", cmd);
   }

   if (done)
      req->stage = REQ_SWITCH_WRITE;
}

/* Process incoming or outgoing data from a control request */
static void Control_req_read_cb(int fd, void *data)
{
   struct control_req *req = data;

   /* Note: These are not if-elses on purpose. */

   if (req->stage == REQ_READ_COMMAND)
      Control_req_read_cmd(req);

   if (req->stage == REQ_READ_BODY) {
      /* Read until the end of the data or error */
      int ret = Control_read_fd(fd, req->body);
      if (ret == 0)
         req->stage = REQ_PREPARE_REPLY;
      else if (ret < 0)
         req->stage = REQ_CLOSE;
   }

   if (req->stage == REQ_PREPARE_REPLY)
      Control_req_prepare_reply(req);

   if (req->stage == REQ_SWITCH_WRITE) {
      /* Switch callback to write */
      a_IOwatch_remove_fd(fd, DIO_READ);
      a_IOwatch_add_fd(fd, DIO_WRITE, Control_req_read_cb, req);
      req->stage = REQ_WRITE_REPLY;
   }

   if (req->stage == REQ_WRITE_REPLY) {
      /* Keep writing until end or error */
      if (Control_write_fd(fd, req->reply, &req->reply_offset) <= 0)
         req->stage = REQ_CLOSE;
   }

   if (req->stage == REQ_CLOSE) {
      /* Remove any callback */
      a_IOwatch_remove_fd(fd, DIO_READ | DIO_WRITE);
      dClose(fd);
      Control_req_free(req);
   }
}

/* Process incoming connection to control socket */
static void Control_listen_cb(int fd, void *data)
{
   _MSG("Control_listen_cb called\n");

   int new_fd = accept(control_fd, NULL, NULL);
   if (new_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
         /* Nothing? */
         return;
      } else {
         MSG_ERR("accept failed: %s\n", strerror(errno));
         exit(1);
      }
   }

   /* From accept(2) man page:
    *
    * On Linux, the new socket returned by accept() does not inherit
    * file status flags such as O_NONBLOCK and O_ASYNC from the
    * listening socket. This behavior differs from the canonical BSD
    * sockets implementation. Portable programs should not rely on
    * inheritance or noninheritance of file status flags and always
    * explicitly set all required flags on the socket returned from
    * accept().
    *
    * So, enable O_NONBLOCK flag explicitly.
    */
   if (fcntl(new_fd, F_SETFL, O_NONBLOCK) == -1) {
      MSG_ERR("control socket fcntl failed: %s\n", strerror(errno));
      exit(1);
   }

   struct control_req *req = Control_req_new(new_fd);
   a_IOwatch_add_fd(new_fd, DIO_READ, Control_req_read_cb, req);
}

/* Timeout has passed, return error */
static void handle_wait_timeout(void *data)
{
   assert(bw_waiting);
   assert(bw_waiting_req);

   struct control_req *req = bw_waiting_req;
   dStr_sprintfa(req->reply, "1\nCurrent url not cached");
   req->stage = REQ_SWITCH_WRITE;

   bw_waiting = NULL;

   /* Run another pass to do progress */
   Control_req_read_cb(req->fd, req);
}

int a_Control_is_waiting(void)
{
   return bw_waiting != NULL;
}

void a_Control_notify_finish(BrowserWindow *bw)
{
   if (bw_waiting == NULL || bw_waiting != bw)
      return;

   _MSG("a_Control_notify_finish matched\n");

   assert(bw_waiting_req);

   struct control_req *req = bw_waiting_req;
   dStr_sprintfa(req->reply, "0\n");
   req->stage = REQ_SWITCH_WRITE;

   a_Timeout_actually_remove(handle_wait_timeout, NULL);
   bw_waiting = NULL;
   bw_waiting_req = NULL;

   /* Run another pass to do progress */
   Control_req_read_cb(req->fd, req);
}

int a_Control_init(void)
{
   int fd;
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      MSG("cannot create control socket: %s\n", strerror(errno));
      return -1;
   }

   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

#define LEN ((int) sizeof(addr.sun_path))

   char ctldir[LEN];
   if (snprintf(ctldir, LEN, "%s/.dillo/ctl", dGethomedir()) >= LEN) {
      MSG("control socket dir path too long: %s/.dillo/ctl\n", dGethomedir());
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
      MSG("control socket path too long: %s/%d\n", ctldir, (int) getpid());
      return -1;
   }

   int slen = sizeof(addr);

   if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
      MSG("control socket fcntl failed: %s\n", strerror(errno));
      return -1;
   }

   int on = 1;
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      MSG("control socket setsockopt failed: %s\n", strerror(errno));
      return -1;
   }

   if (ioctl(fd, FIONBIO, (char *)&on) != 0) {
      MSG("control socket ioctl failed: %s\n", strerror(errno));
      return -1;
   }

   if (bind(fd, (struct sockaddr *) &addr, slen) != 0) {
      MSG("cannot bind control socket: %s\n", strerror(errno));
      return -1;
   }

   if (listen(fd, 32) != 0) {
      MSG("cannot listen control socket: %s\n", strerror(errno));
      return -1;
   }

   control_fd = fd;
   control_path = dStrdup(addr.sun_path);

   a_IOwatch_add_fd(control_fd, DIO_READ, Control_listen_cb, NULL);

   return 0;
}

int a_Control_free(void)
{
   /* Nothing to do */
   if (control_fd == -1)
      return 0;

   assert(control_path);

   a_IOwatch_remove_fd(control_fd, DIO_READ);
   if (dClose(control_fd) != 0) {
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
int a_Control_is_waiting(void) { return 0; }
void a_Control_notify_finish(BrowserWindow *bw) { (void) bw; }
int a_Control_free(void) { return 0; }

#endif
