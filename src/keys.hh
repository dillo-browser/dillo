/*
 * Key parser
 *
 * Copyright (C) 2009 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright (C) 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
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
   KEYS_BOTTOM,
   KEYS_COPY,
   KEYS_ZOOM_IN,
   KEYS_ZOOM_OUT,
   KEYS_ZOOM_RESET,
   KEYS_FOCUS_TAB1,
   KEYS_FOCUS_TAB2,
   KEYS_FOCUS_TAB3,
   KEYS_FOCUS_TAB4,
   KEYS_FOCUS_TAB5,
   KEYS_FOCUS_TAB6,
   KEYS_FOCUS_TAB7,
   KEYS_FOCUS_TAB8,
   KEYS_FOCUS_TAB9,
   KEYS_FOCUS_TAB10
} KeysCommand_t;

class Keys {
private:
   static int nodeByKeyCmp(const void *node, const void *key);
   static void delKeyCmd(int key, int mod);
   static KeysCommand_t getCmdCode(const char *symbolName);
   static int getKeyCode(char *keyName);
   static int getModifier(char *modifierName);
   static void parseKey(char *key, char *symbol);
   static const char *getKeyName(int key);
public:
   static void init();
   static void free();
   static void parse(FILE *fp);
   static KeysCommand_t getKeyCmd(void);
   static int getShortcut(KeysCommand_t cmd);
   static void genAboutKeys(void);
};


#endif /* __KEYS_HH__ */
