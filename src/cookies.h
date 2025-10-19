/*
 * File: cookies.h
 *
 * Copyright 2001 Lars Clausen   <lrclause@cs.uiuc.edu>
 *                Jörgen Viksell <jorgen.viksell@telia.com>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __COOKIES_H__
#define __COOKIES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void  a_Cookies_init( void );

#ifdef DISABLE_COOKIES
# define a_Cookies_get_query(url, requester, r)  dStrdup("")
# define a_Cookies_set()     ;
# define a_Cookies_freeall() ;
#else
  char *a_Cookies_get_query(const DilloUrl *query_url,
                            const DilloUrl *requester,
                            int is_root_url);
  void  a_Cookies_set(Dlist *cookie_string, const DilloUrl *set_url,
                      const char *server_date);
  void  a_Cookies_freeall( void );
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__COOKIES_H__ */
