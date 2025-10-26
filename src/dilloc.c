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
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

int parse_number(const char *str, int *n)
{
   char *end;
   long p = strtol(str, &end, 10);

   if (*end != '\0')
      return -1;

   *n = (int) p;

   return 0;
}

/* Try to locate dillo if there is only one process */
char *get_unique_dillo_pid(void)
{
   char path[1024];
   if (snprintf(path, 1024, "%s/.dillo/ctl", dGethomedir()) >= 1024) {
      fprintf(stderr, "path too long\n");
      return NULL;
   }

   struct dirent *ep;
   DIR *dp = opendir (path);
   if (dp == NULL) {
      fprintf(stderr, "error: cannot open %s directory: %s\n",
            path, strerror(errno));
      return NULL;
   }

   int npids = 0;
   char *pid = NULL;

   while ((ep = readdir (dp)) != NULL) {
      const char *n = ep->d_name;
      /* Skip . and .. directories */
      if (!strcmp(n, ".") || !strcmp(n, ".."))
         continue;

      /* Make sure it is only digits */
      for (const char *p = n; *p; p++) {
         if (!isdigit(*p)) {
            n = NULL;
            break;
         }
      }

      if (n) {
         npids++;
         if (!pid)
            pid = dStrdup(n);
      }
   }

   closedir(dp);

   if (npids == 1)
      return pid;

   if (pid == NULL) {
      fprintf(stderr, "error: no pid files in: %s\n", path);
      return NULL;
   }

   dFree(pid);
   fprintf(stderr, "error: multiple pid files in: %s\n", path);
   return NULL;
}

int get_dillo_pid(void)
{
   /* First try the env var, then try to locate a unique dillo process */
   char *spid = getenv("DILLO_PID");
   if (spid) {
      spid = dStrdup(spid);
   } else if ((spid = get_unique_dillo_pid()) == NULL) {
      fprintf(stderr, "error: cannot find control socket, set DILLO_PID\n");
      return -1;
   }

   int pid;
   if (parse_number(spid, &pid) != 0) {
      fprintf(stderr, "error: cannot parse pid: %s\n", spid);
      dFree(spid);
      return -1;
   }

   dFree(spid);
   return pid;
}

int connect_to_socket(int pid, int *sock)
{
   int fd;
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "socket() failed: %s\n", strerror(errno));
      return -1;
   }

   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

#define LEN ((int) sizeof(addr.sun_path))

   if (snprintf(addr.sun_path, LEN, "%s/.dillo/ctl/%d", dGethomedir(), pid) >= LEN) {
      fprintf(stderr, "path too long\n");
      return -1;
   }

   int slen = sizeof(addr);

   if (connect(fd, (struct sockaddr *) &addr, slen) != 0) {
      fprintf(stderr, "error: cannot connect to %s: %s\n",
            addr.sun_path, strerror(errno));
      return -1;
   }

   *sock = fd;

   return 0;
}


int main(int argc, char *argv[])
{
   if (argc <= 1) {
      fprintf(stderr, "missing command\n");
      return 2;
   }

   int pid = get_dillo_pid();

   if (pid < 0)
      return 2;

   int fd;
   if (connect_to_socket(pid, &fd) != 0)
      return 2;

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

   /* First two bytes determine exit code */
   int saw_rc = 0;
   int rc = 2;

   uint8_t buf[1024];
   while (1) {
      ssize_t r = read(fd, buf, 1024);
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
         size_t w = write(1, p, r);
         r -= w;
         p += w;
      }
   }

   close(fd);

   return rc;
}
