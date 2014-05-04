/*
 * File: url.c
 *
 * Copyright (C) 2001-2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Parse and normalize all URL's inside Dillo.
 *  - <scheme> <authority> <path> <query> and <fragment> point to 'buffer'.
 *  - 'url_string' is built upon demand (transparent to the caller).
 *  - 'hostname' and 'port' are also being handled on demand.
 */

/*
 * Regular Expression as given in RFC3986 for URL parsing.
 *
 *  ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
 *   12            3  4          5       6  7        8 9
 *
 *  scheme    = $2
 *  authority = $4
 *  path      = $5
 *  query     = $7
 *  fragment  = $9
 *
 *
 *  RFC-2396 BNF:
 *
 *  absoluteURI = scheme ":" (hier_part | opaque_part)
 *  hier_part   = (net_path | abs_path) ["?" query]
 *  net_path    = "//" authority[abs_path]
 *  abs_path    = "/" path_segments
 *
 *  Notes:
 *    - "undefined" means "preceeding separator does not appear".
 *    - path is never "undefined" though it may be "empty".
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "url.h"
#include "msg.h"

static const char *HEX = "0123456789ABCDEF";

/* URL-field compare methods */
#define URL_STR_FIELD_CMP(s1,s2) \
   (s1) && (s2) ? strcmp(s1,s2) : !(s1) && !(s2) ? 0 : (s1) ? 1 : -1
#define URL_STR_FIELD_I_CMP(s1,s2) \
   (s1) && (s2) ? dStrAsciiCasecmp(s1,s2) : !(s1) && !(s2) ? 0 : (s1) ? 1 : -1

/*
 * Return the url as a string.
 * (initializing 'url_string' field if necessary)
 */
char *a_Url_str(const DilloUrl *u)
{
   /* Internal url handling IS transparent to the caller */
   DilloUrl *url = (DilloUrl *) u;

   dReturn_val_if_fail (url != NULL, NULL);

   if (!url->url_string) {
      url->url_string = dStr_sized_new(60);
      dStr_sprintf(
         url->url_string, "%s%s%s%s%s%s%s%s%s%s",
         url->scheme    ? url->scheme : "",
         url->scheme    ? ":" : "",
         url->authority ? "//" : "",
         url->authority ? url->authority : "",
         // (url->path && url->path[0] != '/' && url->authority) ? "/" : "",
         (url->authority && (!url->path || *url->path != '/')) ? "/" : "",
         url->path      ? url->path : "",
         url->query     ? "?" : "",
         url->query     ? url->query : "",
         url->fragment  ? "#" : "",
         url->fragment  ? url->fragment : "");
   }

   return url->url_string->str;
}

/*
 * Return the hostname as a string.
 * (initializing 'hostname' and 'port' fields if necessary)
 * Note: a similar approach can be taken for user:password auth.
 */
const char *a_Url_hostname(const DilloUrl *u)
{
   char *p;
   /* Internal url handling IS transparent to the caller */
   DilloUrl *url = (DilloUrl *) u;

   if (!url->hostname && url->authority) {
      if (url->authority[0] == '[' && (p = strchr(url->authority, ']'))) {
         /* numeric ipv6 address, strip the brackets */
         url->hostname = dStrndup(url->authority + 1,
                                  (uint_t)(p - url->authority - 1));
         if ((p = strchr(p, ':'))) {
            url->port = strtol(p + 1, NULL, 10);
         }
      } else {
         /* numeric ipv4 or hostname */
         if ((p = strchr(url->authority, ':'))) {
            url->port = strtol(p + 1, NULL, 10);
            url->hostname = dStrndup(url->authority,
                                     (uint_t)(p - url->authority));
         } else {
            url->hostname = url->authority;
         }
      }
   }

   return url->hostname;
}

/*
 *  Create a DilloUrl object and initialize it.
 *  (buffer, scheme, authority, path, query and fragment).
 */
