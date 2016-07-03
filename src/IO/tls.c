/*
 * File: tls.c
 *
 * Copyright (C) 2011 Benjamin Johnson <obeythepenguin@users.sourceforge.net>
 * (for the https code offered from dplus browser that formed the basis...)
 * Copyright 2016 corvid
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * https://www.ssllabs.com/ssltest/viewMyClient.html
 * https://badssl.com
 *
 * Using TLS in Applications: https://datatracker.ietf.org/wg/uta/documents/
 * TLS: https://datatracker.ietf.org/wg/tls/documents/
 */

#include "config.h"
#include "../msg.h"

#ifndef ENABLE_SSL

void a_Tls_init()
{
   MSG("TLS: Disabled at compilation time.\n");
}

#else

#include <assert.h>
#include <errno.h>

#include "../../dlib/dlib.h"
#include "../dialog.hh"
#include "../klist.h"
#include "iowatch.hh"
#include "tls.h"
#include "Url.h"

#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>  /* random number generator */
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/x509.h>
#include <mbedtls/net.h>    /* net_send, net_recv */

#define CERT_STATUS_NONE 0
#define CERT_STATUS_RECEIVING 1
#define CERT_STATUS_CLEAN 2
#define CERT_STATUS_BAD 3
#define CERT_STATUS_USER_ACCEPTED 4

typedef struct {
   char *hostname;
   int port;
   int cert_status;
} Server_t;

typedef struct {
   char *name;
   Dlist *servers;
} CertAuth_t;

typedef struct {
   int fd;
   int connkey;
} FdMapEntry_t;

/*
 * Data type for TLS connection information
 */
typedef struct {
   int fd;
   DilloUrl *url;
   mbedtls_ssl_context *ssl;
   bool_t connecting;
} Conn_t;

/* List of active TLS connections */
static Klist_t *conn_list = NULL;

static bool_t ssl_enabled = TRUE;
static mbedtls_ssl_config ssl_conf;
static mbedtls_x509_crt cacerts;
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_entropy_context entropy;

static Dlist *servers;
static Dlist *cert_authorities;
static Dlist *fd_map;

static void Tls_connect_cb(int fd, void *vconnkey);

/*
 * Compare by FD.
 */
static int Tls_fd_map_cmp(const void *v1, const void *v2)
{
   int fd = VOIDP2INT(v2);
   const FdMapEntry_t *e = v1;

   return (fd != e->fd);
}

static void Tls_fd_map_add_entry(int fd, int connkey)
{
   FdMapEntry_t *e = dNew0(FdMapEntry_t, 1);
   e->fd = fd;
   e->connkey = connkey;

   if (dList_find_custom(fd_map, INT2VOIDP(e->fd), Tls_fd_map_cmp)) {
      MSG_ERR("TLS FD ENTRY ALREADY FOUND FOR %d\n", e->fd);
      assert(0);
   }

   dList_append(fd_map, e);
//MSG("ADD ENTRY %d %s\n", e->fd, URL_STR(sd->url));
}

/*
 * Remove and free entry from fd_map.
 */
static void Tls_fd_map_remove_entry(int fd)
{
   void *data = dList_find_custom(fd_map, INT2VOIDP(fd), Tls_fd_map_cmp);

//MSG("REMOVE ENTRY %d\n", fd);
   if (data) {
      dList_remove_fast(fd_map, data);
      dFree(data);
   } else {
      MSG("TLS FD ENTRY NOT FOUND FOR %d\n", fd);
   }
}

/*
 * Return TLS connection information for a given file
 * descriptor, or NULL if no TLS connection was found.
 */
void *a_Tls_connection(int fd)
{
   Conn_t *conn;

   if (fd_map) {
      FdMapEntry_t *fme = dList_find_custom(fd_map, INT2VOIDP(fd),
                                            Tls_fd_map_cmp);

      if (fme && (conn = a_Klist_get_data(conn_list, fme->connkey)))
         return conn;
   }
   return NULL;
}

/*
 * Add a new TLS connection information node.
 */
static Conn_t *Tls_conn_new(int fd, const DilloUrl *url,
                            mbedtls_ssl_context *ssl)
{
   Conn_t *conn = dNew0(Conn_t, 1);
   conn->fd = fd;
   conn->url = a_Url_dup(url);
   conn->ssl = ssl;
   conn->connecting = TRUE;
   return conn;
}

static int Tls_make_conn_key(Conn_t *conn)
{
   int key = a_Klist_insert(&conn_list, conn);

   Tls_fd_map_add_entry(conn->fd, key);

   return key;
}

/*
 * Load certificates from a given filename.
 */
static void Tls_load_certificates_from_file(const char *const filename)
{
   int ret = mbedtls_x509_crt_parse_file(&cacerts, filename);

   if (ret < 0) {
      if (ret == MBEDTLS_ERR_PK_FILE_IO_ERROR) {
         /* can't read from file */
      } else {
         MSG("Failed to parse certificates from %s (returned -0x%04x)\n",
             filename, -ret);
      }
   }
}

