/*
 * File: compat.c
 *
 * Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#define _DEFAULT_SOURCE /* Needed to load putenv on glibc */

#include "compat.h"

#include <errno.h> /* errno */
#include <stdlib.h> /* putenv */
#include <string.h> /* strlen */

#if !HAVE_SETENV
int setenv(const char *name, const char *val, int overwrite)
{
   if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL || val == NULL) {
      errno = EINVAL;
      return -1;
   }

   /* Don't do anything if already defined with overwrite unset */
   if (overwrite == 0 && getenv(name) != NULL)
      return 0;

#ifndef __sgi /* Not available on IRIX */
   /* Otherwise undefine first */
   unsetenv(name);
#endif

   int len = strlen(name) + strlen(val) + 2;
   char *env = malloc(len);

   if (env == NULL)
      return -1;

   strcpy(env, name);
   strcat(env, "=");
   strcat(env, val);

   return putenv(env);
}
#endif
