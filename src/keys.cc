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
   int cmd, modifier, key;
} KeyBinding_t;

typedef struct {
   const char *name;
   const int value;
} Mapping_t;


/*
 *  Local data
 */
static const Mapping_t keyNames[] = {
   { "Backspace",   fltk::BackSpaceKey },
   { "Delete",      fltk::DeleteKey    },
   { "Down",        fltk::DownKey      },
   { "End",         fltk::EndKey       },
   { "Esc",         fltk::EscapeKey    },
   { "F1",          fltk::F1Key        },
   { "F2",          fltk::F2Key        },
   { "F3",          fltk::F3Key        },
   { "F4",          fltk::F4Key        },
   { "F5",          fltk::F5Key        },
   { "F6",          fltk::F6Key        },
   { "F7",          fltk::F7Key        },
   { "F8",          fltk::F8Key        },
   { "F9",          fltk::F9Key        },
   { "F10",         fltk::F10Key       },
   { "F11",         fltk::F11Key       },
   { "F12",         fltk::F12Key       },
   { "Home",        fltk::HomeKey      },
   { "Insert",      fltk::InsertKey    },
   { "Left",        fltk::LeftKey      },
   { "PageDown",    fltk::PageDownKey  },
   { "PageUp",      fltk::PageUpKey    },
   { "Print",       fltk::PrintKey     },
   { "Return",      fltk::ReturnKey    },
   { "Right",       fltk::RightKey     },
   { "Space",       fltk::SpaceKey     },
   { "Tab",         fltk::TabKey       },
   { "Up",          fltk::UpKey        }
};

static const Mapping_t modifierNames[] = {
   { "Shift",   fltk::SHIFT   },
   { "Ctrl",    fltk::CTRL    },
   { "Alt",     fltk::ALT     },
   { "Meta",    fltk::META    },
   { "Button1", fltk::BUTTON1 },
   { "Button2", fltk::BUTTON2 },
   { "Button3", fltk::BUTTON3 }
};

static const KeyBinding_t default_keys[] = {
   { "open"         , KEYS_OPEN         , fltk::CTRL   , 'o'                },
   { "new-window"   , KEYS_NEW_WINDOW   , fltk::CTRL   , 'n'                },
   { "new-tab"      , KEYS_NEW_TAB      , fltk::CTRL   , 't'                },
   { "left-tab"     , KEYS_LEFT_TAB     , fltk::SHIFT  , fltk::TabKey       },
   { "right-tab"    , KEYS_RIGHT_TAB    , fltk::CTRL   , fltk::TabKey       },
   { "close-tab"    , KEYS_CLOSE_TAB    , fltk::CTRL   , 'q'                },
   { "find"         , KEYS_FIND         , fltk::CTRL   , 'f'                },
   { "websearch"    , KEYS_WEBSEARCH    , fltk::CTRL   , 's'                },
   { "bookmarks"    , KEYS_BOOKMARKS    , fltk::CTRL   , 'b'                },
   { "fullscreen"   , KEYS_FULLSCREEN   , fltk::CTRL   , fltk::SpaceKey     },
   { "reload"       , KEYS_RELOAD       , fltk::CTRL   , 'r'                },
   { "hide-panels"  , KEYS_HIDE_PANELS  , 0            , fltk::EscapeKey    },
   { "file-menu"    , KEYS_FILE_MENU    , fltk::ALT    , 'f'                },
   { "close-all"    , KEYS_CLOSE_ALL    , fltk::ALT    , 'q'                },
   { "back"         , KEYS_BACK         , 0            , fltk::BackSpaceKey },
   { "back"         , KEYS_BACK         , 0            , ','                },
   { "forward"      , KEYS_FORWARD      , fltk::SHIFT  , fltk::BackSpaceKey },
   { "forward"      , KEYS_FORWARD      , 0            , '.'                },
   { "goto"         , KEYS_GOTO         , fltk::CTRL   , 'l'                },
   { "home"         , KEYS_HOME         , fltk::CTRL   , 'h'                }
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
      node = dNew(KeyBinding_t, 1);
      node->name = dStrdup(default_keys[i].name);
      node->cmd = default_keys[i].cmd;
      node->modifier = default_keys[i].modifier;
      node->key = default_keys[i].key;
      dList_insert_sorted(bindings, node, nodeByKeyCmp);
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
int Keys::getKeyCmd()
{
   int ret = KEYS_NOP;
   KeyBinding_t keyNode;

   if (fltk::event_state() == fltk::SHIFT &&
       ispunct(fltk::event_text()[0])) {
      // Get key code for a shifted character
      keyNode.key = fltk::event_text()[0];
      keyNode.modifier = 0;
   } else {
      keyNode.key = fltk::event_key();
      keyNode.modifier = fltk::event_state();
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
int Keys::getCmdCode(const char *commandName)
{
   uint_t i;

   for (i = 0; i < sizeof(default_keys) / sizeof(KeyBinding_t); i++) {
      if (!dStrcasecmp(default_keys[i].name, commandName))
         return default_keys[i].cmd;
   }
   return -1;
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
 * Parse a key-combination/command-name pair, and
 * insert it into the bindings list.
 */
void Keys::parseKey(char *key, char *commandName)
{
   char *p, *modstr, *keystr;
   int st, symcode = 0, keymod = 0, keycode = 0;

   _MSG("Keys::parseKey key='%s' commandName='%s'\n", key, commandName);

   // Get command code
   if ((st = getCmdCode(commandName)) == -1) {
      MSG("Keys::parseKey: Invalid command name: '%s'\n", commandName);
      return;
   } else
      symcode = st;

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