/*
 * Load certificates from a given pathname.
 */
static void Tls_load_certificates_from_path(const char *const pathname)
{
   int ret = mbedtls_x509_crt_parse_path(&cacerts, pathname);

   if (ret < 0) {
      if (ret == MBEDTLS_ERR_X509_FILE_IO_ERROR) {
         /* can't read from path */
      } else {
         MSG("Failed to parse certificates from %s (returned -0x%04x)\n",
             pathname, -ret);
      }
   }
}

/*
 * Remove duplicate certificates.
 */
static void Tls_remove_duplicate_certificates()
{
   mbedtls_x509_crt *cp, *curr = &cacerts;

   while (curr) {
      cp = curr;
      while (cp->next) {
         if (curr->serial.len == cp->next->serial.len &&
             !memcmp(curr->serial.p, cp->next->serial.p, curr->serial.len) &&
             curr->subject_raw.len == cp->next->subject_raw.len &&
             !memcmp(curr->subject_raw.p, cp->next->subject_raw.p,
                     curr->subject_raw.len)) {
            mbedtls_x509_crt *duplicate = cp->next;

            cp->next = duplicate->next;

            /* clearing the next field prevents it from freeing the whole
             * chain of certificates
             */
            duplicate->next = NULL;
            mbedtls_x509_crt_free(duplicate);
            dFree(duplicate);
         } else {
            cp = cp->next;
         }
      }
      curr = curr->next;
   }
}

/*
 * Load trusted certificates.
 */
static void Tls_load_certificates()
{
   /* curl-7.37.1 says that the following bundle locations are used on "Debian
    * systems", "Redhat and Mandriva", "old(er) Redhat", "FreeBSD", and
    * "OpenBSD", respectively -- and that the /etc/ssl/certs/ path is needed on
    * "SUSE". No doubt it's all changed some over time, but this gives us
    * something to work with.
    */
   uint_t u;
   char *userpath;
   mbedtls_x509_crt *curr;

   static const char *const ca_files[] = {
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/usr/share/ssl/certs/ca-bundle.crt",
      "/usr/local/share/certs/ca-root.crt",
      "/etc/ssl/cert.pem",
      CA_CERTS_FILE
   };

   static const char *const ca_paths[] = {
      "/etc/ssl/certs/",
      CA_CERTS_DIR
   };

   for (u = 0; u < sizeof(ca_files)/sizeof(ca_files[0]); u++) {
      if (*ca_files[u])
         Tls_load_certificates_from_file(ca_files[u]);
   }

   for (u = 0; u < sizeof(ca_paths)/sizeof(ca_paths[0]); u++) {
      if (*ca_paths[u]) {
         Tls_load_certificates_from_path(ca_paths[u]);
      }
   }

   userpath = dStrconcat(dGethomedir(), "/.dillo/certs/", NULL);
   Tls_load_certificates_from_path(userpath);
   dFree(userpath);

   Tls_remove_duplicate_certificates();

   /* Count our trusted certificates */
   u = 0;
   if (cacerts.next) {
      u++;
      for (curr = cacerts.next; curr; curr = curr->next)
         u++;
   } else {
      mbedtls_x509_crt empty;
      mbedtls_x509_crt_init(&empty);

      if (memcmp(&cacerts, &empty, sizeof(mbedtls_x509_crt)))
         u++;
   }
     
   MSG("Trusting %u TLS certificate%s.\n", u, u==1 ? "" : "s");
}

/*
 * Remove the pre-shared key ciphersuites. There are lots of them,
 * and we aren't making any use of them.
 */
static void Tls_remove_psk_ciphersuites()
{
   const mbedtls_ssl_ciphersuite_t *cs_info;
   int *our_ciphers, *q;
   int n = 0;

   const int *default_ciphers = mbedtls_ssl_list_ciphersuites(),
             *p = default_ciphers;

   /* count how many we will want */
   while (*p) {
      cs_info = mbedtls_ssl_ciphersuite_from_id(*p);
      if (!mbedtls_ssl_ciphersuite_uses_psk(cs_info))
         n++;
      p++;
   }
   n++;
   our_ciphers = dNew(int, n);

   /* iterate through again and copy them over */
   p = default_ciphers;
   q = our_ciphers;
   while (*p) {
      cs_info = mbedtls_ssl_ciphersuite_from_id(*p);

      if (!mbedtls_ssl_ciphersuite_uses_psk(cs_info))
         *q++ = *p;
      p++;
   }
   *q = 0;

   mbedtls_ssl_conf_ciphersuites(&ssl_conf, our_ciphers);
}

/*
 * Initialize the mbed TLS library.
 */