static DilloUrl *Url_object_new(const char *uri_str)
{
   DilloUrl *url;
   char *s, *p;

   dReturn_val_if_fail (uri_str != NULL, NULL);

   url = dNew0(DilloUrl, 1);

   /* remove leading & trailing space from buffer */
   url->buffer = dStrstrip(dStrdup(uri_str));

   s = (char *) url->buffer;
   p = strpbrk(s, ":/?#");
   if (p && p[0] == ':' && p > s) {                /* scheme */
      *p = 0;
      url->scheme = s;
      s = ++p;
   }
   /* p = strpbrk(s, "/"); */
   if (p == s && p[0] == '/' && p[1] == '/') {     /* authority */
      s = p + 2;
      p = strpbrk(s, "/?#");
      if (p) {
         memmove(s - 2, s, (size_t)MAX(p - s, 1));
         url->authority = s - 2;
         p[-2] = 0;
         s = p;
      } else if (*s) {
         url->authority = s;
         return url;
      }
   }

   p = strpbrk(s, "?#");
   if (p) {                                        /* path */
      url->path = (p > s) ? s : NULL;
      s = p;
   } else if (*s) {
      url->path = s;
      return url;
   }

   p = strpbrk(s, "?#");
   if (p && p[0] == '?') {                         /* query */
      *p = 0;
      s = p + 1;
      url->query = s;
      p = strpbrk(s, "#");
      url->flags |= URL_Get;
   }
   if (p && p[0] == '#') {                         /* fragment */
      *p = 0;
      s = p + 1;
      url->fragment = s;
   }

   return url;
}

/*
 *  Free a DilloUrl
 *  Do nothing if the argument is NULL
 */
void a_Url_free(DilloUrl *url)
{
   if (url) {
      if (url->url_string)
         dStr_free(url->url_string, TRUE);
      if (url->hostname != url->authority)
         dFree((char *)url->hostname);
      dFree((char *)url->buffer);
      dStr_free(url->data, 1);
      dFree((char *)url->alt);
      dFree(url);
   }
}

/*
 * Resolve the URL as RFC3986 suggests.
 */
