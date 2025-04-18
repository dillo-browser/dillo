// WARNING: This file has been generated. Do not edit!

/*
 * This file is part of RTFL, see <http://home.gna.org/rtfl/>
 * for details.
 *
 * This file (but not RTFL itself) is in the public domain, since it is only a
 * simple implementation of a protocol, containing nothing more than trivial
 * work. However, it would be nice to keep this notice, along with the URL
 * above.
 *
 * ----------------------------------------------------------------------------
 *
 * Defines macros for printing RTFL commands. See documentation for detail
 * (online at <http://home.gna.org/rtfl/doc/rtfl.html>). These macros are only
 * active, when the pre-processor variable DBG_RTFL is defined. If not,
 * alternatives are defined, which have no effect.
 *
 * This variant assumes that __FILE__ is only the base of the source file name,
 * so, to get the full path, CUR_WORKING_DIR has to be defined. See RTFL
 * documentation for more details.
 */

#ifndef __DEBUG_RTFL_HH__
#define __DEBUG_RTFL_HH__

#ifdef DBG_RTFL

// =======================================
//           Used by all modules
// =======================================

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>

#define DBG_IF_RTFL if(1)

#define STMT_START       do
#define STMT_END         while (0)

// Prints an RTFL message to stdout. "fmt" contains simple format
// characters how to deal with the additional arguments (no "%"
// preceding, as in printf) or "q" (which additionally
// (double-)quotes quotation marks) or "c" (short for "#%06x" and used
// for colors), or other characters, which are simply printed. No
// quoting: this function cannot be used to print the characters "d",
// "p", "s" and "q" directly.

inline void rtfl_print (const char *module, const char *version,
                        const char *file, int line, const char *fmt, ...)
{
   // "\n" at the beginning just in case that the previous line is not
   // finished yet.
   printf ("\n[rtfl-%s-%s]%s:%d:%d:", module, version, file, line, getpid ());

   va_list args;
   va_start (args, fmt);

   for (int i = 0; fmt[i]; i++) {
      int n;
      void *p;
      char *s;

      switch (fmt[i]) {
      case 'd':
         n = va_arg(args, int);
         printf ("%d", n);
         break;

      case 'p':
         p = va_arg(args, void*);
         printf ("%p", p);
         break;

      case 's':
         s = va_arg (args, char*);
         for (int j = 0; s[j]; j++) {
            if (s[j] == ':' || s[j] == '\\')
               putchar ('\\');
            putchar (s[j]);
         }
         break;

      case 'q':
         s = va_arg (args, char*);
         for (int j = 0; s[j]; j++) {
            if (s[j] == ':' || s[j] == '\\')
               putchar ('\\');
            else if (s[j] == '\"')
               printf ("\\\\"); // a quoted quoting character
            putchar (s[j]);
         }
         break;

      case 'c':
         n = va_arg(args, int);
         printf ("#%06x", n);
         break;

      default:
         putchar (fmt[i]);
         break;
      }
   }

   va_end (args);

   putchar ('\n');
   fflush (stdout);
}

#define RTFL_PRINT(module, version, cmd, fmt, ...) \
   rtfl_print (module, version, CUR_WORKING_DIR "/" __FILE__, __LINE__, \
               "s:" fmt, cmd, __VA_ARGS__)


// ==================================
//           General module
// ==================================

#define RTFL_GEN_VERSION "1.0"

#define RTFL_GEN_PRINT(cmd, fmt, ...) \
   RTFL_PRINT ("gen", RTFL_GEN_VERSION, cmd, fmt, __VA_ARGS__)

#define DBG_GEN_TIME() \
   STMT_START { \
      struct timeval tv; \
      gettimeofday(&tv, NULL); \
      char buf[32]; \
      snprintf (buf, sizeof (buf), "%ld%06ld", tv.tv_sec, tv.tv_usec); \
      RTFL_GEN_PRINT ("time", "s", buf); \
   } STMT_END


// ==================================
//           Objects module
// ==================================