void a_Tls_init(void)
{
   int ret;

    /* As of 2.3.0 in 2016, the 'default' profile allows SHA1, RIPEMD160,
     * and SHA224 (in addition to the stronger ones), and the 'next' profile
     * doesn't allow anything below SHA256. Since we're never going to hear
     * when/if RIPEMD160 and SHA224 are deprecated, and they're obscure enough
     * not to encounter, let's not allow those.
     * These profiles are for certificates, and mbed tls points out that these
     * have nothing to do with hashes during handshakes.
     * Their 'next' profile only allows "Curves at or above 128-bit security
     * level". For now, we follow 'default' and allow all curves.
     */
   static const mbedtls_x509_crt_profile prof = {
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA256 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA384 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA512 ),
    0xFFFFFFF, /* Any PK alg    */
    0xFFFFFFF, /* Any curve     */
    2048,
   };

   mbedtls_ssl_config_init(&ssl_conf);

   mbedtls_ssl_config_defaults(&ssl_conf, MBEDTLS_SSL_IS_CLIENT,
                               MBEDTLS_SSL_TRANSPORT_STREAM,
                               MBEDTLS_SSL_PRESET_DEFAULT);
   mbedtls_ssl_conf_cert_profile(&ssl_conf, &prof);

   Tls_remove_psk_ciphersuites();

   mbedtls_x509_crt_init(&cacerts);   /* trusted root certificates */
   mbedtls_ctr_drbg_init(&ctr_drbg);  /* Counter mode Deterministic Random Byte
                                       * Generator */
   mbedtls_entropy_init(&entropy);

   if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (unsigned char*)"dillo tls", 9))) {
      ssl_enabled = FALSE;
      MSG_ERR("tls: mbedtls_ctr_drbg_seed() failed. TLS disabled.\n");
      return;
   }

   mbedtls_ssl_conf_authmode(&ssl_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
   mbedtls_ssl_conf_ca_chain(&ssl_conf, &cacerts, NULL);
   mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);

   fd_map = dList_new(20);
   servers = dList_new(8);
   cert_authorities = dList_new(12);

   Tls_load_certificates();
}

/*
 * Ordered comparison of servers.
 */
static int Tls_servers_cmp(const void *v1, const void *v2)
{
   const Server_t *s1 = (const Server_t *)v1, *s2 = (const Server_t *)v2;
   int cmp = dStrAsciiCasecmp(s1->hostname, s2->hostname);

   if (!cmp)
      cmp = s1->port - s2->port;
   return cmp;
}
/*
 * Ordered comparison of server with URL.
 */
static int Tls_servers_by_url_cmp(const void *v1, const void *v2)
{
   const Server_t *s = (const Server_t *)v1;
   const DilloUrl *url = (const DilloUrl *)v2;

   int cmp = dStrAsciiCasecmp(s->hostname, URL_HOST(url));

   if (!cmp)
      cmp = s->port - URL_PORT(url);
   return cmp;
}

/*
 * The purpose here is to permit a single initial connection to a server.
 * Once we have the certificate, know whether we like it -- and whether the
 * user accepts it -- HTTP can run through queued sockets as normal.
 *
 * Return: TLS_CONNECT_READY or TLS_CONNECT_NOT_YET or TLS_CONNECT_NEVER.
 */
int a_Tls_connect_ready(const DilloUrl *url)
{
   Server_t *s;
   int ret = TLS_CONNECT_READY;

   dReturn_val_if_fail(ssl_enabled, TLS_CONNECT_NEVER);

   if ((s = dList_find_sorted(servers, url, Tls_servers_by_url_cmp))) {
      if (s->cert_status == CERT_STATUS_RECEIVING)
         ret = TLS_CONNECT_NOT_YET;
      else if (s->cert_status == CERT_STATUS_BAD)
         ret = TLS_CONNECT_NEVER;

      if (s->cert_status == CERT_STATUS_NONE)
         s->cert_status = CERT_STATUS_RECEIVING;
   } else {
      s = dNew(Server_t, 1);

      s->hostname = dStrdup(URL_HOST(url));
      s->port = URL_PORT(url);
      s->cert_status = CERT_STATUS_RECEIVING;
      dList_insert_sorted(servers, s, Tls_servers_cmp);
   }
   return ret;
}

static int Tls_cert_status(const DilloUrl *url)
{
   Server_t *s = dList_find_sorted(servers, url, Tls_servers_by_url_cmp);

   return s ? s->cert_status : CERT_STATUS_NONE;
}

/*
 * Did we find problems with the certificate, and did the user proceed to
 * reject the connection?
 */
static int Tls_user_said_no(const DilloUrl *url)
{
   return Tls_cert_status(url) == CERT_STATUS_BAD;
}

/*
 * Did everything seem proper with the certificate -- no warnings to
 * click through?
 */
