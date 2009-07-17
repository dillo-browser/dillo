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
   SockHandler *sh;
   char *dpip_tag, *cmd = NULL, *url = NULL, *child_cmd = NULL;
   char *esc_tag, *d_cmd;
   size_t n;
   int ret;
   char buf[4096];
   char *choice[] = {"Window was closed", "Yes", "No",
                      "Could be", "It's OK", "Cancel"};
                   /* "Could>be", ">It's OK", "Can'>cel"};  --for testing */
   int choice_num;

   MSG("starting...\n");

   /* Initialize the SockHandler */
   sh = sock_handler_new(STDIN_FILENO, STDOUT_FILENO, 2*1024);

   /* Read the dpi command from STDIN */
   dpip_tag = sock_handler_read(sh);
   MSG("tag = [%s]\n", dpip_tag);

   cmd = a_Dpip_get_attr(dpip_tag, "cmd");
   url = a_Dpip_get_attr(dpip_tag, "url");

/*-- Dialog part */
{
   char *dpip_tag2, *dialog_msg;

   /* Let's confirm the request */
   /* NOTE: you can send less alternatives (e.g. only alt1 and alt2) */
   d_cmd = a_Dpip_build_cmd(
              "cmd=%s msg=%s alt1=%s alt2=%s alt3=%s alt4=%s alt5=%s",
              "dialog", "Do you want to see the hello page?",
              choice[1], choice[2], choice[3], choice[4], choice[5]);
   sock_handler_write_str(sh, 1, d_cmd);
   dFree(d_cmd);

   /* Get the answer */
   dpip_tag2 = sock_handler_read(sh);
   MSG("tag = [%s]\n", dpip_tag2);

   /* Get "msg" value */
   dialog_msg = a_Dpip_get_attr(dpip_tag2, "msg");
   choice_num = 0;
   if (dialog_msg)
      choice_num = *dialog_msg - '0';

   dFree(dialog_msg);
   dFree(dpip_tag2);
}
/*-- EOD part */

   /* Start sending our answer */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   sock_handler_write_str(sh, 0, d_cmd);
   dFree(d_cmd);

   sock_handler_printf(sh, 0,
      "Content-type: text/html\n\n"
      "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
      "<html>\n"
      "<head><title>Simple dpi test page (hello.dpi)</title></head>\n"
      "<body><hr><h1>Hello world!</h1><hr>\n<br><br>\n");

   /* Show the choice received with the dialog */
   sock_handler_printf(sh, 0,
      "<hr>\n"
      "<table width='100%%' border='1' bgcolor='burlywood'><tr><td>\n"
      "<big><em>Dialog question:</em> Do you want to see the hello page?<br>\n"
      "<em>Answer received:</em> <b>%s</b></big> </table>\n"
      "<hr>\n",
      choice[choice_num]);

   /* Show the dpip tag we received */
   esc_tag = Escape_html_str(dpip_tag);
   sock_handler_printf(sh, 0,
      "<h3>dpip tag received:</h3>\n"
      "<pre>\n%s</pre>\n"
      "<br><small>(<b>dpip:</b> dpi protocol)</small><br><br><br>\n",
      esc_tag);
   dFree(esc_tag);


   /* Now something more interesting,
    * fork a command and show its feedback */
   if (cmd && url) {
      child_cmd = dStrdup("date -R");
      MSG("[%s]\n", child_cmd);

      /* Fork, exec command, get its output and answer */
      if ((in_stream = popen(child_cmd, "r")) == NULL) {
         perror("popen");
         return EXIT_FAILURE;
      }

      sock_handler_printf(sh, 0, "<h3>date:</h3>\n");
      sock_handler_printf(sh, 0, "<pre>\n");

      /* Read/Write */
      while ((n = fread (buf, 1, 4096, in_stream)) > 0) {
         sock_handler_write(sh, 0, buf, n);
      }

      sock_handler_printf(sh, 0, "</pre>\n");

      if ((ret = pclose(in_stream)) != 0)
         MSG("popen: [%d]\n", ret);

      dFree(child_cmd);
   }

   sock_handler_printf(sh, 1, "</body></html>\n");

   dFree(cmd);
   dFree(url);
   dFree(dpip_tag);

   /* Finish the SockHandler */
   sock_handler_close(sh);
   sock_handler_free(sh);

   return 0;
}

