/*
 * File: datauri.c
 *
 * Copyright (C) 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * Filter dpi for the "data:" URI scheme (RFC 2397).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "../dpip/dpip.h"
#include "dpiutil.h"

/*
 * Debugging macros
 */
#define SILENT 1
#define _MSG(...)
#if SILENT
 #define MSG(...)
#else
 #define MSG(...)  fprintf(stderr, "[datauri dpi]: " __VA_ARGS__)
#endif

/*
 * Global variables
 */
static Dsh *sh = NULL;

static void b64strip_illegal_chars(unsigned char* str)
{
   unsigned char *p, *s = str;

   MSG("len=%d{%s}\n", strlen((char*)str), str);

   for (p = s; (*p = *s); ++s) {
      if (isascii(*p) && (isalnum(*p) || strchr("+/=", *p)))
         ++p;
   }

   MSG("len=%d{%s}\n", strlen((char *)str), str);
}

static int b64decode(unsigned char* str)
{
   unsigned char *cur, *start;
   int d, dlast, phase;
   unsigned char c;
   static int table[256] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
      52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
      -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
      15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
      -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
      41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
   };

   d = dlast = phase = 0;
   start = str;
   for (cur = str; *cur != '\0'; ++cur ) {
      // jer: treat line endings as physical breaks.
      //if (*cur == '\n' || *cur == '\r'){phase = dlast = 0; continue;}
      d = table[(int)*cur];
      if (d != -1) {
         switch(phase) {
         case 0:
            ++phase;
            break;
         case 1:
            c = ((dlast << 2) | ((d & 0x30) >> 4));
            *str++ = c;
            ++phase;
            break;
         case 2:
            c = (((dlast & 0xf) << 4) | ((d & 0x3c) >> 2));
            *str++ = c;
            ++phase;
            break;
         case 3:
            c = (((dlast & 0x03 ) << 6) | d);
            *str++ = c;
            phase = 0;
            break;
         }
         dlast = d;
      }
   }
   *str = '\0';
   return str - start;
}

/* Modified from src/url.c --------------------------------------------------*/

/*
 * Given an hex octet (e.g., e3, 2F, 20), return the corresponding
 * character if the octet is valid, and -1 otherwise
 */
static int Url_decode_hex_octet(const char *s)
{
   int hex_value;
   char *tail, hex[3];

   if (s && (hex[0] = s[0]) && (hex[1] = s[1])) {
      hex[2] = 0;
      hex_value = strtol(hex, &tail, 16);
      if (tail - hex == 2)
         return hex_value;
   }
   return -1;
}

/*
 * Parse possible hexadecimal octets in the URI path.
 * Returns a new allocated string.
 */
char *a_Url_decode_hex_str(const char *str, size_t *p_sz)
{
   char *new_str, *dest;
   int i, val;

   if (!str) {
      *p_sz = 0;
      return NULL;
   }

   dest = new_str = dNew(char, strlen(str) + 1);
   for (i = 0; str[i]; i++) {
      *dest++ = (str[i] == '%' && (val = Url_decode_hex_octet(str+i+1)) >= 0) ?
                i+=2, val : str[i];
   }
   *dest = 0;

   *p_sz = (size_t)(dest - new_str);
   new_str = dRealloc(new_str, sizeof(char) * (dest - new_str + 1));
   return new_str;
}

/* end ----------------------------------------------------------------------*/

/*
 * Send decoded data to dillo in an HTTP envelope.
 */
static void send_decoded_data(const char *url, const char *mime_type,
                              unsigned char *data, size_t data_sz)
{
   char *d_cmd;

   /* Send dpip tag */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);

   /* Send HTTP header. */
   a_Dpip_dsh_write_str(sh, 0, "Content-type: ");
   a_Dpip_dsh_write_str(sh, 0, mime_type);
   a_Dpip_dsh_write_str(sh, 1, "\n\n");

   /* Send message */
   a_Dpip_dsh_write(sh, 0, (char *)data, data_sz);
}