int a_Tls_certificate_is_clean(const DilloUrl *url)
{
   return Tls_cert_status(url) == CERT_STATUS_CLEAN;
}

#if 0
/*
 * Print certificate and its chain of issuer certificates.
 */
static void Tls_print_cert_chain(const mbedtls_x509_crt *cert)
{
   /* print for first connection to server */
   const mbedtls_x509_crt *last_cert;
   const uint_t buflen = 2048;
   char buf[buflen];
   int key_bits;
   const char *sigalg;

   while (cert) {
      if (cert->sig_md == MBEDTLS_MD_SHA1) {
         MSG_WARN("In 2015, browsers have begun to deprecate SHA1 "
                  "certificates.\n");
      }

      if (mbedtls_oid_get_sig_alg_desc(&cert->sig_oid, &sigalg))
         sigalg = "(??" ")";

      key_bits = mbedtls_pk_get_bitlen(&cert->pk);
      mbedtls_x509_dn_gets(buf, buflen, &cert->subject);
      MSG("%d-bit %s: %s\n", key_bits, sigalg, buf);

      last_cert = cert;
      cert = cert->next;
   }
   if (last_cert) {
      mbedtls_x509_dn_gets(buf, buflen, &last_cert->issuer);
      MSG("root: %s\n", buf);
   }
}
#endif

/*
 * Generate dialog msg for expired cert.
 */
static void Tls_cert_expired(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const mbedtls_x509_time *date = &cert->valid_to;

   dStr_sprintfa(ds,"Certificate expired at: %04d/%02d/%02d %02d:%02d:%02d.\n",
                 date->year, date->mon, date->day, date->hour, date->min,
                 date->sec);
}

/*
 * Generate dialog msg when certificate is not for this host.
 */
static void Tls_cert_cn_mismatch(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const uint_t buflen = 2048;
   char cert_info_buf[buflen];
   char *san, *s;

   dStr_append(ds, "This host is not one of the hostnames listed on the TLS "
                   "certificate that it sent");
   /*
    *
    * Taking the human-readable certificate info and scraping it is brittle
    * and horrible, but the alternative is to mimic
    * x509_info_subject_alt_name(), an option that seems equally brittle and
    * horrible.
    *
    * Once I find a case where SAN isn't used, I can add code to work with
    * the subject field as well.
    *
    */
   mbedtls_x509_crt_info(cert_info_buf, buflen, "", cert);

   if ((san = strstr(cert_info_buf, "subject alt name  : "))) {
      san += 20;
      s = strchr(san, '\n');
      if (s) {
         *s = '\0';
         dStr_sprintfa(ds, " (%s)", san);
      }
   }
   dStr_append(ds, ".\n");
}

/*
 * Generate dialog msg when certificate is not trusted.
 */
static void Tls_cert_trust_chain_failed(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const uint_t buflen = 2048;
   char buf[buflen];

   while (cert->next)
      cert = cert->next;
   mbedtls_x509_dn_gets(buf, buflen, &cert->issuer);

   dStr_sprintfa(ds, "Couldn't reach any trusted root certificate from "
                     "supplied certificate. The issuer at the end of the "
                     "chain was: \"%s\"\n", buf);
}

/*
 * Generate dialog msg when certificate start date is in the future.
 */
static void Tls_cert_not_valid_yet(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const mbedtls_x509_time *date = &cert->valid_to;

   dStr_sprintfa(ds, "Certificate validity begins in the future at: "
                     "%04d/%02d/%02d %02d:%02d:%02d.\n",
                     date->year, date->mon, date->day, date->hour, date->min,
                     date->sec);
}

/*
 * Generate dialog msg when certificate hash algorithm is not accepted.
 */
static void Tls_cert_bad_hash(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const char *hash = (cert->sig_md == MBEDTLS_MD_SHA1) ? "SHA1" :
                      (cert->sig_md == MBEDTLS_MD_SHA224) ? "SHA224" :
                      (cert->sig_md == MBEDTLS_MD_RIPEMD160) ? "RIPEMD160" :
                      (cert->sig_md == MBEDTLS_MD_SHA256) ? "SHA256" :
                      (cert->sig_md == MBEDTLS_MD_SHA384) ? "SHA384" :
                      (cert->sig_md == MBEDTLS_MD_SHA512) ? "SHA512" :
                      "Unrecognized";

   dStr_sprintfa(ds, "This certificate's hash algorithm is not accepted "
                     "(%s).\n", hash);
}

/*
 * Generate dialog msg when public key algorithm (RSA, ECDSA) is not accepted.
 */
static void Tls_cert_bad_pk_alg(const mbedtls_x509_crt *cert, Dstr *ds)
{
   const char *type_str = mbedtls_pk_get_name(&cert->pk);

   dStr_sprintfa(ds, "This certificate's public key algorithm is not accepted "
                     "(%s).\n", type_str);
}

