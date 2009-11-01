/*
 * File: dpip.c
 *
 * Copyright 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>   /* for close */
#include <fcntl.h>    /* for fcntl */

#include "dpip.h"
#include "d_size.h"

//#define RBUF_SZ 16384
#define RBUF_SZ 1

#define DPIP_TAG_END            " '>"
#define DPIP_MODE_SWITCH_TAG    "cmd='start_send_page' "
#define MSG_ERR(...)            fprintf(stderr, "[dpip]: " __VA_ARGS__)

/*
 * Local variables
 */
static const char Quote = '\'';
static const int DpipTag = 1;

/*
 * Basically the syntax of a dpip tag is:
 *
 *  "<"[*alpha] *(<name>"="Quote<escaped_value>Quote) " "Quote">"
 *
 *   1.- No space is allowed around the "=" sign between a name and its value.
 *   2.- The Quote character is not allowed in <name>.
 *   3.- Attribute values stuff Quote as QuoteQuote.
 *
 * e.g. (with ' as Quote):
 *
 *   <a='b' b='c' '>                 OK
 *   <dpi a='b i' b='12' '>          OK
 *   <a='>' '>                       OK
 *   <a='ain''t no doubt' '>         OK
 *   <a='ain''t b=''no'' b='' doubt' '>    OK
 *   <a = '>' '>                     Wrong
 *
 * Notes:
 *
 *   Restriction #1 is for easy finding of end of tag (EOT=Space+Quote+>).
 *   Restriction #2 can be removed, but what for? ;)
 *   The functions here provide for this functionality.
 */

typedef enum {
   SEEK_NAME,
   MATCH_NAME,
   SKIP_VALUE,
   SKIP_QUOTE,
   FOUND
} DpipTagParsingState;

/* ------------------------------------------------------------------------- */

/*
 * Printf like function for building dpip commands.
 * It takes care of dpip escaping of its arguments.
 * NOTE : It ONLY accepts string parameters, and
 *        only one %s per parameter.
 */
char *a_Dpip_build_cmd(const char *format, ...)
{
   va_list argp;
   char *p, *q, *s;
   Dstr *cmd;

   /* Don't allow Quote characters in attribute names */
   if (strchr(format, Quote))
      return NULL;

   cmd = dStr_sized_new(64);
   dStr_append_c(cmd, '<');
   va_start(argp, format);
   for (p = q = (char*)format; *q;  ) {
      p = strstr(q, "%s");
      if (!p) {
         dStr_append(cmd, q);
         break;
      } else {
         /* Copy format's part */
         while (q != p)
            dStr_append_c(cmd, *q++);
         q += 2;

         dStr_append_c(cmd, Quote);
         /* Stuff-copy of argument */
         s = va_arg (argp, char *);
         for (  ; *s; ++s) {
            dStr_append_c(cmd, *s);
            if (*s == Quote)
               dStr_append_c(cmd, *s);
         }
         dStr_append_c(cmd, Quote);
      }
   }
   va_end(argp);
   dStr_append_c(cmd, ' ');
   dStr_append_c(cmd, Quote);
   dStr_append_c(cmd, '>');

   p = cmd->str;
   dStr_free(cmd, FALSE);
   return p;
}

/*
 * Task: given a tag, its size and an attribute name, return the
 * attribute value (stuffing of ' is removed here).
 *
 * Return value: the attribute value, or NULL if not present or malformed.
 */
