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

/* Handling of cookies takes place here.
 * This implementation aims to follow RFC 2965:
 * http://www.cis.ohio-state.edu/cs/Services/rfc/rfc-text/rfc2965.txt
 */

#define DEBUG_LEVEL 10
#include "debug.h"


#ifdef DISABLE_COOKIES

/*
 * Initialize the cookies module
 */
void a_Cookies_init(void)
{
   DEBUG_MSG(10, "Cookies: absolutely disabled at compilation time.\n");
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>       /* for time() and time_t */
#include <ctype.h>

#include "msg.h"
#include "IO/Url.h"
#include "list.h"
#include "cookies.h"
#include "capi.h"
#include "dpiapi.h"
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
   int fd;

   if ((F_in = fopen(filename, "r")) == NULL) {
      /* Create the file */
      fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
      if (fd != -1) {
         if (init_str)
            write(fd, init_str, strlen(init_str));
         close(fd);

         DEBUG_MSG(10, "Cookies: Created file: %s\n", filename);
         F_in = Cookies_fopen(filename, NULL);
      } else {
         DEBUG_MSG(10, "Cookies: Could not create file: %s!\n", filename);
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
      DEBUG_MSG(10, "Disabling cookies.\n");
      return;
   }

   DEBUG_MSG(10, "Enabling cookies as from cookiesrc...\n");
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
void a_Cookies_set(Dlist *cookie_strings, const DilloUrl *set_url)
{
   CookieControlAction action;
   char *cmd, *cookie_string, numstr[16];
   const char *path;
   int i;

   if (disabled)
      return;

   action = Cookies_control_check(set_url);
   if (action == COOKIE_DENY) {
      DEBUG_MSG(5, "Cookies: denied SET for %s\n", URL_HOST_(set_url));
      return;
   }

   for (i = 0; (cookie_string = dList_nth_data(cookie_strings, i)); ++i) {
      path = URL_PATH_(set_url);
      snprintf(numstr, 16, "%d", URL_PORT(set_url));
      cmd = a_Dpip_build_cmd("cmd=%s cookie=%s host=%s path=%s port=%s",
                             "set_cookie", cookie_string, URL_HOST_(set_url),
                             path ? path : "/", numstr);

      DEBUG_MSG(5, "Cookies.c: a_Cookies_set \n\t \"%s\" \n",cmd );
      a_Capi_dpi_send_cmd(NULL, NULL, cmd, "cookies", 1);
      dFree(cmd);
   }
}

/*
 * Return a string that contains all relevant cookies as headers.
 */
char *a_Cookies_get(const DilloUrl *request_url)
{
   char *cmd, *dpip_tag, *cookie, numstr[16];
   const char *path;
   CookieControlAction action;

   cookie = dStrdup("");

   if (disabled)
      return cookie;

   action = Cookies_control_check(request_url);
   if (action == COOKIE_DENY) {
      DEBUG_MSG(5, "Cookies: denied GET for %s\n", URL_HOST_(request_url));
      return cookie;
   }
   path = URL_PATH_(request_url);

   snprintf(numstr, 16, "%d", URL_PORT(request_url));
   cmd = a_Dpip_build_cmd("cmd=%s scheme=%s host=%s path=%s port=%s",
                          "get_cookie", URL_SCHEME(request_url),
                         URL_HOST(request_url), path ? path : "/", numstr);

   /* Get the answer from cookies.dpi */
   dpip_tag = a_Dpi_send_blocking_cmd("cookies", cmd);
   dFree(cmd);

   if (dpip_tag != NULL) {
      dFree(cookie);
      cookie = a_Dpip_get_attr(dpip_tag, strlen(dpip_tag), "cookie");
      dFree(dpip_tag);
   }
   return cookie;
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
   char *filename;
   char line[LINE_MAXLEN];
   char domain[LINE_MAXLEN];
   char rule[LINE_MAXLEN];
   int i, j;
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
      fgets(line, LINE_MAXLEN, stream);

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if (line[0] != '\0' && line[0] != '#') {
         i = 0;
         j = 0;

         /* Get the domain */
         while (!isspace(line[i]))
            domain[j++] = line[i++];
         domain[j] = '\0';

         /* Skip past whitespaces */
         i++;
         while (isspace(line[i]))
            i++;

         /* Get the rule */
         j = 0;
         while (line[i] != '\0' && !isspace(line[i]))
            rule[j++] = line[i++];
         rule[j] = '\0';

         if (dStrcasecmp(rule, "ACCEPT") == 0)
            cc.action = COOKIE_ACCEPT;
         else if (dStrcasecmp(rule, "ACCEPT_SESSION") == 0)
            cc.action = COOKIE_ACCEPT_SESSION;
         else if (dStrcasecmp(rule, "DENY") == 0)
            cc.action = COOKIE_DENY;
         else {
            MSG("Cookies: rule '%s' for domain '%s' is not recognised.\n",
                rule, domain);
            continue;
         }

         cc.domain = dStrdup(domain);
         if (dStrcasecmp(cc.domain, "DEFAULT") == 0) {
            /* Set the default action */
            default_action = cc.action;
            dFree(cc.domain);
         } else {
            a_List_add(ccontrol, num_ccontrol, num_ccontrol_max);
            ccontrol[num_ccontrol++] = cc;
         }

         if (cc.action != COOKIE_DENY)
            enabled = TRUE;
      }
   }

   fclose(stream);

   return (enabled ? 0 : 1);
}

/*
 * Check the rules for an appropriate action for this domain
 */
static CookieControlAction Cookies_control_check_domain(const char *domain)
{
   int i, diff;

   for (i = 0; i < num_ccontrol; i++) {
      if (ccontrol[i].domain[0] == '.') {
         diff = strlen(domain) - strlen(ccontrol[i].domain);
         if (diff >= 0) {
            if (dStrcasecmp(domain + diff, ccontrol[i].domain) != 0)
               continue;
         } else {
            continue;
         }
      } else {
         if (dStrcasecmp(domain, ccontrol[i].domain) != 0)
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
