/*
 * File: tls_openssl.h
 *
 * Copyright 2004 Garrett Kajmowicz <gkajmowi@tbaytel.net>
 * (for some bits derived from the https dpi, e.g., certificate handling)
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
 * 2009, 2010, 2011, 2012 Free Software Foundation, Inc.
 * (for the certificate hostname checking from wget)
 * Copyright (C) 2011 Benjamin Johnson <obeythepenguin@users.sourceforge.net>
 * (for the https code offered from dplus browser that formed the basis...)
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

#ifndef __TLS_OPENSSL_H__
#define __TLS_OPENSSL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../url.h"

const char *a_Tls_openssl_version(char *buf, int n);
void a_Tls_openssl_init(void);
int a_Tls_openssl_certificate_is_clean(const DilloUrl *url);
int a_Tls_openssl_connect_ready(const DilloUrl *url);
void a_Tls_openssl_reset_server_state(const DilloUrl *url);
void a_Tls_openssl_connect(int fd, const DilloUrl *url);
void *a_Tls_openssl_connection(int fd);
void a_Tls_openssl_freeall(void);
void a_Tls_openssl_close_by_fd(int fd);
int a_Tls_openssl_read(void *conn, void *buf, size_t len);
int a_Tls_openssl_write(void *conn, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __TLS_OPENSSL_H__ */
