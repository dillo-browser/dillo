/*
 * File: auth.c
 *
 * Copyright 2008 Jeremy Henty   <onepoint@starurchin.org>
 * Copyright 2009 Justus Winter  <4winter@informatik.uni-hamburg.de>
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
#include "digest.h"
#include "../dlib/dlib.h"

typedef struct {
   int ok;
   enum AuthParseHTTPAuthType_t type;
   const char *realm;
   const char *nonce;
   const char *opaque;
   int stale;
   enum AuthParseDigestAlgorithm_t algorithm;
   const char *domain;
   enum AuthParseDigestQOP_t qop;
} AuthParse_t;

typedef struct {
   char *scheme;
   char *authority;
   Dlist *realms;
} AuthHost_t;

typedef struct {
   const AuthParse_t *auth_parse;
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
   auth_parse->type = TYPENOTSET;
   auth_parse->realm = NULL;
   auth_parse->nonce = NULL;
   auth_parse->opaque = NULL;
   auth_parse->stale = 0;
   auth_parse->algorithm = ALGORITHMNOTSET;
   auth_parse->domain = NULL;
   auth_parse->qop = QOPNOTSET;
   return auth_parse;
}

static void Auth_parse_free(AuthParse_t *auth_parse)
{
   if (auth_parse) {
      dFree((void *)auth_parse->realm);
      dFree((void *)auth_parse->nonce);
      dFree((void *)auth_parse->opaque);
      dFree((void *)auth_parse->domain);
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
   return (!isascii(c) || strchr(invalid, c) || iscntrl((uchar_t)c)) ? 0 : 1;
}

/*
 * Unquote the content of a (potentially) quoted string.
 * Return: newly allocated unquoted content.
 *
 * Arguments:
 * valuep: pointer to a pointer to the first char.
 *
 * Preconditions:
 * *valuep points to a correctly quoted and escaped string.
 *
 * Postconditions:
 * *valuep points to the first not processed char.
 *
 */
static Dstr *Auth_unquote_value(char **valuep)
{
   char c, quoted;
   char *value = *valuep;
   Dstr *result;

   while (*value == ' ' || *value == '\t')
      value++;

   if ((quoted = *value == '"'))
      value++;

   result = dStr_new(NULL);
   while ((c = *value) &&
          (( quoted && c != '"') ||
           (!quoted && Auth_is_token_char(c)))) {
      dStr_append_c(result, (c == '\\' && value[1]) ? *++value : c);
      value++;
   }

   if (quoted && *value == '\"')
      value++;
   *valuep = value;
   return result;
}

typedef int (Auth_parse_token_value_callback_t)(AuthParse_t *auth_parse,
                                                char *token,
                                                const char *value);


/*
 * Parse authentication challenge into token-value pairs
 * and feed them into the callback function.
 *
 * The parsing is aborted should the callback function return 0.
 *
 * Return: 1 if the parse succeeds, 0 otherwise.
 */
static int Auth_parse_token_value(AuthParse_t *auth_parse, char **auth,
                                  Auth_parse_token_value_callback_t *callback)
{
   char keep_going, expect_quoted;
   char *token, *beyond_token;
   Dstr *value;
   size_t *token_size;

   while (**auth) {
      _MSG("Auth_parse_token_value: remaining: %s\n", *auth);

     /* parse a token */
      token = *auth;

      token_size = 0;
      while (Auth_is_token_char(**auth)) {
         (*auth)++;
         token_size++;
      }
      if (token_size == 0) {
         MSG("Auth_parse_token_value: missing auth token\n");
         return 0;
      }
      beyond_token = *auth;
      /* skip linear whitespace characters */
      while (**auth == ' ' || **auth == '\t')
         (*auth)++;

      /* parse the '=' */
      switch (*(*auth)++) {
      case '=':
         *beyond_token = '\0';
         break;
      case '\0':
      case ',':
         MSG("Auth_parse_token_value: missing auth token value\n");
         return 0;
         break;
      default:
         MSG("Auth_parse_token_value: garbage after auth token\n");
         return 0;
         break;
      }

      value = Auth_unquote_value(auth);
      expect_quoted = !(strcmp(token, "stale") == 0 ||
                        strcmp(token, "algorithm") == 0);

      if (((*auth)[-1] == '"') != expect_quoted)
         MSG_WARN("Auth_parse_token_value: "
                  "Values for key %s should%s be quoted.\n",
                  token, expect_quoted ? "" : " not");

      keep_going = callback(auth_parse, token, value->str);
      dStr_free(value, 1);
      if (!keep_going)
         break;

      /* skip ' ' and ',' */
      while ((**auth == ' ') || (**auth == ','))
         (*auth)++;
   }
   return 1;
}