char *a_Dpip_get_attr_l(const char *tag, size_t tagsize, const char *attrname)
{
   uint_t i, n = 0, found = 0;
   const char *p, *q, *start;
   char *r, *s, *val = NULL;
   DpipTagParsingState state = SEEK_NAME;

   if (!tag || !tagsize || !attrname || !*attrname)
      return NULL;

   for (i = 1; i < tagsize && !found; ++i) {
      switch (state) {
      case SEEK_NAME:
         if (tag[i] == attrname[0] && (tag[i-1] == ' ' || tag[i-1] == '<')) {
            n = 1;
            state = MATCH_NAME;
         } else if (tag[i] == Quote && tag[i-1] == '=')
            state = SKIP_VALUE;
         break;
      case MATCH_NAME:
         if (tag[i] == attrname[n])
            ++n;
         else if (tag[i] == '=' && !attrname[n])
            state = FOUND;
         else
            state = SEEK_NAME;
         break;
      case SKIP_VALUE:
         if (tag[i] == Quote)
            state = (tag[i+1] == Quote) ? SKIP_QUOTE : SEEK_NAME;
         break;
      case SKIP_QUOTE:
         state = SKIP_VALUE;
         break;
      case FOUND:
         found = 1;
         break;
      }
   }

   if (found) {
      p = start = tag + i;
      while ((q = strchr(p, Quote)) && q[1] == Quote)
         p = q + 2;
      if (q && q[1] == ' ') {
         val = dStrndup(start, (uint_t)(q - start));
         for (r = s = val; (*r = *s); ++r, ++s)
            if (s[0] == Quote && s[0] == s[1])
               ++s;
      }
   }
   return val;
}

/*
 * Task: given a tag and an attribute name, return its value.
 * Return value: the attribute value, or NULL if not present or malformed.
 */
char *a_Dpip_get_attr(const char *tag, const char *attrname)
{
   return (tag ? a_Dpip_get_attr_l(tag, strlen(tag), attrname) : NULL);
}

/*
 * Check whether the given 'auth' string equals what dpid saved.
 * Return value: 1 if equal, -1 otherwise
 */
int a_Dpip_check_auth(const char *auth_tag)
{
   char SharedSecret[32];
   FILE *In;
   char *fname, *rcline = NULL, *tail, *cmd, *msg;
   int i, port, ret = -1;

   /* sanity checks */
   if (!auth_tag ||
       !(cmd = a_Dpip_get_attr(auth_tag, "cmd")) || strcmp(cmd, "auth") ||
       !(msg = a_Dpip_get_attr(auth_tag, "msg"))) {
      return ret;
   }

   fname = dStrconcat(dGethomedir(), "/.dillo/dpid_comm_keys", NULL);
   if ((In = fopen(fname, "r")) == NULL) {
      MSG_ERR("[a_Dpip_check_auth] %s\n", dStrerror(errno));
   } else if ((rcline = dGetline(In)) == NULL) {
      MSG_ERR("[a_Dpip_check_auth] empty file: %s\n", fname);
   } else {
      port = strtol(rcline, &tail, 10);
      for (i = 0; *tail && isxdigit(tail[i+1]); ++i)
         SharedSecret[i] = tail[i+1];
      SharedSecret[i] = 0;
      if (strcmp(msg, SharedSecret) == 0)
         ret = 1;
   }
   if (In)
      fclose(In);
   dFree(rcline);
   dFree(fname);
   dFree(msg);
   dFree(cmd);

   return ret;
}

/* --------------------------------------------------------------------------
 * Dpip socket API ----------------------------------------------------------
 */

/*
 * Create and initialize a dpip socket handler
 */
Dsh *a_Dpip_dsh_new(int fd_in, int fd_out, int flush_sz)
{
   Dsh *dsh = dNew(Dsh, 1);

   /* init descriptors and streams */
   dsh->fd_in  = fd_in;
   dsh->fd_out = fd_out;

   /* init buffer */
   dsh->dbuf = dStr_sized_new(8 *1024);
   dsh->rd_dbuf = dStr_sized_new(8 *1024);
   dsh->flush_sz = flush_sz;
   dsh->mode = DPIP_TAG;
   if (fcntl(dsh->fd_in, F_GETFL) & O_NONBLOCK)
      dsh->mode |= DPIP_NONBLOCK;
   dsh->status = 0;

   return dsh;
}

/*
 * Streamed write to socket
 * Return: 0 on success, 1 on error.
 */
