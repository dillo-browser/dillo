/*
 * File : url.h - Dillo
 *
 * Copyright (C) 2001 Jorge Arellano Cid <jcid@dillo.org>
 *
 * Parse and normalize all URL's inside Dillo.
 */

#ifndef __URL_H__
#define __URL_H__

#include "d_size.h"
#include "../dlib/dlib.h"


#define DILLO_URL_HTTP_PORT        80
#define DILLO_URL_HTTPS_PORT       443
#define DILLO_URL_FTP_PORT         21
#define DILLO_URL_MAILTO_PORT      25
#define DILLO_URL_NEWS_PORT        119
#define DILLO_URL_TELNET_PORT      23
#define DILLO_URL_GOPHER_PORT      70


/*
 * Values for DilloUrl->flags.
 * Specifies which which action to perform with an URL.
 */
#define URL_Get                 (1 << 0)
#define URL_Post                (1 << 1)
#define URL_ISindex             (1 << 2)
#define URL_Ismap               (1 << 3)
#define URL_RealmAccess         (1 << 4)

#define URL_E2EQuery            (1 << 5)
#define URL_ReloadImages        (1 << 6)
#define URL_ReloadPage          (1 << 7)
#define URL_ReloadFromCache     (1 << 8)

#define URL_IgnoreScroll        (1 << 9)
#define URL_SpamSafe            (1 << 10)

#define URL_MultipartEnc        (1 << 11)

/*
 * Access methods to fields inside DilloURL.
 * (non '_'-ended macros MUST use these for initialization sake)
 */
/* these MAY return NULL: */
#define URL_SCHEME_(u)              (u)->scheme
#define URL_AUTHORITY_(u)           (u)->authority
#define URL_PATH_(u)                (u)->path
#define URL_QUERY_(u)               (u)->query
#define URL_FRAGMENT_(u)            (u)->fragment
#define URL_HOST_(u)                a_Url_hostname(u)
#define URL_ALT_(u)                 (u)->alt
#define URL_STR_(u)                 a_Url_str(u)
/* this returns a Dstr* */
#define URL_DATA_(u)                (u)->data
/* these return an integer */
#define URL_PORT_(u)                (URL_HOST(u), (u)->port)
#define URL_FLAGS_(u)               (u)->flags
#define URL_ILLEGAL_CHARS_(u)       (u)->illegal_chars
#define URL_ILLEGAL_CHARS_SPC_(u)   (u)->illegal_chars_spc

/*
 * Access methods that never return NULL.
 * When the "empty" and "undefined" concepts of RFC-2396 are irrelevant to
 * the caller, and a string is required, use these methods instead:
 */
#define NPTR2STR(p)                 ((p) ? (p) : "")
#define URL_SCHEME(u)               NPTR2STR(URL_SCHEME_(u))
#define URL_AUTHORITY(u)            NPTR2STR(URL_AUTHORITY_(u))
#define URL_PATH(u)                 NPTR2STR(URL_PATH_(u))
#define URL_QUERY(u)                NPTR2STR(URL_QUERY_(u))
#define URL_FRAGMENT(u)             NPTR2STR(URL_FRAGMENT_(u))
#define URL_HOST(u)                 NPTR2STR(URL_HOST_(u))
#define URL_DATA(u)                 URL_DATA_(u)
#define URL_ALT(u)                  NPTR2STR(URL_ALT_(u))
#define URL_STR(u)                  NPTR2STR(URL_STR_(u))
#define URL_PORT(u)                 URL_PORT_(u)
#define URL_FLAGS(u)                URL_FLAGS_(u)
#define URL_ILLEGAL_CHARS(u)        URL_ILLEGAL_CHARS_(u)
#define URL_ILLEGAL_CHARS_SPC(u)    URL_ILLEGAL_CHARS_SPC_(u)


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
   Dstr  *url_string;
   const char *buffer;
   const char *scheme;            /**/
   const char *authority;         /**/
   const char *path;              /* These are references only */
   const char *query;             /* (no need to free them) */
   const char *fragment;          /**/
   const char *hostname;          /**/
   int port;
   int flags;
   Dstr *data;                    /* POST */
   const char *alt;               /* "alt" text (used by image maps) */
   int ismap_url_len;             /* Used by server side image maps */
   int illegal_chars;             /* number of illegal chars */
   int illegal_chars_spc;         /* number of illegal space chars */
} DilloUrl;


DilloUrl* a_Url_new(const char *url_str, const char *base_url);
void a_Url_free(DilloUrl *u);
char *a_Url_str(const DilloUrl *url);
const char *a_Url_hostname(const DilloUrl *u);
DilloUrl* a_Url_dup(const DilloUrl *u);
int a_Url_cmp(const DilloUrl *A, const DilloUrl *B);
void a_Url_set_flags(DilloUrl *u, int flags);
void a_Url_set_data(DilloUrl *u, Dstr **data);
void a_Url_set_alt(DilloUrl *u, const char *alt);
void a_Url_set_ismap_coords(DilloUrl *u, char *coord_str);
char *a_Url_decode_hex_str(const char *str);
char *a_Url_encode_hex_str(const char *str);
char *a_Url_string_strip_delimiters(const char *str);
bool_t a_Url_same_organization(const DilloUrl *u1, const DilloUrl *u2);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __URL_H__ */
