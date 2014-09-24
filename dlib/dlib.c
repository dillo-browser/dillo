/*
 * File: dlib.c
 *
 * Copyright (C) 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/* Memory allocation, Simple dynamic strings, Lists (simple and sorted),
 * and a few utility functions
 */

/*
 * TODO: vsnprintf() is in C99, maybe a simple replacement if necessary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "dlib.h"

static bool_t dLib_show_msg = TRUE;

/* dlib msgs go to stderr to avoid problems with filter dpis */
#define DLIB_MSG(...)                              \
   D_STMT_START {                                  \
      if (dLib_show_msg)                           \
         fprintf(stderr, __VA_ARGS__);             \
   } D_STMT_END

/*
 *- Memory --------------------------------------------------------------------
 */

void *dMalloc (size_t size)
{
  void *value = malloc (size);
  if (value == 0)
     exit(1);
  return value;
}

void *dRealloc (void *mem, size_t size)
{
  void *value = realloc (mem, size);
  if (value == 0)
     exit(1);
  return value;
}

void *dMalloc0 (size_t size)
{
  void *value = dMalloc (size);
  memset (value, 0, size);
  return value;
}

void dFree (void *mem)
{
   free(mem);
}

/*
 *- strings (char *) ----------------------------------------------------------
 */

char *dStrdup(const char *s)
{
   if (s) {
      int len = strlen(s)+1;
      char *ns = dNew(char, len);
      memcpy(ns, s, len);
      return ns;
   }
   return NULL;
}

char *dStrndup(const char *s, size_t sz)
{
   if (s) {
      char *ns = dNew(char, sz+1);
      memcpy(ns, s, sz);
      ns[sz] = 0;
      return ns;
   }
   return NULL;
}

/*
 * Concatenate a NULL-terminated list of strings
 */
char *dStrconcat(const char *s1, ...)
{
   va_list args;
   char *s, *ns = NULL;

   if (s1) {
      Dstr *dstr = dStr_sized_new(64);
      va_start(args, s1);
      for (s = (char*)s1; s; s = va_arg(args, char*))
         dStr_append(dstr, s);
      va_end(args);
      ns = dstr->str;
      dStr_free(dstr, 0);
   }
   return ns;
}

/*
 * Remove leading and trailing whitespace
 */
char *dStrstrip(char *s)
{
   char *p;
   int len;

   if (s && *s) {
      for (p = s; dIsspace(*p); ++p);
      for (len = strlen(p); len && dIsspace(p[len-1]); --len);
      if (p > s)
         memmove(s, p, len);
      s[len] = 0;
   }
   return s;
}

/*
 * Clear the contents of the string
 */
void dStrshred(char *s)
{
   if (s)
      memset(s, 0, strlen(s));
}

/*
 * Return a new string of length 'len' filled with 'c' characters
 */
char *dStrnfill(size_t len, char c)
{
   char *ret = dNew(char, len+1);
   for (ret[len] = 0; len > 0; ret[--len] = c);
   return ret;
}

/*
 * strsep() implementation
 */
char *dStrsep(char **orig, const char *delim)
{
   char *str, *p;

   if (!(str = *orig))
      return NULL;

   p = strpbrk(str, delim);
   if (p) {
      *p++ = 0;
      *orig = p;
   } else {
      *orig = NULL;
   }
   return str;
}

/*
 * ASCII functions to avoid the case difficulties introduced by I/i in
 * Turkic locales.
 */

/*
 * Case insensitive strstr
 */
char *dStriAsciiStr(const char *haystack, const char *needle)
{
   int i, j;
   char *ret = NULL;

   if (haystack && needle) {
      for (i = 0, j = 0; haystack[i] && needle[j]; ++i)
         if (D_ASCII_TOLOWER(haystack[i]) == D_ASCII_TOLOWER(needle[j])) {
            ++j;
         } else if (j) {
            i -= j;
            j = 0;
         }
      if (!needle[j])                 /* Got all */
         ret = (char *)(haystack + i - j);
   }
   return ret;
}

int dStrAsciiCasecmp(const char *s1, const char *s2)
{
   int ret = 0;

   while ((*s1 || *s2) &&
          !(ret = D_ASCII_TOLOWER(*s1) - D_ASCII_TOLOWER(*s2))) {
      s1++;
      s2++;
   }
   return ret;
}

