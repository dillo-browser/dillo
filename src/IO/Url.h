#ifndef __IO_URL_H__
#define __IO_URL_H__

#include "../chain.h"
#include "../url.h"
#include "../../dlib/dlib.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * External functions
 */
extern void a_Http_freeall(void);
int a_Http_init(void);
int a_Http_proxy_auth(void);
void a_Http_set_proxy_passwd(const char *str);
char *a_Http_make_connect_str(const DilloUrl *url);
const char *a_Http_get_proxy_urlstr();
Dstr *a_Http_make_query_str(const DilloUrl *url, const DilloUrl *requester,
                            int web_flags, bool_t use_proxy);

void a_Http_ccc (int Op, int Branch, int Dir, ChainLink *Info,
                 void *Data1, void *Data2);
void a_IO_ccc   (int Op, int Branch, int Dir, ChainLink *Info,
                 void *Data1, void *Data2);
void a_Dpi_ccc  (int Op, int Branch, int Dir, ChainLink *Info,
                 void *Data1, void *Data2);

char *a_Dpi_send_blocking_cmd(const char *server_name, const char *cmd);
void a_Dpi_dillo_exit(void);
void a_Dpi_init(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IO_URL_H__ */