static Dstr *Url_resolve_relative(const char *RelStr,
                                  DilloUrl *BaseUrlPar,
                                  const char *BaseStr)
{
   char *p, *s, *e;
   int i;
   Dstr *SolvedUrl, *Path;
   DilloUrl *RelUrl, *BaseUrl = NULL;

   /* parse relative URL */
   RelUrl = Url_object_new(RelStr);

   if (BaseUrlPar) {
      BaseUrl = BaseUrlPar;
   } else if (RelUrl->scheme == NULL) {
      /* only required when there's no <scheme> in RelStr */
      BaseUrl = Url_object_new(BaseStr);
   }

   SolvedUrl = dStr_sized_new(64);
   Path = dStr_sized_new(64);

   /* path empty && scheme and authority undefined */
   if (!RelUrl->path && !RelUrl->scheme && !RelUrl->authority) {
      dStr_append(SolvedUrl, BaseStr);
      if ((p = strchr(SolvedUrl->str, '#')))
         dStr_truncate(SolvedUrl, p - SolvedUrl->str);
      if (!BaseUrl->path)
         dStr_append_c(SolvedUrl, '/');

      if (RelUrl->query) {                        /* query */
         if (BaseUrl->query)
            dStr_truncate(SolvedUrl, BaseUrl->query - BaseUrl->buffer - 1);
         dStr_append_c(SolvedUrl, '?');
         dStr_append(SolvedUrl, RelUrl->query);
      }
      if (RelUrl->fragment) {                    /* fragment */
         dStr_append_c(SolvedUrl, '#');
         dStr_append(SolvedUrl, RelUrl->fragment);
      }
      goto done;

   } else if (RelUrl->scheme) {                  /* scheme */
      dStr_append(SolvedUrl, RelStr);
      goto done;

   } else if (RelUrl->authority) {               /* authority */
      // Set the Path buffer and goto "STEP 7";
      if (RelUrl->path)
         dStr_append(Path, RelUrl->path);

   } else {
      if (RelUrl->path && RelUrl->path[0] == '/') {   /* absolute path */
         ; /* Ignore BaseUrl path */
      } else if (BaseUrl->path) {                     /* relative path */
         dStr_append(Path, BaseUrl->path);
         for (i = Path->len; --i >= 0 && Path->str[i] != '/'; ) ;
         if (i >= 0 && Path->str[i] == '/')
            dStr_truncate(Path, ++i);
      }
      if (RelUrl->path)
         dStr_append(Path, RelUrl->path);

      // erase "./"
      while ((p=strstr(Path->str, "./")) &&
             (p == Path->str || p[-1] == '/'))
         dStr_erase(Path, p - Path->str, 2);
      // erase last "."
      if (Path->len && Path->str[Path->len - 1] == '.' &&
          (Path->len == 1 || Path->str[Path->len - 2] == '/'))
         dStr_truncate(Path, Path->len - 1);

      // erase "<segment>/../" and "<segment>/.."
      s = p = Path->str;
      while ( (p = strstr(p, "/..")) != NULL ) {
         if (p[3] == '/' || !p[3]) { //  "/../" | "/.."
            for (e = p + 3 ; p > s && p[-1] != '/'; --p) ;
            dStr_erase(Path, p - Path->str, e - p + (p > s && *e != 0));
            p -= (p > Path->str);
         } else
            p += 3;
      }
   }

   /* STEP 7
    */

   /* scheme */
   if (BaseUrl->scheme) {
      dStr_append(SolvedUrl, BaseUrl->scheme);
      dStr_append_c(SolvedUrl, ':');
   }

   /* authority */
   if (RelUrl->authority) {
      dStr_append(SolvedUrl, "//");
      dStr_append(SolvedUrl, RelUrl->authority);
   } else if (BaseUrl->authority) {
      dStr_append(SolvedUrl, "//");
      dStr_append(SolvedUrl, BaseUrl->authority);
   }

   /* path */
   if ((RelUrl->authority || BaseUrl->authority) &&
       ((Path->len == 0 && (RelUrl->query || RelUrl->fragment)) ||
        (Path->len && Path->str[0] != '/')))
      dStr_append_c(SolvedUrl, '/'); /* hack? */
   dStr_append(SolvedUrl, Path->str);

   /* query */
   if (RelUrl->query) {
      dStr_append_c(SolvedUrl, '?');
      dStr_append(SolvedUrl, RelUrl->query);
   }

   /* fragment */
   if (RelUrl->fragment) {
      dStr_append_c(SolvedUrl, '#');
      dStr_append(SolvedUrl, RelUrl->fragment);
   }

done:
   dStr_free(Path, TRUE);
   a_Url_free(RelUrl);
   if (BaseUrl != BaseUrlPar)
      a_Url_free(BaseUrl);
   return SolvedUrl;
}

/*
 *  Transform (and resolve) an URL string into the respective DilloURL.
 *  If URL  =  "http://dillo.sf.net:8080/index.html?long#part2"
 *  then the resulting DilloURL should be:
 *  DilloURL = {
 *     url_string         = "http://dillo.sf.net:8080/index.html?long#part2"
 *     scheme             = "http"
 *     authority          = "dillo.sf.net:8080:
 *     path               = "/index.html"
 *     query              = "long"
 *     fragment           = "part2"
 *     hostname           = "dillo.sf.net"
 *     port               = 8080
 *     flags              = URL_Get
 *     data               = Dstr * ("")
 *     alt                = NULL
 *     ismap_url_len      = 0
 *  }
 *
 *  Return NULL if URL is badly formed.
 */
