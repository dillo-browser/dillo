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
 * Main functions to set-up dpi information and to initialise sockets
 */
#include <errno.h>
#include <stdlib.h>             /* for exit */
#include <fcntl.h>              /* for F_SETFD, F_GETFD, FD_CLOEXEC */

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <unistd.h>
#include "dpid_common.h"
#include "dpid.h"
#include "dpi.h"
#include "dpi_socket_dir.h"
#include "misc_new.h"

#include "../dpip/dpip.h"

#define QUEUE 5

volatile sig_atomic_t caught_sigchld = 0;
char *SharedKey = NULL;

/*! Remove dpid_comm_keys file.
 * This avoids that dillo instances connect to a stale port after dpid
 * has exited (e.g. after a reboot).
 */
void cleanup()
{
   char *fname;
   fname = dStrconcat(dGethomedir(), "/", dotDILLO_DPID_COMM_KEYS, NULL);
   unlink(fname);
   dFree(fname);
}

/*! Free memory used to describe
 * a set of dpi attributes
 */
void free_dpi_attr(struct dp *dpi_attr)
{
   if (dpi_attr->id != NULL) {
      dFree(dpi_attr->id);
      dpi_attr->id = NULL;
   }
   if (dpi_attr->path != NULL) {
      dFree(dpi_attr->path);
      dpi_attr->path = NULL;
   }
}

/*! Free memory used by the plugin list
 */
void free_plugin_list(struct dp **dpi_attr_list_ptr, int numdpis)
{
   int i;
   struct dp *dpi_attr_list = *dpi_attr_list_ptr;

   if (dpi_attr_list == NULL)
      return;

   for (i = 0; i < numdpis; i++)
      free_dpi_attr(dpi_attr_list + i);

   dFree(dpi_attr_list);
   *dpi_attr_list_ptr = NULL;
}

/*! Free memory used by the services list
 */
void free_services_list(Dlist *s_list)
{
   int i = 0;
   struct service *s;

   for (i=0; i < dList_length(s_list) ; i++) {
      s = dList_nth_data(s_list, i);
      dFree(s->name);
   }
   dList_free(s_list);
}

/*! Signal handler for SIGINT, SIGQUIT, and SIGTERM. Calls cleanup
 */
static void terminator(int sig)
{
   (void) sig; /* suppress unused parameter warning */
   cleanup();
   _exit(0);
}

/*! Establish handler for termination signals
 * and register cleanup with atexit */
void est_dpi_terminator()
{
   struct sigaction act;
   sigset_t block;

   sigemptyset(&block);
   sigaddset(&block, SIGHUP);
   sigaddset(&block, SIGINT);
   sigaddset(&block, SIGQUIT);
   sigaddset(&block, SIGTERM);

   act.sa_handler = terminator;
   act.sa_mask = block;
   act.sa_flags = 0;

   if (sigaction(SIGHUP, &act, NULL) ||
       sigaction(SIGINT, &act, NULL) ||
       sigaction(SIGQUIT, &act, NULL) ||
       sigaction(SIGTERM, &act, NULL)) {
      ERRMSG("est_dpi_terminator", "sigaction", errno);
      exit(1);
   }

   if (atexit(cleanup) != 0) {
      ERRMSG("est_dpi_terminator", "atexit", 0);
      MSG_ERR("Hey! atexit failed, how did that happen?\n");
      exit(1);
   }
}

/*! Identify a given file
 * Currently there is only one file type associated with dpis.
 * More file types will be added as needed
 */
enum file_type get_file_type(char *file_name)
{
   char *dot = strrchr(file_name, '.');

   if (dot && !strcmp(dot, ".dpi"))
      return DPI_FILE;             /* Any filename ending in ".dpi" */
   else {
      MSG_ERR("get_file_type: Unknown file type for %s\n", file_name);
      return UNKNOWN_FILE;
   }
}

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
      MSG_ERR("e.g. dpi_dir=/usr/local/lib/dillo/dpi\n");
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

/*! Scans a service directory in dpi_dir and fills dpi_attr
 * \Note
 * Caller must allocate memory for dpi_attr.
 * \Return
 * \li 0 on success
 * \li -1 on failure
 * \todo
 * Add other file types, but first we need to add files associated with a dpi
 * to the design.
 */