/*
 * Generate dialog msg when the public key is not acceptable. As of 2016,
 * this was triggered by RSA keys below 2048 bits, if I recall correctly.
 */
static void Tls_cert_bad_key(const mbedtls_x509_crt *cert, Dstr *ds) {
   int key_bits = mbedtls_pk_get_bitlen(&cert->pk);
   const char *type_str = mbedtls_pk_get_name(&cert->pk);

   dStr_sprintfa(ds, "This certificate's key is not accepted, which generally "
                     "means it's too weak (%d-bit %s).\n", key_bits, type_str);
}

/*
 * Make a dialog msg containing warnings about problems with the certificate.
 */
static char *Tls_make_bad_cert_msg(const mbedtls_x509_crt *cert,uint32_t flags)
{
   static const struct certerr {
      int val;
      void (*cert_err_fn)(const mbedtls_x509_crt *cert, Dstr *ds);
   } cert_error [] = {
      { MBEDTLS_X509_BADCERT_EXPIRED, Tls_cert_expired},
      { MBEDTLS_X509_BADCERT_CN_MISMATCH, Tls_cert_cn_mismatch},
      { MBEDTLS_X509_BADCERT_NOT_TRUSTED, Tls_cert_trust_chain_failed},
      { MBEDTLS_X509_BADCERT_FUTURE, Tls_cert_not_valid_yet},
      { MBEDTLS_X509_BADCERT_BAD_MD, Tls_cert_bad_hash},
      { MBEDTLS_X509_BADCERT_BAD_PK, Tls_cert_bad_pk_alg},
      { MBEDTLS_X509_BADCERT_BAD_KEY, Tls_cert_bad_key}
   };
   const uint_t ncert_errors = sizeof(cert_error) /sizeof(cert_error[0]);
   char *ret;
   Dstr *ds = dStr_new(NULL);
   uint_t u;

   for (u = 0; u < ncert_errors; u++) {
      if (flags & cert_error[u].val) {
         flags &= ~cert_error[u].val;
         cert_error[u].cert_err_fn(cert, ds);
      }
   }
   if (flags)
      dStr_sprintfa(ds, "Unknown certificate error(s): flag value 0x%04x",
                    flags);
   ret = ds->str;
   dStr_free(ds, 0);
   return ret;
}

static int Tls_cert_auth_cmp(const void *v1, const void *v2)
{
   const CertAuth_t *c1 = (CertAuth_t *)v1, *c2 = (CertAuth_t *)v2;

   return strcmp(c1->name, c2->name);
}

static int Tls_cert_auth_cmp_by_name(const void *v1, const void *v2)
{
   const CertAuth_t *c = (CertAuth_t *)v1;
   const char *name = (char *)v2;

   return strcmp(c->name, name);
}

/*
 * Keep account of on whose authority we are trusting servers.
 */
static void Tls_update_cert_authorities_data(const mbedtls_x509_crt *cert,
                                             Server_t *srv)
{
   const uint_t buflen = 512;
   char buf[buflen];
   const mbedtls_x509_crt *last = cert;

   while (last->next)
      last = last->next;

   mbedtls_x509_dn_gets(buf, buflen, &last->issuer);

   CertAuth_t *ca = dList_find_custom(cert_authorities, buf,
                                      Tls_cert_auth_cmp_by_name);
   if (!ca) {
      ca = dNew(CertAuth_t, 1);
      ca->name = dStrdup(buf);
      ca->servers = dList_new(8);
      dList_insert_sorted(cert_authorities, ca, Tls_cert_auth_cmp);
   }
   dList_append(ca->servers, srv);
}

/*
 * Examine the certificate, and, if problems are detected, ask the user what
 * to do.
 * Return: -1 if connection should be canceled, or 0 if it should continue.
 */
