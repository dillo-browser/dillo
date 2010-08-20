#ifndef __GIF_H__
#define __GIF_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"


void *a_Gif_new(DilloImage *Image, DilloUrl *url, int version);
void a_Gif_callback(int Op, void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__GIF_H__ */