int get_dpi_attr(char *dpi_dir, char *service, struct dp *dpi_attr)
{
   char *service_dir = NULL;
   struct stat statinfo;
   enum file_type ftype;
   int ret = -1;
   DIR *dir_stream;
   struct dirent *dir_entry = NULL;

   service_dir = dStrconcat(dpi_dir, "/", service, NULL);
   if (stat(service_dir, &statinfo) == -1) {
      ERRMSG("get_dpi_attr", "stat", errno);
      MSG_ERR("file=%s\n", service_dir);
   } else if ((dir_stream = opendir(service_dir)) == NULL) {
      ERRMSG("get_dpi_attr", "opendir", errno);
   } else {
      /* Scan the directory looking for dpi files.
       * (currently there's only the dpi program, but in the future
       *  there may also be helper scripts.) */
      while ( (dir_entry = readdir(dir_stream)) != NULL) {
         if (dir_entry->d_name[0] == '.')
            continue;

         ftype = get_file_type(dir_entry->d_name);
         switch (ftype) {
            case DPI_FILE:
               dpi_attr->path =
                  dStrconcat(service_dir, "/", dir_entry->d_name, NULL);
               dpi_attr->id = dStrdup(service);
               dpi_attr->port = 0;
               dpi_attr->pid = 1;
               if (strstr(dpi_attr->path, ".filter") != NULL)
                  dpi_attr->filter = 1;
               else
                  dpi_attr->filter = 0;
               ret = 0;
               break;
            default:
               break;
         }
      }
      closedir(dir_stream);

      if (ret != 0)
         MSG_ERR("get_dpi_attr: No dpi plug-in in %s/%s\n",
                 dpi_dir, service);
   }
   dFree(service_dir);
   return ret;
}

/*! Register a service
 * Retrieves attributes for "service" and stores them
 * in dpi_attr. It looks for "service" in ~/.dillo/dpi
 * first, and then in the system wide dpi directory.
 * Caller must allocate memory for dpi_attr.
 * \Return
 * \li 0 on success
 * \li -1 on failure
 */
int register_service(struct dp *dpi_attr, char *service)
{
   char *user_dpi_dir, *dpidrc, *user_service_dir, *dir = NULL;
   int ret = -1;

   user_dpi_dir = dStrconcat(dGethomedir(), "/", dotDILLO_DPI, NULL);
   user_service_dir =
       dStrconcat(dGethomedir(), "/", dotDILLO_DPI, "/", service, NULL);

   dpidrc = dStrconcat(dGethomedir(), "/", dotDILLO_DPIDRC, NULL);
   if (access(dpidrc, F_OK) == -1) {
      if (access(DPIDRC_SYS, F_OK) == -1) {
         ERRMSG("register_service", "Error ", 0);
         MSG_ERR("\n - There is no %s or %s file\n", dpidrc,
               DPIDRC_SYS);
         dFree(user_dpi_dir);
         dFree(user_service_dir);
         dFree(dpidrc);
         return(-1);
      }
      dFree(dpidrc);
      dpidrc = dStrdup(DPIDRC_SYS);
   }

   /* Check home dir for dpis */
   if (access(user_service_dir, F_OK) == 0) {
      get_dpi_attr(user_dpi_dir, service, dpi_attr);
      ret = 0;
   } else {                     /* Check system wide dpis */
      if ((dir = get_dpi_dir(dpidrc)) != NULL) {
         if (access(dir, F_OK) == 0) {
            get_dpi_attr(dir, service, dpi_attr);
            ret = 0;
         } else {
            ERRMSG("register_service", "get_dpi_attr failed", 0);
         }
      } else {
         ERRMSG("register_service", "dpi_dir: Error getting dpi dir.", 0);
      }
   }
   dFree(user_dpi_dir);
   dFree(user_service_dir);
   dFree(dpidrc);
   dFree(dir);
   return ret;
}

/*!
 * Create dpi directory for available
 * plugins and create plugin list.
 * \Return
 * \li Returns number of available plugins on success
 * \li -1 on failure
 */
