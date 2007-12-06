/*! \file
 * Main functions to set-up dpi information and to initialise sockets
 */

#ifndef DPID_H
#define DPID_H

#include <assert.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <errno.h>

#include "d_size.h"


#define PATH_LEN 50
#define CMDLEN 20
#define MSGLEN 50
/*! \todo: Should read this from dillorc */
#define SRS_NAME "dpid.srs"
char *srs_name;

/*! dpid service request socket */
int srs;

/*! plugin state information
 */
struct dp {
   char *id;
   char *path;
   char *sockpath;
   int socket;
   struct sockaddr_un sa;
   pid_t pid;
   int filter;
};

/*! bind dpi with service
 */
struct service {
   char *name;
   int dp_index;
};

/*! Number of available plugins */
int numdpis;

/*! Number of sockets being watched */
int numsocks;

/*! State information for each plugin. */
struct dp *dpi_attr_list;

/*! service served for each plugin  */
Dlist *services_list;

/*! Set of sockets watched for connections */
fd_set sock_set;

/*! Set to 1 by the SIGCHLD handler dpi_sigchld */
extern volatile sig_atomic_t caught_sigchld;

void rm_dpi_sockets(struct dp *dpi_attr_list, int numdpis);

int rm_inactive_dpi_sockets(struct dp *dpi_attr_list, int numdpis);

void cleanup(char *socket_dir);

void free_dpi_attr(struct dp *dpi_attr);

void free_plugin_list(struct dp **dpi_attr_list_ptr, int numdpis);

void free_services_list(Dlist *s_list);

enum file_type get_file_type(char *file_name);

int get_dpi_attr(char *dpi_dir, char *service, struct dp *dpi_attr);

int register_service(struct dp *dpi_attr, char *service);

int register_all(struct dp **attlist);

int fill_services_list(struct dp *attlist, int numdpis, Dlist **services_list);

int init_srs_socket(char *sockdir);

int init_dpi_socket(struct dp *dpi_attr, char *sockdir);

int init_all_dpi_sockets(struct dp *dpi_attr_list, char *sockdir);

void dpi_sigchld(int sig);

void handle_sigchld(void);

void est_dpi_sigchld(void);

void stop_active_dpis(struct dp *dpi_attr_list, int numdpis);

void ignore_dpi_sockets(struct dp *dpi_attr_list, int numdpis);

int register_all_cmd(char *sockdir);

char *get_message(int sock, char *dpi_tag);

int service_match(const struct service *A, const char *B);

void send_sockpath(int sock, char * dpi_tag, struct dp *dpi_attr_list);

#endif
