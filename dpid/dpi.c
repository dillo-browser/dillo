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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file
 * Access functions for  ~/.dillo/dpi_socket_dir.
 * The most useful function for dillo is a_Dpi_srs, it returns
 * the full path to the dpid service request socket.
 */

#include <errno.h>
#include <stdlib.h>  /* for exit */
#include "dpid_common.h"
#include "dpi.h"
#include "misc_new.h"

/*! \Return
 * Returns path to the dpi_socket_dir file
 * Use dFree to free memory
 */
char *a_Dpi_sockdir_file(void)
{
   char *dpi_socket_dir, *dirfile_path = "/.dillo/dpi_socket_dir";

   dpi_socket_dir = dStrconcat(dGethomedir(), dirfile_path, NULL);
   return dpi_socket_dir;
}

/*! Read socket directory path from ~/.dillo/dpi_socket_dir
 * \Return
 * socket directory path or NULL if the dpi_socket_dir file does not exist.
 * \Note
 * This function exits if ~/.dillo does not exist or
 * if the dpi_socket_dir file cannot be opened for a
 * reason other than it does not exist.
 */

char *a_Dpi_rd_dpi_socket_dir(char *dirname)
{
   FILE *dir;
   char *sockdir = NULL, *rcpath;

   rcpath = dStrconcat(dGethomedir(), "/.dillo", NULL);

   /* If .dillo does not exist it is an unrecoverable error */
   if (access(rcpath, F_OK) == -1) {
      ERRMSG("a_Dpi_rd_dpi_socket_dir", "access", errno);
      MSG_ERR(" - %s\n", rcpath);
      exit(1);
   }
   dFree(rcpath);

   if ((dir = fopen(dirname, "r")) != NULL) {
      sockdir = dGetline(dir);
      fclose(dir);
   } else if (errno == ENOENT) {
      ERRMSG("a_Dpi_rd_dpi_socket_dir", "fopen", errno);
      MSG_ERR(" - %s\n", dirname);
   } else if (errno != ENOENT) {
      ERRMSG("a_Dpi_rd_dpi_socket_dir", "fopen", errno);
      MSG_ERR(" - %s\n", dirname);
      exit(1);
   }

   return sockdir;
}

/*!
 * \Modifies
 * srs_name
 * \Return
 * The service request socket name with its complete path.
 */
char *a_Dpi_srs(void)
{
   char *dirfile_path, *sockdir, *srs_name;

   dirfile_path = a_Dpi_sockdir_file();
   sockdir = dStrstrip(a_Dpi_rd_dpi_socket_dir(dirfile_path));
   srs_name = dStrconcat(sockdir, "/", "dpid.srs", NULL);
   dFree(sockdir);
   dFree(dirfile_path);
   return (srs_name);
}
