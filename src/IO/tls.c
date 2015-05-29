/*
 * File: tls.c
 *
 * Copyright 2004 Garrett Kajmowicz <gkajmowi@tbaytel.net>
 * (for some bits derived from the https dpi, e.g., certificate handling)
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
 * 2009, 2010, 2011, 2012 Free Software Foundation, Inc.
 * (for the certificate hostname checking from wget)
 * Copyright (C) 2011 Benjamin Johnson <obeythepenguin@users.sourceforge.net>
 * (for the https code offered from dplus browser that formed the basis...)
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

/* https://www.ssllabs.com/ssltest/viewMyClient.html */

/*
 * Using TLS in Applications: http://datatracker.ietf.org/wg/uta/documents/
 * TLS: http://datatracker.ietf.org/wg/tls/documents/
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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>            /* tolower for wget stuff */
#include <stdio.h>
#include <errno.h>
#include "../../dlib/dlib.h"
#include "../dialog.hh"
#include "../klist.h"
#include "iowatch.hh"
#include "tls.h"
#include "Url.h"

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>    /* for hostname checking */

#define CERT_STATUS_NONE 0
#define CERT_STATUS_RECEIVING 1
#define CERT_STATUS_GOOD 2
#define CERT_STATUS_BAD 3
#define CERT_STATUS_USER_ACCEPTED 4

typedef struct {
   char *hostname;
   int port;
   int cert_status;
} Server_t;

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
   SSL *ssl;
   bool_t connecting;
} Conn_t;

/* List of active TLS connections */
static Klist_t *conn_list = NULL;

/*
 * If ssl_context is still NULL, this corresponds to TLS being disabled.
 */
static SSL_CTX *ssl_context;
static Dlist *servers;
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
static int Tls_conn_new(int fd, const DilloUrl *url, SSL *ssl)
{
   int key;

   Conn_t *conn = dNew0(Conn_t, 1);
   conn->fd = fd;
   conn->url = a_Url_dup(url);
   conn->ssl = ssl;
   conn->connecting = TRUE;

   key = a_Klist_insert(&conn_list, conn);

   Tls_fd_map_add_entry(fd, key);

   return key;
}

/*
 * Let's monitor for TLS alerts.
 */
static void Tls_info_cb(const SSL *ssl, int where, int ret)
{
   if (where & SSL_CB_ALERT) {
      MSG("TLS ALERT on %s: %s\n", (where & SSL_CB_READ) ? "read" : "write",
          SSL_alert_desc_string_long(ret));
   }
}

/*
 * Load trusted certificates.
 * This is like using SSL_CTX_load_verify_locations() but permitting more
 * than one bundle and more than one directory. Due to the notoriously
 * abysmal openssl documentation, this was worked out from reading discussion
 * on the web and then reading openssl source to see what it normally does.
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
   static const char *ca_files[] = {
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/usr/share/ssl/certs/ca-bundle.crt",
      "/usr/local/share/certs/ca-root.crt",
      "/etc/ssl/cert.pem",
      CA_CERTS_FILE
   };

   static const char *ca_paths[] = {
      "/etc/ssl/certs/",
      CA_CERTS_DIR
   };

   X509_STORE *store = SSL_CTX_get_cert_store(ssl_context);
   X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());

   for (u = 0; u < sizeof(ca_files) / sizeof(ca_files[0]); u++) {
      if (*ca_files[u])
         X509_LOOKUP_load_file(lookup, ca_files[u], X509_FILETYPE_PEM);
   }

   lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir());
   for (u = 0; u < sizeof(ca_paths)/sizeof(ca_paths[0]); u++) {
      if (*ca_paths[u])
         X509_LOOKUP_add_dir(lookup, ca_paths[u], X509_FILETYPE_PEM);
   }

   userpath = dStrconcat(dGethomedir(), "/.dillo/certs/", NULL);
   X509_LOOKUP_add_dir(lookup, userpath, X509_FILETYPE_PEM);
   dFree(userpath);

   /* Clear out errors in the queue (file not found, etc.) */
   while(ERR_get_error())
      ;
}

/*
 * Initialize the OpenSSL library.
 */
