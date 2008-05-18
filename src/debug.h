#ifndef __DEBUG_H__
#define __DEBUG_H__

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

#include <unistd.h>
#include <stdio.h>


# ifdef DEBUG_LEVEL
#    define DEBUG_MSG(level, ...)                     \
        D_STMT_START {                                \
           if (DEBUG_LEVEL && (level) >= DEBUG_LEVEL) \
              printf(__VA_ARGS__);                    \
        } D_STMT_END
# else
#    define DEBUG_MSG(level, ...)
# endif /* DEBUG_LEVEL */

#endif /* __DEBUG_H__ */