int dStrnAsciiCasecmp(const char *s1, const char *s2, size_t n)
{
   int ret = 0;

   while (n-- && (*s1 || *s2) &&
          !(ret = D_ASCII_TOLOWER(*s1) - D_ASCII_TOLOWER(*s2))) {
      s1++;
      s2++;
   }
   return ret;
}

/*
 *- dStr ----------------------------------------------------------------------
 */

/*
 * Private allocator
 */
static void dStr_resize(Dstr *ds, int n_sz, int keep)
{
   if (n_sz >= 0) {
      if (keep && n_sz > ds->len) {
         ds->str = (Dstr_char_t*) dRealloc (ds->str, n_sz*sizeof(Dstr_char_t));
         ds->sz = n_sz;
      } else {
         dFree(ds->str);
         ds->str = dNew(Dstr_char_t, n_sz);
         ds->sz = n_sz;
         ds->len = 0;
         ds->str[0] = 0;
      }
   }
}

/*
 * Create a new string with a given size.
 * Initialized to ""
 */
Dstr *dStr_sized_new (int sz)
{
   Dstr *ds;
   if (sz < 2)
      sz = 2;

   ds = dNew(Dstr, 1);
   ds->str = NULL;
   dStr_resize(ds, sz + 1, 0); /* (sz + 1) for the extra '\0' */
   return ds;
}

/*
 * Return memory if there's too much allocated
 */
void dStr_fit (Dstr *ds)
{
   dStr_resize(ds, ds->len + 1, 1);
}

/*
 * Insert a C string, at a given position, into a Dstr (providing length).
 * Note: It also works with embedded nil characters.
 */
void dStr_insert_l (Dstr *ds, int pos_0, const char *s, int l)
{
   int n_sz;

   if (ds && s && l && pos_0 >= 0 && pos_0 <= ds->len) {
      for (n_sz = ds->sz; ds->len + l >= n_sz; n_sz *= 2);
      if (n_sz > ds->sz) {
         dStr_resize(ds, n_sz, (ds->len > 0) ? 1 : 0);
      }
      if (pos_0 < ds->len)
         memmove(ds->str+pos_0+l, ds->str+pos_0, ds->len-pos_0);
      memcpy(ds->str+pos_0, s, l);
      ds->len += l;
      ds->str[ds->len] = 0;
   }
}

/*
 * Insert a C string, at a given position, into a Dstr
 */
void dStr_insert (Dstr *ds, int pos_0, const char *s)
{
   if (s)
      dStr_insert_l(ds, pos_0, s, strlen(s));
}

/*
 * Append a C string to a Dstr (providing length).
 * Note: It also works with embedded nil characters.
 */
void dStr_append_l (Dstr *ds, const char *s, int l)
{
   dStr_insert_l(ds, ds->len, s, l);
}

/*
 * Append a C string to a Dstr.
 */
void dStr_append (Dstr *ds, const char *s)
{
   dStr_append_l(ds, s, strlen(s));
}

/*
 * Create a new string.
 * Initialized to 's' or empty if 's == NULL'
 */
Dstr *dStr_new (const char *s)
{
   Dstr *ds = dStr_sized_new(0);
   if (s && *s)
      dStr_append(ds, s);
   return ds;
}

/*
 * Free a dillo string.
 * if 'all' free everything, else free the structure only.
 */
void dStr_free (Dstr *ds, int all)
{
   if (ds) {
      if (all)
         dFree(ds->str);
      dFree(ds);
   }
}

/*
 * Append one character.
 */
void dStr_append_c (Dstr *ds, int c)
{
   char cs[2];

   if (ds) {
      if (ds->sz > ds->len + 1) {
         ds->str[ds->len++] = (Dstr_char_t)c;
         ds->str[ds->len] = 0;
      } else {
         cs[0] = (Dstr_char_t)c;
         cs[1] = 0;
         dStr_append_l (ds, cs, 1);
      }
   }
}

/*
 * Truncate a Dstr to be 'len' bytes long.
 */
void dStr_truncate (Dstr *ds, int len)
{
   if (ds && len < ds->len) {
      ds->str[len] = 0;
      ds->len = len;
   }
}

/*
 * Clear a Dstr.
 */
void dStr_shred (Dstr *ds)
{
   if (ds && ds->sz > 0)
      memset(ds->str, '\0', ds->sz);
}

/*
 * Erase a substring.
 */