void a_Tls_init(void)
{
   SSL_library_init();
   SSL_load_error_strings();
   if (RAND_status() != 1) {
      /* The standard solution is to provide it with more entropy, but this
       * involves knowing very well that you are doing exactly the right thing.
       */
      MSG_ERR("Disabling HTTPS: Insufficient entropy for openssl.\n");
      return;
   }

   /* Create SSL context */
   ssl_context = SSL_CTX_new(SSLv23_client_method());
   if (ssl_context == NULL) {
      MSG_ERR("Disabling HTTPS: Error creating SSL context.\n");
      return;
   }

   SSL_CTX_set_info_callback(ssl_context, Tls_info_cb);

   /* Don't want: eNULL, which has no encryption; aNULL, which has no
    * authentication; LOW, which as of 2014 use 64 or 56-bit encryption;
    * EXPORT40, which uses 40-bit encryption; RC4, for which methods were
    * found in 2013 to defeat it somewhat too easily.
    */
   SSL_CTX_set_cipher_list(ssl_context,
                           "ALL:!aNULL:!eNULL:!LOW:!EXPORT40:!RC4");

   /* SSL2 has been known to be insecure forever, disabling SSL3 is in response
    * to POODLE, and disabling compression is in response to CRIME.
    */
   SSL_CTX_set_options(ssl_context,
                       SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_COMPRESSION);

   /* This lets us deal with self-signed certificates */
   SSL_CTX_set_verify(ssl_context, SSL_VERIFY_NONE, NULL);

   Tls_load_certificates();

   fd_map = dList_new(20);
   servers = dList_new(8);
}

/*
 * Save certificate with a hashed filename.
 * Return: 0 on success, 1 on failure.
 */
static int Tls_save_certificate_home(X509 * cert)
{
   char buf[4096];

   FILE * fp = NULL;
   uint_t i = 0;
   int ret = 1;

   /* Attempt to create .dillo/certs blindly - check later */
   snprintf(buf, 4096, "%s/.dillo/", dGethomedir());
   mkdir(buf, 01777);
   snprintf(buf, 4096, "%s/.dillo/certs/", dGethomedir());
   mkdir(buf, 01777);

   do {
      snprintf(buf, 4096, "%s/.dillo/certs/%lx.%u",
               dGethomedir(), X509_subject_name_hash(cert), i);

      fp=fopen(buf, "r");
      if (fp == NULL){
         /* File name doesn't exist so we can use it safely */
         fp=fopen(buf, "w");
         if (fp == NULL){
            MSG("Unable to open cert save file in home dir\n");
            break;
         } else {
            PEM_write_X509(fp, cert);
            fclose(fp);
            MSG("Wrote certificate\n");
            ret = 0;
            break;
         }
      } else {
         fclose(fp);
      }
      i++;
      /* Don't loop too many times - just give up */
   } while (i < 1024);

   return ret;
}

/*
 * Test whether a URL corresponds to a server.
 */
static int Tls_servers_cmp(const void *v1, const void *v2)
{
   Server_t *s = (Server_t *)v1;
   const DilloUrl *url = (const DilloUrl *)v2;
   const char *host = URL_HOST(url);
   int port = URL_PORT(url);

   return (dStrAsciiCasecmp(s->hostname, host) || (port != s->port));
}

/*
 * The purpose here is to permit a single initial connection to a server.
 * Once we have the certificate, know whether we like it -- and whether the
 * user accepts it -- HTTP can run through queued sockets as normal.
 *
 * Return: 1 means yes, 0 means not yet, -1 means never.
 * TODO: Something clearer or different.
 */
int a_Tls_connect_ready(const DilloUrl *url)
{
   Server_t *s;
   int i, len;
   const char *host = URL_HOST(url);
   const int port = URL_PORT(url);
   int ret = TLS_CONNECT_READY;

   dReturn_val_if_fail(ssl_context, TLS_CONNECT_NEVER);

   len = dList_length(servers);

   for (i = 0; i < len; i++) {
      s = dList_nth_data(servers, i);

      if (!dStrAsciiCasecmp(s->hostname, host) && (port == s->port)) {
         if (s->cert_status == CERT_STATUS_RECEIVING)
            ret = TLS_CONNECT_NOT_YET;
         else if (s->cert_status == CERT_STATUS_BAD)
            ret = TLS_CONNECT_NEVER;

         if (s->cert_status == CERT_STATUS_NONE)
            s->cert_status = CERT_STATUS_RECEIVING;
         return ret;
      }
   }
   s = dNew(Server_t, 1);

   s->port = port;
   s->hostname = dStrdup(host);
   s->cert_status = CERT_STATUS_RECEIVING;
   dList_append(servers, s);
   return ret;
}

