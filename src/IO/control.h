/*
 * File: control.h
 *
 * Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef IO_CONTROL_H
#define IO_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "bw.h"

int a_Control_init(void);
int a_Control_is_waiting(void);
void a_Control_notify_finish(BrowserWindow *bw);
int a_Control_free(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IO_CONTROL_H */
