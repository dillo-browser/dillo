/*
 * File: dilloc.c
 *
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include "dlib/dlib.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

static int
is_number(const char *str)
{
   for (const char *p = str; *p; p++) {
      if (!isdigit(*p))
         return 0;
   }

   return 1;
}

static double
get_time_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double) ts.tv_sec + (double) ts.tv_nsec * 1.0e-9;
}

/* Connects to the given pid, retrying up to a timeout. */
static int
connect_given_pid(int *sock, const char *pid)
{
   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

   int fd;
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "socket() failed: %s\n", strerror(errno));
      return -1;
   }

   char *path = addr.sun_path;
   int len = (int) sizeof(addr.sun_path);
   if (snprintf(path, len, "%s/.dillo/ctl/%s", dGethomedir(), pid) >= len) {
      fprintf(stderr, "pid path too long\n");
      return -1;
   }

   double t0 = get_time_s();
   double maxtime = t0 + 10.0; /* 10 seconds */
   double warntime = t0 + 1.0; /* 1 second */
   /* Retry every 50 ms */
   struct timespec dur = { .tv_sec = 0, .tv_nsec = 50UL * 1000UL * 1000UL };
   int warned = 0;
   while (1) {
      if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == 0) {
         *sock = fd;
         return 0;
      } else if (errno != ENOENT) {
         fprintf(stderr, "connect to %s failed: %s\n", path, strerror(errno));
         return -1;
      }

      if (!warned && get_time_s() >= warntime) {
         fprintf(stderr, "connect to %s taking long\n", path);
         warned = 1;
      }

      if (get_time_s() >= maxtime) {
         fprintf(stderr, "timeout connecting to %s: %s\n",
               path, strerror(errno));
         break;
      }

      /* Retry after a bit */
      nanosleep(&dur, NULL);
   }

   dClose(fd);

   return -1;
}

/* Try to locate dillo if there is only one process */
static int
find_working_socket(int *sock)
{
   char ctlpath[PATH_MAX];
   if (snprintf(ctlpath, PATH_MAX, "%s/.dillo/ctl", dGethomedir()) >= PATH_MAX) {
      fprintf(stderr, "path too long\n");
      return -1;
   }

   struct dirent *ep;
   DIR *dp = opendir(ctlpath);
   if (dp == NULL) {
      fprintf(stderr, "error: cannot open %s directory: %s\n",
            ctlpath, strerror(errno));
      fprintf(stderr, "hint: is dillo running?\n");
      return -1;
   }

   int found_pid = 0;
   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

   while ((ep = readdir (dp)) != NULL) {
      const char *num = ep->d_name;
      /* Skip . and .. directories */
      if (!strcmp(num, ".") || !strcmp(num, ".."))
         continue;

      /* Make sure it is only digits */
      if (!is_number(num))
         continue;

#define LEN ((int) sizeof(addr.sun_path))
      if (snprintf(addr.sun_path, LEN, "%s/%s", ctlpath, num) >= LEN) {
         fprintf(stderr, "pid path too long\n");
         return -1;
      }
#undef LEN

      int fd;
      if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
         fprintf(stderr, "socket() failed: %s\n", strerror(errno));
         return -1;
      }

      int ret;
      do {
         ret = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
      } while (ret == -1 && errno == EINTR);

      if (ret == 0) {
         if (++found_pid == 1) {
            *sock = fd; /* Found ok */
            fd = -1;
            /* Leave it open */
         }
      } else {
         /* Remove the PID file if we cannot connect to it */
         if (errno == ECONNREFUSED) {
            /* Try to remove but don't check for errors */
            remove(addr.sun_path);
         } else {
            fprintf(stderr, "cannot connect to %s, skipping: %s\n",
                  addr.sun_path, strerror(errno));
         }
      }

      if (fd != -1 && dClose(fd) != 0) {
         fprintf(stderr, "cannot close fd: %s", strerror(errno));
         return -1;
      }
   }

   closedir(dp);

   if (found_pid == 1)
      return 0;
   else if (found_pid == 0) {
      fprintf(stderr, "error: cannot find ctl socket, is dillo running?\n");
      return -1;
   }

   fprintf(stderr, "multiple ctl sockets found, set DILLO_PID\n");
   dClose(*sock);
   return -1;
}

static int
connect_to_dillo(int *sock)
{
   /* If the PID is given, use only that one */
   char *given_pid = getenv("DILLO_PID");
   int fd = -1;
   if (given_pid) {
      /* Connect to the given pid with retry (dillo may be starting) */
      if (connect_given_pid(&fd, given_pid) != 0)
         return -1;
   } else {
      /* Otherwise, try to find a working pid and remove those that don't work,
       * which are likely dead processes */
      if (find_working_socket(&fd) != 0)
         return -1;
   }

   *sock = fd;

   return 0;
}

int main(int argc, char *argv[])
{
   if (argc <= 1) {
      fprintf(stderr, "missing command, try 'dilloc help'\n");
      return 2;
   }

   int fd = -1;
   if (connect_to_dillo(&fd) != 0)
      return 2;

   int is_load = 0;
   if (argv[1] && (!strcmp(argv[1], "load") || !strcmp(argv[1], "rawload"))) {
      is_load = 1;
   }

   Dstr *cmd = dStr_new("");
   for (int i = 1; i < argc; i++) {
      dStr_append(cmd, argv[i]);
      if (i+1 < argc)
         dStr_append_c(cmd, ' ');
   }

   dStr_append_c(cmd, '\n');

   for (ssize_t w = 0; w < cmd->len; ) {
      w = write(fd, cmd->str + w, cmd->len - w);
      if (w < 0) {
         perror("write");
         return 2;
      }
   }

   dStr_free(cmd, 1);

   /* Forward stdin to the socket */
   if (is_load) {
      uint8_t buf[BUFSIZ];
      while (1) {
         ssize_t r = read(STDIN_FILENO, buf, BUFSIZ);
         if (r == 0) {
            break;
         } else if (r < 0) {
            perror("read");
            return 2;
         }

         uint8_t *p = buf;

         while (r > 0) {
            ssize_t w = write(fd, p, r);
            if (w < 0) {
               perror("write");
               return 2;
            }
            r -= w;
            p += w;
         }
      }

      dClose(fd);
      return 0;
   }

   /* First two bytes determine exit code */
   int saw_rc = 0;
   int rc = 2;

   uint8_t buf[BUFSIZ];
   while (1) {
      ssize_t r = read(fd, buf, BUFSIZ);
      if (r == 0) {
         break;
      } else if (r < 2) {
         perror("read");
         return 2;
      }

      uint8_t *p = buf;

      if (!saw_rc) {
         int ch = buf[0];
         if (!isdigit(ch)) {
            fprintf(stderr, "malformed reply: expected digit at first byte (got %c)\n", ch);
            return 2;
         }
         if (buf[1] != '\n') {
            fprintf(stderr, "malformed reply: expected newline at second byte\n");
            return 2;
         }
         rc = (int) ch - '0';
         r -= 2;
         p += 2;
         saw_rc = 1;
      }

      while (r > 0) {
         size_t w = write(STDOUT_FILENO, p, r);
         r -= w;
         p += w;
      }
   }

   close(fd);

   return rc;
}
