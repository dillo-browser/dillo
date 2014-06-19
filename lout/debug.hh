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

#define D_STMT_NOP        D_STMT_START { } D_STMT_END

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
 * See <http://home.gna.org/rtfl/>.
 */

#ifdef DBG_RTFL

#include <unistd.h>
#include <stdio.h>

#define DBG_IF_RTFL if(1)

// "\n" at the beginning just in case that the previous line is not finished
// yet.
#define RTFL_PREFIX_FMT  "\n[rtfl]%s:%d:%d:"
#define RTFL_PREFIX_ARGS CUR_WORKING_DIR "/" __FILE__, __LINE__, getpid()

#define DBG_OBJ_MSG(aspect, prio, msg) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:%s\n", \
              RTFL_PREFIX_ARGS, this, aspect, prio, msg); \
      fflush (stdout); \
   } D_STMT_END

// Variant which does not use "this", but an explicitly passed
// object. Should be applied to other macros. (Also, for use in C.)
#define DBG_OBJ_MSG_O(aspect, prio, obj, msg) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:%s\n", \
              RTFL_PREFIX_ARGS, obj, aspect, prio, msg); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSGF(aspect, prio, fmt, ...) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:" fmt "\n", \
              RTFL_PREFIX_ARGS, this, aspect, prio, __VA_ARGS__); \
      fflush (stdout); \
   } D_STMT_END

// See DBG_OBJ_MSG_O.
#define DBG_OBJ_MSGF_O(aspect, prio, obj, fmt, ...) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg:%p:%s:%d:" fmt "\n", \
              RTFL_PREFIX_ARGS, obj, aspect, prio, __VA_ARGS__); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSG_START() \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-start:%p\n", \
              RTFL_PREFIX_ARGS, this); \
      fflush (stdout); \
   } D_STMT_END

// See DBG_OBJ_MSG_O.
#define DBG_OBJ_MSG_START_O(obj) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-start:%p\n", \
              RTFL_PREFIX_ARGS, obj); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_MSG_END() \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-end:%p\n", \
              RTFL_PREFIX_ARGS, this); \
      fflush (stdout); \
   } D_STMT_END

// See DBG_OBJ_MSG_O.
#define DBG_OBJ_MSG_END_O(obj) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-msg-end:%p\n", \
              RTFL_PREFIX_ARGS, obj); \
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

#define DBG_OBJ_SET_NUM_O(obj, var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%d\n", \
              RTFL_PREFIX_ARGS, obj, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_SYM(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%s\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_STR(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:\"%s\"\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_PTR(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%p\n", \
              RTFL_PREFIX_ARGS, this, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_BOOL(var, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%s\n", \
              RTFL_PREFIX_ARGS, this, var, val ? "true" : "false");     \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_SET_PTR_O(obj, var, val)         \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s:%p\n", \
              RTFL_PREFIX_ARGS, obj, var, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_NUM(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d:%d\n", \
              RTFL_PREFIX_ARGS, this, var, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_SYM(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d:%s\n", \
              RTFL_PREFIX_ARGS, this, var, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_STR(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d:\"%s\"\n", \
              RTFL_PREFIX_ARGS, this, var, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRSET_PTR(var, ind, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d:%p\n", \
              RTFL_PREFIX_ARGS, this, var, ind, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRATTRSET_NUM(var, ind, attr, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d.%s:%d\n", \
              RTFL_PREFIX_ARGS, this, var, ind, attr, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRATTRSET_SYM(var, ind, attr, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d.%s:%s\n", \
              RTFL_PREFIX_ARGS, this, var, ind, attr, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRATTRSET_STR(var, ind, attr, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d.%s:\"%s\"\n", \
              RTFL_PREFIX_ARGS, this, var, ind, attr, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_ARRATTRSET_PTR(var, ind, attr, val) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-set:%p:%s.%d.%s:%p\n", \
              RTFL_PREFIX_ARGS, this, var, ind, attr, val); \
      fflush (stdout); \
   } D_STMT_END

#define DBG_OBJ_COLOR(color, klass) \
   D_STMT_START { \
      printf (RTFL_PREFIX_FMT "obj-color:%s:%s\n", \
              RTFL_PREFIX_ARGS, color, klass); \
      fflush (stdout); \
   } D_STMT_END

#else /* DBG_RTFL */

#define DBG_IF_RTFL if(0)

#define DBG_OBJ_MSG(aspect, prio, msg)               D_STMT_NOP
#define DBG_OBJ_MSG_O(aspect, prio, obj, msg)        D_STMT_NOP
#define DBG_OBJ_MSGF(aspect, prio, fmt, ...)         D_STMT_NOP
#define DBG_OBJ_MSGF_O(aspect, prio, obj, fmt, ...)  D_STMT_NOP
#define DBG_OBJ_MSG_START()                          D_STMT_NOP
#define DBG_OBJ_MSG_START_O(obj)                     D_STMT_NOP
#define DBG_OBJ_MSG_END()                            D_STMT_NOP
#define DBG_OBJ_MSG_END_O(obj)                       D_STMT_NOP
#define DBG_OBJ_CREATE(klass)                        D_STMT_NOP
#define DBG_OBJ_DELETE()                             D_STMT_NOP
#define DBG_OBJ_BASECLASS(klass)                     D_STMT_NOP
#define DBG_OBJ_ASSOC_PARENT(parent)                 D_STMT_NOP
#define DBG_OBJ_ASSOC_CHILD(child)                   D_STMT_NOP
#define DBG_OBJ_ASSOC(parent, child)                 D_STMT_NOP
#define DBG_OBJ_SET_NUM(var, val)                    D_STMT_NOP
#define DBG_OBJ_SET_NUM_O(obj, var, val)             D_STMT_NOP
#define DBG_OBJ_SET_SYM(var, val)                    D_STMT_NOP
#define DBG_OBJ_SET_STR(var, val)                    D_STMT_NOP
#define DBG_OBJ_SET_PTR(var, val)                    D_STMT_NOP
#define DBG_OBJ_SET_BOOL(var, val)                   D_STMT_NOP
#define DBG_OBJ_SET_PTR_O(obj, var, val)             D_STMT_NOP
#define DBG_OBJ_ARRSET_NUM(var, ind, val)            D_STMT_NOP
#define DBG_OBJ_ARRSET_SYM(var, ind, val)            D_STMT_NOP
#define DBG_OBJ_ARRSET_STR(var, ind, val)            D_STMT_NOP
#define DBG_OBJ_ARRSET_PTR(var, ind, val)            D_STMT_NOP
#define DBG_OBJ_ARRATTRSET_NUM(var, ind, attr, val)  D_STMT_NOP
#define DBG_OBJ_ARRATTRSET_SYM(var, ind, attr, val)  D_STMT_NOP
#define DBG_OBJ_ARRATTRSET_STR(var, ind, attr, val)  D_STMT_NOP
#define DBG_OBJ_ARRATTRSET_PTR(var, ind, attr, val)  D_STMT_NOP
#define DBG_OBJ_COLOR(klass, color)                  D_STMT_NOP

#endif /* DBG_RTFL */

#endif /* __LOUT_DEBUG_H__ */


