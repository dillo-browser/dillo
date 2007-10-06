#ifndef __MSG_H__
#define __MSG_H__

#include <stdio.h>
#include "prefs.h"

/*
 * You can disable any MSG* macro by adding the '_' prefix.
 */
#define _MSG(...)
#define _MSG_WARN(...)
#define _MSG_HTML(...)
#define _MSG_HTTP(...)


#define MSG(...)                                   \
   D_STMT_START {                                  \
      if (prefs.show_msg)                          \
         printf(__VA_ARGS__);                      \
   } D_STMT_END

#define MSG_WARN(...)                              \
   D_STMT_START {                                  \
      if (prefs.show_msg)                          \
         printf("** WARNING **: " __VA_ARGS__);    \
   } D_STMT_END

#define MSG_ERR(...)                               \
   D_STMT_START {                                  \
      if (prefs.show_msg)                          \
         printf("** ERROR **: " __VA_ARGS__);      \
   } D_STMT_END

#define MSG_HTML(...)                              \
   D_STMT_START {                                  \
         Html_msg(html, __VA_ARGS__);              \
   } D_STMT_END

#define MSG_HTTP(...)                              \
   printf("HTTP warning: " __VA_ARGS__)

#endif /* __MSG_H__ */