/*
 * Did we find problems with the certificate, and did the user proceed to
 * reject the connection?
 */
static int Tls_user_said_no(const DilloUrl *url)
{
   Server_t *s = dList_find_custom(servers, url, Tls_servers_cmp);

   if (!s)
      return FALSE;

   return s->cert_status == CERT_STATUS_BAD;
}

/******************** BEGINNING OF STUFF DERIVED FROM wget-1.16.3 */

#define ASTERISK_EXCLUDES_DOT   /* mandated by rfc2818 */

/* Return true is STRING (case-insensitively) matches PATTERN, false
   otherwise.  The recognized wildcard character is "*", which matches
   any character in STRING except ".".  Any number of the "*" wildcard
   may be present in the pattern.

   This is used to match of hosts as indicated in rfc2818: "Names may
   contain the wildcard character * which is considered to match any
   single domain name component or component fragment. E.g., *.a.com
   matches foo.a.com but not bar.foo.a.com. f*.com matches foo.com but
   not bar.com [or foo.bar.com]."

   If the pattern contain no wildcards, pattern_match(a, b) is
   equivalent to !strcasecmp(a, b).  */

static bool_t pattern_match (const char *pattern, const char *string)
{

  const char *p = pattern, *n = string;
  char c;
  for (; (c = tolower (*p++)) != '\0'; n++)
    if (c == '*')
      {
        for (c = tolower (*p); c == '*'; c = tolower (*++p))
          ;
        for (; *n != '\0'; n++)
          if (tolower (*n) == c && pattern_match (p, n))
            return TRUE;
#ifdef ASTERISK_EXCLUDES_DOT
          else if (*n == '.')
            return FALSE;
#endif
        return c == '\0';
      }
    else
      {
        if (c != tolower (*n))
          return FALSE;
      }
  return *n == '\0';
}

/*
 * Check that the certificate corresponds to the site it's presented for.
 *
 * Return TRUE if the hostname matched or the user indicated acceptance.
 * FALSE on failure.
 */
