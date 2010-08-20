#ifndef __JPEG_H__
#define __JPEG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"


void *a_Jpeg_new(DilloImage *Image, DilloUrl *url, int version);
void a_Jpeg_callback(int Op, void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__JPEG_H__ */
