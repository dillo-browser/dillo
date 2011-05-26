/*
 * File: auth.c
 *
 * Copyright 2008 Jeremy Henty   <onepoint@starurchin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/* Handling of HTTP AUTH takes place here.
 * This implementation aims to follow RFC 2617:
 * http://www.ietf.org/rfc/rfc2617.txt
 */


#include <ctype.h> /* iscntrl */
#include "auth.h"
#include "msg.h"
#include "misc.h"
#include "dialog.hh"
#include "../dlib/dlib.h"


typedef struct {
   int ok;
   const char *realm;
} AuthParse_t;

typedef struct {
   char *name;
   Dlist *paths; /* stripped of any trailing '/', so the root path is "" */
   char *authorization; /* the authorization request header */
} AuthRealm_t;

typedef struct {
   char *scheme;
   char *authority;
   Dlist *realms;
} AuthHost_t;

typedef struct {
   const char *realm_name;
   const DilloUrl *url;
} AuthDialogData_t;

/*
 *  Local data
 */
static Dlist *auth_hosts;

/*
 * Initialize the auth module.
 */
void a_Auth_init(void)
{
   auth_hosts = dList_new(1);
}

static AuthParse_t *Auth_parse_new()
{
   AuthParse_t *auth_parse = dNew(AuthParse_t, 1);
   auth_parse->ok = 0;
   auth_parse->realm = NULL;
   return auth_parse;
}

static void Auth_parse_free(AuthParse_t *auth_parse)
{
   if (auth_parse) {
      dFree((void *)auth_parse->realm);
      dFree(auth_parse);
   }
}

static int Auth_path_is_inside(const char *path1, const char *path2, int len)
{
   /*
    * path2 is effectively truncated to length len.  Typically len will be
    * strlen(path2), or 1 less when we want to ignore a trailing '/'.
    */
   return
      strncmp(path1, path2, len) == 0 &&
      (path1[len] == '\0' || path1[len] == '/');
}

/*
 * Check valid chars.
 * Return: 0 if invalid, 1 otherwise.
 */
static int Auth_is_token_char(char c)
{
   const char *invalid = "\"()<>@,;:\\[]?=/{} \t";
   return (strchr(invalid, c) || iscntrl((uchar_t)c)) ? 0 : 1;
}

/*
 * Unquote the content of a quoted string.
 * Return: newly allocated unquoted content.
 *
 * Arguments:
 * quoted: pointer to the first char *after* the initial double quote.
 * size: the number of chars in the result, *not* including a final '\0'.
 *
 * Preconditions:
 * quoted points to a correctly quoted and escaped string.
 * size is the number of characters in the quoted string, *after*
 * removing escape characters.
 *
 */
static const char *Auth_unquote_value(const char *quoted, int size)
{
   char c, *value, *value_ptr;
   value_ptr = value = dNew(char, size + 1);
   while ((c = *quoted++) != '"')
      *value_ptr++ = (c == '\\') ? *quoted++ : c;
   *value_ptr = '\0';
   return value;
}

/*
 * Parse a quoted string. Save the result as the auth realm if required.
 * Return: 1 if the parse succeeds, 0 otherwise.
 */
static int Auth_parse_quoted_string(AuthParse_t *auth_parse, int set_realm,
                                    char **auth)
{
   char *value;
   int size;

   /* parse the '"' */
   switch (*(*auth)++) {
   case '"':
      break;
   case '\0':
   case ',':
      MSG("auth.c: missing Basic auth token value after '='\n");
      return 0;
      break;
   default:
      MSG("auth.c: garbage in Basic auth after '='\n");
      return 0;
      break;
   }

   /* parse the rest */
   value = *auth;
   size = 0;
   while (1) {
      switch (*(*auth)++) {
      case '"':
         if (set_realm) {
            dFree((void *)auth_parse->realm);
            auth_parse->realm = Auth_unquote_value(value, size);
            auth_parse->ok = 1;
         }
         return 1;
         break;
      case '\0':
         MSG("auth.c: auth string ended inside quoted string value\n");
         return 0;
         break;
      case '\\':
         /* end of string? */
         if (!*(*auth)++) {
            MSG("auth.c: "
                "auth string ended inside quoted string value "
                "immediately after \\\n");
            return 0;
         }
         /* fall through to the next case */
      default:
         size++;
         break;
      }
   }
}

/*
 * Parse a token-value pair.
 * Return: 1 if the parse succeeds, 0 otherwise.
 */