#define RTFL_OBJ_VERSION "1.0"

#define RTFL_OBJ_PRINT(cmd, fmt, ...) \
   RTFL_PRINT ("obj", RTFL_OBJ_VERSION, cmd, fmt, __VA_ARGS__)

#define DBG_OBJ_MSG(aspect, prio, msg) \
   DBG_OBJ_MSG_O (aspect, prio, this, msg)

#define DBG_OBJ_MSG_O(aspect, prio, obj, msg) \
   RTFL_OBJ_PRINT ("msg", "p:s:d:s", obj, aspect, prio, msg)

#define DBG_OBJ_MSGF(aspect, prio, fmt, ...) \
   STMT_START { \
      char msg[256]; \
      snprintf (msg, sizeof (msg), fmt, __VA_ARGS__); \
      DBG_OBJ_MSG (aspect, prio, msg); \
   } STMT_END

#define DBG_OBJ_MSGF_O(aspect, prio, obj, fmt, ...) \
   STMT_START { \
      char msg[256]; \
      snprintf (msg, sizeof (msg), fmt, __VA_ARGS__); \
      DBG_OBJ_MSG_O (aspect, prio, obj, msg); \
   } STMT_END

#define DBG_OBJ_MARK(aspect, prio, mark) \
   DBG_OBJ_MARK_O (aspect, prio, this, mark)

#define DBG_OBJ_MARK_O(aspect, prio, obj, mark) \
   RTFL_OBJ_PRINT ("mark", "p:s:d:s", obj, aspect, prio, mark)

#define DBG_OBJ_MARKF(aspect, prio, fmt, ...) \
   STMT_START { \
      char mark[256]; \
      snprintf (mark, sizeof (mark), fmt, __VA_ARGS__); \
      DBG_OBJ_MARK (aspect, prio, mark); \
   } STMT_END

#define DBG_OBJ_MARKF_O(aspect, prio, obj, fmt, ...) \
   STMT_START { \
      char mark[256]; \
      snprintf (mark, sizeof (mark), fmt, __VA_ARGS__); \
      DBG_OBJ_MARK_O (aspect, prio, obj, mark); \
   } STMT_END

#define DBG_OBJ_MSG_START() \
   DBG_OBJ_MSG_START_O (this)

#define DBG_OBJ_MSG_START_O(obj) \
   RTFL_OBJ_PRINT ("msg-start", "p", obj)

#define DBG_OBJ_MSG_END() \
   DBG_OBJ_MSG_END_O (this)

#define DBG_OBJ_MSG_END_O(obj) \
   RTFL_OBJ_PRINT ("msg-end", "p", obj)

#define DBG_OBJ_ENTER0(aspect, prio, funname) \
   DBG_OBJ_ENTER0_O (aspect, prio, this, funname)

#define DBG_OBJ_ENTER0_O(aspect, prio, obj, funname) \
   RTFL_OBJ_PRINT ("enter", "p:s:d:s:", obj, aspect, prio, funname);

