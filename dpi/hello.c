/*
 * Dpi for "Hello World".
 *
 * This server is an example. Play with it and modify to your taste.
 *
 * Copyright 2003-2007 Jorge Arellano Cid <jcid@dillo.org>
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
#define MSG(...)  printf("[hello dpi]: " __VA_ARGS__)

/*---------------------------------------------------------------------------*/


/*
 *
 */
int main(void)
{
   FILE *in_stream;
   Dsh *sh;
   char *dpip_tag, *cmd = NULL, *url = NULL, *child_cmd = NULL;
   char *esc_tag, *d_cmd;
   size_t n;
   int ret;
   char buf[4096];
   char *choice[] = {"Window was closed", "Yes", "No",
                      "Could be", "It's OK", "Cancel"};
                   /* "Could>be", ">It's OK", "Can'>cel"};  --for testing */
   int choice_num = -1;

   MSG("starting...\n");
   /* sleep(20) */

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
   MSG("tag = [%s]\n", dpip_tag);

   /* Now that we have the dpip_tag, let's isolate the command and url */
   cmd = a_Dpip_get_attr(dpip_tag, "cmd");
   url = a_Dpip_get_attr(dpip_tag, "url");

/*-- Dialog part */
/* This is the dialog window. This is an example of interaction with
 * the user. If you're starting to understand dpis, comment this out
 * by switching to "#if 0" and the dialog will be disabled. */
#if 1
{
   char *dpip_tag2, *dialog_msg;

   /* Let's confirm the request */
   /* NOTE: you can send less alternatives (e.g. only alt1 and alt2) */
   d_cmd = a_Dpip_build_cmd(
              "cmd=%s title=%s msg=%s alt1=%s alt2=%s alt3=%s alt4=%s alt5=%s",
              "dialog", "Dillo: Hello", "Do you want to see the hello page?",
              choice[1], choice[2], choice[3], choice[4], choice[5]);
   a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);

   /* Get the answer */
   dpip_tag2 = a_Dpip_dsh_read_token(sh, 1);
   MSG("tag = [%s]\n", dpip_tag2);

   /* Get "msg" value */
   dialog_msg = a_Dpip_get_attr(dpip_tag2, "msg");
   choice_num = 0;
   if (dialog_msg)
      choice_num = *dialog_msg - '0';

   dFree(dialog_msg);
   dFree(dpip_tag2);
}
#endif
/*-- EOD part */

   /* Start sending our answer.
    * (You can read the comments for DPIP API functions in dpip/dpip.c) */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 0, d_cmd);
   dFree(d_cmd);

   a_Dpip_dsh_printf(sh, 0,
      "Content-type: text/html\n\n"
      "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
      "<html>\n"
      "<head><title>Simple dpi test page (hello.dpi)</title></head>\n"
      "<body><hr><h1>Hello world!</h1><hr>\n<br><br>\n");

   /* Show the choice received with the dialog */
   a_Dpip_dsh_printf(sh, 0,
      "<hr>\n"
      "<table width='100%%' border='1' bgcolor='burlywood'><tr><td>\n"
      "<big><em>Dialog question:</em> Do you want to see the hello page?<br>\n"
      "<em>Answer received:</em> <b>%s</b></big> </table>\n"
      "<hr>\n",
      choice_num < 0 ? "There was NO dialog!" : choice[choice_num]);

   /* Show the dpip tag we received */
   esc_tag = Escape_html_str(dpip_tag);
   a_Dpip_dsh_printf(sh, 0,
      "<h3>dpip tag received:</h3>\n"
      "<pre>\n%s</pre>\n"
      "<br><small>(<b>dpip:</b> dpi protocol)</small><br><br><br>\n",
      esc_tag);
   dFree(esc_tag);


   /* Now something more interesting,
    * fork a command and show its feedback.
    * (An example of generating dynamic content with an external
    *  program). */
   if (cmd && url) {
      child_cmd = dStrdup("date -R");
      MSG("[%s]\n", child_cmd);

      /* Fork, exec command, get its output and answer */
      if ((in_stream = popen(child_cmd, "r")) == NULL) {
         perror("popen");
         return EXIT_FAILURE;
      }

      a_Dpip_dsh_write_str(sh, 0, "<h3>date:</h3>\n");
      a_Dpip_dsh_write_str(sh, 0, "<pre>\n");

      /* Read/Write */
      while ((n = fread (buf, 1, 4096, in_stream)) > 0) {
         a_Dpip_dsh_write(sh, 0, buf, n);
      }

      a_Dpip_dsh_write_str(sh, 0, "</pre>\n");

      if ((ret = pclose(in_stream)) != 0)
         MSG("popen: [%d]\n", ret);

      dFree(child_cmd);
   }

   a_Dpip_dsh_write_str(sh, 1, "</body></html>\n");

   dFree(cmd);
   dFree(url);
   dFree(dpip_tag);

   /* Finish the SockHandler */
   a_Dpip_dsh_close(sh);
   a_Dpip_dsh_free(sh);

   return 0;
}

