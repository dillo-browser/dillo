#ifndef __PNG_H__
#define __PNG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"
#include "cache.h"


void *a_Png_new(DilloImage *Image, DilloUrl *url, int version);
void a_Png_callback(int Op, CacheClient_t *Client);
const char *a_Png_version(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__PNG_H__ */
