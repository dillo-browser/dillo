#ifndef __TLS_H__
#define __TLS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../url.h"

#define TLS_CONNECT_NEVER -1
#define TLS_CONNECT_NOT_YET 0
#define TLS_CONNECT_READY 1

void a_Tls_init();


#ifdef ENABLE_SSL
int a_Tls_connect_ready(const DilloUrl *url);
void a_Tls_reset_server_state(const DilloUrl *url);

/* Use to initiate a TLS connection. */
void a_Tls_handshake(int fd, const DilloUrl *url);

void *a_Tls_connection(int fd);

void a_Tls_freeall();

void a_Tls_close_by_fd(int fd);
int a_Tls_read(void *conn, void *buf, size_t len);
int a_Tls_write(void *conn, void *buf, size_t len);
#else

#define a_Tls_connect_ready(url) TLS_CONNECT_NEVER
#define a_Tls_reset_server_state(url) ;
#define a_Tls_handshake(fd, url) ;
#define a_Tls_connection(fd) NULL
#define a_Tls_freeall() ;
#define a_Tls_close_by_fd(fd) ;
#define a_Tls_read(conn, buf, len) 0
#define a_Tls_write(conn, buf, len) 0
#endif
#ifdef __cplusplus
}
#endif

#endif /* __TLS_H__ */

