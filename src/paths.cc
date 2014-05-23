/*
 * File: paths.cc
 *
 * Copyright 2006-2009 Jorge Arellano Cid <jcid@dillo.org>
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
#include "paths.hh"

/*
 * Local data
 */

// Dillo works from an unmounted directory (/tmp)
static char* oldWorkingDir = NULL;

/*
 * Changes current working directory to /tmp and creates ~/.dillo
 * if not exists.
 */
void Paths::init(void)
{
   char *path;
   struct stat st;
   int rc = 0;

   dFree(oldWorkingDir);
   oldWorkingDir = dGetcwd();
   rc = chdir("/tmp");
   if (rc == -1) {
      MSG("paths: Error changing directory to /tmp: %s\n",
          dStrerror(errno));
   }

   path = dStrconcat(dGethomedir(), "/.dillo", NULL);
   if (stat(path, &st) == -1) {
      if (errno == ENOENT) {
         MSG("paths: Creating directory '%s/'\n", path);
         if (mkdir(path, 0700) < 0) {
            MSG("paths: Error creating directory %s: %s\n",
                path, dStrerror(errno));
         }
      } else {
         MSG("Dillo: error reading %s: %s\n", path, dStrerror(errno));
      }
   }

   dFree(path);
}

/*
 * Return the initial current working directory in a string.
 */
char *Paths::getOldWorkingDir(void)
{
   return oldWorkingDir;
}

/*
 * Free memory
 */
void Paths::free(void)
{
   dFree(oldWorkingDir);
}

/*
 * Examines the path for "rcFile" and assign its file pointer to "fp".
 */
FILE *Paths::getPrefsFP(const char *rcFile)
{
   FILE *fp;
   char *path = dStrconcat(dGethomedir(), "/.dillo/", rcFile, NULL);

   if (!(fp = fopen(path, "r"))) {
      MSG("paths: Cannot open file '%s': %s\n", path, dStrerror(errno));

      char *path2 = dStrconcat(DILLO_SYSCONF, rcFile, NULL);
      if (!(fp = fopen(path2, "r"))) {
         MSG("paths: Cannot open file '%s': %s\n", path2, dStrerror(errno));
         MSG("paths: Using internal defaults...\n");
      } else {
         MSG("paths: Using %s\n", path2);
      }
      dFree(path2);
   }

   dFree(path);
   return fp;
}

