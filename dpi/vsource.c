/*
 * Dpi for "View source".
 *
 * This server is an example. Play with it and modify to your taste.
 *
 * Copyright 2010 Jorge Arellano Cid <jcid@dillo.org>
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
#define MSG(...)  printf("[vsource dpi]: " __VA_ARGS__)

/*---------------------------------------------------------------------------*/


/*
 *
 */
int main(void)
{
   Dsh *sh;
   int data_size, bytes_read = 0;
   char *dpip_tag, *cmd = NULL, *url = NULL, *size_str = NULL;
   char *d_cmd, *src_str;

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

   /* Start sending our answer.
    * (You can read the comments for DPIP API functions in dpip/dpip.c) */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 0, d_cmd);
   dFree(d_cmd);

   a_Dpip_dsh_printf(sh, 0,
      "Content-type: text/plain\n\n"
      ".----------------.\n"
      "|  Hello World!  |\n"
      "'----------------'\n\n");

   /* Show the dpip tag we received */
   a_Dpip_dsh_printf(sh, 0, "Dpip tag received: ");
   a_Dpip_dsh_printf(sh, 0, dpip_tag);
   dFree(dpip_tag);

   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   a_Dpip_dsh_printf(sh, 0, "\nDpip tag received: ");
   a_Dpip_dsh_printf(sh, 0, dpip_tag ? dpip_tag : "None");
   a_Dpip_dsh_printf(sh, 1, "\n\n");
   //dFree(dpip_tag);
   size_str = a_Dpip_get_attr(dpip_tag, "data_size");
   data_size = strtol(size_str, NULL, 10);

   while (bytes_read < data_size &&
          (src_str = a_Dpip_dsh_read_token(sh, 1))) {
      bytes_read += strlen(src_str);
      //a_Dpip_dsh_write_str(sh, 0, src_str);
      a_Dpip_dsh_write_str(sh, 1, src_str);
      dFree(src_str);
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

