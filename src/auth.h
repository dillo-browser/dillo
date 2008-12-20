#ifndef __AUTH_H__
#define __AUTH_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"


char *a_Auth_get_auth_str(const DilloUrl *request_url);
int a_Auth_do_auth(Dlist *auth_string, const DilloUrl *url);
void a_Auth_init(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__AUTH_H__ */
