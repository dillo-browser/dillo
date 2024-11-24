/*
 * File: tls.c
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

#include "config.h"
#include "../msg.h"

#include "tls.h"
#include "tls_openssl.h"
#include "tls_mbedtls.h"

/**
 * Get the version of the TLS library.
 */
const char *a_Tls_version(char *buf, int n)
{
#if ! defined(ENABLE_TLS)
   return NULL;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_version(buf, n);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_version(buf, n);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

/**
 * Initialize TLS library.
 */
void a_Tls_init(void)
{
#if ! defined(ENABLE_TLS)
   MSG("TLS: Disabled at compilation time.\n");
#elif defined(HAVE_OPENSSL)
   a_Tls_openssl_init();
#elif defined(HAVE_MBEDTLS)
   a_Tls_mbedtls_init();
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

/**
 * Return TLS connection information for a given file
 * descriptor, or NULL if no TLS connection was found.
 */
void *a_Tls_connection(int fd)
{
#if ! defined(ENABLE_TLS)
   return NULL;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_connection(fd);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_connection(fd);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

/**
 * The purpose here is to permit a single initial connection to a server.
 * Once we have the certificate, know whether we like it -- and whether the
 * user accepts it -- HTTP can run through queued sockets as normal.
 *
 * Return: TLS_CONNECT_READY or TLS_CONNECT_NOT_YET or TLS_CONNECT_NEVER.
 */
int a_Tls_connect_ready(const DilloUrl *url)
{
#if ! defined(ENABLE_TLS)
   return TLS_CONNECT_NEVER;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_connect_ready(url);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_connect_ready(url);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

/**
 * Did everything seem proper with the certificate -- no warnings to
 * click through?.
 */
int a_Tls_certificate_is_clean(const DilloUrl *url)
{
#if ! defined(ENABLE_TLS)
   return 0;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_certificate_is_clean(url);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_certificate_is_clean(url);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

/**
 * Clean up the TLS library.
 */
void a_Tls_freeall(void)
{
#if ! defined(ENABLE_TLS)
   return;
#elif defined(HAVE_OPENSSL)
   a_Tls_openssl_freeall();
#elif defined(HAVE_MBEDTLS)
   a_Tls_mbedtls_freeall();
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}


void a_Tls_reset_server_state(const DilloUrl *url)
{
#if ! defined(ENABLE_TLS)
   return;
#elif defined(HAVE_OPENSSL)
   a_Tls_openssl_reset_server_state(url);
#elif defined(HAVE_MBEDTLS)
   a_Tls_mbedtls_reset_server_state(url);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

void a_Tls_connect(int fd, const DilloUrl *url)
{
#if ! defined(ENABLE_TLS)
   return;
#elif defined(HAVE_OPENSSL)
   a_Tls_openssl_connect(fd, url);
#elif defined(HAVE_MBEDTLS)
   a_Tls_mbedtls_connect(fd, url);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

void a_Tls_close_by_fd(int fd)
{
#if ! defined(ENABLE_TLS)
   return;
#elif defined(HAVE_OPENSSL)
   a_Tls_openssl_close_by_fd(fd);
#elif defined(HAVE_MBEDTLS)
   a_Tls_mbedtls_close_by_fd(fd);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

int a_Tls_read(void *conn, void *buf, size_t len)
{
#if ! defined(ENABLE_TLS)
   return 0;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_read(conn, buf, len);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_read(conn, buf, len);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}

int a_Tls_write(void *conn, void *buf, size_t len)
{
#if ! defined(ENABLE_TLS)
   return 0;
#elif defined(HAVE_OPENSSL)
   return a_Tls_openssl_write(conn, buf, len);
#elif defined(HAVE_MBEDTLS)
   return a_Tls_mbedtls_write(conn, buf, len);
#else
# error "no TLS library found but ENABLE_TLS set"
#endif
}
