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
 * See <http://www.dillo.org/~sgeerken/rtfl/>.
 */

#ifdef DBG_RTFL

#include <unistd.h>
#include <stdio.h>

// "\n" at the beginning just in case that the previous line is not finished
// yet.
#define RTFL_PREFIX_FMT  "\n[rtfl]%s:%d:%d:"
#define RTFL_PREFIX_ARGS __FILE__, __LINE__, getpid()

#define DBG_OBJ_MSG(aspect, prio, msg) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:%s\n", \
              RTFL_PREFIX_ARGS, this, aspect, prio, msg); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSGF(aspect, prio, fmt, ...) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:" fmt "\n", \
              RTFL_PREFIX_ARGS, this, aspect, prio, __VA_ARGS__); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSG_START() \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-start:%p\n", \
              RTFL_PREFIX_ARGS, this); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSG_END() \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-end:%p\n", \
              RTFL_PREFIX_ARGS, this); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_CREATE(klass) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-create:%p:%s\n", \
              RTFL_PREFIX_ARGS, this, klass); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_DELETE() \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-delete:%p\n", \
              RTFL_PREFIX_ARGS, this); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_BASECLASS(klass) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-ident:%p:%p\n", \
              RTFL_PREFIX_ARGS, this, (klass*)this); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ASSOC(parent, child) \
   D_STMT_START { \
      if (child) { \
         printf (RTFL_PREFIX_FMT "obj-assoc:%p:%p\n", \
                 RTFL_PREFIX_ARGS, parent, child); \
         fflush (stdout); \
      } \
   } D_STMT_END

#define DBG_OBJ_ASSOC_PARENT(parent) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-assoc:%p:%p\n", \
              RTFL_PREFIX_ARGS, parent, this); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ASSOC_CHILD(child) \
   D_STMT_START { \
      if (child) { \
         printf (RTFL_PREFIX_FMT "obj-assoc:%p:%p\n", \
                 RTFL_PREFIX_ARGS, this, child); \
         fflush (stdout); \
      } \
   } D_STMT_END

#define DBG_OBJ_SET_NUM(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%d\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_STR(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%s\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_PTR(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%p\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_NUM(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:" var ".%d:%d\n", \
              RTFL_PREFIX_ARGS, this, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_STR(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:" var ".%d:%s\n", \
              RTFL_PREFIX_ARGS, this, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_PTR(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:" var ".%d:%p\n", \
              RTFL_PREFIX_ARGS, this, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_COLOR(color, klass) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-color:%s:%s\n", \
              RTFL_PREFIX_ARGS, color, klass); \
      fflush (stdout); \
   } D_STMT_END

#else /* DBG_RTFL */

#define DBG_OBJ_MSG(aspect, prio, msg)
#define DBG_OBJ_MSGF(aspect, prio, fmt, ...)
#define DBG_OBJ_MSG_START()
#define DBG_OBJ_MSG_END()
#define DBG_OBJ_CREATE(klass)
#define DBG_OBJ_DELETE()
#define DBG_OBJ_BASECLASS(klass)
#define DBG_OBJ_ASSOC_PARENT(parent)
#define DBG_OBJ_ASSOC_CHILD(child)
#define DBG_OBJ_ASSOC(parent, child)
#define DBG_OBJ_SET_NUM(var, val)
#define DBG_OBJ_SET_STR(var, val)
#define DBG_OBJ_SET_PTR(var, val)
#define DBG_OBJ_ARRSET_NUM(var, ind, val)
#define DBG_OBJ_ARRSET_STR(var, ind, val)
#define DBG_OBJ_ARRSET_PTR(var, ind, val)
#define DBG_OBJ_COLOR(klass, color)

#endif /* DBG_RTFL */

#endif /* __LOUT_DEBUG_H__ */


