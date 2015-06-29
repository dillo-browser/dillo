#ifndef __HSTS_H__
#define __HSTS_H__

#include "d_size.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void  a_Hsts_init(FILE *fp);
void  a_Hsts_set(const char *header, const DilloUrl *url);
bool_t a_Hsts_require_https(const char *host);
void  a_Hsts_freeall( void );

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__HSTS_H__ */