int register_all(struct dp **attlist)
{
   DIR *user_dir_stream, *sys_dir_stream;
   char *user_dpidir = NULL, *sys_dpidir = NULL, *dpidrc = NULL;
   struct dirent *user_dirent, *sys_dirent;
   int st;
   int snum;
   size_t dp_sz = sizeof(struct dp);

   if (*attlist != NULL) {
      ERRMSG("register_all", "attlist parameter should be NULL", 0);
      return -1;
   }

   user_dpidir = dStrconcat(dGethomedir(), "/", dotDILLO_DPI, NULL);
   if (access(user_dpidir, F_OK) == -1) {
      /* no dpis in user's space */
      dFree(user_dpidir);
      user_dpidir = NULL;
   }
   dpidrc = dStrconcat(dGethomedir(), "/", dotDILLO_DPIDRC, NULL);
   if (access(dpidrc, F_OK) == -1) {
      dFree(dpidrc);
      dpidrc = dStrdup(DPIDRC_SYS);
      if (access(dpidrc, F_OK) == -1) {
         dFree(dpidrc);
         dpidrc = NULL;
      }
   }
   if (!dpidrc || (sys_dpidir = get_dpi_dir(dpidrc)) == NULL)
      sys_dpidir = NULL;
   dFree(dpidrc);

   if (!user_dpidir && !sys_dpidir) {
      ERRMSG("register_all", "Fatal error ", 0);
      MSG_ERR("\n - Can't find the directory for dpis.\n");
      exit(1);
   }

   /* Get list of services in user's .dillo/dpi directory */
   snum = 0;
   if (user_dpidir && (user_dir_stream = opendir(user_dpidir)) != NULL) {
      while ((user_dirent = readdir(user_dir_stream)) != NULL) {
         if (user_dirent->d_name[0] == '.')
            continue;
         *attlist = (struct dp *) dRealloc(*attlist, (snum + 1) * dp_sz);
         st=get_dpi_attr(user_dpidir, user_dirent->d_name, &(*attlist)[snum]);
         if (st == 0)
            snum++;
      }
      closedir(user_dir_stream);
   }
   if (sys_dpidir && (sys_dir_stream = opendir(sys_dpidir)) != NULL) {
      /* if system service is not in user list then add it */
      while ((sys_dirent = readdir(sys_dir_stream)) != NULL) {
         if (sys_dirent->d_name[0] == '.')
           continue;
         *attlist = (struct dp *) dRealloc(*attlist, (snum + 1) * dp_sz);
         st=get_dpi_attr(sys_dpidir, sys_dirent->d_name, &(*attlist)[snum]);
         if (st == 0)
            snum++;
      }
      closedir(sys_dir_stream);
   }

   dFree(sys_dpidir);
   dFree(user_dpidir);

   /* TODO: do we consider snum == 0 an error?
    *       (if so, we should return -1 )       */
   return (snum);
}

/*
 * Compare two struct service pointers
 * This function is used for sorting services
 */
static int services_alpha_comp(const struct service *s1,
                               const struct service *s2)
{
   return -strcmp(s1->name, s2->name);
}

/*! Add services reading a dpidrc file
 * each non empty or commented line has the form
 * service = path_relative_to_dpidir
 * \Return:
 * \li Returns number of available services on success
 * \li -1 on failure
 */
