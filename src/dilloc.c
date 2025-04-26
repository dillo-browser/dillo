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
      npids++;
      if (!pid)
         pid = dStrdup(n);
   }

   closedir(dp);

   if (npids == 1)
      return pid;

   dFree(pid);

   return NULL;
}

int connect_to_socket(const char *pid, int *sock)
{
   int fd;
   if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "socket() failed: %s\n", strerror(errno));
      return -1;
   }

   struct sockaddr_un addr;
   addr.sun_family = AF_UNIX;

#define LEN ((int) sizeof(addr.sun_path))

   if (snprintf(addr.sun_path, LEN, "%s/.dillo/ctl/%s", dGethomedir(), pid) >= LEN) {
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
      return 1;
   }

   /* First try the env var */
   char *pid = getenv("DILLO_PID");

   /* If not set, try to search for a single pid socket file */
   if (!pid && (pid = get_unique_dillo_pid()) == NULL) {
      fprintf(stderr, "error: cannot find unique control socket, set DILLO_PID\n");
      return 1;
   }

   int fd;
   if (connect_to_socket(pid, &fd) != 0)
      return 1;

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
         return 1;
      }
   }

   uint8_t buf[1024];
   while (1) {
      ssize_t r = read(fd, buf, 1024);
      if (r == 0) {
         break;
      } else if (r < 0) {
         perror("read");
         return 1;
      }

      uint8_t *p = buf;

      while (r > 0) {
         size_t w = write(1, p, r);
         r -= w;
         p += w;
      }
   }

   close(fd);

   return 0;
}
