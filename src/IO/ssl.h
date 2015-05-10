#ifndef __SSL_H__
#define __SSL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../url.h"

#define SSL_CONNECT_NEVER -1
#define SSL_CONNECT_NOT_YET 0
#define SSL_CONNECT_READY 1

void a_Ssl_init();


#ifdef ENABLE_SSL
int a_Ssl_connect_ready(const DilloUrl *url);
void a_Ssl_reset_server_state(const DilloUrl *url);

/* Use to initiate a SSL connection. */
void a_Ssl_handshake(int fd, const DilloUrl *url);

void *a_Ssl_connection(int fd);

void a_Ssl_freeall();

void a_Ssl_close_by_fd(int fd);
int a_Ssl_read(void *conn, void *buf, size_t len);
int a_Ssl_write(void *conn, void *buf, size_t len);
#else

#define a_Ssl_connect_ready(url) SSL_CONNECT_NEVER
#define a_Ssl_reset_server_state(url) ;
#define a_Ssl_handshake(fd, url) ;
#define a_Ssl_connection(fd) NULL
#define a_Ssl_freeall() ;
#define a_Ssl_close_by_fd(fd) ;
#define a_Ssl_read(conn, buf, len) 0
#define a_Ssl_write(conn, buf, len) 0
#endif
#ifdef __cplusplus
}
#endif

#endif /* __SSL_H__ */

