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

#include "debug_rtfl.hh"

/* Some extensions for RTFL dealing with static stuff. */

#ifdef DBG_RTFL

#define DBG_OBJ_MSG_S(aspect, prio, msg)         \
   RTFL_OBJ_PRINT ("msg", "s:s:d:s", "<static>", aspect, prio, msg)

#define DBG_OBJ_MSGF_S(aspect, prio, fmt, ...) \
   STMT_START { \
      char msg[256]; \
      snprintf (msg, sizeof (msg), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("msg", "s:s:d:s", "<static>", aspect, prio, msg) \
   } STMT_END

#define DBG_OBJ_ENTER0_S(aspect, prio, funname) \
   RTFL_OBJ_PRINT ("enter", "s:s:d:s:", "<static>", aspect, prio, funname);

#define DBG_OBJ_ENTER_S(aspect, prio, funname, fmt, ...) \
   STMT_START { \
      char args[256]; \
      snprintf (args, sizeof (args), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("enter", "s:s:d:s:s", "<static>", aspect, prio, funname, \
                      args); \
   } STMT_END

#define DBG_OBJ_LEAVE_S() \
   RTFL_OBJ_PRINT ("leave", "s", "<static>");

#define DBG_OBJ_LEAVE_VAL_S(fmt, ...) \
   STMT_START { \
      char vals[256]; \
      snprintf (vals, sizeof (vals), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("leave", "s:s", "<static>", vals); \
   } STMT_END

#else /* DBG_RTFL */

#define STMT_NOP         do { } while (0)

#define DBG_IF_RTFL if(0)

#define DBG_OBJ_MSG_S(aspect, prio, msg)                  STMT_NOP
#define DBG_OBJ_MSGF_S(aspect, prio, fmt, ...)            STMT_NOP
#define DBG_OBJ_ENTER0_S(aspect, prio, funname)           STMT_NOP
#define DBG_OBJ_ENTER_S(aspect, prio, funname, fmt, ...)  STMT_NOP
#define DBG_OBJ_LEAVE_S()                                 STMT_NOP
#define DBG_OBJ_LEAVE_VAL_S(fmt, ...)                     STMT_NOP

#endif /* DBG_RTFL */

#endif /* __LOUT_DEBUG_H__ */