static bool_t Tls_check_cert_hostname(X509 *cert, const char *host,
                                      int *choice)
{
   dReturn_val_if_fail(cert && host, FALSE);

   char *msg;
   GENERAL_NAMES *subjectAltNames;
   bool_t success = TRUE, alt_name_checked = FALSE;;
   char common_name[256];

  /* Check that HOST matches the common name in the certificate.
     #### The following remains to be done:

     - When matching against common names, it should loop over all
       common names and choose the most specific one, i.e. the last
       one, not the first one, which the current code picks.

     - Ensure that ASN1 strings from the certificate are encoded as
       UTF-8 which can be meaningfully compared to HOST.  */

  subjectAltNames = X509_get_ext_d2i (cert, NID_subject_alt_name, NULL, NULL);

  if (subjectAltNames)
    {
      /* Test subject alternative names */

      Dstr *err = dStr_new("");
      dStr_sprintf(err, "Hostname %s does not match any of certificate's "
                        "Subject Alternative Names: ", host);

      /* Do we want to check for dNSNAmes or ipAddresses (see RFC 2818)?
       * Signal it by host_in_octet_string. */
      ASN1_OCTET_STRING *host_in_octet_string = a2i_IPADDRESS (host);

      int numaltnames = sk_GENERAL_NAME_num (subjectAltNames);
      int i;
      for (i=0; i < numaltnames; i++)
        {
          const GENERAL_NAME *name =
            sk_GENERAL_NAME_value (subjectAltNames, i);
          if (name)
            {
              if (host_in_octet_string)
                {
                  if (name->type == GEN_IPADD)
                    {
                      /* Check for ipAddress */
                      /* TODO: Should we convert between IPv4-mapped IPv6
                       * addresses and IPv4 addresses? */
                      alt_name_checked = TRUE;
                      if (!ASN1_STRING_cmp (host_in_octet_string,
                            name->d.iPAddress))
                        break;
                      dStr_sprintfa(err, "%s ", name->d.iPAddress);
                    }
                }
              else if (name->type == GEN_DNS)
                {
                  /* dNSName should be IA5String (i.e. ASCII), however who
                   * does trust CA? Convert it into UTF-8 for sure. */
                  unsigned char *name_in_utf8 = NULL;

                  /* Check for dNSName */
                  alt_name_checked = TRUE;

                  if (0 <= ASN1_STRING_to_UTF8 (&name_in_utf8, name->d.dNSName))
                    {
                      /* Compare and check for NULL attack in ASN1_STRING */
                      if (pattern_match ((char *)name_in_utf8, host) &&
                            (strlen ((char *)name_in_utf8) ==
                                (size_t)ASN1_STRING_length (name->d.dNSName)))
                        {
                          OPENSSL_free (name_in_utf8);
                          break;
                        }
                      dStr_sprintfa(err, "%s ", name_in_utf8);
                      OPENSSL_free (name_in_utf8);
                    }
                }
            }
        }
      sk_GENERAL_NAME_pop_free(subjectAltNames, GENERAL_NAME_free);
      if (host_in_octet_string)
        ASN1_OCTET_STRING_free(host_in_octet_string);

      if (alt_name_checked == TRUE && i >= numaltnames)
        {
         success = FALSE;
         *choice = a_Dialog_choice("Dillo TLS security warning",
            err->str, "Continue", "Cancel", NULL);

         switch (*choice){
            case 1:
               success = TRUE;
               break;
            case 2:
               break;
            default:
               break;
         }
        }
      dStr_free(err, 1);
    }

  if (alt_name_checked == FALSE)
    {
      /* Test commomName */
      X509_NAME *xname = X509_get_subject_name(cert);
      common_name[0] = '\0';
      X509_NAME_get_text_by_NID (xname, NID_commonName, common_name,
                                 sizeof (common_name));

      if (!pattern_match (common_name, host))
        {
          success = FALSE;
         msg = dStrconcat("Certificate common name ", common_name,
                          " doesn't match requested host name ", host, NULL);
         *choice = a_Dialog_choice("Dillo TLS security warning",
            msg, "Continue", "Cancel", NULL);
         dFree(msg);

         switch (*choice){
            case 1:
               success = TRUE;
               break;
            case 2:
               break;
            default:
               break;
         }
        }
      else
        {
          /* We now determine the length of the ASN1 string. If it
           * differs from common_name's length, then there is a \0
           * before the string terminates.  This can be an instance of a
           * null-prefix attack.
           *
           * https://www.blackhat.com/html/bh-usa-09/bh-usa-09-archives.html#Marlinspike
           * */

          int i = -1, j;
          X509_NAME_ENTRY *xentry;
          ASN1_STRING *sdata;

          if (xname) {
            for (;;)
              {
                j = X509_NAME_get_index_by_NID (xname, NID_commonName, i);
                if (j == -1) break;
                i = j;
              }
          }

          xentry = X509_NAME_get_entry(xname,i);
          sdata = X509_NAME_ENTRY_get_data(xentry);
          if (strlen (common_name) != (size_t)ASN1_STRING_length (sdata))
            {
              success = FALSE;
         msg = dStrconcat("Certificate common name is invalid (contains a NUL "
                          "character). This may be an indication that the "
                          "host is not who it claims to be -- that is, not "
                          "the real ", host, NULL);
         *choice = a_Dialog_choice("Dillo TLS security warning",
            msg, "Continue", "Cancel", NULL);
         dFree(msg);

         switch (*choice){
            case 1:
               success = TRUE;
               break;
            case 2:
               break;
            default:
               break;
         }
            }
        }
    }
    return success;
}

/******************** END OF STUFF DERIVED FROM wget-1.16.3 */

/*
 * Get the certificate at the end of the chain, or NULL on failure.
 *
 * Rumor has it that the stack can be NULL if a connection has been reused
 * and that the stack can then be reconstructed if necessary, but it doesn't
 * sound like a case we'll encounter.
 */
