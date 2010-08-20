#ifndef __MSG_H__
#define __MSG_H__

#include <stdio.h>

#define prefs_show_msg    1

#define D_STMT_START      do
#define D_STMT_END        while (0)

/*
 * You can disable any MSG* macro by adding the '_' prefix.
 */
#define _MSG(...)
#define _MSG_WARN(...)
#define _MSG_ERR(...)


#define MSG(...)                                   \
   D_STMT_START {                                  \
      if (prefs_show_msg){                         \
         printf(__VA_ARGS__);                      \
         fflush (stdout);                          \
      }                                            \
   } D_STMT_END

#define MSG_WARN(...)                              \
   D_STMT_START {                                  \
      if (prefs_show_msg)                          \
         printf("** WARNING **: " __VA_ARGS__);    \
   } D_STMT_END

#define MSG_ERR(...)                               \
   D_STMT_START {                                  \
      if (prefs_show_msg)                          \
         printf("** ERROR **: " __VA_ARGS__);      \
   } D_STMT_END

#endif /* __MSG_H__ */
