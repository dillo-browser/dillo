/*
 * Dpi for HTTPS.
 *
 *
 *
 *                            W A R N I N G
 *
 * One of the important things to have in mind is about whether
 * unix domain sockets (UDS) are secure for https. I mean, root can always
 * snoop on sockets (regardless of permissions) so he'd be able to "see" all
 * the traffic. OTOH, if someone has root access on a machine he can do
 * anything, and that includes modifying the binaries, peeking-up in
 * memory space, installing a key-grabber, ...
 *
 *
 * Copyright 2003, 2004 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright 2004 Garrett Kajmowicz <gkajmowi@tbaytel.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * As a special exception permission is granted to link the code of
 * the https dillo plugin with the OpenSSL project's OpenSSL library
 * (or a modified version of that library), and distribute the linked
 * executables, without including the source code for the SSL library
 * in the source distribution. You must obey the GNU General Public
 * License, version 3, in all respects for all of the code used other
 * than the SSL library.
 *
 */

/*
 * TODO: a lot of things, this is just a bare bones example.
 *
 * For instance:
 * - Handle cookies (now that they arrive with the dpip tag, it needs
 *   testing).
 * - Certificate authentication (asking the user in case it can't be verified)
 * - Certificate management.
 * - Session caching ...
 *
 */

#include <config.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "../dpip/dpip.h"
#include "dpiutil.h"

/*
 * Debugging macros
 */
#define SILENT 1
#define _MSG(...)
#if SILENT
 #define MSG(...)
#else
 #define MSG(...)  fprintf(stderr, "[https dpi]: " __VA_ARGS__)
#endif


#ifdef ENABLE_SSL

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

static int get_network_connection(char * url);
static int handle_certificate_problem(SSL * ssl_connection);
static int save_certificate_home(X509 * cert);

#endif



/*---------------------------------------------------------------------------*/
/*
 * Global variables
 */
static char *root_url = NULL;  /*Holds the URL we are connecting to*/
static Dsh *sh;


#ifdef ENABLE_SSL

/*
 * Read the answer dpip tag for a dialog and return the number for
 * the user-selected alternative.
 * Return: (-1: parse error, 0: window closed, 1-5 alt. number)
 */
static int dialog_get_answer_number(void)
{
   int response_number = -1;
   char *dpip_tag, *response;

   /* Read the dpi command from STDIN */
   dpip_tag = a_Dpip_dsh_read_token(sh, 1);
   response = a_Dpip_get_attr(dpip_tag, "msg");
   response_number = (response) ? strtol (response, NULL, 10) : -1;
   dFree(dpip_tag);
   dFree(response);

   return response_number;
}


/*
 *  This function does all of the work with SSL
 */
