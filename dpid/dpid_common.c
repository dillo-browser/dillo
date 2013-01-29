/*
 * File: dpid_common.c
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "dpid_common.h"

/*
 * Send a verbose error message.
 */
void errmsg(char *caller, char *called, int errornum, char *file, int line)
{
   MSG_ERR("%s:%d: %s: %s\n", file, line, caller, called);
   if (errornum > 0)
      MSG_ERR("%s\n", dStrerror(errornum));
}

/*!
 * Provides an error checked write command.
 * Call this via the CKD_WRITE macro
 * \return write return value
 */
ssize_t ckd_write(int fd, char *msg, char *file, int line)
{
   ssize_t ret;

   do {
      ret = write(fd, msg, strlen(msg));
   } while (ret == -1 && errno == EINTR);
   if (ret == -1) {
      MSG_ERR("%s:%d: write: %s\n", file, line, dStrerror(errno));
   }
   return (ret);
}

/*!
 * Provides an error checked close() call.
 * Call this via the CKD_CLOSE macro
 * \return close return value
 */
ssize_t ckd_close(int fd, char *file, int line)
{
   ssize_t ret;

   ret = dClose(fd);
   if (ret == -1)
      MSG_ERR("%s:%d: close: %s\n", file, line, dStrerror(errno));
   return ret;
}