int fill_services_list(struct dp *attlist, int numdpis, Dlist **services_list)
{
   FILE *dpidrc_stream;
   char *p, *line = NULL, *service, *path;
   int i, st;
   struct service *s;
   char *user_dpidir = NULL, *sys_dpidir = NULL, *dpidrc = NULL;

   user_dpidir = dStrconcat(dGethomedir(), "/", dotDILLO_DPI, NULL);
   if (access(user_dpidir, F_OK) == -1) {
      /* no dpis in user's space */
      dFree(user_dpidir);
      user_dpidir = NULL;
   }
   dpidrc = dStrconcat(dGethomedir(), "/", dotDILLO_DPIDRC, NULL);
   if (access(dpidrc, F_OK) == -1) {
      dFree(dpidrc);
      dpidrc = dStrdup(DPIDRC_SYS);
      if (access(dpidrc, F_OK) == -1) {
         dFree(dpidrc);
         dpidrc = NULL;
      }
   }
   if (!dpidrc || (sys_dpidir = get_dpi_dir(dpidrc)) == NULL)
      sys_dpidir = NULL;

   if (!user_dpidir && !sys_dpidir) {
      ERRMSG("fill_services_list", "Fatal error ", 0);
      MSG_ERR("\n - Can't find the directory for dpis.\n");
      exit(1);
   }

   if ((dpidrc_stream = fopen(dpidrc, "r")) == NULL) {
      ERRMSG("fill_services_list", "popen failed", errno);
      dFree(dpidrc);
      dFree(sys_dpidir);
      dFree(user_dpidir);
      return (-1);
   }

   if (*services_list != NULL) {
      ERRMSG("fill_services_list", "services_list parameter is not NULL", 0);
      return -1;
   }
   *services_list = dList_new(8);

   /* dpidrc parser loop */
   for (;(line = dGetline(dpidrc_stream)) != NULL; dFree(line)) {
      st = dParser_parse_rc_line(&line, &service, &path);
      if (st < 0) {
         MSG_ERR("dpid: Syntax error in %s: service=\"%s\" path=\"%s\"\n",
                 dpidrc, service, path);
         continue;
      } else if (st != 0) {
         continue;
      }

      _MSG("dpid: service=%s, path=%s\n", service, path);

      /* ignore dpi_dir silently */
      if (strcmp(service, "dpi_dir") == 0)
         continue;

      s = dNew(struct service, 1);
      /* init services list entry */
      s->name = dStrdup(service);
      s->dp_index = -1;

      dList_append(*services_list, s);
      /* search the dpi for a service by its path */
      for (i = 0; i < numdpis; i++)
          if ((p = strstr(attlist[i].path, path)) && *(p - 1) == '/' &&
              strlen(p) == strlen(path))
             break;
      /* if the dpi exist bind service and dpi */
      if (i < numdpis)
         s->dp_index = i;
   }
   fclose(dpidrc_stream);

   dList_sort(*services_list, (dCompareFunc)services_alpha_comp);

   dFree(dpidrc);
   dFree(sys_dpidir);
   dFree(user_dpidir);

   return (dList_length(*services_list));
}

/*
 * Return a socket file descriptor
 * (useful to set socket options in a uniform way)
 */
