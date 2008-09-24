#ifndef __LOUT_DEBUG_H__
#define __LOUT_DEBUG_H__

/*
 * Simple debug messages. Add:
 *
 *    #define DEBUG_LEVEL <n>
 *    #include "debug.h"
 *
 * to the file you are working on, or let DEBUG_LEVEL undefined to
 * disable all messages. A higher level denotes a greater importance
 * of the message.
 */

#include <stdio.h>

#define D_STMT_START      do
#define D_STMT_END        while (0)

# ifdef DEBUG_LEVEL
#    define DEBUG_MSG(level, ...)                     \
        D_STMT_START {                                \
           if (DEBUG_LEVEL && (level) >= DEBUG_LEVEL) \
              printf(__VA_ARGS__);                    \
        } D_STMT_END
# else
#    define DEBUG_MSG(level, ...)
# endif /* DEBUG_LEVEL */



/*
 * Following is experimental, and will be explained soon.
 */

#ifdef DBG_RTFL

#include <unistd.h>
#include <stdio.h>

#define DBG_MSG(obj, aspect, prio, msg) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:msg:%p:%s:%d:%s\n", \
              __FILE__, __LINE__, getpid(), obj, aspect, prio, msg); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_MSGF(obj, aspect, prio, fmt, ...) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:msg:%p:%s:%d:" fmt "\n", \
              __FILE__, __LINE__, getpid(), obj, aspect, prio, __VA_ARGS__); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_MSG_START(obj) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:msg-start:%p\n", \
              __FILE__, __LINE__, getpid(), obj); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_MSG_END(obj) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:msg-end:%p\n", \
              __FILE__, __LINE__, getpid(), obj); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_CREATE(obj, klass) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-create:%p:%s\n", \
              __FILE__, __LINE__, getpid(), obj, klass); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ASSOC(child, parent) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-assoc:%p:%p\n", \
              __FILE__, __LINE__, getpid(), child, parent); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_NUM(obj, var, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:%s:%d\n", \
              __FILE__, __LINE__, getpid(), obj, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_STR(obj, var, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:%s:%s\n", \
              __FILE__, __LINE__, getpid(), obj, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_PTR(obj, var, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:%s:%p\n", \
              __FILE__, __LINE__, getpid(), obj, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_NUM(obj, var, ind, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:" var ":%d\n", \
              __FILE__, __LINE__, getpid(), obj, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_STR(obj, var, ind, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:" var ":%s\n", \
              __FILE__, __LINE__, getpid(), obj, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_PTR(obj, var, ind, val) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-set:%p:" var ":%p\n", \
              __FILE__, __LINE__, getpid(), obj, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_COLOR(klass, color) \
   D_STMT_START { \
      printf ("[rtfl]%s:%d:%d:obj-color:%s:%s\n", \
              __FILE__, __LINE__, getpid(), klass, color); \
      fflush (stdout); \
   } D_STMT_END

#else /* DBG_RTFL */

#define DBG_MSG(obj, aspect, prio, msg)
#define DBG_MSGF(obj, aspect, prio, fmt, ...)
#define DBG_MSG_START(obj)
#define DBG_MSG_END(obj)
#define DBG_OBJ_CREATE(obj, klass)
#define DBG_OBJ_ASSOC(child, parent)
#define DBG_OBJ_SET_NUM(obj, var, val)
#define DBG_OBJ_SET_STR(obj, var, val)
#define DBG_OBJ_SET_PTR(obj, var, val)
#define DBG_OBJ_ARRSET_NUM(obj, var, ind, val)
#define DBG_OBJ_ARRSET_STR(obj, var, ind, val)
#define DBG_OBJ_ARRSET_PTR(obj, var, ind, val)
#define DBG_OBJ_COLOR(klass, color)

#endif /* DBG_RTFL */

#endif /* __LOUT_DEBUG_H__ */