int a_Dpip_dsh_write(Dsh *dsh, int flush, const char *Data, int DataSize)
{
   int blocking, old_flags, st, sent = 0, ret = 1;

   /* append to buf */
   dStr_append_l(dsh->dbuf, Data, DataSize);

   if (!flush || dsh->dbuf->len == 0)
      return 0;

   blocking = !(dsh->mode & DPIP_NONBLOCK);
   if (!blocking) {
      /* set BLOCKING temporarily... */
      old_flags = fcntl(dsh->fd_in, F_GETFL);
      fcntl(dsh->fd_in, F_SETFL, old_flags & ~O_NONBLOCK);
   }

   while (1) {
      st = write(dsh->fd_out, dsh->dbuf->str + sent, dsh->dbuf->len - sent);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            dsh->status = DPIP_ERROR;
            break;
         }
      } else {
         sent += st;
         if (sent == dsh->dbuf->len) {
            dStr_truncate(dsh->dbuf, 0);
            ret = 0;
            break;
         }
      }
   }

   if (!blocking) {
      /* restore nonblocking mode */
      fcntl(dsh->fd_in, F_SETFL, old_flags);
   }

   return ret;
}

/*
 * Return value: 1..DataSize sent, -1 eagain, or -3 on big Error
 */
int a_Dpip_dsh_trywrite(Dsh *dsh, const char *Data, int DataSize)
{
   int blocking, old_flags, st, ret = -3, sent = 0;

   blocking = !(dsh->mode & DPIP_NONBLOCK);
   if (blocking) {
      /* set NONBLOCKING temporarily... */
      old_flags = fcntl(dsh->fd_in, F_GETFL);
      fcntl(dsh->fd_in, F_SETFL, O_NONBLOCK | old_flags);
   }

   while (1) {
      st = write(dsh->fd_out, Data + sent, DataSize - sent);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN) {
            dsh->status = DPIP_EAGAIN;
            ret = -1;
            break;
         } else {
            MSG_ERR("[a_Dpip_dsh_trywrite] %s\n", dStrerror(errno));
            dsh->status = DPIP_ERROR;
            ret = -3;
            break;
         }
      }
      sent += st;
      break;
   }

   if (blocking) {
      /* restore blocking mode */
      fcntl(dsh->fd_in, F_SETFL, old_flags);
   }

   return (st > 0 ? sent : ret);
}

/*
 * Convenience function.
 */
int a_Dpip_dsh_write_str(Dsh *dsh, int flush, const char *str)
{
   return a_Dpip_dsh_write(dsh, flush, str, (int)strlen(str));
}

/*
 * Read raw data from the socket into our buffer without blocking.
 */
static void Dpip_dsh_read_nb(Dsh *dsh)
{
   ssize_t st;
   int old_flags, blocking;
   char buf[RBUF_SZ];

   dReturn_if (dsh->status == DPIP_ERROR || dsh->status == DPIP_EOF);

   blocking = !(dsh->mode & DPIP_NONBLOCK);
   if (blocking) {
      /* set NONBLOCKING temporarily... */
      old_flags = fcntl(dsh->fd_in, F_GETFL);
      fcntl(dsh->fd_in, F_SETFL, O_NONBLOCK | old_flags);
   }

   while (1) {
      do
         st = read(dsh->fd_in, buf, RBUF_SZ);
      while (st < 0 && errno == EINTR);

      if (st < 0) {
         if (errno == EAGAIN) {
            /* no problem, return what we've got so far... */
            dsh->status = DPIP_EAGAIN;
         } else {
            MSG_ERR("[Dpip_dsh_read_nb] %s\n", dStrerror(errno));
            dsh->status = DPIP_ERROR;
         }
         break;
      } else if (st == 0) {
         dsh->status = DPIP_EOF;
         break;
      } else {
         /* append to buf */
         dStr_append_l(dsh->rd_dbuf, buf, st);
      }
   }

   if (blocking) {
      /* restore blocking mode */
      fcntl(dsh->fd_in, F_SETFL, old_flags);
   }
}

/*
 * Read raw data from the socket into our buffer in BLOCKING mode.
 */
