#ifndef __WEBP_H__
#define __WEBP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "url.h"
#include "image.hh"


void *a_Webp_new(DilloImage *Image, DilloUrl *url, int version);
void a_Webp_callback(int Op, void *data);
const char *a_Webp_version(char *buf, int n);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__WEBP_H__ */
