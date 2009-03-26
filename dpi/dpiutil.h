/*
 * File: dpiutil.h
 *
 * Copyright 2004-2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * This file contains common functions used by dpi programs.
 * (i.e. a convenience library).
 */

#ifndef __DPIUTIL_H__
#define __DPIUTIL_H__

#include <stdio.h>
#include "d_size.h"
#include "../dlib/dlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define BUFLEN 256
#define TOUT 300


/* Streamed Sockets API (not mandatory)  ----------------------------------*/

typedef struct _SockHandler SockHandler;
struct _SockHandler {
   int fd_in;
   int fd_out;
   /* FILE *in;    --Unused. The stream functions block when reading. */
   FILE *out;

   char *buf;     /* internal buffer */
   uint_t buf_sz;   /* data size */
   uint_t buf_max;  /* allocated size */
   uint_t flush_sz; /* max size before flush */
};

SockHandler *sock_handler_new(int fd_in, int fd_out, int flush_sz);
int sock_handler_write(SockHandler *sh, int flush,
                       const char *Data,size_t DataSize);
int sock_handler_write_str(SockHandler *sh, int flush, const char *str);
char *sock_handler_read(SockHandler *sh);
void sock_handler_close(SockHandler *sh);
void sock_handler_free(SockHandler *sh);

#define sock_handler_printf(sh, flush, ...)                 \
   D_STMT_START {                                           \
      Dstr *dstr = dStr_sized_new(128);                     \
      dStr_sprintf(dstr, __VA_ARGS__);                      \
      sock_handler_write(sh, flush, dstr->str, dstr->len);  \
      dStr_free(dstr, 1);                                   \
   } D_STMT_END

/* ----------------------------------------------------------------------- */

/*
 * Escape URI characters in 'esc_set' as %XX sequences.
 * Return value: New escaped string.
 */
char *Escape_uri_str(const char *str, const char *p_esc_set);

/*
 * Escape unsafe characters as html entities.
 * Return value: New escaped string.
 */
char *Escape_html_str(const char *str);

/*
 * Unescape a few HTML entities (inverse of Escape_html_str)
 * Return value: New unescaped string.
 */
char *Unescape_html_str(const char *str);

/*
 * Filter an SMTP hack with a FTP URI
 */
char *Filter_smtp_hack(char *url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DPIUTIL_H__ */

