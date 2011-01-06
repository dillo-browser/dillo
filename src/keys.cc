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

#include <fltk/events.h>
#include <stdio.h>
#include <stdlib.h>        /* strtol */
#include <string.h>
#include <ctype.h>

#include "dlib/dlib.h"
#include "keys.hh"
#include "msg.h"

/*
 *  Local data types
 */
typedef struct {
   const char *name;
   KeysCommand_t cmd;
   int modifier, key;
} KeyBinding_t;

typedef struct {
   const char *name;
   const int value;
} Mapping_t;


/*
 *  Local data
 */
static const Mapping_t keyNames[] = {
   { "Backspace",   FL_BackSpace },
   { "Delete",      FL_Delete    },
   { "Down",        FL_Down      },
   { "End",         FL_End       },
   { "Esc",         FL_Escape    },
   { "F1",          FL_F1        },
   { "F2",          FL_F2        },
   { "F3",          FL_F3        },
   { "F4",          FL_F4        },
   { "F5",          FL_F5        },
   { "F6",          FL_F6        },
   { "F7",          FL_F7        },
   { "F8",          FL_F8        },
   { "F9",          FL_F9        },
   { "F10",         FL_F10       },
   { "F11",         FL_F11       },
   { "F12",         FL_F12       },
   { "Home",        FL_Home      },
   { "Insert",      FL_Insert    },
   { "Left",        FL_Left      },
   { "PageDown",    FL_Page_Down },
   { "PageUp",      FL_Page_Up   },
   { "Print",       FL_Print     },
   { "Return",      FL_Enter     },
   { "Right",       FL_Right     },
   { "Space",       ' '          },
   { "Tab",         FL_Tab       },
   { "Up",          FL_Up        }
};

static const Mapping_t modifierNames[] = {
   { "Shift",   FL_SHIFT   },
   { "Ctrl",    FL_CTRL    },
   { "Alt",     FL_ALT     },
   { "Meta",    FL_META    },
   { "Button1", FL_BUTTON1 },
   { "Button2", FL_BUTTON2 },
   { "Button3", FL_BUTTON3 }
};

static const KeyBinding_t default_keys[] = {
   { "nop"          , KEYS_NOP          , 0         , 0               },
   { "open"         , KEYS_OPEN         , FL_CTRL   , 'o'             },
   { "new-window"   , KEYS_NEW_WINDOW   , FL_CTRL   , 'n'             },
   { "new-tab"      , KEYS_NEW_TAB      , FL_CTRL   , 't'             },
   { "left-tab"     , KEYS_LEFT_TAB     , FL_SHIFT  , FL_Tab          },
   { "right-tab"    , KEYS_RIGHT_TAB    , FL_CTRL   , FL_Tab          },
   { "close-tab"    , KEYS_CLOSE_TAB    , FL_CTRL   , 'q'             },
   { "find"         , KEYS_FIND         , FL_CTRL   , 'f'             },
   { "websearch"    , KEYS_WEBSEARCH    , FL_CTRL   , 's'             },
   { "bookmarks"    , KEYS_BOOKMARKS    , FL_CTRL   , 'b'             },
   { "fullscreen"   , KEYS_FULLSCREEN   , FL_CTRL   , ' '             },
   { "reload"       , KEYS_RELOAD       , FL_CTRL   , 'r'             },
   { "stop"         , KEYS_STOP         , 0         , 0               },
   { "save"         , KEYS_SAVE         , 0         , 0               },
   { "hide-panels"  , KEYS_HIDE_PANELS  , 0         , FL_Escape       },
   { "file-menu"    , KEYS_FILE_MENU    , FL_ALT    , 'f'             },
   { "close-all"    , KEYS_CLOSE_ALL    , FL_ALT    , 'q'             },
   { "back"         , KEYS_BACK         , 0         , FL_BackSpace    },
   { "back"         , KEYS_BACK         , 0         , ','             },
   { "forward"      , KEYS_FORWARD      , FL_SHIFT  , FL_BackSpace    },
   { "forward"      , KEYS_FORWARD      , 0         , '.'             },
   { "goto"         , KEYS_GOTO         , FL_CTRL   , 'l'             },
   { "home"         , KEYS_HOME         , FL_CTRL   , 'h'             },
   { "screen-up"    , KEYS_SCREEN_UP    , 0         , FL_Page_Up      },
   { "screen-up"    , KEYS_SCREEN_UP    , 0         , 'b'             },
   { "screen-down"  , KEYS_SCREEN_DOWN  , 0         , FL_Page_Down    },
   { "screen-down"  , KEYS_SCREEN_DOWN  , 0         , ' '             },
   { "line-up"      , KEYS_LINE_UP      , 0         , FL_Up           },
   { "line-down"    , KEYS_LINE_DOWN    , 0         , FL_Down         },
   { "left"         , KEYS_LEFT         , 0         , FL_Left         },
   { "right"        , KEYS_RIGHT        , 0         , FL_Right        },
   { "top"          , KEYS_TOP          , 0         , FL_Home         },
   { "bottom"       , KEYS_BOTTOM       , 0         , FL_End          },
};