static int make_socket_fd()
{
   int ret, one = 1;

   if ((ret = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      ERRMSG("make_socket_fd", "socket", errno);
   } else {
      /* avoid delays when sending small pieces of data */
      setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
   }

   /* set some buffering to increase the transfer's speed */
   //setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF,
   //           &sock_buflen, (socklen_t)sizeof(sock_buflen));

   return ret;
}

/*! Bind a socket port on localhost. Try to be close to base_port.
 * \Return
 * \li listening socket file descriptor on success
 * \li -1 on failure
 */
int bind_socket_fd(int base_port, int *p_port)
{
   int sock_fd, port;
   struct sockaddr_in sin;
   int ok = 0, last_port = base_port + 50;

   if ((sock_fd = make_socket_fd()) == -1) {
      return (-1);              /* avoids nested ifs */
   }
   /* Set the socket FD to close on exec */
   fcntl(sock_fd, F_SETFD, FD_CLOEXEC | fcntl(sock_fd, F_GETFD));


   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   /* Try to bind a port on localhost */
   for (port = base_port; port <= last_port; ++port) {
      sin.sin_port = htons(port);
      if ((bind(sock_fd, (struct sockaddr *)&sin, sizeof(sin))) == -1) {
         if (errno == EADDRINUSE || errno == EADDRNOTAVAIL)
            continue;
         ERRMSG("bind_socket_fd", "bind", errno);
      } else if (listen(sock_fd, QUEUE) == -1) {
         ERRMSG("bind_socket_fd", "listen", errno);
      } else {
         *p_port = port;
         ok = 1;
         break;
      }
   }
   if (port > last_port) {
      MSG_ERR("Hey! Can't find an available port from %d to %d\n",
              base_port, last_port);
   }

   return ok ? sock_fd : -1;
}

/*! Save the current port and a shared secret in a file so dillo can find it.
 * \Return:
 * \li -1 on failure
 */
int save_comm_keys(int srs_port)
{
   int fd, ret = -1;
   char *fname, port_str[32];

   fname = dStrconcat(dGethomedir(), "/", dotDILLO_DPID_COMM_KEYS, NULL);
   fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
   dFree(fname);
   if (fd == -1) {
      MSG("save_comm_keys: open %s\n", dStrerror(errno));
   } else {
      snprintf(port_str, 16, "%d %s\n", srs_port, SharedKey);
      if (CKD_WRITE(fd, port_str) != -1 && CKD_CLOSE(fd) != -1) {
         ret = 1;
      }
   }

   return ret;
}

/*! Initialise the service request socket (IDS)
 * \Return:
 * \li Number of sockets (1 == success)
 * \li -1 on failure
 */
int init_ids_srs_socket()
{
   int srs_port, ret = -1;

   FD_ZERO(&sock_set);

   if ((srs_fd = bind_socket_fd(DPID_BASE_PORT, &srs_port)) != -1) {
      /* create the shared secret */
      SharedKey = a_Misc_mksecret(8);
      /* save port number and SharedKey */
      if (save_comm_keys(srs_port) != -1) {
         FD_SET(srs_fd, &sock_set);
         ret = 1;
      }
   }

   return ret;
}

/*! Initialize a single dpi socket
 * \Return
 * \li 1 on success
 * \li -1 on failure
 */
int init_dpi_socket(struct dp *dpi_attr)
{
   int s_fd, port, ret = -1;

   if ((s_fd = bind_socket_fd(DPID_BASE_PORT, &port)) != -1) {
      dpi_attr->sock_fd = s_fd;
      dpi_attr->port = port;
      FD_SET(s_fd, &sock_set);
      ret = 1;
   }

   return ret;
}

/*! Setup sockets for the plugins and add them to
 * the set of sockets (sock_set) watched by select.
 * \Return
 * \li Number of sockets on success
 * \li -1 on failure
 * \Modifies
 * dpi_attr_list.sa, dpi_attr_list.socket, numsocks, sock_set, srs
 * \Uses
 * numdpis, srs, srs_name
 */
int init_all_dpi_sockets(struct dp *dpi_attr_list)
{
   int i;

   /* Initialise sockets for each dpi */
   for (i = 0; i < numdpis; i++) {
      if (init_dpi_socket(dpi_attr_list + i) == -1)
         return (-1);
      numsocks++;
   }

   return (numsocks);
}

/*! SIGCHLD handler
 */
void dpi_sigchld(int sig)
{
   if (sig == SIGCHLD)
      caught_sigchld = 1;
}

/*! Called by main loop when caught_sigchld == 1 */
void handle_sigchld(void)
{
   // pid_t pid;
   int i, status; //, num_active;

   /* For all of the dpis in the current list
    *    add the ones that have exited to the set of sockets being
    *    watched by 'select'.
    */
   for (i = 0; i < numdpis; i++) {
      if (waitpid(dpi_attr_list[i].pid, &status, WNOHANG) > 0) {
         dpi_attr_list[i].pid = 1;
         FD_SET(dpi_attr_list[i].sock_fd, &sock_set);
         numsocks++;
      }
   }

   /* Wait for any old dpis that have exited */
   while (waitpid(-1, &status, WNOHANG) > 0)
      ;
}

/*! Establish SIGCHLD handler */
void est_dpi_sigchld(void)
{
   struct sigaction sigact;
   sigset_t set;

   (void) sigemptyset(&set);
   sigact.sa_handler = dpi_sigchld;
   sigact.sa_mask = set;
   sigact.sa_flags = SA_NOCLDSTOP;
   if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
      ERRMSG("est_dpi_sigchld", "sigaction", errno);
      exit(1);
   }
}

/*! EINTR aware connect() call */
int ckd_connect (int sock_fd, struct sockaddr *addr, socklen_t len)
{
   ssize_t ret;

   do {
      ret = connect(sock_fd, addr, len);
   } while (ret == -1 && errno == EINTR);
   if (ret == -1) {
      ERRMSG("dpid.c", "connect", errno);
   }
   return ret;
}

/*! Send DpiBye command to all active non-filter dpis
 */
