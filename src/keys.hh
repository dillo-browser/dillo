/*
 * Key parser
 *
 * Copyright (C) 2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __KEYS_HH__
#define __KEYS_HH__


enum {
   KEYS_NOP,   /* No operation bound */
   KEYS_OPEN,
   KEYS_NEW_WINDOW,
   KEYS_NEW_TAB,
   KEYS_LEFT_TAB,
   KEYS_RIGHT_TAB,
   KEYS_CLOSE_TAB,
   KEYS_FIRST_TAB,
   KEYS_LAST_TAB,
   KEYS_FIND,
   KEYS_WEBSEARCH,
   KEYS_BOOKMARKS,
   KEYS_FULLSCREEN,
   KEYS_RELOAD,
   KEYS_HIDE_PANELS,
   KEYS_FILE_MENU,
   KEYS_CLOSE_ALL,
   KEYS_BACK,
   KEYS_FORWARD,
   KEYS_GOTO,
   KEYS_HOME
};

class Keys {
public:
   static void init();
   static void free();
   static int nodeByKeyCmp(const void *node, const void *key);
   static int getKeyCmd(void);
   static void delKeyCmd(int key, int mod);
   static int getCmdCode(const char *symbolName);
   static int getKeyCode(char *keyName);
   static int getModifier(char *modifierName);
   static void parseKey(char *key, char *symbol);
   static void parse(FILE *fp);
};


#endif /* __KEYS_HH__ */