static int Auth_parse_basic_challenge_cb(AuthParse_t *auth_parse, char *token,
                                         const char *value)
{
   if (dStrAsciiCasecmp("realm", token) == 0) {
      if (!auth_parse->realm)
         auth_parse->realm = strdup(value);
      return 0; /* end parsing */
   } else
      MSG("Auth_parse_basic_challenge_cb: Ignoring unknown parameter: %s = "
          "'%s'\n", token, value);
   return 1;
}

static int Auth_parse_digest_challenge_cb(AuthParse_t *auth_parse, char *token,
                                          const char *value)
{
   const char *const fn = "Auth_parse_digest_challenge_cb";

   if (!dStrAsciiCasecmp("realm", token) && !auth_parse->realm)
      auth_parse->realm = strdup(value);
   else if (!strcmp("domain", token) && !auth_parse->domain)
      auth_parse->domain = strdup(value);
   else if (!strcmp("nonce", token)  && !auth_parse->nonce)
      auth_parse->nonce = strdup(value);
   else if (!strcmp("opaque", token) && !auth_parse->opaque)
      auth_parse->opaque = strdup(value);
   else if (strcmp("stale", token) == 0) {
      if (dStrAsciiCasecmp("true", value) == 0)
         auth_parse->stale = 1;
      else if (dStrAsciiCasecmp("false", value) == 0)
         auth_parse->stale = 0;
      else {
         MSG("%s: Invalid stale value: %s\n", fn, value);
         return 0;
      }
   } else if (strcmp("algorithm", token) == 0) {
      if (strcmp("MD5", value) == 0)
         auth_parse->algorithm = MD5;
      else if (strcmp("MD5-sess", value) == 0) {
         /* auth_parse->algorithm = MD5SESS; */
         MSG("%s: MD5-sess algorithm disabled (not tested because 'not "
             "correctly implemented yet' in Apache 2.2)\n", fn);
         return 0;
      } else {
         MSG("%s: Unknown algorithm: %s\n", fn, value);
         return 0;
      }
   } else if (strcmp("qop", token) == 0) {
      while (*value) {
         int len = strcspn(value, ", \t");
         if (len == 4 && strncmp("auth", value, 4) == 0) {
            auth_parse->qop = AUTH;
            break;
         }
         if (len == 8 && strncmp("auth-int", value, 8) == 0) {
            /* auth_parse->qop = AUTHINT; */
            /* Keep searching; maybe we'll find an "auth" yet. */
            MSG("%s: auth-int qop disabled (not tested because 'not "
                "implemented yet' in Apache 2.2)\n", fn);
         } else {
            MSG("%s: Unknown qop value in %s\n", fn, value);
         }
         value += len;
         while (*value == ' ' || *value == '\t')
            value++;
         if (*value == ',')
            value++;
         while (*value == ' ' || *value == '\t')
            value++;
      }
   } else {
      MSG("%s: Ignoring unknown parameter: %s = '%s'\n", fn, token, value);
   }
   return 1;
}

static void Auth_parse_challenge_args(AuthParse_t *auth_parse,
                                      char **challenge,
                                      Auth_parse_token_value_callback_t *cb)
{
   /* parse comma-separated token-value pairs */
   while (1) {
      /* skip space and comma characters */
      while (**challenge == ' ' || **challenge == ',')
         (*challenge)++;
      /* end of string? */
      if (!**challenge)
         break;
      /* parse token-value pair */
      if (!Auth_parse_token_value(auth_parse, challenge, cb))
         break;
   }

   if (auth_parse->type == BASIC) {
      if (auth_parse->realm) {
         auth_parse->ok = 1;
      } else {
         MSG("Auth_parse_challenge_args: missing Basic auth realm\n");
         return;
      }
   } else if (auth_parse->type == DIGEST) {
      if (auth_parse->realm && auth_parse->nonce) {
         auth_parse->ok = 1;
      } else {
         MSG("Auth_parse_challenge_args: Digest challenge incomplete\n");
         return;
      }
   }
}

