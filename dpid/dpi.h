/*! \file
 * Access functions for  ~/.dillo/dpi_socket_dir.
 * The most useful function for dillo is a_Dpi_srs, it returns
 * the full path to the dpid service request socket.
 */

#ifndef DPI_H
#define DPI_H

#include <unistd.h>      /* for socklen_t */
#include <sys/socket.h>  /* for socklen_t and AF_LOCAL */

/* Some systems may not have this one... */
#ifndef AF_LOCAL
   #define AF_LOCAL AF_UNIX
#endif

/* This one is tricky, some sources state it should include the byte
 * for the terminating NULL, and others say it shouldn't.
 * The other way is to only use this one when a native SUN_LEN is not present,
 * but as dillo has used this for a long time successfully, here it goes.
 */
# define D_SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) \
                        + strlen ((ptr)->sun_path))

/*!
 * dpi commands
 */
enum {
   UNKNOWN_CMD,
   AUTH_CMD, /* authentication */
   BYE_CMD,  /* "DpiBye" */
   CHECK_SERVER_CMD, /* "check_server" */
   REGISTER_ALL_CMD, /* "register_all" */
   REGISTER_SERVICE_CMD /* "register_service" */
};


char *a_Dpi_sockdir_file(void);

char *a_Dpi_rd_dpi_socket_dir(char *dirname);

char *a_Dpi_srs(void);

#endif
