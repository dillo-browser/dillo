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

#include <errno.h>       /* for ckd_write */
#include <unistd.h>      /* for ckd_write */
#include <stdlib.h>      /* for exit */
#include <assert.h>      /* for assert */
#include <sys/stat.h>    /* for umask */

#include "dpid_common.h"
#include "dpid.h"
#include "dpi.h"
#include "dpi_socket_dir.h"
#include "misc_new.h"

#include "../dlib/dlib.h"
#include "../dpip/dpip.h"

sigset_t mask_sigchld;


/* Start a dpi filter plugin after accepting the pending connection
 * \Return
 * \li Child process ID on success
 * \li 0 on failure
 */
static int start_filter_plugin(struct dp dpi_attr)
{
   int newsock, old_stdout=-1, old_stdin=-1;
   socklen_t csz;
   struct sockaddr_un clnt_addr;
   pid_t pid;

   csz = (socklen_t) sizeof(clnt_addr);

   newsock = accept(dpi_attr.sock_fd, (struct sockaddr *) &clnt_addr, &csz);
   if (newsock == -1)
      ERRMSG("start_plugin", "accept", errno);

   dup2(STDIN_FILENO, old_stdin);
   if (dup2(newsock, STDIN_FILENO) == -1) {
      ERRMSG("start_plugin", "dup2", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }

   dup2(STDOUT_FILENO, old_stdout);
   if (dup2(newsock, STDOUT_FILENO) == -1) {
      ERRMSG("start_plugin", "dup2", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }
   if ((pid = fork()) == -1) {
      ERRMSG("main", "fork", errno);
      return 0;
   }
   if (pid == 0) {
      /* Child, start plugin */
      if (execl(dpi_attr.path, dpi_attr.path, (char*)NULL) == -1) {
         ERRMSG("start_plugin", "execl", errno);
         MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
         exit(1);
      }
   }

   /* Parent, Close sockets fix stdio and return pid */
   if (dClose(newsock) == -1) {
      ERRMSG("start_plugin", "close", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }
   dClose(STDIN_FILENO);
   dClose(STDOUT_FILENO);
   dup2(old_stdin, STDIN_FILENO);
   dup2(old_stdout, STDOUT_FILENO);
   return pid;
}

static void start_server_plugin(struct dp dpi_attr)
{
   if (dup2(dpi_attr.sock_fd, STDIN_FILENO) == -1) {
      ERRMSG("start_plugin", "dup2", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }
   if (dClose(dpi_attr.sock_fd) == -1) {
      ERRMSG("start_plugin", "close", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }
   if (execl(dpi_attr.path, dpi_attr.path, (char*)NULL) == -1) {
      ERRMSG("start_plugin", "execl", errno);
      MSG_ERR("ERROR in child proc for %s\n", dpi_attr.path);
      exit(1);
   }
}

/*!
 * Read service request from sock
 * \Return
 * pointer to dynamically allocated request tag
 */
static char *get_request(Dsh *sh)
{
   char *dpip_tag;

   (void) sigprocmask(SIG_BLOCK, &mask_sigchld, NULL);
   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   (void) sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);

   return dpip_tag;
}

/*!
 * Get value of cmd field in dpi_tag
 * \Return
 * command code on success, -1 on failure
 */
static int get_command(Dsh *sh, char *dpi_tag)
{
   char *cmd, *d_cmd;
   int COMMAND;

   if (dpi_tag == NULL) {
      _ERRMSG("get_command", "dpid tag is NULL", 0);
      return (-1);
   }

   cmd = a_Dpip_get_attr(dpi_tag, "cmd");

   if (cmd == NULL) {
      ERRMSG("get_command", "a_Dpip_get_attr", 0);
      MSG_ERR(": dpid failed to parse cmd in %s\n", dpi_tag);
      d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                               "DpiError", "Failed to parse request");
      a_Dpip_dsh_write_str(sh, 1, d_cmd);
      dFree(d_cmd);
      COMMAND = -1;
   } else if (strcmp("auth", cmd) == 0) {
      COMMAND = AUTH_CMD;
   } else if (strcmp("DpiBye", cmd) == 0) {
      COMMAND = BYE_CMD;
   } else if (strcmp("check_server", cmd) == 0) {
      COMMAND = CHECK_SERVER_CMD;
   } else if (strcmp("register_all", cmd) == 0) {
      COMMAND = REGISTER_ALL_CMD;
   } else if (strcmp("register_service", cmd) == 0) {
      COMMAND = REGISTER_SERVICE_CMD;
   } else {                     /* Error unknown command */
      COMMAND = UNKNOWN_CMD;
   }

   dFree(cmd);
   return (COMMAND);
}

/*
 * Check whether a dpi server is running
 */
static int server_is_running(char *server_id)
{
   int i;

   /* Search in the set of running servers */
   for (i = 0; i < numdpis; i++) {
      if (!dpi_attr_list[i].filter && dpi_attr_list[i].pid > 1 &&
          strcmp(dpi_attr_list[i].id, server_id) == 0)
         return 1;
   }
   return 0;
}


/*
 * Get MAX open FD limit (yes, it's tricky --Jcid).
 */
static int get_open_max(void)
{
#ifdef OPEN_MAX
   return OPEN_MAX;
#else
   int ret = sysconf(_SC_OPEN_MAX);
   if (ret < 0)
      ret = 256;
   return ret;
#endif
}

/*! \todo
 * \li Add a dpid_idle_timeout variable to dpidrc
 * \bug Infinite loop if plugin crashes before it accepts a connection
 */
int main(void)
{
   int i, n = 0, open_max;
   int dpid_idle_timeout = 60 * 60; /* default, in seconds */
   struct timeval select_timeout;
   sigset_t mask_none;
   fd_set selected_set;

   dpi_attr_list = NULL;
   services_list = NULL;
   //daemon(0,0); /* Use 0,1 for feedback */
   /* TODO: call setsid() ?? */

   /* Allow read and write access, but only for the user.
    * TODO: can this cause trouble with umount? */
   umask(0077);
   /* TODO: make dpid work on any directory. */
   // chdir("/");

   /* close inherited file descriptors */
   open_max = get_open_max();
   for (i = 3; i < open_max; i++)
      dClose(i);

   /* this sleep used to unmask a race condition */
   // sleep(2);

   dpi_errno = no_errors;

   /* Get list of available dpis */
   numdpis = register_all(&dpi_attr_list);

#if 0
   /* Get name of socket directory */
   dirname = a_Dpi_sockdir_file();
   if ((sockdir = init_sockdir(dirname)) == NULL) {
      ERRMSG("main", "init_sockdir", 0);
      MSG_ERR("Failed to create socket directory\n");
      exit(1);
   }
#endif

   /* Init and get services list */
   fill_services_list(dpi_attr_list, numdpis, &services_list);

   /* Remove any sockets that may have been leftover from a crash */
   //cleanup();

   /* Initialise sockets */
   if ((numsocks = init_ids_srs_socket()) == -1) {
      switch (dpi_errno) {
      case dpid_srs_addrinuse:
         MSG_ERR("dpid refuses to start, possibly because:\n");
         MSG_ERR("\t1) An instance of dpid is already running.\n");
         MSG_ERR("\t2) A previous dpid didn't clean up on exit.\n");
         exit(1);
      default:
         //ERRMSG("main", "init_srs_socket failed", 0);
         ERRMSG("main", "init_ids_srs_socket failed", 0);
         exit(1);
      }
   }
   numsocks = init_all_dpi_sockets(dpi_attr_list);
   est_dpi_terminator();
   est_dpi_sigchld();

   (void) sigemptyset(&mask_sigchld);
   (void) sigaddset(&mask_sigchld, SIGCHLD);
   (void) sigemptyset(&mask_none);
   (void) sigprocmask(SIG_SETMASK, &mask_none, NULL);

   printf("dpid started\n");
/* Start main loop */
   while (1) {
      do {
         (void) sigprocmask(SIG_BLOCK, &mask_sigchld, NULL);
         if (caught_sigchld) {
            handle_sigchld();
            caught_sigchld = 0;
         }
         (void) sigprocmask(SIG_UNBLOCK, &mask_sigchld, NULL);
         select_timeout.tv_sec = dpid_idle_timeout;
         select_timeout.tv_usec = 0;
         selected_set = sock_set;
         n = select(FD_SETSIZE, &selected_set, NULL, NULL, &select_timeout);
         if (n == 0) { /* select timed out, try to exit */
            /* BUG: This is a workaround for dpid not to exit when the
             * downloads server is active. The proper way to handle it is with
             * a dpip command that asks the server whether it's busy.
             * Note: the cookies server may lose session info too. */
            if (server_is_running("downloads"))
               continue;

            stop_active_dpis(dpi_attr_list, numdpis);
            //cleanup();
            exit(0);
         }
      } while (n == -1 && errno == EINTR);

      if (n == -1) {
         ERRMSG("main", "select", errno);
         exit(1);
      }
      /* If the service req socket is selected then service the req. */
      if (FD_ISSET(srs_fd, &selected_set)) {
         int sock_fd;
         socklen_t sin_sz;
         struct sockaddr_in sin;
         char *req = NULL;

         --n;
         assert(n >= 0);
         sin_sz = (socklen_t) sizeof(sin);
         sock_fd = accept(srs_fd, (struct sockaddr *)&sin, &sin_sz);
         if (sock_fd == -1) {
            ERRMSG("main", "accept", errno);
            MSG_ERR("accept on srs socket failed\n");
            MSG_ERR("service pending connections, and continue\n");
         } else {
            int command;
            Dsh *sh;

            sh = a_Dpip_dsh_new(sock_fd, sock_fd, 1024);
read_next:
            req = get_request(sh);
            command = get_command(sh, req);
            switch (command) {
            case AUTH_CMD:
               if (a_Dpip_check_auth(req) != -1) {
                  dFree(req);
                  goto read_next;
               }
               break;
            case BYE_CMD:
               stop_active_dpis(dpi_attr_list, numdpis);
               //cleanup();
               exit(0);
               break;
            case CHECK_SERVER_CMD:
               send_sockport(sock_fd, req, dpi_attr_list);
               break;
            case REGISTER_ALL_CMD:
               register_all_cmd();
               break;
            case UNKNOWN_CMD:
               {
               char *d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                                              "DpiError", "Unknown command");
               (void) CKD_WRITE(sock_fd, d_cmd);
               dFree(d_cmd);
               ERRMSG("main", "Unknown command", 0);
               MSG_ERR(" for request: %s\n", req);
               break;
               }
            case -1:
               _ERRMSG("main", "get_command failed", 0);
               break;
            }
            if (req)
               free(req);
            a_Dpip_dsh_close(sh);
            a_Dpip_dsh_free(sh);
         }
      }

      /* While there's a request on one of the plugin sockets
       * find the matching plugin and start it. */
      for (i = 0; n > 0 && i < numdpis; i++) {
         if (FD_ISSET(dpi_attr_list[i].sock_fd, &selected_set)) {
            --n;
            assert(n >= 0);

            if (dpi_attr_list[i].filter) {
               /* start a dpi filter plugin and continue watching its socket
                * for new connections */
               (void) sigprocmask(SIG_SETMASK, &mask_none, NULL);
               start_filter_plugin(dpi_attr_list[i]);
            } else {
               /* start a dpi server plugin but don't wait for new connections
                * on its socket */
               numsocks--;
               assert(numsocks >= 0);
               FD_CLR(dpi_attr_list[i].sock_fd, &sock_set);
               if ((dpi_attr_list[i].pid = fork()) == -1) {
                  ERRMSG("main", "fork", errno);
                  /* exit(1); */
               } else if (dpi_attr_list[i].pid == 0) {
                  /* child */
                  (void) sigprocmask(SIG_SETMASK, &mask_none, NULL);
                  start_server_plugin(dpi_attr_list[i]);
               }
            }
         }
      }
   }
}