void stop_active_dpis(struct dp *dpi_attr_list, int numdpis)
{
   char *bye_cmd, *auth_cmd;
   int i, sock_fd;
   struct sockaddr_in sin;

   bye_cmd = a_Dpip_build_cmd("cmd=%s", "DpiBye");
   auth_cmd = a_Dpip_build_cmd("cmd=%s msg=%s", "auth", SharedKey);

   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   for (i = 0; i < numdpis; i++) {
      /* Skip inactive dpis and filters */
      if (dpi_attr_list[i].pid == 1 || dpi_attr_list[i].filter)
         continue;

      if ((sock_fd = make_socket_fd()) == -1) {
         ERRMSG("stop_active_dpis", "socket", errno);
         continue;
      }

      sin.sin_port = htons(dpi_attr_list[i].port);
      if (ckd_connect(sock_fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
         ERRMSG("stop_active_dpis", "connect", errno);
         MSG_ERR("%s\n", dpi_attr_list[i].path);
      } else if (CKD_WRITE(sock_fd, auth_cmd) == -1) {
         ERRMSG("stop_active_dpis", "write", errno);
      } else if (CKD_WRITE(sock_fd, bye_cmd) == -1) {
         ERRMSG("stop_active_dpis", "write", errno);
      }
      dClose(sock_fd);
   }

   dFree(auth_cmd);
   dFree(bye_cmd);

   /* Allow child dpis some time to read dpid_comm_keys before erasing it */
   sleep (1);
}

/*! Removes dpis in dpi_attr_list from the
 * set of sockets watched by select and
 * closes their sockets.
 */
void ignore_dpi_sockets(struct dp *dpi_attr_list, int numdpis)
{
   int i;

   for (i = 0; i < numdpis; i++) {
      FD_CLR(dpi_attr_list[i].sock_fd, &sock_set);
      dClose(dpi_attr_list[i].sock_fd);
   }
}

/*! Registers available dpis and stops active non-filter dpis.
 * Called when dpid receives
 * cmd='register' service='all'
 * command
 * \Return
 * Number of available dpis
 */
int register_all_cmd()
{
   stop_active_dpis(dpi_attr_list, numdpis);
   free_plugin_list(&dpi_attr_list, numdpis);
   free_services_list(services_list);
   services_list = NULL;
   numdpis = 0;
   numsocks = 1;                /* the srs socket */
   FD_ZERO(&sock_set);
   FD_SET(srs_fd, &sock_set);
   numdpis = register_all(&dpi_attr_list);
   fill_services_list(dpi_attr_list, numdpis, &services_list);
   numsocks = init_all_dpi_sockets(dpi_attr_list);
   return (numdpis);
}

/*!
 * Get value of msg field from dpi_tag
 * \Return
 * message on success, NULL on failure
 */
char *get_message(int sock_fd, char *dpi_tag)
{
   char *msg, *d_cmd;

   msg = a_Dpip_get_attr(dpi_tag, "msg");
   if (msg == NULL) {
      ERRMSG("get_message", "failed to parse msg", 0);
      d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                               "DpiError", "Failed to parse request");
      (void) CKD_WRITE(sock_fd, d_cmd);
      dFree(d_cmd);
   }
   return (msg);
}

/*
 * Compare a struct service pointer and a service name
 * This function is used for searching services by name
 */
int service_match(const struct service *A, const char *B)
{
   int A_len, B_len, len;

   A_len = strlen(A->name);
   B_len = strlen(B);
   len = MAX (A_len, B_len);

   if (A->name[A_len - 1] == '*')
      len = A_len - 1;

   return(dStrnAsciiCasecmp(A->name, B, len));
}

/*!
 * Send socket port that matches dpi_id to client
 */
void send_sockport(int sock_fd, char *dpi_tag, struct dp *dpi_attr_list)
{
   int i;
   char *dpi_id, *d_cmd, port_str[16];
   struct service *serv;

   dReturn_if_fail((dpi_id = get_message(sock_fd, dpi_tag)) != NULL);

   serv = dList_find_custom(services_list,dpi_id,(dCompareFunc)service_match);

   if (serv == NULL || (i = serv->dp_index) == -1)
      for (i = 0; i < numdpis; i++)
         if (!strncmp(dpi_attr_list[i].id, dpi_id,
                      dpi_attr_list[i].id - strchr(dpi_attr_list[i].id, '.')))
            break;

   if (i < numdpis) {
      /* found */
      snprintf(port_str, 8, "%d", dpi_attr_list[i].port);
      d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s", "send_data", port_str);
      (void) CKD_WRITE(sock_fd, d_cmd);
      dFree(d_cmd);
   }

   dFree(dpi_id);
}