static int Tls_examine_certificate(mbedtls_ssl_context *ssl, Server_t *srv)
{
   const mbedtls_x509_crt *cert;
   uint32_t st;
   int choice = -1, ret = -1;
   char *title = dStrconcat("Dillo TLS security warning: ",srv->hostname,NULL);

   cert = mbedtls_ssl_get_peer_cert(ssl);
   if (cert == NULL){
      /* Inform user that remote system cannot be trusted */
      choice = a_Dialog_choice(title,
         "No certificate received from this site. Can't verify who it is.",
         "Continue", "Cancel", NULL);

      /* Abort on anything but "Continue" */
      if (choice == 1){
         ret = 0;
      }
   } else {
      /* check the certificate */
      st = mbedtls_ssl_get_verify_result(ssl);
      if (st == 0) {
         if (srv->cert_status == CERT_STATUS_RECEIVING) {
            /* first connection to server */
#if 0
            Tls_print_cert_chain(cert);
#endif
            Tls_update_cert_authorities_data(cert, srv);
         }
         ret = 0;
      } else if (st == 0xFFFFFFFF) {
         /* "result is not available (eg because the handshake was aborted too
          * early)" is what the documentation says. Maybe it's only what
          * happens if you call get_verify_result() too early or when the
          * handshake failed. But just in case...
          */
         MSG_ERR("mbedtls_ssl_get_verify_result: result is not available");
      } else {
         char *dialog_warning_msg = Tls_make_bad_cert_msg(cert, st);

         choice = a_Dialog_choice(title, dialog_warning_msg, "Continue",
                                  "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         dFree(dialog_warning_msg);
      }
   }
   dFree(title);

   if (choice == -1) {
      srv->cert_status = CERT_STATUS_CLEAN;          /* no warning popups */
   } else if (choice == 1) {
      srv->cert_status = CERT_STATUS_USER_ACCEPTED;  /* clicked Continue */
   } else {
      /* 2 for Cancel, or 0 when window closed. */
      srv->cert_status = CERT_STATUS_BAD;
   }
   return ret;
}

/*
 * If the connection was closed before we got the certificate, we need to
 * reset state so that we'll try again.
 */
void a_Tls_reset_server_state(const DilloUrl *url)
{
   if (servers) {
      Server_t *s = dList_find_sorted(servers, url, Tls_servers_by_url_cmp);

      if (s && s->cert_status == CERT_STATUS_RECEIVING)
         s->cert_status = CERT_STATUS_NONE;
   }
}

/*
 * Close an open TLS connection.
 */
static void Tls_close_by_key(int connkey)
{
   Conn_t *c;

   if ((c = a_Klist_get_data(conn_list, connkey))) {
      a_Tls_reset_server_state(c->url);
      if (c->connecting) {
         a_IOwatch_remove_fd(c->fd, -1);
         dClose(c->fd);
      }
      mbedtls_ssl_close_notify(c->ssl);
      mbedtls_ssl_free(c->ssl);
      dFree(c->ssl);

      a_Url_free(c->url);
      Tls_fd_map_remove_entry(c->fd);
      a_Klist_remove(conn_list, connkey);
      dFree(c);
   }
}

/*
 * Print a message about the fatal alert.
 *
 * The values have gaps, and a few are never fatal error values, and some may
 * never be sent to clients, but let's go ahead and translate every value that
 * we recognize.
 */
static void Tls_fatal_error_msg(int error_type)
{
   const char *errmsg;

   if (error_type == MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY)
      errmsg = "close notify";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE)
      errmsg = "unexpected message received";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_BAD_RECORD_MAC)
      errmsg = "record received with incorrect MAC";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_DECRYPTION_FAILED) {
      /* last used in TLS 1.1 */
      errmsg = "decryption failed";
   } else if (error_type == MBEDTLS_SSL_ALERT_MSG_RECORD_OVERFLOW)
      errmsg = "\"A TLSCiphertext record was received that had a length more "
               "than 2^14+2048 bytes, or a record decrypted to a TLSCompressed"
               " record with more than 2^14+1024 bytes.\"";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_DECOMPRESSION_FAILURE)
      errmsg = "\"decompression function received improper input\"";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE)
      errmsg = "\"sender was unable to negotiate an acceptable set of security"
               " parameters given the options available\"";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_NO_CERT)
      errmsg = "no cert (an obsolete alert last used in SSL3)";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_BAD_CERT)
      errmsg = "bad certificate";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_CERT)
      errmsg = "certificate of unsupported type";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_CERT_REVOKED)
      errmsg = "certificate revoked by its signer";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_CERT_EXPIRED)
      errmsg = "certificate expired or not currently valid";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_CERT_UNKNOWN)
      errmsg = "certificate error of an unknown sort";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER)
      errmsg = "illegal parameter in handshake";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNKNOWN_CA)
      errmsg = "unknown CA";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_ACCESS_DENIED)
      errmsg = "access denied";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR)
      errmsg = "decode error";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR)
      errmsg = "decrypt error";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_EXPORT_RESTRICTION) {
      /* last used in TLS 1.0 */
      errmsg = "export restriction";
   } else if (error_type == MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION)
      errmsg = "protocol version is recognized but not supported";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_INSUFFICIENT_SECURITY)
      errmsg = "server requires ciphers more secure than those supported by "
               "the client";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR)
      errmsg = "internal error (not the client's fault)";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_INAPROPRIATE_FALLBACK)
      errmsg = "inappropriate fallback";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_USER_CANCELED)
      errmsg = "\"handshake is being canceled for some reason unrelated to a "
               "protocol failure\"";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_NO_RENEGOTIATION)
      errmsg = "no renegotiation";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT)
      errmsg = "unsupported ext";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNRECOGNIZED_NAME)
      errmsg = "unrecognized name";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_UNKNOWN_PSK_IDENTITY)
      errmsg = "unknown psk identity";
   else if (error_type == MBEDTLS_SSL_ALERT_MSG_NO_APPLICATION_PROTOCOL)
      errmsg = "no application protocol";
   else errmsg = "unknown alert value";

   MSG_WARN("mbedtls_ssl_handshake() received TLS fatal alert %d (%s)\n",
            error_type, errmsg);
}

