/*
 * File: dpip.c
 *
 * Copyright 2005-2015 Jorge Arellano Cid <jcid@dillo.org>
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

#define RBUF_SZ 16*1024
//#define RBUF_SZ 1

#define DPIP_TAG_END            " '>"
#define DPIP_MODE_SWITCH_TAG    "cmd='start_send_page' "
#define MSG_ERR(...)            fprintf(stderr, "[dpip]: " __VA_ARGS__)

/*
 * Local variables
 */
static const char Quote = '\'';

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
      if (tail && port != 0) {
         for (i = 0; *tail && isxdigit(tail[i+1]); ++i)
            SharedSecret[i] = tail[i+1];
         SharedSecret[i] = 0;
         if (strcmp(msg, SharedSecret) == 0)
            ret = 1;
      }
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
   dsh->wrbuf = dStr_sized_new(8 *1024);
   dsh->rdbuf = dStr_sized_new(8 *1024);
   dsh->flush_sz = flush_sz;
   dsh->mode = DPIP_TAG;
   if (fcntl(dsh->fd_in, F_GETFL) & O_NONBLOCK)
      dsh->mode |= DPIP_NONBLOCK;
   dsh->status = 0;

   return dsh;
}

/*
 * Return value: 1..DataSize sent, -1 eagain, or -3 on big Error
 */
static int Dpip_dsh_write(Dsh *dsh, int nb, const char *Data, int DataSize)
{
   int req_mode, old_flags = 0, st, ret = -3, sent = 0;

   req_mode = (nb) ? DPIP_NONBLOCK : 0;
   if ((dsh->mode & DPIP_NONBLOCK) != req_mode) {
      /* change mode temporarily... */
      old_flags = fcntl(dsh->fd_out, F_GETFL);
      fcntl(dsh->fd_out, F_SETFL,
            (nb) ? O_NONBLOCK | old_flags : old_flags & ~O_NONBLOCK);
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
            MSG_ERR("[Dpip_dsh_write] %s\n", dStrerror(errno));
            dsh->status = DPIP_ERROR;
            break;
         }
      } else {
         sent += st;
         if (nb || sent == DataSize) {
            ret = sent;
            break;
         }
      }
   }

   if ((dsh->mode & DPIP_NONBLOCK) != req_mode) {
      /* restore old mode */
      fcntl(dsh->fd_out, F_SETFL, old_flags);
   }

   return ret;
}

/*
 * Streamed write to socket
 * Return: 0 on success, 1 on error.
 */
int a_Dpip_dsh_write(Dsh *dsh, int flush, const char *Data, int DataSize)
{
   int ret = 1;

   /* append to buf */
   dStr_append_l(dsh->wrbuf, Data, DataSize);

   if (!flush || dsh->wrbuf->len == 0)
      return 0;

   ret = Dpip_dsh_write(dsh, 0, dsh->wrbuf->str, dsh->wrbuf->len);
   if (ret == dsh->wrbuf->len) {
      dStr_truncate(dsh->wrbuf, 0);
      ret = 0;
   }

   return ret;
}

/*
 * Return value: 0 on success or empty buffer,
 *               1..DataSize sent, -1 eagain, or -3 on big Error
 */
int a_Dpip_dsh_tryflush(Dsh *dsh)
{
   int st;

   if (dsh->wrbuf->len == 0) {
      st = 0;
   } else {
      st = Dpip_dsh_write(dsh, 1, dsh->wrbuf->str, dsh->wrbuf->len);
      if (st > 0) {
         /* update internal buffer */
         dStr_erase(dsh->wrbuf, 0, st);
      }
   }
   return (dsh->wrbuf->len == 0) ? 0 : st;
}

/*
 * Return value: 1..DataSize sent, -1 eagain, or -3 on big Error
 */
int a_Dpip_dsh_trywrite(Dsh *dsh, const char *Data, int DataSize)
{
   int st;

   if ((st = Dpip_dsh_write(dsh, 1, Data, DataSize)) > 0) {
      /* update internal buffer */
      if (st < DataSize)
         dStr_append_l(dsh->wrbuf, Data + st, DataSize - st);
   }
   return st;
}

/*
 * Convenience function.
 */
int a_Dpip_dsh_write_str(Dsh *dsh, int flush, const char *str)
{
   return a_Dpip_dsh_write(dsh, flush, str, (int)strlen(str));
}

/*
 * Read raw data from the socket into our buffer in
 * either BLOCKING or NONBLOCKING mode.
 */