static void yes_ssl_support(void)
{
   /* The following variable will be set to 1 in the event of
    * an error and skip any further processing
    */
   int exit_error = 0;
   SSL_CTX * ssl_context = NULL;
   SSL * ssl_connection = NULL;

   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *http_query = NULL,
        *proxy_url = NULL, *proxy_connect = NULL, *check_cert = NULL;
   char buf[4096];
   int ret = 0;
   int network_socket = -1;


   MSG("{In https.filter.dpi}\n");

   /*Initialize library*/
   SSL_load_error_strings();
   SSL_library_init();
   if (RAND_status() != 1){
      /*Insufficient entropy.  Deal with it?*/
      MSG("Insufficient random entropy\n");
   }

   /*Create context and SSL object*/
   if (exit_error == 0){
      ssl_context = SSL_CTX_new(SSLv23_client_method());
      if (ssl_context == NULL){
         MSG("Error creating SSL context\n");
         exit_error = 1;
      }
   }

   /* SSL2 has been known to be insecure forever, disabling SSL3 is in response
    * to POODLE, and disabling compression is in response to CRIME.
    */
   if (exit_error == 0){
      SSL_CTX_set_options(ssl_context,
                        SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_COMPRESSION);
   }

   /*Set directory to load certificates from*/
   /*FIXME - provide for sysconfdir variables and such*/
   if (exit_error == 0){
      if (SSL_CTX_load_verify_locations(
         ssl_context, NULL, "/etc/ssl/certs/" ) == 0){
         MSG("Error opening system x509 certificate location\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0){
      snprintf(buf, 4095, "%s/.dillo/certs/", dGethomedir());
      if (SSL_CTX_load_verify_locations(ssl_context, NULL, buf )==0){
         MSG("Error opening user x509 certificate location\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0){
      ssl_connection = SSL_new(ssl_context);
      if (ssl_connection == NULL){
         MSG("Error creating SSL connection\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0){
      /* Don't want: eNULL, which has no encryption; aNULL, which has no
       * authentication; LOW, which as of 2014 use 64 or 56-bit encryption;
       * EXPORT40, which uses 40-bit encryption; RC4, for which methods were
       * found in 2013 to defeat it somewhat too easily.
       */
      SSL_CTX_set_cipher_list(ssl_context,
                              "ALL:!aNULL:!eNULL:!LOW:!EXPORT40:!RC4");

      /* Need to do this if we want to have the option of dealing
       * with self-signed certs
       */
      SSL_set_verify(ssl_connection, SSL_VERIFY_NONE, 0);

      /*Get the network address and command to be used*/
      dpip_tag = a_Dpip_dsh_read_token(sh, 1);
      cmd = a_Dpip_get_attr(dpip_tag, "cmd");
      proxy_url = a_Dpip_get_attr(dpip_tag, "proxy_url");
      proxy_connect =
                  a_Dpip_get_attr(dpip_tag, "proxy_connect");
      url = a_Dpip_get_attr(dpip_tag, "url");
      http_query = a_Dpip_get_attr(dpip_tag, "query");
      if (!(check_cert = a_Dpip_get_attr(dpip_tag, "check_cert"))) {
         /* allow older dillo versions use this dpi */
         check_cert = dStrdup("true");
      }

      if (!cmd || !url || !http_query) {
         MSG("***Value of cmd, url or http_query is NULL"
                    " - cannot continue\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0){
      char *connect_url = proxy_url ? proxy_url : url;

      network_socket = get_network_connection(connect_url);
      if (network_socket<0){
         MSG("Network socket create error\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0 && proxy_connect != NULL) {
      ssize_t St;
      const char *p = proxy_connect;
      int writelen = strlen(proxy_connect);

      while (writelen > 0) {
         St = write(network_socket, p, writelen);
         if (St < 0) {
            /* Error */
            if (errno != EINTR) {
               MSG("Error writing to proxy.\n");
               exit_error = 1;
               break;
            }
         } else {
            p += St;
            writelen -= St;
         }
      }
      if (exit_error == 0) {
         const size_t buflen = 200;
         char buf[buflen];
         Dstr *reply = dStr_new("");

         while (1) {
            St = read(network_socket, buf, buflen);
            if (St > 0) {
               dStr_append_l(reply, buf, St);
               if (strstr(reply->str, "\r\n\r\n")) {
                  /* have whole reply header */
                  if (reply->len >= 12 && reply->str[9] == '2') {
                     /* e.g. "HTTP/1.1 200 Connection established[...]" */
                     MSG("CONNECT through proxy succeeded.\n");
                  } else {
                     /* TODO: send reply body to dillo */
                     exit_error = 1;
                     MSG("CONNECT through proxy failed.\n");
                  }
                  break;
               }
            } else if (St < 0) {
               if (errno != EINTR) {
                  exit_error = 1;
                  MSG("Error reading from proxy.\n");
                  break;
               }
            }
         }
         dStr_free(reply, 1);
      }
   }

   if (exit_error == 0){
      /* Configure SSL to use network file descriptor */
      if (SSL_set_fd(ssl_connection, network_socket) == 0){
         MSG("Error connecting network socket to SSL\n");
         exit_error = 1;
     }
   }

   if (exit_error == 0){
      /*Actually do SSL connection handshake*/
      if (SSL_connect(ssl_connection) != 1){
         MSG("SSL_connect failed\n");
         ERR_print_errors_fp(stderr);
         exit_error = 1;
      }
   }

   /*Use handle error function to decide what to do*/
   if (exit_error == 0){
      if (strcmp(check_cert, "true") == 0 &&
          handle_certificate_problem(ssl_connection) < 0){
         MSG("Certificate verification error\n");
         exit_error = 1;
      }
   }

   if (exit_error == 0) {
      char *d_cmd;

      /*Send query we want*/
      SSL_write(ssl_connection, http_query, (int)strlen(http_query));

      /*Analyse response from server*/

      /*Send dpi command*/
      d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
      a_Dpip_dsh_write_str(sh, 1, d_cmd);
      dFree(d_cmd);

      /*Send remaining data*/

      while ((ret = SSL_read(ssl_connection, buf, 4096)) > 0 ){
         /* flush is good for dialup speed */
         a_Dpip_dsh_write(sh, 1, buf, (size_t)ret);
      }
   }

   /*Begin cleanup of all resources used*/
   dFree(dpip_tag);
   dFree(cmd);
   dFree(url);
   dFree(http_query);
   dFree(proxy_url);
   dFree(proxy_connect);
   dFree(check_cert);

   if (network_socket != -1){
      dClose(network_socket);
      network_socket = -1;
   }
   if (ssl_connection != NULL){
      SSL_free(ssl_connection);
      ssl_connection = NULL;
   }
   if (ssl_context != NULL){
      SSL_CTX_free(ssl_context);
      ssl_context = NULL;
   }
}

/*
 * The following function attempts to open up a connection to the
 * remote server and return the file descriptor number of the
 * socket.  Returns -1 in the event of an error
 */
static int get_network_connection(char * url)
{
   struct sockaddr_in address;
   struct hostent *hp;

   int s;
   int url_offset = 0;
   int portnum = 443;
   uint_t url_look_up_length = 0;
   char * url_look_up = NULL;

   /*Determine how much of url we chop off as unneeded*/
   if (dStrnAsciiCasecmp(url, "https://", 8) == 0){
      url_offset = 8;
   } else if (dStrnAsciiCasecmp(url, "http://", 7) == 0) {
      url_offset = 7;
      portnum = 80;
   }

   /*Find end of URL*/

   if (strpbrk(url+url_offset, ":/") != NULL){
      url_look_up_length = strpbrk(url+url_offset, ":/") - (url+url_offset);
      url_look_up = dStrndup(url+url_offset, url_look_up_length);

      /*Check for port number*/
      if (strchr(url+url_offset, ':') ==
          (url + url_offset + url_look_up_length)){
         portnum = strtol(url + url_offset + url_look_up_length + 1, NULL, 10);
      }
   } else {
      url_look_up = url + url_offset;
   }

   root_url = dStrdup(url_look_up);
   hp=gethostbyname(url_look_up);

   /*url_look_uip no longer needed, so free if necessary*/
   if (url_look_up_length != 0){
      dFree(url_look_up);
   }

   if (hp == NULL){
      MSG("gethostbyname() failed\n");
      return -1;
   }

   memset(&address,0,sizeof(address));
   memcpy((char *)&address.sin_addr, hp->h_addr, (size_t)hp->h_length);
   address.sin_family=hp->h_addrtype;
   address.sin_port= htons((u_short)portnum);

   s = socket(hp->h_addrtype, SOCK_STREAM, 0);
   if (connect(s, (struct sockaddr *)&address, sizeof(address)) != 0){
      dClose(s);
      s = -1;
      MSG("errno: %i\n", errno);
   }
   return s;
}


/* This function is run only when the certificate cannot
 * be completely trusted.  This will notify the user and
 * allow the user to decide what to do.  It may save the
 * certificate to the user's .dillo directory if it is
 * trusted.
 *
 * TODO: Rearrange this to get rid of redundancy.
 *
 * Return value: -1 on abort, 0 or higher on continue
 */
static int handle_certificate_problem(SSL * ssl_connection)
{
   int response_number;
   int ret = -1;
   long st;
   char *cn;
   char buf[4096], *d_cmd, *msg;

   X509 * remote_cert;

   remote_cert = SSL_get_peer_certificate(ssl_connection);
   if (remote_cert == NULL){
      /*Inform user that remote system cannot be trusted*/
      d_cmd = a_Dpip_build_cmd(
         "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
         "dialog",
         "Dillo HTTPS: No certificate!",
         "The remote system is NOT presenting a certificate.\n"
         "This site CAN NOT be trusted. Sending data is NOT SAFE.\n"
         "What do I do?",
         "Continue", "Cancel");
      a_Dpip_dsh_write_str(sh, 1, d_cmd);
      dFree(d_cmd);

      /*Read the user's response*/
      response_number = dialog_get_answer_number();

      /*Abort on anything but "Continue"*/
      if (response_number == 1){
         ret = 0;
      }

   } else {
      /*Figure out if (and why) the remote system can't be trusted*/
      st = SSL_get_verify_result(ssl_connection);
      switch (st) {
      case X509_V_OK:      /*Everything is Kosher*/
         ret = 0;
         break;
      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
         /*Either self signed and untrusted*/
         /*Extract CN from certificate name information*/
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
                          "untrusted.\nFor address: ", buf, NULL);
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s alt3=%s",
            "dialog",
            "Dillo HTTPS: Untrusted certificate!", msg,
            "Continue", "Cancel", "Save Certificate");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         dFree(msg);

         response_number = dialog_get_answer_number();
         switch (response_number){
            case 1:
               ret = 0;
               break;
            case 2:
               break;
            case 3:
               /*Save certificate to a file here and recheck the chain*/
               /*Potential security problems because we are writing
                *to the filesystem*/
               save_certificate_home(remote_cert);
               ret = 1;
               break;
            default:
               break;
         }
         break;
      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
      case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Missing certificate issuer!",
            "The issuer for the remote certificate cannot be found\n"
            "The authenticity of the remote certificate cannot be trusted",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);

         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;

      case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
      case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
      case X509_V_ERR_CERT_SIGNATURE_FAILURE:
      case X509_V_ERR_CRL_SIGNATURE_FAILURE:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Invalid certificate!",
            "The remote certificate signature could not be read\n"
            "or is invalid and should not be trusted",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);

         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_CERT_NOT_YET_VALID:
      case X509_V_ERR_CRL_NOT_YET_VALID:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Certificate not yet valid!",
            "Part of the remote certificate is not yet valid\n"
            "Certificates usually have a range of dates over which\n"
            "they are to be considered valid, and the certificate\n"
            "presented has a starting validity after today's date\n"
            "You should be cautious about using this site",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);

         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_CERT_HAS_EXPIRED:
      case X509_V_ERR_CRL_HAS_EXPIRED:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Expired certificate!",
            "The remote certificate has expired.  The certificate\n"
            "wasn't designed to last this long. You should avoid \n"
            "this site.",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
      case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
      case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
      case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Certificate error!",
            "There was an error in the certificate presented.\n"
            "Some of the certificate data was improperly formatted\n"
            "making it impossible to determine if the certificate\n"
            "is valid.  You should not trust this certificate.",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_INVALID_CA:
      case X509_V_ERR_INVALID_PURPOSE:
      case X509_V_ERR_CERT_UNTRUSTED:
      case X509_V_ERR_CERT_REJECTED:
      case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Certificate chain error!",
            "One of the certificates in the chain is being used\n"
            "incorrectly (possibly due to configuration problems\n"
            "with the remote system.  The connection should not\n"
            "be trusted",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
      case X509_V_ERR_AKID_SKID_MISMATCH:
      case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Certificate mismatch!",
            "Some of the information presented by the remote system\n"
            "does not match other information presented\n"
            "This may be an attempt to eavesdrop on communications",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Self signed certificate!",
            "Self signed certificate in certificate chain. The certificate "
            "chain could be built up using the untrusted certificates but the "
            "root could not be found locally.",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Missing issuer certificate!",
            "Unable to get local issuer certificate. The issuer certificate "
            "of an untrusted certificate cannot be found.",
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         if (response_number == 1) {
            ret = 0;
         }
         break;
      default:             /*Need to add more options later*/
         snprintf(buf, 80,
                  "The remote certificate cannot be verified (code %ld)", st);
         d_cmd = a_Dpip_build_cmd(
            "cmd=%s title=%s msg=%s alt1=%s alt2=%s",
            "dialog",
            "Dillo HTTPS: Unverifiable certificate!", buf,
            "Continue", "Cancel");
         a_Dpip_dsh_write_str(sh, 1, d_cmd);
         dFree(d_cmd);
         response_number = dialog_get_answer_number();
         /*abort on anything but "Continue"*/
         if (response_number == 1){
            ret = 0;
         }
      }
      X509_free(remote_cert);
      remote_cert = 0;
   }

   return ret;
}

/*
 * Save certificate with a hashed filename.
 * Return: 0 on success, 1 on failure.
 */
static int save_certificate_home(X509 * cert)
{
   char buf[4096];

   FILE * fp = NULL;
   uint_t i = 0;
   int ret = 1;

   /*Attempt to create .dillo/certs blindly - check later*/
   snprintf(buf,4096,"%s/.dillo/", dGethomedir());
   mkdir(buf, 01777);
   snprintf(buf,4096,"%s/.dillo/certs/", dGethomedir());
   mkdir(buf, 01777);

   do {
      snprintf(buf, 4096, "%s/.dillo/certs/%lx.%u",
               dGethomedir(), X509_subject_name_hash(cert), i);

      fp=fopen(buf, "r");
      if (fp == NULL){
         /*File name doesn't exist so we can use it safely*/
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
      /*Don't loop too many times - just give up*/
   } while (i < 1024);

   return ret;
}



#else


/*
 * Call this function to display an error message if SSL support
 * isn't available for some reason
 */
static void no_ssl_support(void)
{
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *http_query = NULL;
   char *d_cmd;

   /* Read the dpi command from STDIN */
   dpip_tag = a_Dpip_dsh_read_token(sh, 1);

   MSG("{In https.filter.dpi}\n");
   MSG("no_ssl_support version\n");

   cmd = a_Dpip_get_attr(dpip_tag, "cmd");
   url = a_Dpip_get_attr(dpip_tag, "url");
   http_query = a_Dpip_get_attr(dpip_tag, "query");

   MSG("{ cmd: %s}\n", cmd);
   MSG("{ url: %s}\n", url);
   MSG("{ http_query:\n%s}\n", http_query);

   MSG("{ sending dpip cmd...}\n");

   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);

   MSG("{ dpip cmd sent.}\n");

   MSG("{ sending HTML...}\n");

   a_Dpip_dsh_printf(sh, 1,
      "Content-type: text/html\n\n"
      "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
      "<html><head><title>SSL support is disabled</title></head>\n"
      "<body>\n"
      "<p>\n"
      "  The https dpi was unable to send\n"
      "  the following HTTP query:\n"
      "  <blockquote><pre>%s</pre></blockquote>\n"
      "  because Dillo's prototype plugin for https support"
      "  is disabled.\n\n"
      "<p>\n"
      "  If you want to test this <b>alpha</b> support code,\n"
      "  just reconfigure with <code>--enable-ssl</code>,\n"
      "  recompile and reinstall.\n\n"
      "  (Beware that this https support is very limited now)\n\n"
      "  To use https and SSL, you must have \n"
      "  the OpenSSL development libraries installed.  Check your\n"
      "  O/S distribution provider, or check out\n"
      "  <a href=\"http://www.openssl.org\">www.openssl.org</a>.\n\n"
      "</p>\n\n"
      "</body></html>\n",
      http_query
   );
   MSG("{ HTML content sent.}\n");

   dFree(cmd);
   dFree(url);
   dFree(http_query);
   dFree(dpip_tag);

   MSG("{ exiting https.dpi}\n");

}

#endif


/*---------------------------------------------------------------------------*/
int main(void)
{
   char *dpip_tag;

   /* Initialize the SockHandler for this filter dpi */
   sh = a_Dpip_dsh_new(STDIN_FILENO, STDOUT_FILENO, 8*1024);

   /* Authenticate our client... */
   if (!(dpip_tag = a_Dpip_dsh_read_token(sh, 1)) ||
       a_Dpip_check_auth(dpip_tag) < 0) {
      MSG("can't authenticate request: %s\n", dStrerror(errno));
      a_Dpip_dsh_close(sh);
      return 1;
   }
   dFree(dpip_tag);

#ifdef ENABLE_SSL
   yes_ssl_support();
#else
   no_ssl_support();
#endif

   /* Finish the SockHandler */
   a_Dpip_dsh_close(sh);
   a_Dpip_dsh_free(sh);

   dFree(root_url);

   MSG("{ exiting https.dpi}\n");

   return 0;
}