static int Auth_parse_token_value(AuthParse_t *auth_parse, char **auth)
{
   char *token;
   int token_size, set_realm;
   static const char realm_token[] = "realm";

   /* parse a token */
   token = *auth;
   token_size = 0;
   while (Auth_is_token_char(**auth)) {
      (*auth)++;
      token_size++;
   }
   if (token_size == 0) {
      MSG("auth.c: Auth_parse_token_value: "
          "missing Basic auth token\n");
      return 0;
   }

   /* skip space characters */
   while (**auth == ' ')
      (*auth)++;

   /* parse the '=' */
   switch (*(*auth)++) {
   case '=':
      break;
   case '\0':
   case ',':
      MSG("auth.c: Auth_parse_token_value: "
          "missing Basic auth token value\n");
      return 0;
      break;
   default:
      MSG("auth.c: Auth_parse_token_value: "
          "garbage after Basic auth token\n");
      return 0;
      break;
   }

   /* skip space characters */
   while (**auth == ' ')
      (*auth)++;

   /* is this value the realm? */
   set_realm =
      auth_parse->realm == NULL &&
      dStrncasecmp(realm_token,token,token_size) == 0 &&
      strlen(realm_token) == (size_t)token_size;

   return Auth_parse_quoted_string(auth_parse, set_realm, auth);
}

static void Auth_parse_auth_basic(AuthParse_t *auth_parse, char **auth)
{
   int token_value_pairs_found;

   /* parse comma-separated token-value pairs */
   token_value_pairs_found = 0;
   while (1) {
      /* skip space and comma characters */
      while (**auth == ' ' || **auth == ',')
         (*auth)++;
      /* end of string? */
      if (!**auth)
         break;
      /* parse token-value pair */
      if (!Auth_parse_token_value(auth_parse, auth))
         break;
      token_value_pairs_found = 1;
   }

   if (!token_value_pairs_found) {
      MSG("auth.c: Auth_parse_auth_basic: "
          "missing Basic auth token-value pairs\n");
      return;
   }

   if (!auth_parse->realm) {
      MSG("auth.c: Auth_parse_auth_basic: "
          "missing Basic auth realm\n");
      return;
   }
}

static void Auth_parse_auth(AuthParse_t *auth_parse, char *auth)
{
   _MSG("auth.c: Auth_parse_auth: auth = '%s'\n", auth);
   if (dStrncasecmp(auth, "Basic ", 6) == 0) {
      auth += 6;
      Auth_parse_auth_basic(auth_parse, &auth);
   } else {
      MSG("auth.c: Auth_parse_auth: "
          "unknown authorization scheme: auth = {%s}\n",
          auth);
   }
}

/*
 * Return the host that contains a URL, or NULL if there is no such host.
 */
static AuthHost_t *Auth_host_by_url(const DilloUrl *url)
{
   AuthHost_t *host;
   int i;

   for (i = 0; (host = dList_nth_data(auth_hosts, i)); i++)
      if (((dStrcasecmp(URL_SCHEME(url), host->scheme) == 0) &&
           (dStrcasecmp(URL_AUTHORITY(url), host->authority) == 0)))
         return host;

   return NULL;
}

/*
 * Search all realms for the one with the given name.
 */
static AuthRealm_t *Auth_realm_by_name(const AuthHost_t *host,
                                           const char *name)
{
   AuthRealm_t *realm;
   int i;

   for (i = 0; (realm = dList_nth_data(host->realms, i)); i++)
      if (strcmp(realm->name,name) == 0)
         return realm;

   return NULL;
}

/*
 * Search all realms for the one with the best-matching path.
 */
static AuthRealm_t *Auth_realm_by_path(const AuthHost_t *host,
                                       const char *path)
{
   AuthRealm_t *realm_best, *realm;
   int i, j;
   int match_length;

   match_length = 0;
   realm_best = NULL;
   for (i = 0; (realm = dList_nth_data(host->realms, i)); i++) {
      char *realm_path;

      for (j = 0; (realm_path = dList_nth_data(realm->paths, j)); j++) {
         int realm_path_length = strlen(realm_path);
         if (Auth_path_is_inside(path, realm_path, realm_path_length) &&
             !(realm_best && match_length >= realm_path_length)) {
            realm_best = realm;
            match_length = realm_path_length;
         }
      }
   }

   return realm_best;
}

static int Auth_realm_includes_path(const AuthRealm_t *realm, const char *path)
{
   int i;
   char *realm_path;

   for (i = 0; (realm_path = dList_nth_data(realm->paths, i)); i++)
      if (Auth_path_is_inside(path, realm_path, strlen(realm_path)))
         return 1;

   return 0;
}

static void Auth_realm_add_path(AuthRealm_t *realm, const char *path)
{
   int len, i;
   char *realm_path, *n_path;

   n_path = dStrdup(path);
   len = strlen(n_path);

   /* remove trailing '/' */
   if (len && n_path[len - 1] == '/')
      n_path[--len] = 0;

   /* delete existing paths that are inside the new one */
   for (i = 0; (realm_path = dList_nth_data(realm->paths, i)); i++) {
      if (Auth_path_is_inside(realm_path, path, len)) {
         dList_remove_fast(realm->paths, realm_path);
         dFree(realm_path);
         i--; /* reconsider this slot */
      }
   }

   dList_append(realm->paths, n_path);
}

/*
 * Return the authorization header for an HTTP query.
 */