static Dlist *bindings;



/*
 * Initialize the bindings list
 */
void Keys::init()
{
   KeyBinding_t *node;

   // Fill our key bindings list
   bindings = dList_new(32);
   for (uint_t i = 0; i < sizeof(default_keys) / sizeof(KeyBinding_t); i++) {
      if (default_keys[i].key) {
         node = dNew(KeyBinding_t, 1);
         node->name = dStrdup(default_keys[i].name);
         node->cmd = default_keys[i].cmd;
         node->modifier = default_keys[i].modifier;
         node->key = default_keys[i].key;
         dList_insert_sorted(bindings, node, nodeByKeyCmp);
      }
   }
}

/*
 * Free data
 */
void Keys::free()
{
   KeyBinding_t *node;

   while ((node = (KeyBinding_t*)dList_nth_data(bindings, 0))) {
      dFree((char*)node->name);
      dList_remove_fast(bindings, node);
      dFree(node);
   }
   dList_free(bindings);
}

/*
 * Compare function by {key,modifier} pairs.
 */
int Keys::nodeByKeyCmp(const void *node, const void *key)
{
   KeyBinding_t *n = (KeyBinding_t*)node, *k = (KeyBinding_t*)key;
   _MSG("Keys::nodeByKeyCmp modifier=%d\n", k->modifier);
   return (n->key != k->key) ? (n->key - k->key) : (n->modifier - k->modifier);
}

/*
 * Look if the just pressed key is bound to a command.
 * Return value: The command if found, KEYS_NOP otherwise.
 */
KeysCommand_t Keys::getKeyCmd()
{
   KeysCommand_t ret = KEYS_NOP;
   KeyBinding_t keyNode;
   // We're only interested in some flags
   keyNode.modifier = fltk::event_state() &
     (FL_SHIFT | FL_CTRL | FL_ALT | FL_META);

   if (keyNode.modifier == FL_SHIFT &&
       ispunct(fltk::event_text()[0])) {
      // Get key code for a shifted character
      keyNode.key = fltk::event_text()[0];
      keyNode.modifier = 0;
   } else {
      keyNode.key = fltk::event_key();
   }

   _MSG("getKeyCmd: key=%d, mod=%d\n", keyNode.key, keyNode.modifier);
   void *data = dList_find_sorted(bindings, &keyNode, nodeByKeyCmp);
   if (data)
      ret = ((KeyBinding_t*)data)->cmd;
   return ret;
}

/*
 * Remove a key binding from the table.
 */
void Keys::delKeyCmd(int key, int mod)
{
   KeyBinding_t keyNode, *node;
   keyNode.key = key;
   keyNode.modifier = mod;

   node = (KeyBinding_t*) dList_find_sorted(bindings, &keyNode, nodeByKeyCmp);
   if (node) {
      dList_remove(bindings, node);
      dFree((char*)node->name);
      dFree(node);
   }
}

/*
 * Takes a key name and looks it up in the mapping table. If
 * found, its key code is returned. Otherwise -1 is given
 * back.
 */
int Keys::getKeyCode(char *keyName)
{
   uint_t i;
   for (i = 0; i < sizeof(keyNames) / sizeof(Mapping_t); i++) {
      if (!dStrcasecmp(keyNames[i].name, keyName)) {
         return keyNames[i].value;
      }
   }

   return -1;
}

/*
 * Takes a command name and searches it in the mapping table.
 * Return value: command code if found, -1 otherwise
 */
