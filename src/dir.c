/*
 * File: dir.c
 *
 * Copyright 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "msg.h"
#include "../dlib/dlib.h"


/*
 * Local data
 */
/* Dillo works from an unmounted directory (/tmp). */
static char *OldWorkingDirectory = NULL;

/*
 * Change current working directory to "/tmp".
 */
void a_Dir_init(void)
{
   dFree(OldWorkingDirectory);
   OldWorkingDirectory = dGetcwd();
   chdir("/tmp");
}

/*
 * Return the initial current working directory in a string.
 */
char *a_Dir_get_owd(void)
{
   return OldWorkingDirectory;
}

/*
 * Free memory
 */
void a_Dir_free(void)
{
   dFree(OldWorkingDirectory);
}

/*
 * Check if '~/.dillo' directory exists.
 * If not, try to create it.
 */
void a_Dir_check_dillorc_directory(void)
{
   char *dir;
   struct stat st;

   dir = dStrconcat(dGethomedir(), "/.dillo", NULL);
   if (stat(dir, &st) == -1) {
      if (errno == ENOENT) {
         MSG("Dillo: creating directory %s.\n", dir);
         if (mkdir(dir, 0700) < 0) {
            MSG("Dillo: error creating directory %s: %s\n", dir,
                dStrerror(errno));
         }
      } else {
         MSG("Dillo: error reading %s: %s\n", dir, dStrerror(errno));
      }
   }
   dFree(dir);
}

