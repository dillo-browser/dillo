/*
 * File: tls.h
 *
 * Copyright (C) 2011 Benjamin Johnson <obeythepenguin@users.sourceforge.net>
 * (for the https code offered from dplus browser that formed the basis...)
 * Copyright 2016 corvid
 * Copyright (C) 2023-2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * As a special exception, permission is granted to link Dillo with the OpenSSL
 * or LibreSSL library, and distribute the linked executables without
 * including the source code for OpenSSL or LibreSSL in the source
 * distribution. You must obey the GNU General Public License, version 3, in
 * all respects for all of the code used other than OpenSSL or LibreSSL.
 */

#ifndef __TLS_H__
#define __TLS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../url.h"

#define TLS_CONNECT_NEVER -1
#define TLS_CONNECT_NOT_YET 0
#define TLS_CONNECT_READY 1

const char *a_Tls_version(char *buf, int n);
void a_Tls_init(void);
int a_Tls_certificate_is_clean(const DilloUrl *url);
int a_Tls_connect_ready(const DilloUrl *url);
void a_Tls_reset_server_state(const DilloUrl *url);
void a_Tls_connect(int fd, const DilloUrl *url);
void *a_Tls_connection(int fd);
void a_Tls_freeall(void);
void a_Tls_close_by_fd(int fd);
int a_Tls_read(void *conn, void *buf, size_t len);
int a_Tls_write(void *conn, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __TLS_H__ */

