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
 * Main functions to set-up dpi information and to initialise sockets
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dpid_common.h"
#include "dpid.h"
#include "dpi.h"
#include "dpi_socket_dir.h"
#include "dpi_service.h"
#include "misc_new.h"

#include "../dpip/dpip.h"

#define QUEUE 5

volatile sig_atomic_t caught_sigchld = 0;

/*! Return the basename of a filename
 */
static char *get_basename(char *filename)
{
   char *p;

   if (filename && (p = strrchr(filename, '/'))) {
      filename = p + 1;
   }
   return filename;
}

/*! Close and remove the sockets in the
 * given dpi attribute list
 */
void rm_dpi_sockets(struct dp *dpi_attr_list, int numdpis)
{
   int i;

   for (i = 0; i < numdpis; i++) {
      a_Misc_close_fd(dpi_attr_list[i].socket);
      (void) unlink(dpi_attr_list[i].sockpath);
   }
}

/*! Close and remove inactive dpi sockets
 * \Return
 * Number of active dpis.
 */
int rm_inactive_dpi_sockets(struct dp *dpi_attr_list, int numdpis)
{
   int i, active = 0;

   for (i = 0; i < numdpis; i++) {
      if (dpi_attr_list[i].pid == 1) {
         a_Misc_close_fd(dpi_attr_list[i].socket);
         (void) unlink(dpi_attr_list[i].sockpath);
      } else
         active++;
   }
   return (active);
}

/*! Remove sockets
 */
