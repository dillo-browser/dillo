#ifndef __PNG_H__
#define __PNG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"


void *a_Png_new(DilloImage *Image, DilloUrl *url, int version);
void a_Png_callback(int Op, CacheClient_t *Client);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__PNG_H__ */
