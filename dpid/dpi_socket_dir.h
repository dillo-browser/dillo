/*! \file
 * Create a per user temporary directory for dpi sockets
 */

#ifndef DPI_SOCKET_DIR_H
#define DPI_SOCKET_DIR_H


int w_dpi_socket_dir(char *dirname, char *sockdir);

int tst_dir(char *dir);

char *mk_sockdir(void);

char *init_sockdir(char *dpi_socket_dir);

#endif
