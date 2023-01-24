/*! \file
 * Main functions to set-up dpi information and to initialise sockets
 */

#ifndef DPID_H
#define DPID_H

#include <sys/socket.h>
#include <sys/select.h>   /* for fd_set */
#include <sys/un.h>
#include <signal.h>       /* for sig_atomic_t */
#include <netinet/in.h>   /* for ntohl, IPPORT_USERRESERVED and stuff */

#include "d_size.h"

/* FreeBSD 6.4 doesn't have it */
#ifndef IPPORT_USERRESERVED
 #define IPPORT_USERRESERVED 5000
#endif

#define PATH_LEN 50
#define CMDLEN 20
#define MSGLEN 50
#define DPID_BASE_PORT (IPPORT_USERRESERVED + 20)

/*! \TODO: Should read this from dillorc */
#define SRS_NAME "dpid.srs"
extern char *srs_name;

/*! dpid's service request socket file descriptor */
extern int srs_fd;

/*! plugin state information
 */
struct dp {
   char *id;
   char *path;
   int sock_fd;
   int port;
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
extern int numdpis;

/*! Number of sockets being watched */
extern int numsocks;

/*! State information for each plugin. */
extern struct dp *dpi_attr_list;

/*! service served for each plugin  */
extern Dlist *services_list;

/*! Set of sockets watched for connections */
extern fd_set sock_set;

/*! Set to 1 by the SIGCHLD handler dpi_sigchld */
extern volatile sig_atomic_t caught_sigchld;

void rm_dpi_sockets(struct dp *dpi_attr_list, int numdpis);

void cleanup();

void free_dpi_attr(struct dp *dpi_attr);

void free_plugin_list(struct dp **dpi_attr_list_ptr, int numdpis);

void free_services_list(Dlist *s_list);

enum file_type get_file_type(char *file_name);

int get_dpi_attr(char *dpi_dir, char *service, struct dp *dpi_attr);

int register_service(struct dp *dpi_attr, char *service);

int register_all(struct dp **attlist);

int fill_services_list(struct dp *attlist, int numdpis, Dlist **services_list);

int init_ids_srs_socket();

int init_dpi_socket(struct dp *dpi_attr);

int init_all_dpi_sockets(struct dp *dpi_attr_list);

void dpi_sigchld(int sig);

void handle_sigchld(void);

void est_dpi_sigchld(void);

void est_dpi_terminator(void);

void stop_active_dpis(struct dp *dpi_attr_list, int numdpis);

void ignore_dpi_sockets(struct dp *dpi_attr_list, int numdpis);

int register_all_cmd();

char *get_message(int sock, char *dpi_tag);

int service_match(const struct service *A, const char *B);

void send_sockport(int sock_fd, char * dpi_tag, struct dp *dpi_attr_list);

#endif
