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


typedef enum {
   KEYS_INVALID = -1,
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
   KEYS_RELOAD,
   KEYS_STOP,
   KEYS_SAVE,
   KEYS_HIDE_PANELS,
   KEYS_FILE_MENU,
   KEYS_CLOSE_ALL,
   KEYS_BACK,
   KEYS_FORWARD,
   KEYS_GOTO,
   KEYS_HOME,
   KEYS_VIEW_SOURCE,
   KEYS_SCREEN_UP,
   KEYS_SCREEN_DOWN,
   KEYS_SCREEN_LEFT,
   KEYS_SCREEN_RIGHT,
   KEYS_LINE_UP,
   KEYS_LINE_DOWN,
   KEYS_LEFT,
   KEYS_RIGHT,
   KEYS_TOP,
   KEYS_BOTTOM
} KeysCommand_t;

class Keys {
private:
   static int nodeByKeyCmp(const void *node, const void *key);
   static void delKeyCmd(int key, int mod);
   static KeysCommand_t getCmdCode(const char *symbolName);
   static int getKeyCode(char *keyName);
   static int getModifier(char *modifierName);
   static void parseKey(char *key, char *symbol);
public:
   static void init();
   static void free();
   static void parse(FILE *fp);
   static KeysCommand_t getKeyCmd(void);
   static int getShortcut(KeysCommand_t cmd);
};


#endif /* __KEYS_HH__ */
