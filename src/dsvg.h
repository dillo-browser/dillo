#ifndef __SVG_H__
#define __SVG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"


void *a_Svg_new(DilloImage *Image, DilloUrl *url, int version);
void a_Svg_callback(int Op, CacheClient_t *Client);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__SVG_H__ */
