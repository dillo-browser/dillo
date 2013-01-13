/*
 * File: cookies.c
 *
 * Copyright 2001 Lars Clausen   <lrclause@cs.uiuc.edu>
 *                Jörgen Viksell <jorgen.viksell@telia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/* Handling of cookies takes place here. */

#include "msg.h"

#ifdef DISABLE_COOKIES

/*
 * Initialize the cookies module
 */
void a_Cookies_init(void)
{
   MSG("Cookies: absolutely disabled at compilation time.\n");
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "IO/Url.h"
#include "list.h"
#include "cookies.h"
#include "capi.h"
#include "../dpip/dpip.h"


/* The maximum length of a line in the cookie file */
#define LINE_MAXLEN 4096

typedef enum {
   COOKIE_ACCEPT,
   COOKIE_ACCEPT_SESSION,
   COOKIE_DENY
} CookieControlAction;

typedef struct {
   CookieControlAction action;
   char *domain;
} CookieControl;

/* Variables for access control */
static CookieControl *ccontrol = NULL;
static int num_ccontrol = 0;
static int num_ccontrol_max = 1;
static CookieControlAction default_action = COOKIE_DENY;

static bool_t disabled;

static FILE *Cookies_fopen(const char *file, char *init_str);
static CookieControlAction Cookies_control_check(const DilloUrl *url);
static CookieControlAction Cookies_control_check_domain(const char *domain);
static int Cookie_control_init(void);

/*
 * Return a file pointer. If the file doesn't exist, try to create it,
 * with the optional 'init_str' as its content.
 */
static FILE *Cookies_fopen(const char *filename, char *init_str)
{
   FILE *F_in;
   int fd, rc;

   if ((F_in = fopen(filename, "r")) == NULL) {
      /* Create the file */
      fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
      if (fd != -1) {
         if (init_str) {
            rc = write(fd, init_str, strlen(init_str));
            if (rc == -1) {
               MSG("Cookies: Could not write initial string to file %s: %s\n",
                   filename, dStrerror(errno));
            }
         }
         dClose(fd);

         MSG("Cookies: Created file: %s\n", filename);
         F_in = fopen(filename, "r");
      } else {
         MSG("Cookies: Could not create file: %s!\n", filename);
      }
   }

   if (F_in) {
      /* set close on exec */
      fcntl(fileno(F_in), F_SETFD, FD_CLOEXEC | fcntl(fileno(F_in), F_GETFD));
   }

   return F_in;
}

/*
 * Initialize the cookies module
 * (The 'disabled' variable is writable only within a_Cookies_init)
 */
void a_Cookies_init(void)
{
   /* Default setting */
   disabled = TRUE;

   /* Read and parse the cookie control file (cookiesrc) */
   if (Cookie_control_init() != 0) {
      MSG("Disabling cookies.\n");
      return;
   }

   MSG("Enabling cookies as from cookiesrc...\n");
   disabled = FALSE;
}

/*
 * Flush cookies to disk and free all the memory allocated.
 */
void a_Cookies_freeall()
{
}

/*
 * Set the value corresponding to the cookie string
 */
void a_Cookies_set(Dlist *cookie_strings, const DilloUrl *set_url,
                   const char *date)
{
   CookieControlAction action;
   char *cmd, *cookie_string, *dpip_tag;
   const char *path;
   int i;

   if (disabled)
      return;

   action = Cookies_control_check(set_url);
   if (action == COOKIE_DENY) {
      _MSG("Cookies: denied SET for %s\n", URL_HOST_(set_url));
      return;
   }

   for (i = 0; (cookie_string = dList_nth_data(cookie_strings, i)); ++i) {
      path = URL_PATH_(set_url);
      if (date)
         cmd = a_Dpip_build_cmd("cmd=%s cookie=%s host=%s path=%s date=%s",
                                "set_cookie", cookie_string,
                                URL_HOST_(set_url), path ? path : "/", date);
      else
         cmd = a_Dpip_build_cmd("cmd=%s cookie=%s host=%s path=%s",
                                "set_cookie", cookie_string,
                                URL_HOST_(set_url), path ? path : "/");

      _MSG("Cookies.c: a_Cookies_set \n\t \"%s\" \n",cmd );
      /* This call is commented because it doesn't guarantee the order
       * in which cookies are set and got. (It may deadlock too) */
      //a_Capi_dpi_send_cmd(NULL, NULL, cmd, "cookies", 1);

      dpip_tag = a_Dpi_send_blocking_cmd("cookies", cmd);
      _MSG("a_Cookies_set: dpip_tag = {%s}\n", dpip_tag);
      dFree(dpip_tag);
      dFree(cmd);
   }
}

/*
 * Return a string containing cookie data for an HTTP query.
 */
char *a_Cookies_get_query(const DilloUrl *query_url, const DilloUrl *requester)
{
   char *cmd, *dpip_tag, *query;
   const char *path;
   CookieControlAction action;

   if (disabled)
      return dStrdup("");

   action = Cookies_control_check(query_url);
   if (action == COOKIE_DENY) {
      _MSG("Cookies: denied GET for %s\n", URL_HOST_(query_url));
      return dStrdup("");
   }

   if (requester == NULL) {
      /* request made by user */
   } else if (!a_Url_same_organization(query_url, requester)) {
      MSG("Cookies: not sent for request by '%s' for '%s'\n",
          URL_HOST(requester), URL_HOST(query_url));
      return dStrdup("");
   }

   path = URL_PATH_(query_url);

   cmd = a_Dpip_build_cmd("cmd=%s scheme=%s host=%s path=%s",
                          "get_cookie", URL_SCHEME(query_url),
                         URL_HOST(query_url), path ? path : "/");

   /* Get the answer from cookies.dpi */
   _MSG("cookies.c: a_Dpi_send_blocking_cmd cmd = {%s}\n", cmd);
   dpip_tag = a_Dpi_send_blocking_cmd("cookies", cmd);
   _MSG("cookies.c: after a_Dpi_send_blocking_cmd resp={%s}\n", dpip_tag);
   dFree(cmd);

   if (dpip_tag != NULL) {
      query = a_Dpip_get_attr(dpip_tag, "cookie");
      dFree(dpip_tag);
   } else {
      query = dStrdup("");
   }
   return query;
}

/* -------------------------------------------------------------
 *                    Access control routines
 * ------------------------------------------------------------- */


/*
 * Get the cookie control rules (from cookiesrc).
 * Return value:
 *   0 = Parsed OK, with cookies enabled
 *   1 = Parsed OK, with cookies disabled
 *   2 = Can't open the control file
 */
static int Cookie_control_init(void)
{
   CookieControl cc;
   FILE *stream;
   char *filename, *rc;
   char line[LINE_MAXLEN];
   char domain[LINE_MAXLEN];
   char rule[LINE_MAXLEN];
   bool_t enabled = FALSE;

   /* Get a file pointer */
   filename = dStrconcat(dGethomedir(), "/.dillo/cookiesrc", NULL);
   stream = Cookies_fopen(filename, "DEFAULT DENY\n");
   dFree(filename);

   if (!stream)
      return 2;

   /* Get all lines in the file */
   while (!feof(stream)) {
      line[0] = '\0';
      rc = fgets(line, LINE_MAXLEN, stream);
      if (!rc && ferror(stream)) {
         MSG("Cookies1: Error while reading rule from cookiesrc: %s\n",
             dStrerror(errno));
         fclose(stream);
         return 2; /* bail out */
      }

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if (line[0] != '\0' && line[0] != '#') {
         int i = 0, j = 0;

         /* Get the domain */
         while (line[i] != '\0' && !dIsspace(line[i]))
            domain[j++] = line[i++];
         domain[j] = '\0';

         /* Skip past whitespaces */
         while (dIsspace(line[i]))
            i++;

         /* Get the rule */
         j = 0;
         while (line[i] != '\0' && !dIsspace(line[i]))
            rule[j++] = line[i++];
         rule[j] = '\0';

         if (dStrAsciiCasecmp(rule, "ACCEPT") == 0)
            cc.action = COOKIE_ACCEPT;
         else if (dStrAsciiCasecmp(rule, "ACCEPT_SESSION") == 0)
            cc.action = COOKIE_ACCEPT_SESSION;
         else if (dStrAsciiCasecmp(rule, "DENY") == 0)
            cc.action = COOKIE_DENY;
         else {
            MSG("Cookies: rule '%s' for domain '%s' is not recognised.\n",
                rule, domain);
            continue;
         }

         cc.domain = dStrdup(domain);
         if (dStrAsciiCasecmp(cc.domain, "DEFAULT") == 0) {
            /* Set the default action */
            default_action = cc.action;
            dFree(cc.domain);
         } else {
            int i;
            uint_t len = strlen(cc.domain);

            /* Insert into list such that longest rules come first. */
            a_List_add(ccontrol, num_ccontrol, num_ccontrol_max);
            for (i = num_ccontrol++;
                 i > 0 && (len > strlen(ccontrol[i-1].domain));
                 i--) {
               ccontrol[i] = ccontrol[i-1];
            }
            ccontrol[i] = cc;
         }

         if (cc.action != COOKIE_DENY)
            enabled = TRUE;
      }
   }

   fclose(stream);

   return (enabled ? 0 : 1);
}

/*
 * Check the rules for an appropriate action for this domain.
 * The rules are ordered by domain length, with longest first, so the
 * first match is the most specific.
 */
static CookieControlAction Cookies_control_check_domain(const char *domain)
{
   int i, diff;

   for (i = 0; i < num_ccontrol; i++) {
      if (ccontrol[i].domain[0] == '.') {
         diff = strlen(domain) - strlen(ccontrol[i].domain);
         if (diff >= 0) {
            if (dStrAsciiCasecmp(domain + diff, ccontrol[i].domain) != 0)
               continue;
         } else {
            continue;
         }
      } else {
         if (dStrAsciiCasecmp(domain, ccontrol[i].domain) != 0)
            continue;
      }

      /* If we got here we have a match */
      return( ccontrol[i].action );
   }

   return default_action;
}

/*
 * Same as the above except it takes an URL
 */
static CookieControlAction Cookies_control_check(const DilloUrl *url)
{
   return Cookies_control_check_domain(URL_HOST(url));
}

#endif /* !DISABLE_COOKIES */
