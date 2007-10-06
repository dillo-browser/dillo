/*
 * File: dir.c
 *
 * Copyright 2006 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <unistd.h>

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

