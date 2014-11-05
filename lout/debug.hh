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

#endif /* __LOUT_DEBUG_H__ */


