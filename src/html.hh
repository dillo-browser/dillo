/*
 * File: html.hh
 *
 * Copyright (C) 2005-2009 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __HTML_HH__
#define __HTML_HH__

#include "url.h"               // for DilloUrl

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Keep in sync with the length of the Tags array. It is protected by an
 * static assert in html.cc to prevent errors) */
#define HTML_NTAGS 93

/*
 * Exported functions
 */
void a_Html_load_images(void *v_html, DilloUrl *pattern);
void a_Html_form_submit(void *v_html, void *v_form);
void a_Html_form_reset(void *v_html, void *v_form);
void a_Html_form_display_hiddens(void *v_html, void *v_form, bool_t display);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HTML_HH__ */
