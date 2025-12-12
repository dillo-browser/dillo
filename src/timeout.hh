/*
 * File: timeout.hh
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __TIMEOUT_HH__
#define __TIMEOUT_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*TimeoutCb_t)(void *data);

void a_Timeout_add(float t, TimeoutCb_t cb, void *cbdata);
void a_Timeout_repeat(float t, TimeoutCb_t cb, void *cbdata);
void a_Timeout_remove(void);
void a_Timeout_actually_remove(TimeoutCb_t cb, void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TIMEOUT_HH__ */