static void Dpip_dsh_read(Dsh *dsh)
{

   ssize_t st;
   int old_flags, non_blocking;
   char buf[RBUF_SZ];

   dReturn_if (dsh->status == DPIP_ERROR || dsh->status == DPIP_EOF);

   non_blocking = (dsh->mode & DPIP_NONBLOCK);
   if (non_blocking) {
      /* set blocking mode temporarily */
      old_flags = fcntl(dsh->fd_in, F_GETFL);
      fcntl(dsh->fd_in, F_SETFL, old_flags);
   }

   while (1) {
      do
         st = read(dsh->fd_in, buf, RBUF_SZ);
      while (st < 0 && errno == EINTR);

      if (st < 0) {
         MSG_ERR("[Dpip_dsh_read] %s\n", dStrerror(errno));
         dsh->status = DPIP_ERROR;
         break;
      } else if (st == 0) {
         dsh->status = DPIP_EOF;
         break;
      } else {
         /* append to buf */
         dStr_append_l(dsh->rd_dbuf, buf, st);
         break;
      }
   }

   if (non_blocking) {
      /* restore non blocking mode */
      fcntl(dsh->fd_in, F_SETFL, old_flags);
   }

   /* assert there's no more data in the wire... */
   Dpip_dsh_read_nb(dsh);
}

/*
 * Return a newlly allocated string with the next dpip token in the socket.
 * Return value: token string on success, NULL otherwise
 */
char *a_Dpip_dsh_read_token(Dsh *dsh, int blocking)
{
   char *p, *ret = NULL;

   /* Read all available data without blocking */
   Dpip_dsh_read_nb(dsh);

   /* switch mode upon request */
   if (dsh->mode & DPIP_LAST_TAG)
      dsh->mode = DPIP_RAW;

   if (blocking && dsh->mode & DPIP_TAG) {
      /* Only wait for data when the tag is incomplete */
      if (!strstr(dsh->rd_dbuf->str, DPIP_TAG_END)) {
         do {
            Dpip_dsh_read(dsh);
            p = strstr(dsh->rd_dbuf->str, DPIP_TAG_END);
         } while (!p && dsh->status == EAGAIN);
      }
   }

   if (dsh->mode & DPIP_TAG) {
      /* return a full tag */
      if ((p = strstr(dsh->rd_dbuf->str, DPIP_TAG_END))) {
         ret = dStrndup(dsh->rd_dbuf->str, p - dsh->rd_dbuf->str + 3);
         dStr_erase(dsh->rd_dbuf, 0, p - dsh->rd_dbuf->str + 3);
         if (strstr(ret, DPIP_MODE_SWITCH_TAG))
            dsh->mode |= DPIP_LAST_TAG;
      }
   } else {
      /* raw mode, return what we have "as is" */
      if (dsh->rd_dbuf->len > 0) {
         ret = dStrndup(dsh->rd_dbuf->str, dsh->rd_dbuf->len);
         dStr_truncate(dsh->rd_dbuf, 0);
      }
   }

   return ret;
}

/*
 * Close this socket for reading and writing.
 * (flush pending data)
 */
void a_Dpip_dsh_close(Dsh *dsh)
{
   int st;

   /* flush internal buffer */
   a_Dpip_dsh_write(dsh, 1, "", 0);

   /* close fds */
   while((st = close(dsh->fd_in)) < 0 && errno == EINTR) ;
   if (st < 0)
      MSG_ERR("[a_Dpip_dsh_close] close: %s\n", dStrerror(errno));
   if (dsh->fd_out != dsh->fd_in) {
      while((st = close(dsh->fd_out)) < 0 && errno == EINTR) ;
      if (st < 0)
         MSG_ERR("[a_Dpip_dsh_close] close: %s\n", dStrerror(errno));
   }
}

/*
 * Free the SockHandler structure
 */
void a_Dpip_dsh_free(Dsh *dsh)
{
   dStr_free(dsh->dbuf, 1);
   dStr_free(dsh->rd_dbuf, 1);
   dFree(dsh);
}

