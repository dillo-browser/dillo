/*
 * File: actions.h
 *
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ACTIONS_H
#define ACTIONS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "dlib/dlib.h"

typedef struct {
   char *label;
   char *cmd;
} Action;

void a_Actions_init(void);
Dlist *a_Actions_link_get(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* ACTIONS_H*/
