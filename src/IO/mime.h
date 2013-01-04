/*
 * File: mime.h
 *
 * Copyright (C) 2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __MIME_H__
#define __MIME_H__

#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "../cache.h"

typedef void* (*Viewer_t) (const char*, void*, CA_Callback_t*, void**);

/*
 * Function prototypes defined elsewhere
 */
void *a_Html_text (const char *Type,void *web, CA_Callback_t *Call,
                       void **Data);
void *a_Plain_text(const char *Type,void *web, CA_Callback_t *Call,
                       void **Data);
void *a_Dicache_png_image (const char *Type,void *web, CA_Callback_t *Call,
                           void **Data);
void *a_Dicache_gif_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                          void **Data);
void *a_Dicache_jpeg_image(const char *Type, void *Ptr, CA_Callback_t *Call,
                           void **Data);

/*
 * Functions defined inside Mime module
 */
void a_Mime_init(void);
Viewer_t a_Mime_get_viewer(const char *content_type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MIME_H__ */
