/*
 * Dpi for "View source".
 *
 * This server is an example. Play with it and modify to your taste.
 *
 * Copyright 2010-2015 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../dpip/dpip.h"
#include "dpiutil.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  fprintf(stderr, "[vsource dpi]: " __VA_ARGS__)

/*---------------------------------------------------------------------------*/

const char *DOCTYPE=
 "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>";


void send_dpip_tag(Dsh *sh, char *dpip_tag)
{
   a_Dpip_dsh_write_str(sh, 0, "\nDpip tag received: ");
   a_Dpip_dsh_write_str(sh, 0, dpip_tag ? dpip_tag : "None");
   a_Dpip_dsh_write_str(sh, 1, "\n\n");
}

/*
 * Send source as plain text
 * (handles embedded null chars correctly).
 */
void send_plain_text(Dsh *sh, int data_size)
{
   char *token;
   int bytes_read = 0, token_size;

   /* Send HTTP header for plain text MIME type */
   a_Dpip_dsh_write_str(sh, 0, "Content-type: text/plain\n\n");

   while (bytes_read < data_size &&
          (token = a_Dpip_dsh_read_token2(sh, 1, &token_size))) {
      bytes_read += token_size;
      _MSG("data_size=%d bytes_read=%d\n", data_size, bytes_read);
      a_Dpip_dsh_write(sh, 1, token, token_size);
      dFree(token);
   }
}

/*
 * Send source as plain text with line numbers
 * (handles embedded null chars correctly).
 */
void send_numbered_text(Dsh *sh, int data_size)
{
   int bytes_read = 0, line = 1, token_size = 0;
   char *p, *q, *token, line_str[32];

   /* Send HTTP header for plain text MIME type */
   a_Dpip_dsh_write_str(sh, 0, "Content-type: text/plain\n\n");

   while (bytes_read < data_size &&
          (token = a_Dpip_dsh_read_token2(sh, 1, &token_size))) {
      bytes_read += token_size;
      p = q = token;

      while (*p) {
         snprintf(line_str, 32, "%2d: ", line);
         a_Dpip_dsh_write_str(sh, 0, line_str);
         if ((p = strpbrk(q, "\r\n"))) {
            a_Dpip_dsh_write(sh, 0, q, p - q + 1);
            if (*p == '\r' && p[1] == '\n')
               ++p;
            ++line;
         } else {
            /* send all the rest */
            a_Dpip_dsh_write(sh, 1, q, token_size - (q - token));
            break;
         }
         q = ++p;
      }
      dFree(token);
   }
}

/*
 * Send source as html text with line numbers
 * (handles embedded null chars correctly).
 */
void send_html_text(Dsh *sh, const char *url, int data_size)
{
   int bytes_read = 0, old_line = 0, line = 1, token_size = 0;
   char *p, *q, *token, line_str[128];

   if (dStrnAsciiCasecmp(url, "dpi:", 4) == 0 &&
       strncmp(url+4, "/vsource/:", 10) == 0)
      url += 14;

   /* Send HTTP header for html text MIME type */
   a_Dpip_dsh_write_str(sh, 0, "Content-type: text/html\n\n");

   a_Dpip_dsh_write_str(sh, 0, DOCTYPE);
   a_Dpip_dsh_printf(sh, 0,
                     "\n"
                     "<html><head>\n"
                     "<title>Source for %s</title>\n"
                     "<style type=\"text/css\">\n"
                     " body {white-space: pre-wrap; font-family: monospace}\n"
                     " td.r1 {background-color:#B87333}\n"
                     " td.r2 {background-color:#DD7F32}\n"
                     "</style>\n"
                     "</head>\n"
                     "<body id=\"dillo_vs\">\n<table cellpadding='0'>\n", url);

   while (bytes_read < data_size &&
          (token = a_Dpip_dsh_read_token2(sh, 1, &token_size))) {
      bytes_read += token_size;
      p = q = token;

      while (*p) {
         if (line > old_line) {
            snprintf(line_str, 128,
                     "<tr><td class='%s'>%d%s<td>",
                     (line & 1) ? "r1" : "r2", line,
                     (line == 1 || (line % 10) == 0) ? "&nbsp;" : "");
            a_Dpip_dsh_write_str(sh, 0, line_str);
            old_line = line;
         }
         if ((p = strpbrk(q, "\r\n<&"))) {
            if (*p == '\r' || *p == '\n') {
               a_Dpip_dsh_write(sh, 0, q, p - q + 1);
               if (*p == '\r' && p[1] == '\n')
                  p++;
               ++line;
            } else {
               a_Dpip_dsh_write(sh, 0, q, p - q);
               a_Dpip_dsh_write_str(sh, 0, (*p == '<') ? "&lt;" : "&amp;");
            }
         } else { 
            /* send all the rest */
            a_Dpip_dsh_write(sh, 1, q, token_size - (q - token));
            break;
         }
         q = ++p;
      }
      dFree(token);
   }

   a_Dpip_dsh_write_str(sh, 1, "</table></body></html>");
}

