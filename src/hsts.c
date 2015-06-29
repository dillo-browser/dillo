/*
 * File: hsts.c
 * HTTP Strict Transport Security
 *
 * Copyright 2015 corvid
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/* To preload hosts, as of 2015, chromium is the list keeper:
 * https://src.chromium.org/viewvc/chrome/trunk/src/net/http/transport_security_state_static.json
 * although mozilla's is easier to work from (and they trim it based on
 * criteria such as max-age must be at least some number of months)
 * https://mxr.mozilla.org/mozilla-central/source/security/manager/ssl/nsSTSPreloadList.inc?raw=1
 */

#include <time.h>
#include <errno.h>
#include <limits.h> /* for INT_MAX */
#include <ctype.h> /* for isspace */
#include <stdlib.h> /* for strtol */

#include "hsts.h"
#include "msg.h"
#include "../dlib/dlib.h"
#include "IO/tls.h"

typedef struct {
   char *host;
   time_t expires_at;
   bool_t subdomains;
} HstsData_t;

/* When there is difficulty in representing future dates, use the (by far)
 * most likely latest representable time of January 19, 2038.
 */
static time_t hsts_latest_representable_time;
static Dlist *domains;

static void Hsts_free_policy(HstsData_t *p)
{
   dFree(p->host);
   dFree(p);
}

void a_Hsts_freeall()
{
   HstsData_t *policy;
   int i, n = dList_length(domains);

   for (i = 0; i < n; i++) {
      policy = dList_nth_data(domains, i);
      Hsts_free_policy(policy);
   }
   dList_free(domains);
}

/*
 * Compare function for searching a domain node by domain string
 */
static int Domain_node_domain_str_cmp(const void *v1, const void *v2)
{
   const HstsData_t *node = v1;
   const char *host = v2;

   return dStrAsciiCasecmp(node->host, host);
}

static HstsData_t *Hsts_get_policy(const char *host)
{
   return dList_find_sorted(domains, host, Domain_node_domain_str_cmp);
}

static void Hsts_remove_policy(HstsData_t *policy)
{
   if (policy) {
      _MSG("HSTS: removed policy for %s\n", policy->host);
      Hsts_free_policy(policy);
      dList_remove(domains, policy);
   }
}

/*
 * Return the time_t for a future time.
 */
static time_t Hsts_future_time(long seconds_from_now)
{
   time_t ret, now = time(NULL);
   struct tm *tm = gmtime(&now);

   if (seconds_from_now > INT_MAX - tm->tm_sec)
      tm->tm_sec = INT_MAX;
   else
      tm->tm_sec += seconds_from_now;

   ret = mktime(tm);
   if (ret == (time_t) -1)
      ret = hsts_latest_representable_time;

   return ret;
}

/*
 * Compare function for searching domains.
 */
static int Domain_node_cmp(const void *v1, const void *v2)
{
   const HstsData_t *node1 = v1, *node2 = v2;

   return dStrAsciiCasecmp(node1->host, node2->host);
}

static void Hsts_set_policy(const char *host, long max_age, bool_t subdomains)
{
   time_t exp = Hsts_future_time(max_age);
   HstsData_t *policy = Hsts_get_policy(host);

   _MSG("HSTS: %s %s%s: until %s", (policy ? "modify" : "add"), host,
       (subdomains ? " and subdomains" : ""), ctime(&exp));

   if (policy == NULL) {
      policy = dNew0(HstsData_t, 1);
      policy->host = dStrdup(host);
      dList_insert_sorted(domains, policy, Domain_node_cmp);
   }
   policy->subdomains = subdomains;
   policy->expires_at = exp;
}

/*
 * Read the next attribute.
 */
static char *Hsts_parse_attr(const char **header_str)
{
   const char *str;
   uint_t len;

   while (dIsspace(**header_str))
      (*header_str)++;

   str = *header_str;
   /* find '=' at end of attr, ';' after attr/val pair, '\0' end of string */
   len = strcspn(str, "=;");
   *header_str += len;

   while (len && (str[len - 1] == ' ' || str[len - 1] == '\t'))
      len--;
   return dStrndup(str, len);
}

/*
 * Get the value in *header_str.
 */
static char *Hsts_parse_value(const char **header_str)
{
   uint_t len;
   const char *str;

   if (**header_str == '=') {
      (*header_str)++;
      while (dIsspace(**header_str))
         (*header_str)++;

      str = *header_str;
      /* finds ';' after attr/val pair or '\0' at end of string */
      len = strcspn(str, ";");
      *header_str += len;

      while (len && (str[len - 1] == ' ' || str[len - 1] == '\t'))
         len--;
   } else {
      str = *header_str;
      len = 0;
   }
   return dStrndup(str, len);
}

/*
 * Advance past any value.
 */
static void Hsts_eat_value(const char **str)
{
   if (**str == '=')
      *str += strcspn(*str, ";");
}