DilloUrl* a_Url_new(const char *url_str, const char *base_url)
{
   DilloUrl *url;
   char *urlstr = (char *)url_str;  /* auxiliar variable, don't free */
   char *p, *str1 = NULL, *str2 = NULL;
   Dstr *SolvedUrl;
   int i, n_ic, n_ic_spc;

   dReturn_val_if_fail (url_str != NULL, NULL);

   /* Count illegal characters (0x00-0x1F, 0x7F-0xFF and space) */
   n_ic = n_ic_spc = 0;
   for (p = (char*)url_str; *p; p++) {
      n_ic_spc += (*p == ' ') ? 1 : 0;
      n_ic += (*p != ' ' && *p > 0x1F && *p < 0x7F) ? 0 : 1;
   }
   if (n_ic) {
      /* Encode illegal characters (they could also be stripped).
       * There's no standard for illegal chars; we chose to encode. */
      p = str1 = dNew(char, strlen(url_str) + 2*n_ic + 1);
      for (i = 0; url_str[i]; ++i)
         if (url_str[i] > 0x1F && url_str[i] < 0x7F && url_str[i] != ' ')
            *p++ = url_str[i];
         else  {
           *p++ = '%';
           *p++ = HEX[(url_str[i] >> 4) & 15];
           *p++ = HEX[url_str[i] & 15];
         }
      *p = 0;
      urlstr = str1;
   }

   /* let's use a heuristic to set http: as default */
   if (!base_url) {
      base_url = "http:";
      if (urlstr[0] != '/') {
         p = strpbrk(urlstr, "/#?:");
         if (!p || *p != ':')
            urlstr = str2 = dStrconcat("//", urlstr, NULL);
      } else if (urlstr[1] != '/')
         urlstr = str2 = dStrconcat("/", urlstr, NULL);
   }

   /* Resolve the URL */
   SolvedUrl = Url_resolve_relative(urlstr, NULL, base_url);
   _MSG("SolvedUrl = %s\n", SolvedUrl->str);

   /* Fill url data */
   url = Url_object_new(SolvedUrl->str);
   url->data = dStr_new("");
   url->url_string = SolvedUrl;
   url->illegal_chars = n_ic;
   url->illegal_chars_spc = n_ic_spc;

   dFree(str1);
   dFree(str2);
   return url;
}


/*
 *  Duplicate a Url structure
 */
DilloUrl* a_Url_dup(const DilloUrl *ori)
{
   DilloUrl *url;

   url = Url_object_new(URL_STR_(ori));
   dReturn_val_if_fail (url != NULL, NULL);

   url->url_string           = dStr_new(URL_STR(ori));
   url->port                 = ori->port;
   url->flags                = ori->flags;
   url->alt                  = dStrdup(ori->alt);
   url->ismap_url_len        = ori->ismap_url_len;
   url->illegal_chars        = ori->illegal_chars;
   url->illegal_chars_spc    = ori->illegal_chars_spc;
   url->data                 = dStr_sized_new(URL_DATA(ori)->len);
   dStr_append_l(url->data, URL_DATA(ori)->str, URL_DATA(ori)->len);
   return url;
}

/*
 *  Compare two Url's to check if they're the same, or which one is bigger.
 *
 *  The fields which are compared here are:
 *  <scheme>, <authority>, <path>, <query> and <data>
 *  Other fields are left for the caller to check
 *
 *  Return value: 0 if equal, > 0 if A > B, < 0 if A < B.
 *
 *  Note: this function defines a sorting order different from strcmp!
 */
int a_Url_cmp(const DilloUrl *A, const DilloUrl *B)
{
   int st;

   dReturn_val_if_fail(A && B, 1);

   if (A == B ||
       ((st = URL_STR_FIELD_I_CMP(A->authority, B->authority)) == 0 &&
        (st = strcmp(A->path ? A->path + (*A->path == '/') : "",
                     B->path ? B->path + (*B->path == '/') : "")) == 0 &&
        //(st = URL_STR_FIELD_CMP(A->path, B->path)) == 0 &&
        (st = URL_STR_FIELD_CMP(A->query, B->query)) == 0 &&
        (st = dStr_cmp(A->data, B->data)) == 0 &&
        (st = URL_STR_FIELD_I_CMP(A->scheme, B->scheme)) == 0))
      return 0;
   return st;
}

/*
 * Set DilloUrl flags
 */
void a_Url_set_flags(DilloUrl *u, int flags)
{
   if (u)
      u->flags = flags;
}

/*
 * Set DilloUrl data (like POST info, etc.)
 */
