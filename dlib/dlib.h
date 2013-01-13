#ifndef __DLIB_H__
#define __DLIB_H__

#include <stdio.h>     /* for FILE*  */
#include <stddef.h>    /* for size_t */
#include <stdarg.h>    /* for va_list */
#include <string.h>    /* for strerror */

#include "d_size.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 *-- Common macros -----------------------------------------------------------
 */
#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

/* Handle signed char */
#define dIsspace(c) isspace((uchar_t)(c))
#define dIsalnum(c) isalnum((uchar_t)(c))

#define D_ASCII_TOUPPER(c) (((c) >= 'a' && (c) <= 'z') ? (c) - 0x20 : (c))
#define D_ASCII_TOLOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c) + 0x20 : (c))
/*
 *-- Casts -------------------------------------------------------------------
 */
/* TODO: include a void* size test in configure.in */
/* (long) works for both 32bit and 64bit */
#define VOIDP2INT(p)    ((long)(p))
#define INT2VOIDP(i)    ((void*)((long)(i)))

/*
 *-- Memory -------------------------------------------------------------------
 */
#define dNew(type, count) \
   ((type *) dMalloc ((unsigned) sizeof (type) * (count)))
#define dNew0(type, count) \
   ((type *) dMalloc0 ((unsigned) sizeof (type) * (count)))

void *dMalloc (size_t size);
void *dRealloc (void *mem, size_t size);
void *dMalloc0 (size_t size);
void dFree (void *mem);

/*
 *- Debug macros --------------------------------------------------------------
 */
#define D_STMT_START      do
#define D_STMT_END        while (0)
#define dReturn_if(expr)               \
   D_STMT_START{                       \
      if (expr) { return; };           \
   }D_STMT_END
#define dReturn_val_if(expr,val)       \
   D_STMT_START{                       \
      if (expr) { return val; };       \
   }D_STMT_END
#define dReturn_if_fail(expr)          \
   D_STMT_START{                       \
      if (!(expr)) { return; };        \
   }D_STMT_END
#define dReturn_val_if_fail(expr,val)  \
   D_STMT_START{                       \
      if (!(expr)) { return val; };    \
   }D_STMT_END

/*
 *- C strings -----------------------------------------------------------------
 */
char *dStrdup(const char *s);
char *dStrndup(const char *s, size_t sz);
char *dStrconcat(const char *s1, ...);
char *dStrstrip(char *s);
char *dStrnfill(size_t len, char c);
char *dStrsep(char **orig, const char *delim);
void dStrshred(char *s);
char *dStriAsciiStr(const char *haystack, const char *needle);
int dStrAsciiCasecmp(const char *s1, const char *s2);
int dStrnAsciiCasecmp(const char *s1, const char *s2, size_t n);

#define dStrerror strerror

/*
 *-- dStr ---------------------------------------------------------------------
 */
#define Dstr_char_t    char

typedef struct {
   int sz;          /* allocated size (private) */
   int len;
   Dstr_char_t *str;
} Dstr;

Dstr *dStr_new (const char *s);
Dstr *dStr_sized_new (int sz);
void dStr_fit (Dstr *ds);
void dStr_free (Dstr *ds, int all);
void dStr_append_c (Dstr *ds, int c);
void dStr_append (Dstr *ds, const char *s);
void dStr_append_l (Dstr *ds, const char *s, int l);
void dStr_insert (Dstr *ds, int pos_0, const char *s);
void dStr_insert_l (Dstr *ds, int pos_0, const char *s, int l);
void dStr_truncate (Dstr *ds, int len);
void dStr_shred (Dstr *ds);
void dStr_erase (Dstr *ds, int pos_0, int len);
void dStr_vsprintfa (Dstr *ds, const char *format, va_list argp);
void dStr_vsprintf (Dstr *ds, const char *format, va_list argp);
void dStr_sprintf (Dstr *ds, const char *format, ...);
void dStr_sprintfa (Dstr *ds, const char *format, ...);
int  dStr_cmp(Dstr *ds1, Dstr *ds2);
char *dStr_memmem(Dstr *haystack, Dstr *needle);
const char *dStr_printable(Dstr *in, int maxlen);

/*
 *-- dList --------------------------------------------------------------------
 */
typedef struct {
   int sz;          /* allocated size (private) */
   int len;
   void **list;
} Dlist;

/* dCompareFunc:
 * Return: 0 if parameters are equal (for dList_find_custom).
 * Return: 0 if equal,  < 0 if (a < b),  > 0 if (b < a) --for insert sorted.
 *
 * For finding a data node with an external key, the comparison function
 * parameters are: first the data node, and then the key.
 */
typedef int (*dCompareFunc) (const void *a, const void *b);


Dlist *dList_new(int size);
void dList_free (Dlist *lp);
void dList_append (Dlist *lp, void *data);
void dList_prepend (Dlist *lp, void *data);
void dList_insert_pos (Dlist *lp, void *data, int pos0);
int  dList_length (Dlist *lp);
void dList_remove (Dlist *lp, const void *data);
void dList_remove_fast (Dlist *lp, const void *data);
void *dList_nth_data (Dlist *lp, int n0);
void *dList_find (Dlist *lp, const void *data);
int  dList_find_idx (Dlist *lp, const void *data);
void *dList_find_custom (Dlist *lp, const void *data, dCompareFunc func);
void dList_sort (Dlist *lp, dCompareFunc func);
void dList_insert_sorted (Dlist *lp, void *data, dCompareFunc func);
void *dList_find_sorted (Dlist *lp, const void *data, dCompareFunc func);

/*
 *- Parse function ------------------------------------------------------------
 */
int dParser_parse_rc_line(char **line, char **name, char **value);

/*
 *- Dlib messages -------------------------------------------------------------
 */
void dLib_show_messages(bool_t show);

/*
 *- Misc utility functions ----------------------------------------------------
 */
char *dGetcwd();
char *dGethomedir();
char *dGetline(FILE *stream);
int dClose(int fd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DLIB_H__ */