void dStr_erase (Dstr *ds, int pos_0, int len)
{
   if (ds && pos_0 >= 0 && len > 0 && pos_0 + len <= ds->len) {
      memmove(ds->str + pos_0, ds->str + pos_0 + len, ds->len - pos_0 - len);
      ds->len -= len;
      ds->str[ds->len] = 0;
   }
}

/*
 * vsprintf-like function that appends.
 * Used by: dStr_vsprintf(), dStr_sprintf() and dStr_sprintfa().
 */
void dStr_vsprintfa (Dstr *ds, const char *format, va_list argp)
{
   int n, n_sz;

   if (ds && format) {
      va_list argp2;         /* Needed in case of looping on non-32bit arch */
      while (1) {
         va_copy(argp2, argp);
         n = vsnprintf(ds->str + ds->len, ds->sz - ds->len, format, argp2);
         va_end(argp2);
#if defined(__sgi)
         /* IRIX does not conform to C99; if the entire argument did not fit
          * into the buffer, n = buffer space used (minus 1 for terminator)
          */
         if (n > -1 && n + 1 < ds->sz - ds->len) {
            ds->len += n;      /* Success! */
            break;
         } else {
            n_sz = ds->sz * 2;
         }
#else
         if (n > -1 && n < ds->sz - ds->len) {
            ds->len += n;      /* Success! */
            break;
         } else if (n > -1) {  /* glibc >= 2.1 */
            n_sz = ds->len + n + 1;
         } else {              /* old glibc */
            n_sz = ds->sz * 2;
         }
#endif
         dStr_resize(ds, n_sz, (ds->len > 0) ? 1 : 0);
      }
   }
}

/*
 * vsprintf-like function.
 */
void dStr_vsprintf (Dstr *ds, const char *format, va_list argp)
{
   if (ds) {
      dStr_truncate(ds, 0);
      dStr_vsprintfa(ds, format, argp);
   }
}

/*
 * Printf-like function
 */
void dStr_sprintf (Dstr *ds, const char *format, ...)
{
   va_list argp;

   if (ds && format) {
      va_start(argp, format);
      dStr_vsprintf(ds, format, argp);
      va_end(argp);
   }
}

/*
 * Printf-like function that appends.
 */
void dStr_sprintfa (Dstr *ds, const char *format, ...)
{
   va_list argp;

   if (ds && format) {
      va_start(argp, format);
      dStr_vsprintfa(ds, format, argp);
      va_end(argp);
   }
}

/*
 * Compare two dStrs.
 */
int dStr_cmp(Dstr *ds1, Dstr *ds2)
{
   int ret = 0;

   if (ds1 && ds2)
      ret = memcmp(ds1->str, ds2->str, MIN(ds1->len+1, ds2->len+1));
   return ret;
}

/*
 * Return a pointer to the first occurrence of needle in haystack.
 */
char *dStr_memmem(Dstr *haystack, Dstr *needle)
{
   int i;

   if (needle && haystack) {
      if (needle->len == 0)
         return haystack->str;

      for (i = 0; i <= (haystack->len - needle->len); i++) {
         if (haystack->str[i] == needle->str[0] &&
             !memcmp(haystack->str + i, needle->str, needle->len))
            return haystack->str + i;
      }
   }
   return NULL;
}

/*
 * Return a printable representation of the provided Dstr, limited to a length
 * of roughly maxlen.
 *
 * This is NOT threadsafe.
 */
const char *dStr_printable(Dstr *in, int maxlen)
{
   int i;
   static const char *const HEX = "0123456789ABCDEF";
   static Dstr *out = NULL;

   if (in == NULL)
      return NULL;

   if (out)
      dStr_truncate(out, 0);
   else
      out = dStr_sized_new(in->len);

   for (i = 0; (i < in->len) && (out->len < maxlen); ++i) {
      if (isprint(in->str[i]) || (in->str[i] == '\n')) {
         dStr_append_c(out, in->str[i]);
      } else {
         dStr_append_l(out, "\\x", 2);
         dStr_append_c(out, HEX[(in->str[i] >> 4) & 15]);
         dStr_append_c(out, HEX[in->str[i] & 15]);
      }
   }
   if (out->len >= maxlen)
      dStr_append(out, "...");
   return out->str;
}

/*
 *- dList ---------------------------------------------------------------------
 */

/*
 * Create a new empty list
 */
Dlist *dList_new(int size)
{
   Dlist *l;
   if (size <= 0)
      return NULL;

   l = dNew(Dlist, 1);
   l->len = 0;
   l->sz = size;
   l->list = dNew(void*, l->sz);
   return l;
}