static void send_failure_message(const char *url, const char *mime_type,
                                 unsigned char *data, size_t data_sz)
{
   char *d_cmd;
   char buf[1024];

   const char *msg =
"<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
"<html><body>\n"
"<hr><h1>Datauri dpi</h1><hr>\n"
"<p><b>Can't parse datauri:</b><br>\n";
   const char *msg_mime_type="text/html";

   /* Send dpip tag */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);

   /* Send HTTP header. */
   a_Dpip_dsh_write_str(sh, 0, "Content-type: ");
   a_Dpip_dsh_write_str(sh, 0, msg_mime_type);
   a_Dpip_dsh_write_str(sh, 1, "\n\n");

   /* Send message */
   a_Dpip_dsh_write_str(sh, 0, msg);

   /* send some debug info */
   snprintf(buf, 1024, "mime_type: %s<br>data size: %d<br>data: %s<br>",
            mime_type, (int)data_sz, data);
   a_Dpip_dsh_write_str(sh, 0, buf);

   /* close page */
   a_Dpip_dsh_write_str(sh, 0, "</body></html>");
}

/*
 * Get mime type from the data URI.
 */
static char *datauri_get_mime(char *url)
{
   char buf[256];
   char *mime_type = NULL, *p;
   size_t len = 0;

   if (dStrnAsciiCasecmp(url, "data:", 5) == 0) {
      if ((p = strchr(url, ',')) && p - url < 256) {
         url += 5;
         len = p - url;
         strncpy(buf, url, len);
         buf[len] = 0;
         /* strip ";base64" */
         if (len >= 7 && dStrAsciiCasecmp(buf + len - 7, ";base64") == 0) {
            len -= 7;
            buf[len] = 0;
         }
      }

      /* that's it, now handle omitted types */
      if (len == 0) {
         mime_type = dStrdup("text/plain;charset=US-ASCII");
      } else if (!dStrnAsciiCasecmp(buf, "charset", 7)) {
         mime_type = dStrconcat("text/plain;", buf, NULL);
      } else {
         mime_type = dStrdup(buf);
      }
   }

   return mime_type;
}

/*
 * Return a decoded data string.
 */
static unsigned char *datauri_get_data(char *url, size_t *p_sz)
{
   char *p;
   int is_base64 = 0;
   unsigned char *data = NULL;

   if ((p = strchr(url, ',')) && p - url >= 12 &&  /* "data:;base64" */
       dStrnAsciiCasecmp(p - 7, ";base64", 7) == 0) {
      is_base64 = 1;
   }

   if (p) {
      ++p;
      if (is_base64) {
         data = (unsigned char *)Unescape_uri_str(p);
         b64strip_illegal_chars(data);
         *p_sz = (size_t) b64decode(data);
      } else {
         data = (unsigned char *)a_Url_decode_hex_str(p, p_sz);
      }
   } else {
      data = (unsigned char *)dStrdup("");
      *p_sz = 0;
   }

   return data;
}

/*
 *
 */
int main(void)
{
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *mime_type;
   unsigned char *data;
   int rc;
   size_t data_size = 0;

   /* Initialize the SockHandler */
   sh = a_Dpip_dsh_new(STDIN_FILENO, STDOUT_FILENO, 8*1024);

   rc = chdir("/tmp");
   if (rc == -1) {
      MSG("paths: error changing directory to /tmp: %s\n",
          dStrerror(errno));
   }

   /* Authenticate our client... */
   if (!(dpip_tag = a_Dpip_dsh_read_token(sh, 1)) ||
       a_Dpip_check_auth(dpip_tag) < 0) {
      MSG("can't authenticate request: %s\n", dStrerror(errno));
      a_Dpip_dsh_close(sh);
      return 1;
   }
   dFree(dpip_tag);

   /* Read the dpi command from STDIN */
   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   MSG("[%s]\n", dpip_tag);

   cmd = a_Dpip_get_attr(dpip_tag, "cmd");
   url = a_Dpip_get_attr(dpip_tag, "url");
   if (!cmd || !url) {
      MSG("Error, cmd=%s, url=%s\n", cmd, url);
      exit (EXIT_FAILURE);
   }

   /* Parse the data URI */
   mime_type = datauri_get_mime(url);
   data = datauri_get_data(url, &data_size);

   MSG("mime_type: %s\n", mime_type);
   MSG("data_size: %d\n", (int)data_size);
   MSG("data: {%s}\n", data);

   if (mime_type && data) {
      /* good URI */
      send_decoded_data(url, mime_type, data, data_size);
   } else {
      /* malformed URI */
      send_failure_message(url, mime_type, data, data_size);
   }

   dFree(data);
   dFree(mime_type);
   dFree(url);
   dFree(cmd);
   dFree(dpip_tag);

   /* Finish the SockHandler */
   a_Dpip_dsh_close(sh);
   a_Dpip_dsh_free(sh);

   return 0;
}