#define DBG_OBJ_ENTER(aspect, prio, funname, fmt, ...) \
   STMT_START { \
      char args[256]; \
      snprintf (args, sizeof (args), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("enter", "p:s:d:s:s", this, aspect, prio, funname, \
                      args); \
   } STMT_END

#define DBG_OBJ_ENTER_O(aspect, prio, obj, funname, fmt, ...) \
   STMT_START { \
      char args[256]; \
      snprintf (args, sizeof (args), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("enter", "p:s:d:s:s", obj, aspect, prio, funname, \
                      args); \
   } STMT_END

#define DBG_OBJ_LEAVE() \
   DBG_OBJ_LEAVE_O (this)

#define DBG_OBJ_LEAVE_O(obj) \
   RTFL_OBJ_PRINT ("leave", "p", obj);

#define DBG_OBJ_LEAVE_VAL(fmt, ...) \
   STMT_START { \
      char vals[256]; \
      snprintf (vals, sizeof (vals), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("leave", "p:s", this, vals); \
   } STMT_END

#define DBG_OBJ_LEAVE_VAL_O(obj, fmt, ...) \
   STMT_START { \
      char vals[256]; \
      snprintf (vals, sizeof (vals), fmt, __VA_ARGS__); \
      RTFL_OBJ_PRINT ("leave", "p:s", obj, vals); \
   } STMT_END

#define DBG_OBJ_LEAVE_VAL0(val) \
   DBG_OBJ_LEAVE_VAL0_O (this, val)

#define DBG_OBJ_LEAVE_VAL0_O(obj, val) \
   RTFL_OBJ_PRINT ("leave", "p:s:", obj, val)

#define DBG_OBJ_CREATE(klass) \
   DBG_OBJ_CREATE_O (this, klass)

#define DBG_OBJ_CREATE_O(obj, klass) \
   RTFL_OBJ_PRINT ("create", "p:s", obj, klass);

#define DBG_OBJ_DELETE() \
   DBG_OBJ_DELETE_O (this)

#define DBG_OBJ_DELETE_O(obj) \
   RTFL_OBJ_PRINT ("delete", "p", obj);

#define DBG_OBJ_BASECLASS(klass) \
   RTFL_OBJ_PRINT ("ident", "p:p", this, (klass*)this);

#define DBG_OBJ_ASSOC(parent, child) \
   RTFL_OBJ_PRINT ("assoc", "p:p", parent, child); \

#define DBG_OBJ_ASSOC_PARENT(parent) \
   DBG_OBJ_ASSOC (parent, this);

#define DBG_OBJ_ASSOC_CHILD(child) \
   DBG_OBJ_ASSOC (this, child);

#define DBG_OBJ_SET_NUM(var, val) \
   DBG_OBJ_SET_NUM_O (this, var, val)

#define DBG_OBJ_SET_NUM_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:d", obj, var, val)

#define DBG_OBJ_SET_SYM(var, val) \
   DBG_OBJ_SET_SYM_O (this, var, val)

#define DBG_OBJ_SET_SYM_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:s", obj, var, val)

#define DBG_OBJ_SET_BOOL(var, val) \
   DBG_OBJ_SET_BOOL_O (this, var, val)

#define DBG_OBJ_SET_BOOL_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:s", obj, var, (val) ? "true" : "false")

#define DBG_OBJ_SET_STR(var, val) \
   DBG_OBJ_SET_STR_O (this, var, val)

#define DBG_OBJ_SET_STR_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:\"q\"", obj, var, val)

#define DBG_OBJ_SET_PTR(var, val) \
   DBG_OBJ_SET_PTR_O (this, var, val)

#define DBG_OBJ_SET_PTR_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:p", obj, var, val)

#define DBG_OBJ_SET_COL(var, val) \
   DBG_OBJ_SET_COL_O (this, var, val)

#define DBG_OBJ_SET_COL_O(obj, var, val) \
   RTFL_OBJ_PRINT ("set", "p:s:c", obj, var, val)

#define DBG_OBJ_ARRSET_NUM(var, ind, val) \
   DBG_OBJ_ARRSET_NUM_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_NUM_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:d", obj, var, ind, val)

#define DBG_OBJ_ARRSET_SYM(var, ind, val) \
   DBG_OBJ_ARRSET_SYM_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_SYM_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:s", obj, var, ind, val)

#define DBG_OBJ_ARRSET_BOOL(var, ind, val) \
   DBG_OBJ_ARRSET_BOOL_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_BOOL_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:s", obj, var, ind, (val) ? "true" : "false")

#define DBG_OBJ_ARRSET_STR(var, ind, val) \
   DBG_OBJ_ARRSET_STR_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_STR_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:\"q\"", obj, var, ind, val)

#define DBG_OBJ_ARRSET_PTR(var, ind, val) \
   DBG_OBJ_ARRSET_PTR_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_PTR_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:p", obj, var, ind, val)

#define DBG_OBJ_ARRSET_COL(var, ind, val) \
   DBG_OBJ_ARRSET_COL_O (this, var, ind, val)

#define DBG_OBJ_ARRSET_COL_O(obj, var, ind, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d:c", obj, var, ind, val)

#define DBG_OBJ_ARRATTRSET_NUM(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_NUM_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_NUM_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:d", obj, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_SYM(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_SYM_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_SYM_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:s", obj, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_BOOL(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_BOOL_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_BOOL_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:s", obj, var, ind, attr, \
                   (val) ? "true" : "false")

#define DBG_OBJ_ARRATTRSET_STR(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_STR_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_STR_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:\"q\"", obj, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_PTR(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_PTR_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_PTR_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:p", obj, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_COL(var, ind, attr, val) \
   DBG_OBJ_ARRATTRSET_COL_O (this, var, ind, attr, val)

#define DBG_OBJ_ARRATTRSET_COL_O(obj, var, ind, attr, val) \
   RTFL_OBJ_PRINT ("set", "p:s.d.s:c", obj, var, ind, attr, val)

#define DBG_OBJ_CLASS_COLOR(klass, color) \
   RTFL_OBJ_PRINT ("class-color", "s:s", klass, color)

#else /* DBG_RTFL */

#define STMT_NOP         do { } while (0)

#define DBG_IF_RTFL if(0)

#define DBG_GEN_TIME()                                         STMT_NOP
#define DBG_OBJ_MSG(aspect, prio, msg)                         STMT_NOP
#define DBG_OBJ_MSG_O(aspect, prio, obj, msg)                  STMT_NOP
#define DBG_OBJ_MSGF(aspect, prio, fmt, ...)                   STMT_NOP
#define DBG_OBJ_MSGF_O(aspect, prio, obj, fmt, ...)            STMT_NOP
#define DBG_OBJ_MARK(aspect, prio, mark)                       STMT_NOP
#define DBG_OBJ_MARK_O(aspect, prio, obj, mark)                STMT_NOP
#define DBG_OBJ_MARKF(aspect, prio, fmt, ...)                  STMT_NOP
#define DBG_OBJ_MARKF_O(aspect, prio, obj, fmt, ...)           STMT_NOP
#define DBG_OBJ_MSG_START()                                    STMT_NOP
#define DBG_OBJ_MSG_START_O(obj)                               STMT_NOP
#define DBG_OBJ_MSG_END()                                      STMT_NOP
#define DBG_OBJ_MSG_END_O(obj)                                 STMT_NOP
#define DBG_OBJ_ENTER0(aspect, prio, funname)                  STMT_NOP
#define DBG_OBJ_ENTER0_O(aspect, prio, obj, funname)           STMT_NOP
#define DBG_OBJ_ENTER(aspect, prio, funname, fmt, ...)         STMT_NOP
#define DBG_OBJ_ENTER_O(aspect, prio, obj, funname, fmt, ...)  STMT_NOP
#define DBG_OBJ_LEAVE()                                        STMT_NOP
#define DBG_OBJ_LEAVE_O(obj)                                   STMT_NOP
#define DBG_OBJ_LEAVE_VAL(fmt, ...)                            STMT_NOP
#define DBG_OBJ_LEAVE_VAL_O(obj, fmt, ...)                     STMT_NOP
#define DBG_OBJ_LEAVE_VAL0(val)                                STMT_NOP
#define DBG_OBJ_LEAVE_VAL0_O(obj, val)                         STMT_NOP
#define DBG_OBJ_CREATE(klass)                                  STMT_NOP
#define DBG_OBJ_CREATE_O(obj, klass)                           STMT_NOP
#define DBG_OBJ_DELETE()                                       STMT_NOP
#define DBG_OBJ_DELETE_O(obj)                                  STMT_NOP
#define DBG_OBJ_BASECLASS(klass)                               STMT_NOP
#define DBG_OBJ_ASSOC(parent, child)                           STMT_NOP
#define DBG_OBJ_ASSOC_PARENT(parent)                           STMT_NOP
#define DBG_OBJ_ASSOC_CHILD(child)                             STMT_NOP
#define DBG_OBJ_SET_NUM(var, val)                              STMT_NOP
#define DBG_OBJ_SET_NUM_O(obj, var, val)                       STMT_NOP
#define DBG_OBJ_SET_SYM(var, val)                              STMT_NOP
#define DBG_OBJ_SET_SYM_O(obj, var, val)                       STMT_NOP
#define DBG_OBJ_SET_BOOL(var, val)                             STMT_NOP
#define DBG_OBJ_SET_BOOL_O(obj, var, val)                      STMT_NOP
#define DBG_OBJ_SET_STR(var, val)                              STMT_NOP
#define DBG_OBJ_SET_STR_O(obj, var, val)                       STMT_NOP
#define DBG_OBJ_SET_PTR(var, val)                              STMT_NOP
#define DBG_OBJ_SET_PTR_O(obj, var, val)                       STMT_NOP
#define DBG_OBJ_SET_COL(var, val)                              STMT_NOP
#define DBG_OBJ_SET_COL_O(obj, var, val)                       STMT_NOP
#define DBG_OBJ_ARRSET_NUM(var, ind, val)                      STMT_NOP
#define DBG_OBJ_ARRSET_NUM_O(obj, var, ind, val)               STMT_NOP
#define DBG_OBJ_ARRSET_SYM(var, ind, val)                      STMT_NOP
#define DBG_OBJ_ARRSET_SYM_O(obj, var, ind, val)               STMT_NOP
#define DBG_OBJ_ARRSET_BOOL(var, ind, val)                     STMT_NOP
#define DBG_OBJ_ARRSET_BOOL_O(obj, var, ind, val)              STMT_NOP
#define DBG_OBJ_ARRSET_STR(var, ind, val)                      STMT_NOP
#define DBG_OBJ_ARRSET_STR_O(obj, var, ind, val)               STMT_NOP
#define DBG_OBJ_ARRSET_PTR(var, ind, val)                      STMT_NOP
#define DBG_OBJ_ARRSET_PTR_O(obj, var, ind, val)               STMT_NOP
#define DBG_OBJ_ARRSET_COL(var, ind, val)                      STMT_NOP
#define DBG_OBJ_ARRSET_COL_O(obj, var, ind, val)               STMT_NOP
#define DBG_OBJ_ARRATTRSET_NUM(var, ind, attr, val)            STMT_NOP
#define DBG_OBJ_ARRATTRSET_NUM_O(obj, var, ind, attr, val)     STMT_NOP
#define DBG_OBJ_ARRATTRSET_SYM(var, ind, attr, val)            STMT_NOP
#define DBG_OBJ_ARRATTRSET_SYM_O(obj, var, ind, attr, val)     STMT_NOP
#define DBG_OBJ_ARRATTRSET_BOOL(var, ind, attr, val)           STMT_NOP
#define DBG_OBJ_ARRATTRSET_BOOL_O(obj, var, ind, attr, val)    STMT_NOP
#define DBG_OBJ_ARRATTRSET_STR(var, ind, attr, val)            STMT_NOP
#define DBG_OBJ_ARRATTRSET_STR_O(obj, var, ind, attr, val)     STMT_NOP
#define DBG_OBJ_ARRATTRSET_PTR(var, ind, attr, val)            STMT_NOP
#define DBG_OBJ_ARRATTRSET_PTR_O(obj, var, ind, attr, val)     STMT_NOP
#define DBG_OBJ_ARRATTRSET_COL(var, ind, attr, val)            STMT_NOP
#define DBG_OBJ_ARRATTRSET_COL_O(obj, var, ind, attr, val)     STMT_NOP
#define DBG_OBJ_CLASS_COLOR(klass, color)                      STMT_NOP

#endif /* DBG_RTFL */

#endif /* __DEBUG_RTFL_HH__ */