/*
 * Free a list (not its elements)
 */
void dList_free (Dlist *lp)
{
   if (!lp)
      return;

   dFree(lp->list);
   dFree(lp);
}

/*
 * Insert an element at a given position [0 based]
 */
void dList_insert_pos (Dlist *lp, void *data, int pos0)
{
   int i;

   if (!lp || pos0 < 0 || pos0 > lp->len)
      return;

   if (lp->sz == lp->len) {
      lp->sz *= 2;
      lp->list = (void**) dRealloc (lp->list, lp->sz*sizeof(void*));
   }
   ++lp->len;

   for (i = lp->len - 1; i > pos0; --i)
      lp->list[i] = lp->list[i - 1];
   lp->list[pos0] = data;
}

/*
 * Append a data item to the list
 */
void dList_append (Dlist *lp, void *data)
{
   dList_insert_pos(lp, data, lp->len);
}

/*
 * Prepend a data item to the list
 */
void dList_prepend (Dlist *lp, void *data)
{
   dList_insert_pos(lp, data, 0);
}

/*
 * For completing the ADT.
 */
int dList_length (Dlist *lp)
{
   if (!lp)
      return 0;
   return lp->len;
}

/*
 * Remove a data item without preserving order.
 */
void dList_remove_fast (Dlist *lp, const void *data)
{
   int i;

   if (!lp)
      return;

   for (i = 0; i < lp->len; ++i) {
      if (lp->list[i] == data) {
         lp->list[i] = lp->list[--lp->len];
         break;
      }
   }
}

/*
 * Remove a data item preserving order.
 */
void dList_remove (Dlist *lp, const void *data)
{
   int i, j;

   if (!lp)
      return;

   for (i = 0; i < lp->len; ++i) {
      if (lp->list[i] == data) {
         --lp->len;
         for (j = i; j < lp->len; ++j)
            lp->list[j] = lp->list[j + 1];
         break;
      }
   }
}

/*
 * Return the nth data item,
 * NULL when not found or 'n0' is out of range
 */
void *dList_nth_data (Dlist *lp, int n0)
{
   if (!lp || n0 < 0 || n0 >= lp->len)
      return NULL;
   return lp->list[n0];
}

/*
 * Return the found data item, or NULL if not present.
 */
void *dList_find (Dlist *lp, const void *data)
{
   int i = dList_find_idx(lp, data);
   return (i >= 0) ? lp->list[i] : NULL;
}

/*
 * Search a data item.
 * Return value: its index if found, -1 if not present.
 * (this is useful for a list of integers, for finding number zero).
 */
int dList_find_idx (Dlist *lp, const void *data)
{
   int i, ret = -1;

   if (!lp)
      return ret;

   for (i = 0; i < lp->len; ++i) {
      if (lp->list[i] == data) {
         ret = i;
         break;
      }
   }
   return ret;
}

/*
 * Search a data item using a custom function.
 * func() is given the list item and the user data as parameters.
 * Return: data item when found, NULL otherwise.
 */
void *dList_find_custom (Dlist *lp, const void *data, dCompareFunc func)
{
   int i;
   void *ret = NULL;

   if (!lp)
      return ret;

   for (i = 0; i < lp->len; ++i) {
      if (func(lp->list[i], data) == 0) {
         ret = lp->list[i];
         break;
      }
   }
   return ret;
}

/*
 * QuickSort implementation.
 * This allows for a simple compare function for all the ADT.
 */
static void QuickSort(void **left, void **right, dCompareFunc compare)
{
  void **p = left, **q = right, **t = left;

  while (1) {
     while (p != t && compare(*p, *t) < 0)
        ++p;
     while (q != t && compare(*q, *t) > 0)
        --q;
     if (p > q)
        break;
     if (p < q) {
        void *tmp = *p;
        *p = *q;
        *q = tmp;
        if (t == p)
           t = q;
        else if (t == q)
           t = p;
     }
     if (++p > --q)
        break;
  }

  if (left < q)
     QuickSort(left, q, compare);
  if (p < right)
     QuickSort(p, right, compare);
}

/*
 * Sort the list using a custom function
 */
void dList_sort (Dlist *lp, dCompareFunc func)
{
   if (lp && lp->len > 1) {
      QuickSort(lp->list, lp->list + lp->len - 1, func);
   }
}