/*
 * Connect, set a callback if it's still not completed. If completed, check
 * the certificate and report back to http.
 */
static void Tls_connect(int fd, int connkey)
{
   int ret;
   bool_t ongoing = FALSE, failed = TRUE;
   Conn_t *conn;

   if (!(conn = a_Klist_get_data(conn_list, connkey))) {
      MSG("Tls_connect: conn for fd %d not valid\n", fd);
      return;
   }

   if (conn->ssl->state != MBEDTLS_SSL_HANDSHAKE_OVER) {
      ret = mbedtls_ssl_handshake(conn->ssl);

      if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
          ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
         int want = ret == MBEDTLS_ERR_SSL_WANT_READ ? DIO_READ : DIO_WRITE;

         _MSG("iowatching fd %d for tls -- want %s\n", fd,
             ret == MBEDTLS_ERR_SSL_WANT_READ ? "read" : "write");
         a_IOwatch_remove_fd(fd, -1);
         a_IOwatch_add_fd(fd, want, Tls_connect_cb, INT2VOIDP(connkey));
         ongoing = TRUE;
         failed = FALSE;
      } else if (ret == 0) {
         Server_t *srv = dList_find_sorted(servers, conn->url,
                                           Tls_servers_by_url_cmp);

         if (srv->cert_status == CERT_STATUS_RECEIVING) {
            /* Making first connection with the server. Show cipher used. */
            mbedtls_ssl_context *ssl = conn->ssl;
            const char *version = mbedtls_ssl_get_version(ssl),
                       *cipher = mbedtls_ssl_get_ciphersuite(ssl);

            MSG("%s: %s, cipher %s\n", URL_AUTHORITY(conn->url), version,
                cipher);
         }
         if (srv->cert_status == CERT_STATUS_USER_ACCEPTED ||
             (Tls_examine_certificate(conn->ssl, srv) != -1)) {
            failed = FALSE;
         }
      } else if (ret == MBEDTLS_ERR_NET_SEND_FAILED) {
         MSG("mbedtls_ssl_handshake() send failed. Server may not be accepting"
             " connections.\n");
      } else if (ret == MBEDTLS_ERR_NET_CONNECT_FAILED) {
         MSG("mbedtls_ssl_handshake() connect failed.\n");
      } else if (ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE) {
         /* Paul Bakker, the mbed tls guy, says "beware, this might change in
          * future versions" and "ssl->in_msg[1] is not going to change anytime
          * soon, unless there are radical changes". It seems to be the best of
          * the alternatives.
          */
         Tls_fatal_error_msg(conn->ssl->in_msg[1]);
      } else if (ret == MBEDTLS_ERR_SSL_INVALID_RECORD) {
         MSG("mbedtls_ssl_handshake() failed upon receiving 'an invalid "
             "record'.\n");
      } else if (ret == MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE) {
         MSG("mbedtls_ssl_handshake() failed: 'The requested feature is not "
             "available.'\n");
      } else if (ret == MBEDTLS_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE) {
         MSG("mbedtls_ssl_handshake() failed: 'Processing of the "
             "ServerKeyExchange handshake message failed.'\n");
      } else if (ret == MBEDTLS_ERR_SSL_CONN_EOF) {
         MSG("mbedtls_ssl_handshake() failed: 'Read EOF. Connection closed "
             "by server.\n");
      } else {
         MSG("mbedtls_ssl_handshake() failed with error -0x%04x\n", -ret);
      }
   }

   /*
    * If there were problems with the certificate, the connection may have
    * been closed by the server if the user responded too slowly to a popup.
    */

   if (!ongoing) {
      if (a_Klist_get_data(conn_list, connkey)) {
         conn->connecting = FALSE;
         if (failed) {
            Tls_close_by_key(connkey);
         }
         a_IOwatch_remove_fd(fd, -1);
         a_Http_connect_done(fd, failed ? FALSE : TRUE);
      } else {
         MSG("Connection disappeared. Too long with a popup popped up?\n");
      }
   }
}

static void Tls_connect_cb(int fd, void *vconnkey)
{
   Tls_connect(fd, VOIDP2INT(vconnkey));
}

/*
 * Perform the TLS handshake on an open socket.
 */