const char *a_Auth_get_auth_str(const DilloUrl *url)
{
   AuthHost_t *host;
   AuthRealm_t *realm;

   return
      ((host = Auth_host_by_url(url)) &&
       (realm = Auth_realm_by_path(host, URL_PATH(url)))) ?
      realm->authorization : NULL;
}

/*
 * Determine whether the user needs to authenticate.
 */
static int Auth_do_auth_required(const char *realm_name, const DilloUrl *url)
{
   /*
    * TO DO: I dislike the way that this code must decide whether we
    * sent authentication during the request and trust us to resend it
    * after the reload.  Could it be more robust if every DilloUrl
    * recorded its authentication, and whether it was accepted?  (JCH)
    */

   AuthHost_t *host;
   AuthRealm_t *realm;

   /*
    * The size of the following comments reflects the concerns in the
    * TO DO at the top of this function.  It should not be so hard to
    * explain why code is correct! (JCH)
    */

   /*
    * If we have authentication but did not send it (because we did
    * not know this path was in the realm) then we update the realm.
    * We do not re-authenticate because our authentication is probably
    * OK.  Thanks to the updated realm the forthcoming reload will
    * make us send the authentication.  If our authentication is not
    * OK the server will challenge us again after the reload and then
    * we will re-authenticate.
    */
   if ((host = Auth_host_by_url(url)) &&
       (realm = Auth_realm_by_name(host, realm_name)) &&
       (!Auth_realm_includes_path(realm, URL_PATH(url)))) {
      _MSG("Auth_do_auth_required: updating realm '%s' with URL '%s'\n",
           realm_name, URL_STR(url));
      Auth_realm_add_path(realm, URL_PATH(url));
      return 0;
   }

   /*
    * Either we had no authentication or we sent it and the server
    * rejected it, so we must re-authenticate.
    */
   return 1;
}

static void Auth_do_auth_dialog_cb(const char *user, const char *password,
                                   void *vData)
{
   AuthDialogData_t *data;
   AuthHost_t *host;
   AuthRealm_t *realm;
   char *user_password, *response, *authorization, *authorization_old;

   data = (AuthDialogData_t *)vData;

   /* find or create the host */
   if (!(host = Auth_host_by_url(data->url))) {
      /* create a new host */
      host = dNew(AuthHost_t, 1);
      host->scheme = dStrdup(URL_SCHEME(data->url));
      host->authority = dStrdup(URL_AUTHORITY(data->url));
      host->realms = dList_new(1);
      dList_append(auth_hosts, host);
   }

   /* find or create the realm */
   if (!(realm = Auth_realm_by_name(host, data->realm_name))) {
      /* create a new realm */
      realm = dNew(AuthRealm_t, 1);
      realm->name = dStrdup(data->realm_name);
      realm->paths = dList_new(1);
      realm->authorization = NULL;
      dList_append(host->realms, realm);
   }

   Auth_realm_add_path(realm, URL_PATH(data->url));

   /* create and set the authorization */
   user_password = dStrconcat(user, ":", password, NULL);
   response = a_Misc_encode_base64(user_password);
   authorization =
      dStrconcat("Authorization: Basic ", response, "\r\n", NULL);
   authorization_old = realm->authorization;
   realm->authorization = authorization;
   dFree(authorization_old);
   dFree(user_password);
   dFree(response);
}

static int Auth_do_auth_dialog(const char *realm, const DilloUrl *url)
{
   int ret;
   char *message;
   AuthDialogData_t *data;

   _MSG("auth.c: Auth_do_auth_dialog: realm = '%s'\n", realm);
   message = dStrconcat("The server at ", URL_HOST(url), " requires a username"
                        " and password for  \"", realm, "\".", NULL);
   data = dNew(AuthDialogData_t, 1);
   data->realm_name = dStrdup(realm);
   data->url = a_Url_dup(url);
   ret = a_Dialog_user_password(message, Auth_do_auth_dialog_cb, data);
   dFree(message);
   dFree((void*)data->realm_name);
   a_Url_free((void*)data->url);
   dFree(data);
   return ret;
}

/*
 * Do authorization for an auth string.
 */
static int Auth_do_auth(char *auth, const DilloUrl *url)
{
   int reload;
   AuthParse_t *auth_parse;

   _MSG("auth.c: Auth_do_auth: auth={%s}\n", auth);
   reload = 0;
   auth_parse = Auth_parse_new();
   Auth_parse_auth(auth_parse, auth);
   if (auth_parse->ok)
      reload =
         Auth_do_auth_required(auth_parse->realm, url) ?
         Auth_do_auth_dialog(auth_parse->realm, url)
         : 1;
   Auth_parse_free(auth_parse);

   return reload;
}

/*
 * Do authorization for a set of auth strings.
 */
int a_Auth_do_auth(Dlist *auths, const DilloUrl *url)
{
   int reload, i;
   char *auth;

   reload = 0;
   for (i = 0; (auth = dList_nth_data(auths, i)); ++i)
      if (Auth_do_auth(auth, url))
         reload = 1;

   return reload;
}