static void Auth_parse_challenge(AuthParse_t *auth_parse, char *challenge)
{
   Auth_parse_token_value_callback_t *cb;

   MSG("auth.c: Auth_parse_challenge: challenge = '%s'\n", challenge);
   if (auth_parse->type == DIGEST) {
      challenge += 7;
      cb = Auth_parse_digest_challenge_cb;
   } else {
      challenge += 6;
      cb = Auth_parse_basic_challenge_cb;
   }
   Auth_parse_challenge_args(auth_parse, &challenge, cb);
}

/*
 * Return the host that contains a URL, or NULL if there is no such host.
 */
static AuthHost_t *Auth_host_by_url(const DilloUrl *url)
{
   AuthHost_t *host;
   int i;

   for (i = 0; (host = dList_nth_data(auth_hosts, i)); i++)
      if (((dStrAsciiCasecmp(URL_SCHEME(url), host->scheme) == 0) &&
           (dStrAsciiCasecmp(URL_AUTHORITY(url), host->authority) == 0)))
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
      if (strcmp(realm->name, name) == 0)
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
   int match_length = 0;

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

static void Auth_realm_delete(AuthRealm_t *realm)
{
   int i;

   MSG("Auth_realm_delete: \"%s\"\n", realm->name);
   for (i = dList_length(realm->paths) - 1; i >= 0; i--)
      dFree(dList_nth_data(realm->paths, i));
   dList_free(realm->paths);
   dFree(realm->name);
   dFree(realm->username);
   dFree(realm->authorization);
   dFree(realm->cnonce);
   dFree(realm->nonce);
   dFree(realm->opaque);
   dFree(realm->domain);
   dFree(realm);
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
 * request_uri is a separate argument because we want it precisely as
 *   formatted in the request.
 */
char *a_Auth_get_auth_str(const DilloUrl *url, const char *request_uri)
{
   char *ret = NULL;
   AuthHost_t *host;
   AuthRealm_t *realm;

   if ((host = Auth_host_by_url(url)) &&
       (realm = Auth_realm_by_path(host, URL_PATH(url)))) {
      if (realm->type == BASIC)
         ret = dStrdup(realm->authorization);
      else if (realm->type == DIGEST)
         ret = a_Digest_authorization_hdr(realm, url, request_uri);
      else
         MSG("a_Auth_get_auth_str() got an unknown realm type: %i.\n",
             realm->type);
   }
   return ret;
}

/*
 * Determine whether the user needs to authenticate.
 */
static int Auth_do_auth_required(const AuthParse_t *auth_parse,
                                 const DilloUrl *url)
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
       (realm = Auth_realm_by_name(host, auth_parse->realm))) {
      if (!Auth_realm_includes_path(realm, URL_PATH(url))) {
         _MSG("Auth_do_auth_required: updating realm '%s' with URL '%s'\n",
              auth_parse->realm, URL_STR(url));
         Auth_realm_add_path(realm, URL_PATH(url));
         return 0;
      }

      if (auth_parse->type == DIGEST && auth_parse->stale) {
         /* we do have valid credentials but our nonce is old */
         dFree((void *)realm->nonce);
         realm->nonce = dStrdup(auth_parse->nonce);
         return 0;
      }
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
   if (!(realm = Auth_realm_by_name(host, data->auth_parse->realm))) {
      realm = dNew0(AuthRealm_t, 1);
      realm->name = dStrdup(data->auth_parse->realm);
      realm->paths = dList_new(1);
      dList_append(host->realms, realm);
   }
   realm->type = data->auth_parse->type;
   dFree(realm->authorization);
   realm->authorization = NULL;

   Auth_realm_add_path(realm, URL_PATH(data->url));

   if (realm->type == BASIC) {
      char *user_password = dStrconcat(user, ":", password, NULL);
      char *response = a_Misc_encode_base64(user_password);
      char *authorization =
         dStrconcat("Authorization: Basic ", response, "\r\n", NULL);
      dFree(realm->authorization);
      realm->authorization = authorization;
      dFree(response);
      dStrshred(user_password);
      dFree(user_password);
   } else if (realm->type == DIGEST) {
      dFree(realm->username);
      realm->username = dStrdup(user);
      realm->nonce_count = 0;
      dFree(realm->nonce);
      realm->nonce = dStrdup(data->auth_parse->nonce);
      dFree(realm->opaque);
      realm->opaque = dStrdup(data->auth_parse->opaque);
      realm->algorithm = data->auth_parse->algorithm;
      dFree(realm->domain);
      realm->domain = dStrdup(data->auth_parse->domain);
      realm->qop = data->auth_parse->qop;
      dFree(realm->cnonce);
      if (realm->qop != QOPNOTSET)
         realm->cnonce = a_Digest_create_cnonce();
      if (!a_Digest_compute_digest(realm, user, password)) {
         MSG("Auth_do_auth_dialog_cb: a_Digest_compute_digest failed.\n");
         dList_remove_fast(host->realms, realm);
         Auth_realm_delete(realm);
      }
   } else {
      MSG("Auth_do_auth_dialog_cb: Unknown auth type: %i\n",
          realm->type);
   }
   dStrshred((char *)password);
}

/*
 * Return: Nonzero if we got new credentials from the user and everything
 * seems fine.
 */
static int Auth_do_auth_dialog(const AuthParse_t *auth_parse,
                               const DilloUrl *url)
{
   int ret;
   char *title, *msg;
   AuthDialogData_t *data;
   const char *typestr = auth_parse->type == DIGEST ? "Digest" : "Basic";

   _MSG("auth.c: Auth_do_auth_dialog: realm = '%s'\n", auth_parse->realm);

   title = dStrconcat("Dillo: Password for ", auth_parse->realm, NULL);
   msg = dStrconcat("The server at ", URL_HOST(url), " requires a username"
                    " and password for  \"", auth_parse->realm, "\".\n\n"
                    "Authentication scheme: ", typestr, NULL);
   data = dNew(AuthDialogData_t, 1);
   data->auth_parse = auth_parse;
   data->url = a_Url_dup(url);
   ret = a_Dialog_user_password(title, msg, Auth_do_auth_dialog_cb, data);
   dFree(title); dFree(msg);
   a_Url_free((void *)data->url);
   dFree(data);
   return ret;
}

/*
 * Do authorization for an auth string.
 */
static int Auth_do_auth(char *challenge, enum AuthParseHTTPAuthType_t type,
                        const DilloUrl *url)
{
   AuthParse_t *auth_parse;
   int reload = 0;

   _MSG("auth.c: Auth_do_auth: challenge={%s}\n", challenge);
   auth_parse = Auth_parse_new();
   auth_parse->type = type;
   Auth_parse_challenge(auth_parse, challenge);
   if (auth_parse->ok)
      reload =
         Auth_do_auth_required(auth_parse, url) ?
         Auth_do_auth_dialog(auth_parse, url)
         : 1;
   Auth_parse_free(auth_parse);

   return reload;
}

/*
 * Given authentication challenge(s), prepare authorization.
 * Return: 0 on failure
 *         nonzero on success. A new query will be sent to the server.
 */
int a_Auth_do_auth(Dlist *challenges, const DilloUrl *url)
{
   int i;
   char *chal;

   for (i = 0; (chal = dList_nth_data(challenges, i)); ++i)
      if (!dStrnAsciiCasecmp(chal, "Digest ", 7))
         if (Auth_do_auth(chal, DIGEST, url))
            return 1;
   for (i = 0; (chal = dList_nth_data(challenges, i)); ++i)
      if (!dStrnAsciiCasecmp(chal, "Basic ", 6))
         if (Auth_do_auth(chal, BASIC, url))
            return 1;

   return 0;
}