void a_Tls_handshake(int fd, const DilloUrl *url)
{
   mbedtls_ssl_context *ssl = dNew0(mbedtls_ssl_context, 1);
   bool_t success = TRUE;
   int connkey = -1;
   int ret;

   if (!ssl_enabled)
      success = FALSE;

   if (success && Tls_user_said_no(url)) {
      success = FALSE;
   }

   if (success && (ret = mbedtls_ssl_setup(ssl, &ssl_conf))) {
      MSG("mbedtls_ssl_setup failed %d\n", ret);
      success = FALSE;
   }

   /* assign TLS connection to this file descriptor */
   if (success) {
      Conn_t *conn = Tls_conn_new(fd, url, ssl);
      connkey = Tls_make_conn_key(conn);
      mbedtls_ssl_set_bio(ssl, &conn->fd, mbedtls_net_send, mbedtls_net_recv,
                          NULL);
   }

   if (success && (ret = mbedtls_ssl_set_hostname(ssl, URL_HOST(url)))) {
      MSG("mbedtls_ssl_set_hostname failed %d\n", ret);
      success = FALSE;
   }

   if (!success) {
      a_Tls_reset_server_state(url);
      a_Http_connect_done(fd, success);
   } else {
      Tls_connect(fd, connkey);
   }
}

/*
 * Read data from an open TLS connection.
 */
int a_Tls_read(void *conn, void *buf, size_t len)
{
   Conn_t *c = (Conn_t*)conn;
   int ret = mbedtls_ssl_read(c->ssl, buf, len);

   if (ret < 0) {
      if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
         /* treat it as EOF */
         ret = 0;
      } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
         ret = -1;
         errno = EAGAIN; /* already happens to be set, but let's make sure */
      } else if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
         MSG("READ failed: TLS connection reset by server.\n");
      } else {
         MSG("READ failed with -0x%04x: an mbed tls error.\n", -ret);
      }
   }
   return ret;
}

/*
 * Write data to an open TLS connection.
 */
int a_Tls_write(void *conn, void *buf, size_t len)
{
   Conn_t *c = (Conn_t*)conn;
   int ret = mbedtls_ssl_write(c->ssl, buf, len);

   if (ret < 0) {
      MSG("WRITE failed with -0x%04x: an mbed tls error\n", -ret);
   }
   return ret;
}

void a_Tls_close_by_fd(int fd)
{
   FdMapEntry_t *fme = dList_find_custom(fd_map, INT2VOIDP(fd),
                                         Tls_fd_map_cmp);

   if (fme) {
      Tls_close_by_key(fme->connkey);
   }
}

static void Tls_cert_authorities_print_summary()
{
   const int ca_len = dList_length(cert_authorities);
   int i, j;

   if (ca_len)
      MSG("TLS: Trusted during this session:\n");

   for (i = 0; i < ca_len; i++) {
      CertAuth_t *ca = (CertAuth_t *)dList_nth_data(cert_authorities, i);
      const int servers_len = ca->servers ? dList_length(ca->servers) : 0;
      char *ca_name = strstr(ca->name, "CN=");

      if (!ca_name)
         ca_name = strstr(ca->name, "OU=");

      if (ca_name)
         ca_name += 3;
      else
         ca_name = ca->name;
      MSG("- %s for: ", ca_name);

      for (j = 0; j < servers_len; j++) {
         Server_t *s = dList_nth_data(ca->servers, j);

         MSG("%s:%d ", s->hostname, s->port);
      }
      MSG("\n");
   }

}

/*
 * Free mbed tls's chain of certificates and free our data on which root
 * certificates caused us to trust which servers.
 */
static void Tls_cert_authorities_freeall()
{
   if (cacerts.next)
      mbedtls_x509_crt_free(cacerts.next);

   if (cert_authorities) {
      CertAuth_t *ca;
      int i, n = dList_length(cert_authorities);

      for (i = 0; i < n; i++) {
         ca = (CertAuth_t *) dList_nth_data(cert_authorities, i);
         dFree(ca->name);
         if (ca->servers)
            dList_free(ca->servers);
         dFree(ca);
      }
      dList_free(cert_authorities);
   }
}

static void Tls_servers_freeall()
{
   if (servers) {
      Server_t *s;
      int i, n = dList_length(servers);

      for (i = 0; i < n; i++) {
         s = (Server_t *) dList_nth_data(servers, i);
         dFree(s->hostname);
         dFree(s);
      }
      dList_free(servers);
   }
}

static void Tls_fd_map_remove_all()
{
   if (fd_map) {
      FdMapEntry_t *fme;
      int i, n = dList_length(fd_map);

      for (i = 0; i < n; i++) {
         fme = (FdMapEntry_t *) dList_nth_data(fd_map, i);
         dFree(fme);
      }
      dList_free(fd_map);
   }
}

/*
 * Clean up
 */
void a_Tls_freeall(void)
{
   Tls_cert_authorities_print_summary();

   Tls_fd_map_remove_all();
   Tls_cert_authorities_freeall();
   Tls_servers_freeall();
}

#endif /* ENABLE_SSL */