static X509 *Tls_get_end_of_chain(SSL *ssl)
{
   STACK_OF(X509) *sk = SSL_get_peer_cert_chain(ssl);

   return sk ? sk_X509_value(sk, sk_X509_num(sk) - 1) : NULL;
}

static void Tls_get_issuer_name(X509 *cert, char *buf, uint_t buflen)
{
   if (cert) {
      X509_NAME_oneline(X509_get_issuer_name(cert), buf, buflen);
   } else {
      strncpy(buf, "(unknown)", buflen);
      buf[buflen-1] = '\0';
   }
}

static void Tls_get_expiration_str(X509 *cert, char *buf, uint_t buflen)
{
   ASN1_TIME *exp_date = X509_get_notAfter(cert);
   BIO *b = BIO_new(BIO_s_mem());
   int rc = ASN1_TIME_print(b, exp_date);

   if (rc > 0) {
      rc = BIO_gets(b, buf, buflen);
   }
   if (rc <= 0) {
      strncpy(buf, "(unknown)", buflen);
      buf[buflen-1] = '\0';
   }
   BIO_free(b);
}

/*
 * Examine the certificate, and, if problems are detected, ask the user what
 * to do.
 * Return: -1 if connection should be canceled, or 0 if it should continue.
 */
static int Tls_examine_certificate(SSL *ssl, Server_t *srv,const char *host)
{
   X509 *remote_cert;
   long st;
   const uint_t buflen = 4096;
   char buf[buflen], *cn, *msg;
   int choice = -1, ret = -1;
   char *title = dStrconcat("Dillo TLS security warning: ", host, NULL);

   remote_cert = SSL_get_peer_certificate(ssl);
   if (remote_cert == NULL){
      /* Inform user that remote system cannot be trusted */
      choice = a_Dialog_choice(title,
         "The remote system is not presenting a certificate. "
         "This site cannot be trusted. Sending data is not safe.",
         "Continue", "Cancel", NULL);

      /* Abort on anything but "Continue" */
      if (choice == 1){
         ret = 0;
      }

   } else if (Tls_check_cert_hostname(remote_cert, host, &choice)) {
      /* Figure out if (and why) the remote system can't be trusted */
      st = SSL_get_verify_result(ssl);
      switch (st) {
      case X509_V_OK:
         ret = 0;
         break;
      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
         /* Either self signed and untrusted */
         /* Extract CN from certificate name information */
         if ((cn = strstr(remote_cert->name, "/CN=")) == NULL) {
            strcpy(buf, "(no CN given)");
         } else {
            char *cn_end;

            cn += 4;

            if ((cn_end = strstr(cn, "/")) == NULL )
               cn_end = cn + strlen(cn);

            strncpy(buf, cn, (size_t) (cn_end - cn));
            buf[cn_end - cn] = '\0';
         }
         msg = dStrconcat("The remote certificate is self-signed and "
                          "untrusted. For address: ", buf, NULL);
         choice = a_Dialog_choice(title,
            msg, "Continue", "Cancel", "Save Certificate", NULL);
         dFree(msg);

         switch (choice){
            case 1:
               ret = 0;
               break;
            case 2:
               break;
            case 3:
               /* Save certificate to a file here and recheck the chain */
               /* Potential security problems because we are writing
                * to the filesystem */
               Tls_save_certificate_home(remote_cert);
               ret = 1;
               break;
            default:
               break;
         }
         break;
      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
      case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
         choice = a_Dialog_choice(title,
            "The issuer for the remote certificate cannot be found. "
            "The authenticity of the remote certificate cannot be trusted.",
            "Continue", "Cancel", NULL);

         if (choice == 1) {
            ret = 0;
         }
         break;

      case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
      case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
      case X509_V_ERR_CERT_SIGNATURE_FAILURE:
      case X509_V_ERR_CRL_SIGNATURE_FAILURE:
         choice = a_Dialog_choice(title,
            "The remote certificate signature could not be read "
            "or is invalid and should not be trusted",
            "Continue", "Cancel", NULL);

         if (choice == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_CERT_NOT_YET_VALID:
      case X509_V_ERR_CRL_NOT_YET_VALID:
         choice = a_Dialog_choice(title,
            "Part of the remote certificate is not yet valid. "
            "Certificates usually have a range of dates over which "
            "they are to be considered valid, and the certificate "
            "presented has a starting validity after today's date "
            "You should be cautious about using this site",
            "Continue", "Cancel", NULL);

         if (choice == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_CERT_HAS_EXPIRED:
      case X509_V_ERR_CRL_HAS_EXPIRED:
         Tls_get_expiration_str(remote_cert, buf, buflen);
         msg = dStrconcat("The remote certificate expired on: ", buf,
                          ". This site can no longer be trusted.", NULL);

         choice = a_Dialog_choice(title, msg, "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         dFree(msg);
         break;
      case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
      case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
      case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
      case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
         choice = a_Dialog_choice(title,
            "There was an error in the certificate presented. "
            "Some of the certificate data was improperly formatted "
            "making it impossible to determine if the certificate "
            "is valid.  You should not trust this certificate.",
            "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_INVALID_CA:
      case X509_V_ERR_INVALID_PURPOSE:
      case X509_V_ERR_CERT_UNTRUSTED:
      case X509_V_ERR_CERT_REJECTED:
      case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
         choice = a_Dialog_choice(title,
            "One of the certificates in the chain is being used "
            "incorrectly (possibly due to configuration problems "
            "with the remote system.  The connection should not "
            "be trusted",
            "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
      case X509_V_ERR_AKID_SKID_MISMATCH:
      case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
         choice = a_Dialog_choice(title,
            "Some of the information presented by the remote system "
            "does not match other information presented. "
            "This may be an attempt to eavesdrop on communications",
            "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
         Tls_get_issuer_name(Tls_get_end_of_chain(ssl), buf, buflen);
         msg = dStrconcat("Certificate chain led to a self-signed certificate "
                          "instead of a trusted root. Name: ",  buf , NULL);
         choice = a_Dialog_choice(title, msg, "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         dFree(msg);
         break;
      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
         Tls_get_issuer_name(Tls_get_end_of_chain(ssl), buf, buflen);
         msg = dStrconcat("The issuer certificate of an untrusted certificate "
                          "cannot be found. Issuer: ", buf, NULL);
         choice = a_Dialog_choice(title, msg, "Continue", "Cancel", NULL);
         if (choice == 1) {
            ret = 0;
         }
         dFree(msg);
         break;
      default:             /* Need to add more options later */
         snprintf(buf, 80,
                  "The remote certificate cannot be verified (code %ld)", st);
         choice = a_Dialog_choice(title,
            buf, "Continue", "Cancel", NULL);
         /* abort on anything but "Continue" */
         if (choice == 1){
            ret = 0;
         }
      }
      X509_free(remote_cert);
      remote_cert = 0;
   }
   dFree(title);

   if (choice == 2)
      srv->cert_status = CERT_STATUS_BAD;
   else if (choice == -1)
      srv->cert_status = CERT_STATUS_GOOD;
   else
      srv->cert_status = CERT_STATUS_USER_ACCEPTED;

   return ret;
}

/*
 * If the connection was closed before we got the certificate, we need to
 * reset state so that we'll try again.
 */
void a_Tls_reset_server_state(const DilloUrl *url)
{
   if (servers) {
      Server_t *s = dList_find_custom(servers, url, Tls_servers_cmp);

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
      SSL_shutdown(c->ssl);
      SSL_free(c->ssl);

      a_Url_free(c->url);
      Tls_fd_map_remove_entry(c->fd);
      a_Klist_remove(conn_list, connkey);
      dFree(c);
   }
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

   assert(!ERR_get_error());

   ret = SSL_connect(conn->ssl);

   if (ret <= 0) {
      int err1_ret = SSL_get_error(conn->ssl, ret);
      if (err1_ret == SSL_ERROR_WANT_READ ||
          err1_ret == SSL_ERROR_WANT_WRITE) {
         int want = err1_ret == SSL_ERROR_WANT_READ ? DIO_READ : DIO_WRITE;

         _MSG("iowatching fd %d for tls -- want %s\n", fd,
             err1_ret == SSL_ERROR_WANT_READ ? "read" : "write");
         a_IOwatch_remove_fd(fd, -1);
         a_IOwatch_add_fd(fd, want, Tls_connect_cb, INT2VOIDP(connkey));
         ongoing = TRUE;
         failed = FALSE;
      } else if (err1_ret == SSL_ERROR_SYSCALL || err1_ret == SSL_ERROR_SSL) {
         unsigned long err2_ret = ERR_get_error();

         if (err2_ret) {
            do {
               MSG("SSL_connect() failed: %s\n",
                   ERR_error_string(err2_ret, NULL));
            } while ((err2_ret = ERR_get_error()));
         } else {
            /* nothing in the error queue */
            if (ret == 0) {
               MSG("TLS connect error: \"an EOF was observed that violates "
                   "the protocol\"\n");
               /*
                * I presume we took too long on our side and the server grew
                * impatient.
                */
            } else if (ret == -1) {
               MSG("TLS connect error: %s\n", dStrerror(errno));

               /* If the following can happen, I'll add code to handle it, but
                * I don't want to add code blindly if it isn't getting used
                */
               assert(errno != EAGAIN && errno != EINTR);
            } else {
               MSG_ERR("According to the man page for SSL_get_error(), this "
                       "was not a possibility (ret %d).\n", ret);
            }
         }
      } else {
         MSG("SSL_get_error() returned %d on a connect.\n", err1_ret);
      }
   } else {
      Server_t *srv = dList_find_custom(servers, conn->url, Tls_servers_cmp);

      if (srv->cert_status == CERT_STATUS_RECEIVING) {
         /* Making first connection with the server */
         const char *version = SSL_get_version(conn->ssl);
         const SSL_CIPHER *cipher = SSL_get_current_cipher(conn->ssl);

         MSG("%s: %s, cipher %s\n", URL_AUTHORITY(conn->url), version,
             SSL_CIPHER_get_name(cipher));
      }

      if (srv->cert_status == CERT_STATUS_USER_ACCEPTED ||
          (Tls_examine_certificate(conn->ssl, srv, URL_HOST(conn->url))!=-1)) {
         failed = FALSE;
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
         a_IOwatch_remove_fd(fd, DIO_READ|DIO_WRITE);
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
   SSL *ssl;
   bool_t success = TRUE;
   int connkey = -1;

   if (!ssl_context)
      success = FALSE;

   if (success && Tls_user_said_no(url)) {
      success = FALSE;
   }

   assert(!ERR_get_error());

   if (success && !(ssl = SSL_new(ssl_context))) {
      unsigned long err_ret = ERR_get_error();
      do {
         MSG("SSL_new() failed: %s\n", ERR_error_string(err_ret, NULL));
      } while ((err_ret = ERR_get_error()));
      success = FALSE;
   }

   /* assign TLS connection to this file descriptor */
   if (success && !SSL_set_fd(ssl, fd)) {
      unsigned long err_ret = ERR_get_error();
      do {
         MSG("SSL_set_fd() failed: %s\n", ERR_error_string(err_ret, NULL));
      } while ((err_ret = ERR_get_error()));
      success = FALSE;
   }

   if (success)
      connkey = Tls_conn_new(fd, url, ssl);

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
   /* Server Name Indication. From the openssl changelog, it looks like this
    * came along in 2010.
    */
   if (success && !a_Url_host_is_ip(URL_HOST(url)))
      SSL_set_tlsext_host_name(ssl, URL_HOST(url));
#endif

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
   return SSL_read(c->ssl, buf, len);
}

/*
 * Write data to an open TLS connection.
 */
int a_Tls_write(void *conn, void *buf, size_t len)
{
   Conn_t *c = (Conn_t*)conn;
   return SSL_write(c->ssl, buf, len);
}

void a_Tls_close_by_fd(int fd)
{
   FdMapEntry_t *fme = dList_find_custom(fd_map, INT2VOIDP(fd),
                                         Tls_fd_map_cmp);

   if (fme) {
      Tls_close_by_key(fme->connkey);
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
 * Clean up the OpenSSL library
 */
void a_Tls_freeall(void)
{
   if (ssl_context)
      SSL_CTX_free(ssl_context);
   Tls_fd_map_remove_all();
   Tls_servers_freeall();
}

#endif /* ENABLE_SSL */