void cleanup(char *socket_dir)
{
   DIR *dir;
   struct dirent *dir_entry = NULL;
   char *sockpath;

   dir = opendir(socket_dir);
   if (dir == NULL) {
      ERRMSG("cleanup", "opendir", errno);
      return;
   }
   while ( (dir_entry = readdir(dir)) != NULL ) {
      if (dir_entry->d_name[0] == '.')
         continue;
      sockpath = dStrconcat(socket_dir, "/", dir_entry->d_name, NULL);
      unlink(sockpath);
      dFree(sockpath);
   }
   closedir(dir);
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
   if (dpi_attr->sockpath != NULL) {
      dFree(dpi_attr->sockpath);
      dpi_attr->sockpath = NULL;
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

/*! \todo
 * Remove terminator and est_terminator unless we really want to clean up
 * on abnormal exit.
 */
#if 0
/*! Signal handler for SIGINT, SIGQUIT, and SIGTERM. Calls cleanup
 */
void terminator(int sig)
{
   (void) signal(SIGCHLD, SIG_DFL);
   cleanup();
   (void) signal(sig, SIG_DFL);
   (void) raise(sig);
   _exit(0);
}

/*! Establish handler for termination signals
 * and register cleanup with atexit */
void est_terminator(void)
{
   struct sigaction act;
   sigset_t block;

   (void) sigemptyset(&block);
   (void) sigaddset(&block, SIGINT);
   (void) sigaddset(&block, SIGQUIT);
   (void) sigaddset(&block, SIGTERM);
   (void) sigaddset(&block, SIGSEGV);

   act.sa_handler = terminator;
   act.sa_mask = block;
   act.sa_flags = 0;

   if (sigaction(SIGINT, &act, NULL) ||
       sigaction(SIGQUIT, &act, NULL) ||
       sigaction(SIGTERM, &act, NULL) || sigaction(SIGSEGV, &act, NULL)) {
      ERRMSG("est_terminator", "sigaction", errno);
      exit(1);
   }

   if (atexit(cleanup) != 0) {
      ERRMSG("est_terminator", "atexit", 0);
      MSG_ERR("Hey! atexit failed, how did that happen?\n");
      exit(1);
   }
}
#endif

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
   int retval = -1;
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
               dpi_attr->sockpath = NULL;
               dpi_attr->pid = 1;
               if (strstr(dpi_attr->path, ".filter") != NULL)
                  dpi_attr->filter = 1;
               else
                  dpi_attr->filter = 0;
               retval = 0;
               break;
            default:
               break;
         }
      }
      closedir(dir_stream);

      if (retval != 0)
         MSG_ERR("get_dpi_attr: No dpi plug-in in %s/%s\n",
                 dpi_dir, service);
   }
   dFree(service_dir);
   return retval;
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
   int retval = -1;

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
      retval = 0;
   } else {                     /* Check system wide dpis */
      if ((dir = get_dpi_dir(dpidrc)) != NULL) {
         if (access(dir, F_OK) == 0) {
            get_dpi_attr(dir, service, dpi_attr);
            retval = 0;
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
   return (retval);
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
   int snum, usr_srv_num;
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
   snum = usr_srv_num = 0;
   if (user_dpidir && (user_dir_stream = opendir(user_dpidir)) != NULL) {
      while ((user_dirent = readdir(user_dir_stream)) != NULL) {
         if (user_dirent->d_name[0] == '.')
            continue;
         *attlist = (struct dp *) dRealloc(*attlist, (snum + 1) * dp_sz);
         st=get_dpi_attr(user_dpidir, user_dirent->d_name, &(*attlist)[snum]);
         if (st == 0)
            snum++;
      }
      usr_srv_num = snum;
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

   /* todo: do we consider snum == 0 an error?
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
   char *p, *line = NULL;
   char *service, *path;
   int i;
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
   while ((line = dGetline(dpidrc_stream)) != NULL) {

      if (dParser_get_rc_pair(&line, &service, &path) == -1) {
         if (line[0] && line[0] != '#' && (!service || !path)) {
            MSG_ERR("Syntax error in %s: service=\"%s\" path=\"%s\"\n",
                    dpidrc, service, path);
         }
         continue;
      }

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
      dFree(line);
   }
   fclose(dpidrc_stream); 

   dList_sort(*services_list, (dCompareFunc)services_alpha_comp);

   dFree(dpidrc);
   dFree(sys_dpidir);
   dFree(user_dpidir);

   return (dList_length(*services_list));
}

/*! Initialise the service request socket
 * \Return:
 * \li Number of sockets (1 == success)
 * \li -1 on failure
 */
int init_srs_socket(char *sockdir)
{
   int retval = -1;
   struct sockaddr_un srs_sa;
   size_t sun_path_len;
   socklen_t addr_sz;

   srs_name = dStrconcat(sockdir, "/", SRS_NAME, NULL);
   FD_ZERO(&sock_set);

   /* Initialise srs, service request socket on startup */
   if ((srs = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
      ERRMSG("init_srs_socket", "socket", errno);
      return (retval);              /* avoids nesting ifs too deeply */
   }
   /* Set srs to close on exec */
   fcntl(srs, F_SETFD, FD_CLOEXEC | fcntl(srs, F_GETFD));

   srs_sa.sun_family = AF_LOCAL;

   sun_path_len = sizeof(srs_sa.sun_path);
   if (strlen(srs_name) > sun_path_len) {
      ERRMSG("init_srs_socket", "srs_name is too long", 0);
      MSG_ERR("\n - it should be <= %lu chars", (ulong_t)sun_path_len);
      MSG_ERR("\n - srs_name = %s\n", srs_name);
      return(retval);
   }
   strncpy(srs_sa.sun_path, srs_name, sun_path_len);
   addr_sz = (socklen_t) D_SUN_LEN(&srs_sa);

   if ((bind(srs, (struct sockaddr *) &srs_sa, addr_sz)) == -1) {
      if (errno == EADDRINUSE) {
         ERRMSG("init_srs_socket", "bind", errno);
         MSG_ERR("srs_sa.sun_path = %s\n", srs_sa.sun_path);
         dpi_errno = dpid_srs_addrinuse;
      } else {
         ERRMSG("init_srs_socket", "bind", errno);
         MSG_ERR("srs_sa.sun_path = %s\n", srs_sa.sun_path);
      }
   } else if (chmod(srs_sa.sun_path, S_IRUSR | S_IWUSR) == -1) {
      ERRMSG("init_srs_socket", "chmod", errno);
      MSG_ERR("srs_sa.sun_path = %s\n", srs_sa.sun_path);
   } else if (listen(srs, QUEUE) == -1) {
      ERRMSG("init_srs_socket", "listen", errno);
   } else {
      retval = 1;
   }

   FD_SET(srs, &sock_set);
   return (retval);
}

/*! Initialise a single dpi socket
 * \Return
 * \li 1 on success
 * \li -1 on failure
 */
int init_dpi_socket(struct dp *dpi_attr, char *sockdir)
{
   int caught_error = 0, s;
   char *dpi_nm;                /* pointer to basename in dpi_attr->path */
   struct sockaddr_un sa;
   size_t sp_len;
   socklen_t addr_sz;
   size_t sock_buflen = 8192;

   sp_len = sizeof(sa.sun_path);
   if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
      ERRMSG("init_all_dpi_sockets", "socket", errno);
      return (-1);              /* avoids nested ifs */
   }
   /* Set the socket FD to close on exec */
   fcntl(s, F_SETFD, FD_CLOEXEC | fcntl(s, F_GETFD));

   /* set some buffering to increase the transfer's speed */
   setsockopt(s, SOL_SOCKET, SO_SNDBUF,
              &sock_buflen, (socklen_t)sizeof(sock_buflen));

   dpi_attr->socket = s;
   dpi_attr->sa.sun_family = AF_LOCAL;
   dpi_nm = get_basename(dpi_attr->path);

   dpi_attr->sockpath = dStrconcat(sockdir, "/", dpi_nm, "-XXXXXX", NULL);
   mktemp(dpi_attr->sockpath);
   if (strlen(dpi_attr->sockpath) > sp_len) {
      ERRMSG("init_all_dpi_sockets", "socket path is too long", 0);
      MSG_ERR("\n - it should be <= %lu chars", (ulong_t)sp_len);
      MSG_ERR("\n - socket path = %s\n", dpi_attr->sockpath);
      return(-1);
   }
   strncpy(dpi_attr->sa.sun_path, dpi_attr->sockpath, sp_len);
   addr_sz = (socklen_t) D_SUN_LEN(&dpi_attr->sa);

   if ((bind(s, (struct sockaddr *) &dpi_attr->sa, addr_sz)) == -1) {
      ERRMSG("init_all_dpi_sockets", "bind", errno);
      MSG_ERR("%s\n", dpi_attr->sa.sun_path);
      caught_error = 1;
   } else if (chmod(dpi_attr->sa.sun_path, S_IRUSR | S_IWUSR) == -1) {
      ERRMSG("init_all_dpi_sockets", "chmod", errno);
      MSG_ERR("%s\n", dpi_attr->sa.sun_path);
      caught_error = 1;
   } else if (listen(s, QUEUE) == -1) {
      ERRMSG("init_all_dpi_sockets", "listen", errno);
      caught_error = 1;
   }

   if (caught_error) {
      return (-1);
   } else {
      FD_SET(s, &sock_set);
      return (1);
   }
}

/*! Setup sockets for the plugins and add them to
 * to the set of sockets (sock_set) watched by select.
 * \Return
 * \li Number of sockets on success
 * \li -1 on failure
 * \Modifies
 * dpi_attr_list.sa, dpi_attr_list.socket, numsocks, sock_set, srs
 * \Uses
 * numdpis, srs, srs_name
 */
int init_all_dpi_sockets(struct dp *dpi_attr_list, char *sockdir)
{
   int i;
   struct sockaddr_un sa;
   size_t sp_len;

   sp_len = sizeof(sa.sun_path);

   /* Initialise sockets for each dpi */
   for (i = 0; i < numdpis; i++) {
      if (init_dpi_socket(dpi_attr_list + i, sockdir) == -1)
         return (-1);
      numsocks++;
   }

   return (numsocks);
}

/*! SIGCHLD handler
 */
void dpi_sigchld(int sig)
{
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
         FD_SET(dpi_attr_list[i].socket, &sock_set);
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

/*! Send DpiBye command to all active non-filter dpis
 */
void stop_active_dpis(struct dp *dpi_attr_list, int numdpis)
{
   static char *DpiBye_cmd = NULL;
   int i, dpi_socket;
   struct sockaddr_un dpi_addr;
   struct sockaddr_un sa;
   size_t sun_path_len, addr_len;

   if (!DpiBye_cmd)
      DpiBye_cmd = a_Dpip_build_cmd("cmd=%s", "DpiBye");

   sun_path_len = sizeof(sa.sun_path);
   addr_len = sizeof(dpi_addr);

   dpi_addr.sun_family = AF_LOCAL;

   for (i = 0; i < numdpis; i++) {
      /* Skip inactive dpis and filters */
      if (dpi_attr_list[i].pid == 1 || dpi_attr_list[i].filter)
         continue;

      if ((dpi_socket = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
         ERRMSG("stop_active_dpis", "socket", errno);
      }
      if (strlen(dpi_attr_list[i].sockpath) > sun_path_len) {
         ERRMSG("stop_active_dpis", "socket path is too long", 0);
         MSG_ERR("\n - it should be <= %lu chars",(ulong_t)sun_path_len);
         MSG_ERR("\n - socket path = %s\n", dpi_attr_list[i].sockpath);
      }
      strncpy(dpi_addr.sun_path, dpi_attr_list[i].sockpath, sun_path_len);
      if (connect(dpi_socket, (struct sockaddr *) &dpi_addr, addr_len) == -1) {
         ERRMSG("stop_active_dpis", "connect", errno);
         MSG_ERR("%s\n", dpi_addr.sun_path);
      }
      (void) write(dpi_socket, DpiBye_cmd, strlen(DpiBye_cmd));
      a_Misc_close_fd(dpi_socket);
   }
}

/*! Removes dpis in dpi_attr_list from the
 * set of sockets watched by select and
 * closes their sockets.
 */
void ignore_dpi_sockets(struct dp *dpi_attr_list, int numdpis)
{
   int i;

   for (i = 0; i < numdpis; i++) {
      FD_CLR(dpi_attr_list[i].socket, &sock_set);
      a_Misc_close_fd(dpi_attr_list[i].socket);
   }
}

/*! Registers available dpis and stops active non-filter dpis.
 * Called when dpid receives
 * cmd='register' service='all'
 * command
 * \Return
 * Number of available dpis
 */
int register_all_cmd(char *sockdir)
{
   stop_active_dpis(dpi_attr_list, numdpis);
   rm_dpi_sockets(dpi_attr_list, numdpis);
   free_plugin_list(&dpi_attr_list, numdpis);
   free_services_list(services_list);
   services_list = NULL;
   numdpis = 0;
   numsocks = 1;                /* the srs socket */
   FD_ZERO(&sock_set);
   FD_SET(srs, &sock_set);
   numdpis = register_all(&dpi_attr_list);
   fill_services_list(dpi_attr_list, numdpis, &services_list);
   numsocks = init_all_dpi_sockets(dpi_attr_list, sockdir);
   return (numdpis);
}

/*!
 * Get value of msg field from dpi_tag
 * \Return
 * message on success, NULL on failure
 */
char *get_message(int sock, char *dpi_tag)
{
   char *msg, *d_cmd;

   msg = a_Dpip_get_attr(dpi_tag, strlen(dpi_tag), "msg");
   if (msg == NULL) {
      ERRMSG("get_message", "failed to parse msg", 0);
      d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                               "DpiError", "Failed to parse request");
      (void) CKD_WRITE(sock, d_cmd);
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

   return(dStrncasecmp(A->name, B, len));
}

/*!
 * Send socket path that matches dpi_id to client
 */
void send_sockpath(int sock, char *dpi_tag, struct dp *dpi_attr_list)
{
   int i;
   char *dpi_id;
   char *d_cmd;
   struct service *serv;

   dReturn_if_fail((dpi_id = get_message(sock, dpi_tag)) != NULL);

   serv = dList_find_custom(services_list,dpi_id,(dCompareFunc)service_match);

   if (serv == NULL || (i = serv->dp_index) == -1)
      for (i = 0; i < numdpis; i++)
         if (!strncmp(dpi_attr_list[i].id, dpi_id,
        dpi_attr_list[i].id - strchr(dpi_attr_list[i].id, '.')))
            break;

   if (i < numdpis) {
      /* found */
      if (access(dpi_attr_list[i].path, F_OK) == -1) {
         ERRMSG("send_sockpath", "access", errno);
         MSG_ERR(" - %s\n", dpi_attr_list[i].sockpath);
         d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                                  "DpiError", "Plugin currently unavailable");
         (void) CKD_WRITE(sock, d_cmd);
         dFree(d_cmd);
      } else {
         d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s",
                                  "send_data", dpi_attr_list[i].sockpath);
         (void) CKD_WRITE(sock, d_cmd);
         dFree(d_cmd);
      }
   }

   dFree(dpi_id);
}
