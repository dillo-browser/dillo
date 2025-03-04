/*
 * File: compat.h
 *
 * Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMPAT_H
#define COMPAT_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !HAVE_DECL_SETENV
int setenv(const char *name, const char *val, int overwrite);
#endif

//# include <stdlib.h> /* for setenv */

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_H */
