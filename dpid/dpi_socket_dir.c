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
 * Create a per user temporary directory for dpi sockets
 */

#include <errno.h>
#include <stdlib.h>
#include "dpid_common.h"
#include "dpi.h"
#include "misc_new.h"
#include "dpi_socket_dir.h" /* for function prototypes */

/*! Save socket directory name in ~/.dillo/dpi_socket_dir
 * \Return
 * \li 1 on success
 * \li -1 on failure
 */
int w_dpi_socket_dir(char *dirname, char *sockdir)
{
   FILE *dir;

   if ((dir = fopen(dirname, "w")) == NULL) {
      ERRMSG("w_dpi_socket_dir", "fopen", errno);
      return (-1);
   }
   fprintf(dir, "%s", sockdir);
   fclose(dir);
   return (1);
}

/*! Test that socket directory exists and is a directory
 * \Return
 * \li 1 on success
 * \li 0 on failure
 * \bug Does not check permissions or that it's a symbolic link
 * to another directory.
 */
int tst_dir(char *dir)
{
   char *dirtest;
   int ret = 0;

   /* test for a directory */
   dirtest = dStrconcat(dir, "/", NULL);
   if (access(dirtest, F_OK) == -1) {
      ERRMSG("tst_dir", "access", errno);
      MSG_ERR(" - %s\n", dirtest);
   } else {
      ret = 1;
   }
   dFree(dirtest);

   return ret;
}

/*! Create socket directory
 * \Return
 * \li Socket directory path on success
 * \li NULL on failure
 */
char *mk_sockdir(void)
{
   char *template, *logname;

   logname = getenv("LOGNAME") ? getenv("LOGNAME") : "dillo";
   template = dStrconcat("/tmp/", logname, "-", "XXXXXX", NULL);
   if (a_Misc_mkdtemp(template) == NULL) {
      ERRMSG("mk_sockdir", "a_Misc_mkdtemp", 0);
      MSG_ERR(" - %s\n", template);
      dFree(template);
      return (NULL);
   }
   return template;
}

/*! Create socket directory if it does not exist and save its name in
 * ~/.dillo/dpi_socket_dir.
 * \Return
 * \li Socket directory name on success
 * \li NULL on failure.
 */
char *init_sockdir(char *dpi_socket_dir)
{
   char *sockdir = NULL;
   int dir_ok = 0;

   if ((sockdir = a_Dpi_rd_dpi_socket_dir(dpi_socket_dir)) == NULL) {
      MSG_ERR("init_sockdir: The dpi_socket_dir file %s does not exist\n",
              dpi_socket_dir);
   } else {
      if ((dir_ok = tst_dir(sockdir)) == 1) {
         MSG_ERR("init_sockdir: The socket directory %s exists and is OK\n",
                 sockdir);
      } else {
         MSG_ERR("init_sockdir: The socket directory %s does not exist "
                 "or is not a directory\n", sockdir);
         dFree(sockdir);
      }
   }
   if (!dir_ok) {
      sockdir = mk_sockdir();
      if (sockdir == NULL) {
         ERRMSG("init_sockdir", "mk_sockdir", 0);
         MSG_ERR(" - Failed to create dpi socket directory\n");
      } else if ((w_dpi_socket_dir(dpi_socket_dir, sockdir)) == -1) {
         ERRMSG("init_sockdir", "w_dpi_socket_dir", 0);
         MSG_ERR(" - failed to save %s\n", sockdir);
         dFree(sockdir);
         sockdir = NULL;
      }
   }
   return (sockdir);
}
