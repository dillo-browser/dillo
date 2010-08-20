#ifndef __MSG_H__
#define __MSG_H__

#include <stdio.h>
#include "prefs.h"

/*
 * You can disable any MSG* macro by adding the '_' prefix.
 */
#define _MSG(...)
#define _MSG_WARN(...)
#define _MSG_HTTP(...)

#define MSG_INNARDS(prefix, ...)                   \
   D_STMT_START {                                  \
      if (prefs.show_msg){                         \
         printf(prefix __VA_ARGS__);               \
         fflush (stdout);                          \
      }                                            \
   } D_STMT_END

#define MSG(...) MSG_INNARDS("", __VA_ARGS__)
#define MSG_WARN(...) MSG_INNARDS("** WARNING **: ", __VA_ARGS__)
#define MSG_ERR(...) MSG_INNARDS("** ERROR **: ", __VA_ARGS__)
#define MSG_HTTP(...) MSG_INNARDS("HTTP warning: ", __VA_ARGS__)

#endif /* __MSG_H__ */
