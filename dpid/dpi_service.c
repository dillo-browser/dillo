/*
   Copyright (C) 2003  Ferdi Franceschini <ferdif@optusnet.com.au>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*! \file
 * \todo
 * This module should be removed because its original functions
 * have been removed or modified.
 * Put these functions in dpid.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "dpid_common.h"
#include "dpid.h"
#include "../dpip/dpip.h"

#ifdef TEST
#include "testdat.h"
#endif

/* exported functions */
char *get_dpi_dir(char *dpidrc);


/*! Get dpi directory path from dpidrc
 * \Return
 * dpi directory on success, NULL on failure
 * \Important
 * The dpi_dir definition in dpidrc must have no leading white space.
 */
char *get_dpi_dir(char *dpidrc)
{
   FILE *In;
   int len;
   char *rcline = NULL, *value = NULL, *p;

   if ((In = fopen(dpidrc, "r")) == NULL) {
      ERRMSG("dpi_dir", "fopen", errno);
      MSG_ERR(" - %s\n", dpidrc);
      return (NULL);
   }

   while ((rcline = dGetline(In)) != NULL) {
      if (strncmp(rcline, "dpi_dir", 7) == 0)
         break;
      dFree(rcline);
   }
   fclose(In);

   if (!rcline) {
      ERRMSG("dpi_dir", "Failed to find a dpi_dir entry in dpidrc", 0);
      MSG_ERR("Put your dillo plugins path in %s\n", dpidrc);
      MSG_ERR("eg. dpi_dir=/usr/local/lib/dillo/dpi ");
      MSG_ERR("with no leading spaces.\n");
      value = NULL;
   } else {
      len = (int) strlen(rcline);
      if (len && rcline[len - 1] == '\n')
         rcline[len - 1] = 0;

      if ((p = strchr(rcline, '='))) {
         while (*++p == ' ');
         value = dStrdup(p);
      } else {
         ERRMSG("dpi_dir", "strchr", 0);
         MSG_ERR(" - '=' not found in %s\n", rcline);
         value = NULL;
      }
   }

   dFree(rcline);
   return (value);
}

/*! Send the list of available dpi IDs to a client
 * \Return
 * 1 on success, -1 on failure.
 *
static int send_service_list(int sock, struct dp *dpi_attr_list, int srv_num)
{
   int i;
   char *buf;
   ssize_t wlen = 0;

   for (i = 0; i < srv_num && wlen != -1; i++) {
      d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                               "send_data", dpi_attr_list[i].id);
      wlen = write(sock, d_cmd, strlen(d_cmd));
      dFree(d_cmd);
   }
   if (wlen == -1) {
      ERRMSG("send_service_list", "write", errno);
      return (-1);
   }
   return (1);
}
 */
