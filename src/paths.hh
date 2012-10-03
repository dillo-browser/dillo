/*
 * File: paths.hh
 *
 * Copyright 2006-2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __PATHS_HH__
#define __PATHS_HH__

#define PATHS_RC_PREFS  "dillorc"
#define PATHS_RC_KEYS   "keysrc"
#define PATHS_RC_DOMAIN "domainrc"

class Paths {
public:
   static void init(void);
   static void free(void);
   static char *getOldWorkingDir(void);
   static FILE *getPrefsFP(const char *rcFile);
};

#endif /* __PATHS_HH__ */
