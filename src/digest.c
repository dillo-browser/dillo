/*
 * File: digest.c
 *
 * Copyright 2009 Justus Winter  <4winter@informatik.uni-hamburg.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdlib.h>
#include "digest.h"
#include "md5.h"
#include "msg.h"
#include "../dlib/dlib.h"

static const char *ALGORITHM2STR[] = { NULL, "MD5", "MD5-sess" };
static const char *QOP2STR[] = { NULL, "auth", "auth-int" };
static const char hexchars[] = "0123456789abcdef";

static Dstr *md5hexdigest(const Dstr *data)
{
   md5_state_t state;
   md5_byte_t digest[16];
   Dstr *result = dStr_sized_new(33);
   int i;

   md5_init(&state);
   md5_append(&state, (const md5_byte_t *)data->str, data->len);
   md5_finish(&state, digest);

   for (i = 0; i < 16; i++)
      dStr_sprintfa(result, "%02x", digest[i]);
   return result;
}

/*
 * Returns a pointer to a newly allocated string containing a cnonce
 */
char *a_Digest_create_cnonce(void)
{
   int i;
   char *result = dNew(char, 33);
   for (i = 0; i < 32; i++)
      result[i] = hexchars[rand() % 16];
   result[32] = 0;
   return result;
}

/*
 * This portion only has to be calculated once.
 */
int a_Digest_compute_digest(AuthRealm_t *realm, const char *username,
                            const char *passwd)
{
   Dstr *a1;
   Dstr *digest;

   if (realm->algorithm == MD5 || realm->algorithm == ALGORITHMNOTSET) {
      /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
      a1 = dStr_new(NULL);
      dStr_sprintf(a1, "%s:%s:%s", username, realm->name, passwd);
   } else if (realm->algorithm == MD5SESS) {
      /* A1 = H( unq(username-value) ":" unq(realm-value)
      **         ":" passwd )
      **         ":" unq(nonce-value) ":" unq(cnonce-value)
      */
      Dstr *a0 = dStr_new(NULL);
      dStr_sprintf(a0, "%s:%s:%s", username, realm->name, passwd);
      Dstr *ha0 = md5hexdigest(a0);
      a1 = dStr_new(NULL);
      dStr_sprintf(a1, "%s:%s:%s", ha0, realm->nonce, realm->cnonce);
      dStr_free(a0, 1);
      dStr_free(ha0, 1);
   } else {
      MSG("a_Digest_create_auth: Unknown algorithm.\n");
      return 0;
   }

   digest = md5hexdigest(a1);
   realm->authorization = digest->str;
   dStr_shred(a1);
   dStr_free(a1, 1);
   dStr_free(digest, 0);
   return 1;
}

/*
 * This portion is calculatd for each request.
 */
static Dstr *Digest_create_response(AuthRealm_t *realm, const char *method,
                                    const char *digest_uri,
                                    const Dstr *entity_body)
{
   Dstr *ha2;
   Dstr *result;

   if (realm->qop == QOPNOTSET || realm->qop == AUTH) {
      /* A2 = Method ":" digest-uri-value */
      Dstr *a2 = dStr_new(NULL);
      dStr_sprintf(a2, "%s:%s", method, digest_uri);
      ha2 = md5hexdigest(a2);
      dStr_free(a2, 1);
   } else if (realm->qop == AUTHINT) {
      /* A2 = Method ":" digest-uri-value ":" H(entity-body) */
      Dstr *hentity = md5hexdigest(entity_body);
      Dstr *a2 = dStr_new(NULL);
      dStr_sprintf(a2, "%s:%s:%s", method, digest_uri, hentity->str);
      ha2 = md5hexdigest(a2);
      dStr_free(hentity, 1);
      dStr_free(a2, 1);
   } else {
      MSG("a_Digest_create_auth: Unknown qop value.\n");
      return NULL;
   }
   result = dStr_new(NULL);

   if (realm->qop == AUTH || realm->qop == AUTHINT) {
      dStr_sprintf(result,
                   "%s:%s:%08x:%s:%s:%s",
                   realm->authorization,
                   realm->nonce,
                   realm->nonce_count,
                   realm->cnonce,
                   QOP2STR[realm->qop],
                   ha2->str);
   } else {
      dStr_sprintf(result,
                   "%s:%s:%s",
                   realm->authorization,
                   realm->nonce,
                   ha2->str);
   }

   Dstr *request_digest = md5hexdigest(result);
   dStr_free(result, 1);
   dStr_free(ha2, 1);
   return request_digest;
}

static void Digest_Dstr_append_token_value(Dstr *str, int delimiter,
                                           const char *token,
                                           const char *value, int quoted)
{
   char c;
   dStr_sprintfa(str, "%s%s=", (delimiter ? ", " : ""), token);
   if (quoted) {
      dStr_append_c(str, '"');
      while ((c = *value++)) {
         if (c == '"')
            dStr_append_c(str, '\\');
         dStr_append_c(str, c);
      }
      dStr_append_c(str, '"');
   } else {
      dStr_append(str, value);
   }
}

/*
 * Construct Digest Authorization header.
 *
 * Field ordering: furaisanjin reports that his DVD recorder requires the
 * order that IE happens to use: "username, realm, nonce, uri, cnonce, nc,
 * algorithm, response, qop". It apparently doesn't use "opaque", so that's
 * been left where it already was.
 */
char *a_Digest_authorization_hdr(AuthRealm_t *realm, const DilloUrl *url,
                                 const char *digest_uri)
{
   char *ret;
   Dstr *response, *result;
   const char *method = URL_FLAGS(url) & URL_Post ? "POST" : "GET";

   realm->nonce_count++;
   response = Digest_create_response(realm, method, digest_uri, URL_DATA(url));
   if (!response)
      return NULL;
   result = dStr_new("Authorization: Digest ");
   Digest_Dstr_append_token_value(result, 0, "username", realm->username, 1);
   Digest_Dstr_append_token_value(result, 1, "realm", realm->name, 1);
   Digest_Dstr_append_token_value(result, 1, "nonce", realm->nonce, 1);
   Digest_Dstr_append_token_value(result, 1, "uri", digest_uri, 1);
   if (realm->qop != QOPNOTSET) {
      Digest_Dstr_append_token_value(result, 1, "cnonce", realm->cnonce, 1);
      dStr_sprintfa(result, ", nc=%08x", realm->nonce_count);
   }
   if (realm->algorithm != ALGORITHMNOTSET) {
      Digest_Dstr_append_token_value(result, 1, "algorithm",
                                     ALGORITHM2STR[realm->algorithm], 0);
   }
   Digest_Dstr_append_token_value(result, 1, "response", response->str, 1);
   if (realm->opaque)
      Digest_Dstr_append_token_value(result, 1, "opaque", realm->opaque, 1);
   if (realm->qop != QOPNOTSET)
      Digest_Dstr_append_token_value(result, 1, "qop", QOP2STR[realm->qop], 1);
   dStr_sprintfa(result, "\r\n");

   dStr_free(response, 1);
   ret = result->str;
   dStr_free(result, 0);
   return ret;
}