/*
 * The reponse for this url had an HSTS header, so let's take action.
 */
void a_Hsts_set(const char *header, const DilloUrl *url)
{
   long max_age;
   const char *host = URL_HOST(url);
   bool_t max_age_valid = FALSE, subdomains = FALSE;

   _MSG("HSTS header for %s: %s\n", host, header);

   if (!a_Tls_certificate_is_clean(url)) {
      /* RFC 6797 gives rationale in section 14.3. */
      _MSG("But there were certificate warnings, so ignore it (!)\n");
      return;
   }

   /* Iterate until there is nothing left of the string */
   while (*header) {
      char *attr;
      char *value;

      /* Get attribute */
      attr = Hsts_parse_attr(&header);

      /* Get the value for the attribute and store it */
      if (dStrAsciiCasecmp(attr, "max-age") == 0) {
         value = Hsts_parse_value(&header);
         if (isdigit(*value)) {
            errno = 0;
            max_age = strtol(value, NULL, 10);
            if (errno == ERANGE)
               max_age = INT_MAX;
            max_age_valid = TRUE;
         }
         dFree(value);
      } else if (dStrAsciiCasecmp(attr, "includeSubDomains") == 0) {
         subdomains = TRUE;
         Hsts_eat_value(&header);
      } else if (dStrAsciiCasecmp(attr, "preload") == 0) {
         /* 'preload' is not part of the RFC, but what does google care for
          * standards? They require that 'preload' be specified by a domain
          * in order to be added to their preload list.
          */
      } else {
         MSG("HSTS: header contains unknown attribute: '%s'\n", attr);
         Hsts_eat_value(&header);
      }

      dFree(attr);

      if (*header == ';')
         header++;
   }
   if (max_age_valid) {
      if (max_age > 0)
         Hsts_set_policy(host, max_age, subdomains);
      else
         Hsts_remove_policy(Hsts_get_policy(host));
   }
}

static bool_t Hsts_expired(HstsData_t *policy)
{
   time_t now = time(NULL);
   bool_t ret = (now > policy->expires_at) ? TRUE : FALSE;

   if (ret) {
      _MSG("HSTS: expired\n");
   }
   return ret;
}

bool_t a_Hsts_require_https(const char *host)
{
   bool_t ret = FALSE;

   if (host) {
      HstsData_t *policy = Hsts_get_policy(host);

      if (policy) {
         _MSG("HSTS: matched host %s\n", host);
         if (Hsts_expired(policy))
            Hsts_remove_policy(policy);
         else
            ret = TRUE;
      }
      if (!ret) {
         const char *domain_str;

         for (domain_str = strchr(host+1, '.');
              domain_str != NULL && *domain_str;
              domain_str = strchr(domain_str+1, '.')) {
            policy = Hsts_get_policy(domain_str+1);

            if (policy && policy->subdomains) {
               _MSG("HSTS: matched %s under %s subdomain rule\n", host,
                   policy->host);
               if (Hsts_expired(policy)) {
                  Hsts_remove_policy(policy);
               } else {
                  ret = TRUE;
                  break;
               }
            }
         }
      }
   }
   return ret;
}

static void Hsts_preload(FILE *stream)
{
   const int LINE_MAXLEN = 4096;
   const long ONE_YEAR = 60 * 60 * 24 * 365;

   char *rc, *subdomains;
   char line[LINE_MAXLEN];
   char domain[LINE_MAXLEN];

   /* Get all lines in the file */
   while (!feof(stream)) {
      line[0] = '\0';
      rc = fgets(line, LINE_MAXLEN, stream);
      if (!rc && ferror(stream)) {
         MSG_WARN("HSTS: Error while reading preload entries: %s\n",
                  dStrerror(errno));
         return; /* bail out */
      }

      /* Remove leading and trailing whitespace */
      dStrstrip(line);

      if (line[0] != '\0' && line[0] != '#') {
         int i = 0, j = 0;

         /* Get the domain */
         while (line[i] != '\0' && !dIsspace(line[i]))
            domain[j++] = line[i++];
         domain[j] = '\0';

         /* Skip past whitespace */
         while (dIsspace(line[i]))
            i++;

         subdomains = line + i;

         if (dStrAsciiCasecmp(subdomains, "true") == 0)
            Hsts_set_policy(domain, ONE_YEAR, TRUE);
         else if (dStrAsciiCasecmp(subdomains, "false") == 0)
            Hsts_set_policy(domain, ONE_YEAR, FALSE);
         else {
            MSG_WARN("HSTS: format of line not recognized. Ignoring '%s'.\n",
                     line);
         }
      }
   }
}

void a_Hsts_init(FILE *preload_file)
{
   struct tm future_tm = {7, 14, 3, 19, 0, 138, 0, 0, 0, 0, 0};

   hsts_latest_representable_time = mktime(&future_tm);
   domains = dList_new(32);

   if (preload_file)
      Hsts_preload(preload_file);
}

