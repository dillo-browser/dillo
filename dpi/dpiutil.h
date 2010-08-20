/*
 * File: dpiutil.h
 *
 * Copyright 2004-2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * This file contains common functions used by dpi programs.
 * (i.e. a convenience library).
 */

#ifndef __DPIUTIL_H__
#define __DPIUTIL_H__

#include <stdio.h>
#include "d_size.h"
#include "../dlib/dlib.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Escape URI characters in 'esc_set' as %XX sequences.
 * Return value: New escaped string.
 */
char *Escape_uri_str(const char *str, const char *p_esc_set);

/*
 * Unescape %XX sequences in a string.
 * Return value: a new unescaped string
 */
char *Unescape_uri_str(const char *str);

/*
 * Escape unsafe characters as html entities.
 * Return value: New escaped string.
 */
char *Escape_html_str(const char *str);

/*
 * Unescape a few HTML entities (inverse of Escape_html_str)
 * Return value: New unescaped string.
 */
char *Unescape_html_str(const char *str);

/*
 * Filter an SMTP hack with a FTP URI
 */
char *Filter_smtp_hack(char *url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DPIUTIL_H__ */