/*
 * Insert an element into a sorted list.
 * The comparison function receives two list elements.
 */
void dList_insert_sorted (Dlist *lp, void *data, dCompareFunc func)
{
   int i, st, min, max;

   if (lp) {
      min = st = i = 0;
      max = lp->len - 1;
      while (min <= max) {
         i = (min + max) / 2;
         st = func(lp->list[i], data);
         if (st < 0) {
            min = i + 1;
         } else if (st > 0) {
            max = i - 1;
         } else {
            break;
         }
      }
      dList_insert_pos(lp, data, (st >= 0) ? i : i+1);
   }
}

/*
 * Search a sorted list.
 * Return the found data item, or NULL if not present.
 * func() is given the list item and the user data as parameters.
 */
void *dList_find_sorted (Dlist *lp, const void *data, dCompareFunc func)
{
   int i, st, min, max;
   void *ret = NULL;

   if (lp && lp->len) {
      min = 0;
      max = lp->len - 1;
      while (min <= max) {
         i = (min + max) / 2;
         st = func(lp->list[i], data);
         if (st < 0) {
            min = i + 1;
         } else if (st > 0) {
            max = i - 1;
         } else {
            ret = lp->list[i];
            break;
         }
      }
   }

   return ret;
}

/*
 *- Parse function ------------------------------------------------------------
 */

/*
 * Take a dillo rc line and return 'name' and 'value' pointers to it.
 * Notes:
 *    - line is modified!
 *    - it skips blank lines and lines starting with '#'
 *
 * Return value: 1 on blank line or comment, 0 on successful value/pair,
 *              -1 otherwise.
 */
int dParser_parse_rc_line(char **line, char **name, char **value)
{
   char *eq, *p;
   int len, ret = -1;

   dReturn_val_if_fail(*line, ret);

   *name = NULL;
   *value = NULL;
   dStrstrip(*line);
   if (!*line[0] || *line[0] == '#') {
      /* blank line or comment */
      ret = 1;
   } else if ((eq = strchr(*line, '='))) {
      /* get name */
      for (p = *line; *p && *p != '=' && !dIsspace(*p); ++p);
      *p = 0;
      *name = *line;

      /* skip whitespace */
      if (p < eq)
         for (++p; dIsspace(*p); ++p);

      /* get value */
      if (p == eq) {
         for (++p; dIsspace(*p); ++p);
         len = strlen(p);
         if (len >= 2 && *p == '"' && p[len-1] == '"') {
            p[len-1] = 0;
            ++p;
         }
         *value = p;
         ret = 0;
      }
   }

   return ret;
}

/*
 *- Dlib messages -------------------------------------------------------------
 */
void dLib_show_messages(bool_t show)
{
   dLib_show_msg = show;
}

/*
 *- Misc utility functions ----------------------------------------------------
 */

/*
 * Return the current working directory in a new string
 */
char *dGetcwd ()
{
  size_t size = 128;

  while (1) {
      char *buffer = dNew(char, size);
      if (getcwd (buffer, size) == buffer)
        return buffer;
      dFree (buffer);
      if (errno != ERANGE)
        return 0;
      size *= 2;
  }
}

/*
 * Return the home directory in a static string (don't free)
 */
char *dGethomedir ()
{
   static char *homedir = NULL;

   if (!homedir) {
      if (getenv("HOME")) {
         homedir = dStrdup(getenv("HOME"));

      } else if (getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
         homedir = dStrconcat(getenv("HOMEDRIVE"), getenv("HOMEPATH"), NULL);
      } else {
         DLIB_MSG("dGethomedir: $HOME not set, using '/'.\n");
         homedir = dStrdup("/");
      }
   }
   return homedir;
}

/*
 * Get a line from a FILE stream.
 * Return value: read line on success, NULL on EOF.
 */
char *dGetline (FILE *stream)
{
   int ch;
   Dstr *dstr;
   char *line;

   dReturn_val_if_fail (stream, 0);

   dstr = dStr_sized_new(64);
   while ((ch = fgetc(stream)) != EOF) {
      dStr_append_c(dstr, ch);
      if (ch == '\n')
         break;
   }

   line = (dstr->len) ? dstr->str : NULL;
   dStr_free(dstr, (line) ? 0 : 1);
   return line;
}

/*
 * Close a FD handling EINTR.
 */
int dClose(int fd)
{
   int st;

   do
      st = close(fd);
   while (st == -1 && errno == EINTR);
   return st;
}