void a_Url_set_data(DilloUrl *u, Dstr **data)
{
   if (u) {
      dStr_free(u->data, 1);
      u->data = *data;
      *data = NULL;
   }
}

/*
 * Set DilloUrl alt (alternate text to the URL. Used by image maps)
 */
void a_Url_set_alt(DilloUrl *u, const char *alt)
{
   if (u) {
      dFree((char *)u->alt);
      u->alt = dStrdup(alt);
   }
}

/*
 * Set DilloUrl ismap coordinates
 * (this is optimized for not hogging the CPU)
 */
void a_Url_set_ismap_coords(DilloUrl *u, char *coord_str)
{
   dReturn_if_fail (u && coord_str);

   if (!u->ismap_url_len) {
      /* Save base-url length (without coords) */
      u->ismap_url_len  = URL_STR_(u) ? u->url_string->len : 0;
      a_Url_set_flags(u, URL_FLAGS(u) | URL_Ismap);
   }
   if (u->url_string) {
      dStr_truncate(u->url_string, u->ismap_url_len);
      dStr_append(u->url_string, coord_str);
      u->query = u->url_string->str + u->ismap_url_len + 1;
   }
}

/*
 * Given an hex octet (e.g., e3, 2F, 20), return the corresponding
 * character if the octet is valid, and -1 otherwise
 */
static int Url_decode_hex_octet(const char *s)
{
   int hex_value;
   char *tail, hex[3];

   if (s && (hex[0] = s[0]) && (hex[1] = s[1])) {
      hex[2] = 0;
      hex_value = strtol(hex, &tail, 16);
      if (tail - hex == 2)
        return hex_value;
   }
   return -1;
}

/*
 * Parse possible hexadecimal octets in the URI path.
 * Returns a new allocated string.
 */
char *a_Url_decode_hex_str(const char *str)
{
   char *new_str, *dest;
   int i, val;

   if (!str)
      return NULL;

   /* most cases won't have hex octets */
   if (!strchr(str, '%'))
      return dStrdup(str);

   dest = new_str = dNew(char, strlen(str) + 1);

   for (i = 0; str[i]; i++) {
      *dest++ = (str[i] == '%' && (val = Url_decode_hex_octet(str+i+1)) >= 0) ?
                i+=2, val : str[i];
   }
   *dest++ = 0;

   new_str = dRealloc(new_str, sizeof(char) * (dest - new_str));
   return new_str;
}

/*
 * Urlencode 'str'
 * -RL :: According to the RFC 1738, only alphanumerics, the special
 *        characters "$-_.+!*'(),", and reserved characters ";/?:@=&" used
 *        for their *reserved purposes* may be used unencoded within a URL.
 * We'll escape everything but alphanumeric and "-_.*" (as lynx).  --Jcid
 *
 * Note: the content type "application/x-www-form-urlencoded" is used:
 *       i.e., ' ' -> '+' and '\n' -> CR LF (see HTML 4.01, Sec. 17.13.4)
 */
char *a_Url_encode_hex_str(const char *str)
{
   static const char *const verbatim = "-_.*";
   char *newstr, *c;

   if (!str)
      return NULL;

   newstr = dNew(char, 6*strlen(str)+1);

   for (c = newstr; *str; str++)
      if ((dIsalnum(*str) && isascii(*str)) || strchr(verbatim, *str))
         *c++ = *str;
      else if (*str == ' ')
         *c++ = '+';
      else if (*str == '\n') {
         *c++ = '%';
         *c++ = '0';
         *c++ = 'D';
         *c++ = '%';
         *c++ = '0';
         *c++ = 'A';
      } else {
         *c++ = '%';
         *c++ = HEX[(*str >> 4) & 15];
         *c++ = HEX[*str & 15];
      }
   *c = 0;

  return newstr;
}


/*
 * RFC-3986 suggests this stripping when "importing" URLs from other media.
 * Strip: "URL:", enclosing < >, and embedded whitespace.
 * (We also strip illegal chars: 00-1F and 7F-FF)
 */
