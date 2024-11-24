/*
 * File: tls_mbedtls.h
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
 */

#ifndef __TLS_MBEDTLS_H__
#define __TLS_MBEDTLS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../url.h"

const char *a_Tls_mbedtls_version(char *buf, int n);
void a_Tls_mbedtls_init(void);
int a_Tls_mbedtls_certificate_is_clean(const DilloUrl *url);
int a_Tls_mbedtls_connect_ready(const DilloUrl *url);
void a_Tls_mbedtls_reset_server_state(const DilloUrl *url);
void a_Tls_mbedtls_connect(int fd, const DilloUrl *url);
void *a_Tls_mbedtls_connection(int fd);
void a_Tls_mbedtls_freeall(void);
void a_Tls_mbedtls_close_by_fd(int fd);
int a_Tls_mbedtls_read(void *conn, void *buf, size_t len);
int a_Tls_mbedtls_write(void *conn, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __TLS_MBEDTLS_H__ */