/*
 *
 */
int main(void)
{
   Dsh *sh;
   int data_size;
   char *dpip_tag, *cmd = NULL, *cmd2 = NULL, *url = NULL, *size_str = NULL;
   char *d_cmd;

   _MSG("starting...\n");
   //sleep(20);

   /* Initialize the SockHandler.
    * This means we'll use stdin for input and stdout for output.
    * In case of a server dpi, we'd use a socket and pass its file descriptor
    * twice (e.g. a_Dpip_dsh_new(sock_fd, sock_fd, 1024).
    * (Note: by now the last parameter is not used) */
   sh = a_Dpip_dsh_new(STDIN_FILENO, STDOUT_FILENO, 2*1024);

   /* Authenticate our client...
    * As we're using Internet domain sockets, DPIP checks whether the client
    * runs with the user's ID, by means of a shared secret. The DPIP API does
    * the work for us. */
   if (!(dpip_tag = a_Dpip_dsh_read_token(sh, 1)) ||
       a_Dpip_check_auth(dpip_tag) < 0) {
      MSG("can't authenticate request: %s\n", dStrerror(errno));
      a_Dpip_dsh_close(sh);
      return 1;
   }
   dFree(dpip_tag);

   /* Read the dpi command from STDIN
    * Now we're past the authentication phase, let's see what's dillo
    * asking from us. a_Dpip_dsh_read_token() will block and return
    * a full dpip token or null on error (it's commented in dpip.c) */
   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   _MSG("tag = [%s]\n", dpip_tag);

   /* Now that we have the dpip_tag, let's isolate the command and url */
   cmd = a_Dpip_get_attr(dpip_tag, "cmd");
   url = a_Dpip_get_attr(dpip_tag, "url");

   /* Start sending our answer.
    * (You can read the comments for DPIP API functions in dpip/dpip.c) */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 0, d_cmd);
   dFree(d_cmd);
   dFree(dpip_tag);

   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   cmd2 = a_Dpip_get_attr(dpip_tag, "cmd");
   if (cmd2) {
      if (strcmp(cmd2, "start_send_page") == 0 &&
          (size_str = a_Dpip_get_attr(dpip_tag, "data_size"))) {
         data_size = strtol(size_str, NULL, 10);
         /* Choose your flavour */
         //send_plain_text(sh, data_size);
         //send_numbered_text(sh, data_size);
         send_html_text(sh, url, data_size);
      } else if (strcmp(cmd2, "DpiError") == 0) {
         /* Dillo detected an error (other failures just close the socket) */
         a_Dpip_dsh_write_str(sh, 0, "Content-type: text/plain\n\n");
         a_Dpip_dsh_write_str(sh, 1, "[vsource dpi]: "
                                     "ERROR: Page not cached.\n");
      }
      dFree(cmd2);
   }

   dFree(cmd);
   dFree(url);
   dFree(size_str);
   dFree(dpip_tag);

   /* Finish the SockHandler */
   a_Dpip_dsh_close(sh);
   a_Dpip_dsh_free(sh);

   return 0;
}