static void Dpip_dsh_read(Dsh *dsh, int blocking)
{
   char buf[RBUF_SZ];
   int req_mode, old_flags = 0, st, nb = !blocking;

   dReturn_if (dsh->status == DPIP_ERROR || dsh->status == DPIP_EOF);

   req_mode = (nb) ? DPIP_NONBLOCK : 0;
   if ((dsh->mode & DPIP_NONBLOCK) != req_mode) {
      /* change mode temporarily... */
      old_flags = fcntl(dsh->fd_in, F_GETFL);
      fcntl(dsh->fd_in, F_SETFL,
            (nb) ? O_NONBLOCK | old_flags : old_flags & ~O_NONBLOCK);
   }

   while (1) {
      st = read(dsh->fd_in, buf, RBUF_SZ);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else if (errno == EAGAIN) {
            dsh->status = DPIP_EAGAIN;
            break;
         } else {
            MSG_ERR("[Dpip_dsh_read] %s\n", dStrerror(errno));
            dsh->status = DPIP_ERROR;
            break;
         }
      } else if (st == 0) {
         dsh->status = DPIP_EOF;
         break;
      } else {
         /* append to buf */
         dStr_append_l(dsh->rdbuf, buf, st);
         if (blocking)
            break;
      }
   }

   if ((dsh->mode & DPIP_NONBLOCK) != req_mode) {
      /* restore old mode */
      fcntl(dsh->fd_out, F_SETFL, old_flags);
   }

   /* assert there's no more data in the wire...
    * (st < buf upon interrupt || st == buf and no more data) */
   if (blocking)
      Dpip_dsh_read(dsh, 0);
}

/*
 * Return a newlly allocated string with the next dpip token in the socket.
 * Return value: token string and length on success, NULL otherwise.
 * (useful for handling null characters in the data stream)
 */
char *a_Dpip_dsh_read_token2(Dsh *dsh, int blocking, int *DataSize)
{
   char *p, *ret = NULL;
   *DataSize = 0;

   /* Read all available data without blocking */
   Dpip_dsh_read(dsh, 0);

   /* switch mode upon request */
   if (dsh->mode & DPIP_LAST_TAG)
      dsh->mode = DPIP_RAW;

   if (blocking) {
      if (dsh->mode & DPIP_TAG) {
         /* Only wait for data when the tag is incomplete */
         if (!strstr(dsh->rdbuf->str, DPIP_TAG_END)) {
             do {
                Dpip_dsh_read(dsh, 1);
                p = strstr(dsh->rdbuf->str, DPIP_TAG_END);
             } while (!p && dsh->status == EAGAIN);
         }

      } else if (dsh->mode & DPIP_RAW) {
         /* Wait for data when the buffer is empty and there's no ERR/EOF */
         while (dsh->rdbuf->len == 0 &&
                dsh->status != DPIP_ERROR && dsh->status != DPIP_EOF)
            Dpip_dsh_read(dsh, 1);
      }
   }

   if (dsh->mode & DPIP_TAG) {
      /* return a full tag */
      if ((p = strstr(dsh->rdbuf->str, DPIP_TAG_END))) {
         ret = dStrndup(dsh->rdbuf->str, p - dsh->rdbuf->str + 3);
         *DataSize = p - dsh->rdbuf->str + 3;
         dStr_erase(dsh->rdbuf, 0, p - dsh->rdbuf->str + 3);
         if (strstr(ret, DPIP_MODE_SWITCH_TAG))
            dsh->mode |= DPIP_LAST_TAG;
      }
   } else {
      /* raw mode, return what we have "as is" */
      if (dsh->rdbuf->len > 0) {
         ret = dStrndup(dsh->rdbuf->str, dsh->rdbuf->len);
         *DataSize = dsh->rdbuf->len;
         dStr_truncate(dsh->rdbuf, 0);
      }
   }

   return ret;
}

/*
 * Return a newlly allocated string with the next dpip token in the socket.
 * Return value: token string on success, NULL otherwise
 */
char *a_Dpip_dsh_read_token(Dsh *dsh, int blocking)
{
   int token_size;

   return a_Dpip_dsh_read_token2(dsh, blocking, &token_size);
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
   st = dClose(dsh->fd_in);
   if (st < 0)
      MSG_ERR("[a_Dpip_dsh_close] close: %s\n", dStrerror(errno));
   if (dsh->fd_out != dsh->fd_in) {
      st = dClose(dsh->fd_out);
      if (st < 0)
         MSG_ERR("[a_Dpip_dsh_close] close: %s\n", dStrerror(errno));
   }
}

/*
 * Free the SockHandler structure
 */
void a_Dpip_dsh_free(Dsh *dsh)
{
   dReturn_if (dsh == NULL);

   dStr_free(dsh->wrbuf, 1);
   dStr_free(dsh->rdbuf, 1);
   dFree(dsh);
}