KeysCommand_t Keys::getCmdCode(const char *commandName)
{
   uint_t i;

   for (i = 0; i < sizeof(default_keys) / sizeof(KeyBinding_t); i++) {
      if (!dStrcasecmp(default_keys[i].name, commandName))
         return default_keys[i].cmd;
   }
   return KEYS_INVALID;
}

/*
 * Takes a modifier name and looks it up in the mapping table. If
 * found, its key code is returned. Otherwise -1 is given back.
 */
int Keys::getModifier(char *modifierName)
{
   uint_t i;
   for (i = 0; i < sizeof(modifierNames) / sizeof(Mapping_t); i++) {
      if (!dStrcasecmp(modifierNames[i].name, modifierName)) {
         return modifierNames[i].value;
      }
   }

   return -1;
}

/*
 * Given a keys command, return a shortcut for it, or 0 if there is none
 * (e.g., for KEYS_NEW_WINDOW, return CTRL+'n').
 */
int Keys::getShortcut(KeysCommand_t cmd)
{
   int len = dList_length(bindings);

   for (int i = 0; i < len; i++) {
      KeyBinding_t *node = (KeyBinding_t*)dList_nth_data(bindings, i);
      if (cmd == node->cmd)
         return node->modifier + node->key;
   }
   return 0;
}

/*
 * Parse a key-combination/command-name pair, and
 * insert it into the bindings list.
 */
void Keys::parseKey(char *key, char *commandName)
{
   char *p, *modstr, *keystr;
   KeysCommand_t symcode;
   int st, keymod = 0, keycode = 0;

   _MSG("Keys::parseKey key='%s' commandName='%s'\n", key, commandName);

   // Get command code
   if ((symcode = getCmdCode(commandName)) == KEYS_INVALID) {
      MSG("Keys::parseKey: Invalid command name: '%s'\n", commandName);
      return;
   }

   // Skip space
   for (  ; isspace(*key); ++key) ;
   // Get modifiers
   while(*key == '<' && (p = strchr(key, '>'))) {
      ++key;
      modstr = dStrndup(key, p - key);
      if ((st = getModifier(modstr)) == -1) {
         MSG("Keys::parseKey unknown modifier: %s\n", modstr);
      } else {
         keymod |= st;
      }
      dFree(modstr);
      key = p + 1;
   }
   // Allow trailing space after keyname
   keystr = (*key && (p = strchr(key + 1, ' '))) ? dStrndup(key, p - key - 1) :
                                                   dStrdup(key);
   // Get key code
   if (!key[1]) {
      keycode = *key;
   } else if (key[0] == '0' && key[1] == 'x') {
      /* keysym. For details on values reported, see fltk's fltk/events.h */
      keycode = strtol(key, NULL, 0x10);
   } else if ((st = getKeyCode(keystr)) == -1) {
      MSG("Keys::parseKey unknown keyname: %s\n", keystr);
   } else {
      keycode = st;
   }
   dFree(keystr);

   // Set binding
   if (keycode) {
      delKeyCmd(keycode, keymod);
      if (symcode != KEYS_NOP) {
         KeyBinding_t *node = dNew(KeyBinding_t, 1);
         node->name = dStrdup(commandName);
         node->cmd = symcode;
         node->modifier = keymod;
         node->key = keycode;
         dList_insert_sorted(bindings, node, nodeByKeyCmp);
         _MSG("parseKey: Adding key=%d, mod=%d\n", node->key, node->modifier);
      }
   }
}

/*
 * Parse the keysrc.
 */
void Keys::parse(FILE *fp)
{
   char *line, *keycomb, *command;
   int st, lineno = 1;

   // scan the file line by line
   while ((line = dGetline(fp)) != NULL) {
      st = dParser_parse_rc_line(&line, &keycomb, &command);

      if (st == 0) {
         _MSG("Keys::parse: keycomb=%s, command=%s\n", keycomb, command);
         parseKey(keycomb, command);
      } else if (st < 0) {
         MSG("Keys::parse: Syntax error in keysrc line %d: "
             "keycomb=\"%s\" command=\"%s\"\n", lineno, keycomb, command);
      }

      dFree(line);
      ++lineno;
   }
   fclose(fp);
}
