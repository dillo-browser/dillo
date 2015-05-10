/*
 * Preferences parser
 *
 * Copyright (C) 2006-2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __PREFS_HH__
#define __PREFS_HH__

#ifdef __cplusplus
class PrefsParser {
public:
   static int parseLine(char *line, char *name, char *value);
   static void parse(FILE *fp);
};
#endif /* __cplusplus */

#endif /* __PREFS_HH__ */