char *a_Url_string_strip_delimiters(const char *str)
{
   char *p, *new_str, *text;

   new_str = text = dStrdup(str);

   if (new_str) {
      if (strncmp(new_str, "URL:", 4) == 0)
         text += 4;
      if (*text == '<')
         text++;

      for (p = new_str; *text; text++)
         if (*text > 0x1F && *text < 0x7F && *text != ' ')
            *p++ = *text;
      if (p > new_str && p[-1] == '>')
         --p;
      *p = 0;
   }
   return new_str;
}

/*
 * Is the provided hostname an IP address?
 */
static bool_t Url_host_is_ip(const char *host)
{
   uint_t len;

   if (!host || !*host)
      return FALSE;

   len = strlen(host);

   if (len == strspn(host, "0123456789.")) {
      _MSG("an IPv4 address\n");
      return TRUE;
   }
   if (strchr(host, ':') &&
       (len == strspn(host, "0123456789abcdefABCDEF:."))) {
      /* The precise format is shown in section 3.2.2 of rfc 3986 */
      MSG("an IPv6 address\n");
      return TRUE;
   }
   return FALSE;
}

/*
 * How many internal dots are in the public portion of this hostname?
 * e.g., for "www.dillo.org", it is one because everything under "dillo.org",
 * as a .org domain, is part of one organization.
 *
 * Of course this is only a simple and imperfect approximation of
 * organizational boundaries.
 */
static uint_t Url_host_public_internal_dots(const char *host)
{
   uint_t ret = 1;

   if (host) {
      int start, after, tld_len;

      /* We may be able to trust the format of the host string more than
       * I am here. Trailing dots and no dots are real possibilities, though.
       */
      after = strlen(host);
      if (after > 0 && host[after - 1] == '.')
         after--;
      start = after;
      while (start > 0 && host[start - 1] != '.')
         start--;
      tld_len = after - start;

      if (tld_len > 0) {
         /* These TLDs were chosen by examining the current publicsuffix list
          * in October 2014 and picking out those where it was simplest for
          * them to describe the situation by beginning with a "*.[tld]" rule
          * or every rule was "[something].[tld]".
          *
          * TODO: Consider the old publicsuffix code again. This TLD list has
          * shrunk and shrunk over the years, and has become a poorer and
          * poorer approximation of administrative boundaries.
          */
         const char *const tlds[] = {"bd","bn","ck","cy","er","fj","fk",
                                     "gu","il","jm","ke","kh","kw","mm","mz",
                                     "ni","np","pg","ye","za","zm","zw"};
         uint_t i, tld_num = sizeof(tlds) / sizeof(tlds[0]);

         for (i = 0; i < tld_num; i++) {
            if (strlen(tlds[i]) == (uint_t) tld_len &&
                !dStrnAsciiCasecmp(tlds[i], host + start, tld_len)) {
               _MSG("TLD code matched %s\n", tlds[i]);
               ret++;
               break;
            }
         }
      }
   }
   return ret;
}

/*
 * Given a URL host string, return the portion that is public, i.e., the
 * domain that is in a registry outside the organization.
 * For 'www.dillo.org', that would be 'dillo.org'.
 */
static const char *Url_host_find_public_suffix(const char *host)
{
   const char *s;
   uint_t dots;

   if (!host || !*host || Url_host_is_ip(host))
      return host;

   s = host;

   while (s[1])
      s++;

   if (s > host && *s == '.') {
      /* don't want to deal with trailing dot */
      s--;
   }

   dots = Url_host_public_internal_dots(host);

   /* With a proper host string, we should not be pointing to a dot now. */

   while (s > host) {
      if (s[-1] == '.') {
         if (dots == 0)
            break;
         else
            dots--;
      }
      s--;
   }

   _MSG("public suffix of %s is %s\n", host, s);
   return s;
}

bool_t a_Url_same_organization(const DilloUrl *u1, const DilloUrl *u2)
{
   if (!u1 || !u2)
      return FALSE;

   return dStrAsciiCasecmp(Url_host_find_public_suffix(URL_HOST(u1)),
                           Url_host_find_public_suffix(URL_HOST(u2)))
          ? FALSE : TRUE;
}
