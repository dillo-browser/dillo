/*
 * File: cookies.c
 * Cookies server.
 *
 * Copyright 2001 Lars Clausen   <lrclause@cs.uiuc.edu>
 *                Jörgen Viksell <jorgen.viksell@telia.com>
 * Copyright 2002-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/* This is written to follow the HTTP State Working Group's
 * draft-ietf-httpstate-cookie-01.txt.
 *
 * We depart from the draft spec's domain format in that, rather than
 * using a host-only flag, we continue to use the .domain notation
 * internally to indicate cookies that may also be returned to subdomains.
 *
 * Info on cookies in the wild:
 *  http://www.ietf.org/mail-archive/web/http-state/current/msg00078.html
 * And dates specifically:
 *  http://www.ietf.org/mail-archive/web/http-state/current/msg00128.html
 */

#ifdef DISABLE_COOKIES

int main(void)
{
   return 0; /* never called */
}

#else


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>       /* for time() and time_t */
#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include "dpiutil.h"
#include "../dpip/dpip.h"


/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[cookies dpi]: " __VA_ARGS__)


/*
 * a_List_add()
 *
 * Make sure there's space for 'num_items' items within the list
 * (First, allocate an 'alloc_step' sized chunk, after that, double the
 *  list size --to make it faster)
 */
#define a_List_add(list,num_items,alloc_step) \
   if (!list) { \
      list = dMalloc(alloc_step * sizeof((*list))); \
   } \
   if (num_items >= alloc_step){ \
      while ( num_items >= alloc_step ) \
         alloc_step <<= 1; \
      list = dRealloc(list, alloc_step * sizeof((*list))); \
   }

/* The maximum length of a line in the cookie file */
#define LINE_MAXLEN 4096

typedef enum {
   COOKIE_ACCEPT,
   COOKIE_ACCEPT_SESSION,
   COOKIE_DENY
} CookieControlAction;

typedef struct {
   char *domain;
   CookieControlAction action;
} CookieControl;

typedef struct {
   char *domain;
   Dlist *dlist;
} CookieNode;

typedef struct {
   char *name;
   char *value;
   char *domain;
   char *path;
   time_t expires_at;
   bool_t secure;
   bool_t session_only;
   long last_used;
} CookieData_t;

typedef struct {
   Dsh *sh;
   int status;
} ClientInfo;

/*
 * Local data
 */

/* List of CookieNode. Each node holds a domain and its list of cookies */
static Dlist *cookies;

/* Variables for access control */
static CookieControl *ccontrol = NULL;
static int num_ccontrol = 0;
static int num_ccontrol_max = 1;
static CookieControlAction default_action = COOKIE_DENY;

static long cookies_use_counter = 0;
static bool_t disabled;
static FILE *file_stream;
static const char *const cookies_txt_header_str =
"# HTTP Cookie File\n"
"# This is a generated file!  Do not edit.\n"
"# [domain  TRUE  path  secure  expiry_time  name  value]\n\n";


/*
 * Forward declarations
 */

static CookieControlAction Cookies_control_check_domain(const char *domain);
static int Cookie_control_init(void);
static void Cookies_add_cookie(CookieData_t *cookie);
static void Cookies_remove_cookie(CookieData_t *cookie);
static int Cookies_cmp(const void *a, const void *b);

/*
 * Compare function for searching a cookie node
 */
static int Cookie_node_cmp(const void *v1, const void *v2)
{
   const CookieNode *n1 = v1, *n2 = v2;

   return dStrcasecmp(n1->domain, n2->domain);
}

/*
 * Compare function for searching a cookie node by domain
 */
static int Cookie_node_by_domain_cmp(const void *v1, const void *v2)
{
   const CookieNode *node = v1;
   const char *domain = v2;

   return dStrcasecmp(node->domain, domain);
}

/*
 * Return a file pointer. If the file doesn't exist, try to create it,
 * with the optional 'init_str' as its content.
 */
static FILE *Cookies_fopen(const char *filename, const char *mode,
                           const char *init_str)
{
   FILE *F_in;
   int fd, rc;

   if ((F_in = fopen(filename, mode)) == NULL) {
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
         close(fd);

         MSG("Created file: %s\n", filename);
         F_in = fopen(filename, mode);
      } else {
         MSG("Could not create file: %s!\n", filename);
      }
   }

   if (F_in) {
      /* set close on exec */
      fcntl(fileno(F_in), F_SETFD, FD_CLOEXEC | fcntl(fileno(F_in), F_GETFD));
   }

   return F_in;
}

static void Cookies_free_cookie(CookieData_t *cookie)
{
   dFree(cookie->name);
   dFree(cookie->value);
   dFree(cookie->domain);
   dFree(cookie->path);
   dFree(cookie);
}

/*
 * Initialize the cookies module
 * (The 'disabled' variable is writeable only within Cookies_init)
 */
static void Cookies_init()
{
   CookieData_t *cookie;
   char *filename, *rc = NULL;
   char line[LINE_MAXLEN];
#ifndef HAVE_LOCKF
   struct flock lck;
#endif

   /* Default setting */
   disabled = TRUE;

   /* Read and parse the cookie control file (cookiesrc) */
   if (Cookie_control_init() != 0) {
      MSG("Disabling cookies.\n");
      return;
   }

   /* Get a stream for the cookies file */
   filename = dStrconcat(dGethomedir(), "/.dillo/cookies.txt", NULL);
   file_stream = Cookies_fopen(filename, "r+", cookies_txt_header_str);

   dFree(filename);

   if (!file_stream) {
      MSG("ERROR: Can't open ~/.dillo/cookies.txt; disabling cookies\n");
      return;
   }

   /* Try to get a lock from the file descriptor */
#ifdef HAVE_LOCKF
   disabled = (lockf(fileno(file_stream), F_TLOCK, 0) == -1);
#else /* POSIX lock */
   lck.l_start = 0; /* start at beginning of file */
   lck.l_len = 0;  /* lock entire file */
   lck.l_type = F_WRLCK;
   lck.l_whence = SEEK_SET;  /* absolute offset */

   disabled = (fcntl(fileno(file_stream), F_SETLK, &lck) == -1);
#endif
   if (disabled) {
      MSG("The cookies file has a file lock; disabling cookies!\n");
      fclose(file_stream);
      return;
   }

   MSG("Enabling cookies as per cookiesrc...\n");

   cookies = dList_new(32);

   /* Get all lines in the file */
   while (!feof(file_stream)) {
      line[0] = '\0';
      rc = fgets(line, LINE_MAXLEN, file_stream);
      if (!rc && ferror(file_stream)) {
         MSG("Error while reading from cookies.txt: %s\n", dStrerror(errno));
         break; /* bail out */
      }

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if ((line[0] != '\0') && (line[0] != '#')) {
         /*
          * Split the row into pieces using a tab as the delimiter.
          * pieces[0] The domain name
          * pieces[1] TRUE/FALSE: is the domain a suffix, or a full domain?
          * pieces[2] The path
          * pieces[3] TRUE/FALSE: is the cookie for secure use only?
          * pieces[4] Timestamp of expire date
          * pieces[5] Name of the cookie
          * pieces[6] Value of the cookie
          */
         CookieControlAction action;
         char *piece;
         char *line_marker = line;

         cookie = dNew0(CookieData_t, 1);

         cookie->session_only = FALSE;
         cookie->domain = dStrdup(dStrsep(&line_marker, "\t"));
         dStrsep(&line_marker, "\t"); /* we use domain always as sufix */
         cookie->path = dStrdup(dStrsep(&line_marker, "\t"));
         piece = dStrsep(&line_marker, "\t");
         if (piece != NULL && piece[0] == 'T')
            cookie->secure = TRUE;
         piece = dStrsep(&line_marker, "\t");
         if (piece != NULL)
            cookie->expires_at = (time_t) strtol(piece, NULL, 10);
         cookie->name = dStrdup(dStrsep(&line_marker, "\t"));
         cookie->value = dStrdup(dStrsep(&line_marker, "\t"));

         if (!cookie->domain || cookie->domain[0] == '\0' ||
             !cookie->path || cookie->path[0] != '/' ||
             !cookie->name || cookie->name[0] == '\0' ||
             !cookie->value) {
            MSG("Malformed line in cookies.txt file!\n");
            Cookies_free_cookie(cookie);
            continue;
         }

         action = Cookies_control_check_domain(cookie->domain);
         if (action == COOKIE_DENY) {
            Cookies_free_cookie(cookie);
            continue;
         } else if (action == COOKIE_ACCEPT_SESSION) {
            cookie->session_only = TRUE;
         }

         /* Save cookie in memory */
         Cookies_add_cookie(cookie);
      }
   }
}

/*
 * Flush cookies to disk and free all the memory allocated.
 */
static void Cookies_save_and_free()
{
   int i, fd;
   CookieNode *node;
   CookieData_t *cookie;

#ifndef HAVE_LOCKF
   struct flock lck;
#endif

   if (disabled)
      return;

   rewind(file_stream);
   fd = fileno(file_stream);
   if (ftruncate(fd, 0) == -1)
      MSG("Cookies: Truncate file stream failed: %s\n", dStrerror(errno));
   fprintf(file_stream, "%s", cookies_txt_header_str);

   /* Iterate cookies per domain, saving and freeing */
   while ((node = dList_nth_data(cookies, 0))) {
      for (i = 0; (cookie = dList_nth_data(node->dlist, i)); ++i) {
         if (!cookie->session_only) {
            fprintf(file_stream, "%s\tTRUE\t%s\t%s\t%ld\t%s\t%s\n",
                    cookie->domain,
                    cookie->path,
                    cookie->secure ? "TRUE" : "FALSE",
                    (long)cookie->expires_at,
                    cookie->name,
                    cookie->value);
         }

         Cookies_free_cookie(cookie);
      }
      dList_remove(cookies, node);
      dFree(node->domain);
      dList_free(node->dlist);
      dFree(node);
   }

#ifdef HAVE_LOCKF
   lockf(fd, F_ULOCK, 0);
#else  /* POSIX file lock */
   lck.l_start = 0; /* start at beginning of file */
   lck.l_len = 0;  /* lock entire file */
   lck.l_type = F_UNLCK;
   lck.l_whence = SEEK_SET;  /* absolute offset */

   fcntl(fileno(file_stream), F_SETLKW, &lck);
#endif
   fclose(file_stream);
}

/*
 * Take a months name and return a number between 1-12.
 * E.g. 'April' -> 4
 */
static int Cookies_get_month(const char *month_name)
{
   static const char *const months[] =
   { "",
     "Jan", "Feb", "Mar",
     "Apr", "May", "Jun",
     "Jul", "Aug", "Sep",
     "Oct", "Nov", "Dec"
   };
   int i;

   for (i = 1; i <= 12; i++) {
      if (!dStrncasecmp(months[i], month_name, 3))
         return i;
   }
   return 0;
}

/*
 * Return a local timestamp from a GMT date string
 * Accept: RFC-1123 | RFC-850 | ANSI asctime | Old Netscape format.
 *
 *   Wdy, DD-Mon-YY HH:MM:SS GMT
 *   Wdy, DD-Mon-YYYY HH:MM:SS GMT
 *   Weekday, DD-Mon-YY HH:MM:SS GMT
 *   Weekday, DD-Mon-YYYY HH:MM:SS GMT
 *   Tue May 21 13:46:22 1991\n
 *   Tue May 21 13:46:22 1991
 *
 *   Let's add:
 *   Mon Jan 11 08:00:00 2010 GMT
 *
 * (return 0 on malformed date string syntax)
 *
 * NOTE that the draft spec wants user agents to be more flexible in what
 * they accept. For now, let's hack in special cases when they're encountered.
 * Why? Because this function is currently understandable, and I don't want to
 * abandon that (or at best decrease that -- see section 5.1.1) until there
 * is known to be good reason.
 */
static time_t Cookies_create_timestamp(const char *expires)
{
   time_t ret;
   int day, month, year, hour, minutes, seconds;
   char *cp;
   const char *const E_msg =
      "Expire date is malformed!\n"
      " (should be RFC-1123 | RFC-850 | ANSI asctime)\n"
      " Discarding cookie: ";

   cp = strchr(expires, ',');
   if (!cp && strlen(expires)>20 && expires[13] == ':' && expires[16] == ':') {
      /* Looks like ANSI asctime format... */
      cp = (char *)expires;
      day = strtol(cp + 8, NULL, 10);       /* day */
      month = Cookies_get_month(cp + 4);    /* month */
      year = strtol(cp + 20, NULL, 10);     /* year */
      hour = strtol(cp + 11, NULL, 10);     /* hour */
      minutes = strtol(cp + 14, NULL, 10);  /* minutes */
      seconds = strtol(cp + 17, NULL, 10);  /* seconds */

   } else if (cp && (cp - expires == 3 || cp - expires > 5) &&
                    (strlen(cp) == 24 || strlen(cp) == 26)) {
      /* RFC-1123 | RFC-850 format | Old Netscape format */
      day = strtol(cp + 2, NULL, 10);
      month = Cookies_get_month(cp + 5);
      year = strtol(cp + 9, &cp, 10);
      /* TODO: tricky, because two digits for year IS ambiguous! */
      year += (year < 70) ? 2000 : ((year < 100) ? 1900 : 0);
      hour = strtol(cp + 1, NULL, 10);
      minutes = strtol(cp + 4, NULL, 10);
      seconds = strtol(cp + 7, NULL, 10);

   } else {
      MSG("%s%s\n", E_msg, expires);
      return (time_t) 0;
   }

   /* Error checks  --this may be overkill */
   if (!(day > 0 && day < 32 && month > 0 && month < 13 && year >= 1970 &&
         hour >= 0 && hour < 24 && minutes >= 0 && minutes < 60 &&
         seconds >= 0 && seconds < 60)) {
      MSG("%s%s\n", E_msg, expires);
      return (time_t) 0;
   }

   /* Calculate local timestamp.
    * [stolen from Lynx... (http://lynx.browser.org)] */
   month -= 3;
   if (month < 0) {
      month += 12;
      year--;
   }

   day += (year - 1968) * 1461 / 4;
   day += ((((month * 153) + 2) / 5) - 672);
   ret = (time_t)((day * 60 * 60 * 24) +
                  (hour * 60 * 60) +
                  (minutes * 60) +
                  seconds);

   return ret;
}

/*
 * Remove the least recently used cookie in the list.
 */
static void Cookies_remove_LRU(Dlist *cookies)
{
   int n = dList_length(cookies);

   if (n > 0) {
      int i;
      CookieData_t *lru = dList_nth_data(cookies, 0);

      for (i = 1; i < n; i++) {
         CookieData_t *curr = dList_nth_data(cookies, i);

         if (curr->last_used < lru->last_used)
            lru = curr;
      }
      dList_remove(cookies, lru);
      MSG("removed LRU cookie \'%s=%s\'\n", lru->name, lru->value);
      Cookies_free_cookie(lru);
   }
}

static void Cookies_add_cookie(CookieData_t *cookie)
{
   Dlist *domain_cookies;
   CookieData_t *c;
   CookieNode *node;

   node = dList_find_sorted(cookies, cookie->domain,Cookie_node_by_domain_cmp);
   domain_cookies = (node) ? node->dlist : NULL;

   if (domain_cookies) {
      /* Remove any cookies with the same name and path */
      while ((c = dList_find_custom(domain_cookies, cookie, Cookies_cmp))){
         Cookies_remove_cookie(c);
      }

      /* Respect the limit of 20 cookies per domain */
      if (dList_length(domain_cookies) >= 20) {
         MSG("There are too many cookies for this domain (%s)\n",
             cookie->domain);
         Cookies_remove_LRU(domain_cookies);
      }

   }

   /* Don't add an expired cookie. Whether expiring now == expired, exactly,
    * is arguable, but we definitely do not want to add a Max-Age=0 cookie.
    */
   if (cookie->expires_at <= time(NULL)) {
      MSG("Goodbye, expired cookie %s=%s d:%s p:%s\n", cookie->name,
          cookie->value, cookie->domain, cookie->path);
      Cookies_free_cookie(cookie);
      return;
   }

   cookie->last_used = cookies_use_counter++;

   /* add cookie to domain list */
   node = dList_find_sorted(cookies, cookie->domain,Cookie_node_by_domain_cmp);
   domain_cookies = (node) ? node->dlist : NULL;
   if (!domain_cookies) {
      domain_cookies = dList_new(5);
      dList_append(domain_cookies, cookie);
      node = dNew(CookieNode, 1);
      node->domain = dStrdup(cookie->domain);
      node->dlist = domain_cookies;
      dList_insert_sorted(cookies, node, Cookie_node_cmp);
   } else {
      dList_append(domain_cookies, cookie);
   }
}

/*
 * Remove the cookie from the domain list.
 * If the domain list is empty, remove the node too.
 * Free the cookie.
 */
static void Cookies_remove_cookie(CookieData_t *cookie)
{
   CookieNode *node;

   node = dList_find_sorted(cookies, cookie->domain,Cookie_node_by_domain_cmp);
   if (node) {
      dList_remove(node->dlist, cookie);
      if (dList_length(node->dlist) == 0) {
         dList_remove(cookies, node);
         dFree(node->domain);
         dList_free(node->dlist);
      }
   } else {
      MSG("Attempting to remove a cookie that doesn't exist!\n");
   }

   Cookies_free_cookie(cookie);
}

/*
 * Return the attribute that is present at *cookie_str.
 */
static char *Cookies_parse_attr(char **cookie_str)
{
   char *str;
   uint_t len;

   while (dIsspace(**cookie_str))
      (*cookie_str)++;

   str = *cookie_str;
   /* find '=' at end of attr, ';' after attr/val pair, '\0' end of string */
   len = strcspn(str, "=;");
   *cookie_str += len;

   while (len && (str[len - 1] == ' ' || str[len - 1] == '\t'))
      len--;
   return dStrndup(str, len);
}

/*
 * Get the value in *cookie_str.
 */
static char *Cookies_parse_value(char **cookie_str)
{
   uint_t len;
   char *str;

   if (**cookie_str == '=') {
      (*cookie_str)++;
      while (dIsspace(**cookie_str))
         (*cookie_str)++;

      str = *cookie_str;
      /* finds ';' after attr/val pair or '\0' at end of string */
      len = strcspn(str, ";");
      *cookie_str += len;

      while (len && (str[len - 1] == ' ' || str[len - 1] == '\t'))
         len--;
   } else {
      str = *cookie_str;
      len = 0;
   }
   return dStrndup(str, len);
}

/*
 * Advance past any value
 */
static void Cookies_eat_value(char **cookie_str)
{
   if (**cookie_str == '=')
      *cookie_str += strcspn(*cookie_str, ";");
}

/*
 * Parse cookie. A cookie might look something like:
 * "Name=Val; Domain=example.com; Max-Age=3600; HttpOnly"
 */
static CookieData_t *Cookies_parse(char *cookie_str, const char *server_date)
{
   CookieData_t *cookie = NULL;
   char *str = cookie_str;
   bool_t first_attr = TRUE;
   bool_t max_age = FALSE;
   bool_t expires = FALSE;

   /* Iterate until there is nothing left of the string */
   while (*str) {
      char *attr;
      char *value;

      /* Get attribute */
      attr = Cookies_parse_attr(&str);

      /* Get the value for the attribute and store it */
      if (first_attr) {
         if (!*str && !*attr) {
            dFree(attr);
            return NULL;
         }
         cookie = dNew0(CookieData_t, 1);
         /* let's arbitrarily choose a year for now */
         cookie->expires_at = time(NULL) + 60 * 60 * 24 * 365;

         if (*str != '=') {
            /* NOTE it seems possible that the Working Group will decide
             * against allowing nameless cookies.
             */
            cookie->name = dStrdup("");
            cookie->value = attr;
         } else {
            cookie->name = dStrdup(attr);
            cookie->value = Cookies_parse_value(&str);
         }
      } else if (dStrcasecmp(attr, "Path") == 0) {
         value = Cookies_parse_value(&str);
         dFree(cookie->path);
         cookie->path = value;
      } else if (dStrcasecmp(attr, "Domain") == 0) {
         value = Cookies_parse_value(&str);
         dFree(cookie->domain);
         cookie->domain = value;
      } else if (dStrcasecmp(attr, "Max-Age") == 0) {
         value = Cookies_parse_value(&str);
         if (isdigit(*value) || *value == '-') {
            cookie->expires_at = time(NULL) + strtol(value, NULL, 10);
            expires = max_age = TRUE;
         }
         dFree(value);
      } else if (dStrcasecmp(attr, "Expires") == 0) {
         if (!max_age) {
            value = Cookies_parse_value(&str);
            cookie->expires_at = Cookies_create_timestamp(value);
            if (cookie->expires_at && server_date) {
               time_t server_time = Cookies_create_timestamp(server_date);

               if (server_time) {
                  time_t now = time(NULL);
                  time_t client_time = cookie->expires_at + now - server_time;

                  if (server_time == cookie->expires_at) {
                      cookie->expires_at = now;
                  } else if ((cookie->expires_at > now) ==
                             (client_time > now)) {
                     cookie->expires_at = client_time;
                  } else {
                     /* It seems not at all unlikely that bad server code will
                      * fail to take normal clock skew into account when
                      * setting max/min cookie values.
                      */
                     MSG("At %ld, %ld was trying to turn into %ld\n",
                         (long)now, (long)cookie->expires_at,
                         (long)client_time);
                  }
               }
            }
            expires = TRUE;
            dFree(value);
            MSG("Expires in %ld seconds, at %s",
                (long)cookie->expires_at - time(NULL),
                ctime(&cookie->expires_at));

         }
      } else if (dStrcasecmp(attr, "Secure") == 0) {
         cookie->secure = TRUE;
         Cookies_eat_value(&str);
      } else if (dStrcasecmp(attr, "HttpOnly") == 0) {
         Cookies_eat_value(&str);
      } else {
         MSG("Cookie contains unknown attribute: '%s'\n", attr);
         Cookies_eat_value(&str);
      }

      if (first_attr)
         first_attr = FALSE;
      else
         dFree(attr);

      if (*str == ';')
         str++;
   }
   cookie->session_only = expires == FALSE;
   return cookie;
}

/*
 * Compare cookies by name and path (return 0 if equal)
 */
static int Cookies_cmp(const void *a, const void *b)
{
   const CookieData_t *ca = a, *cb = b;
   int ret;

   if (!(ret = strcmp(ca->name, cb->name)))
      ret = strcmp(ca->path, cb->path);
   return ret;
}

/*
 * Check whether url_path path-matches cookie_path
 *
 * Note different user agents apparently vary in path-matching behaviour,
 * but this is the recommended method at the moment.
 */
static bool_t Cookies_path_matches(const char *url_path,
                                   const char *cookie_path)
{
   bool_t ret = TRUE;

   if (!url_path || !cookie_path) {
      ret = FALSE;
   } else {
      uint_t c_len = strlen(cookie_path);
      uint_t u_len = strlen(url_path);

      ret = (!strncmp(cookie_path, url_path, c_len) &&
             ((c_len == u_len) ||
              (c_len > 0 && cookie_path[c_len - 1] == '/') ||
              (url_path[c_len] == '/')));
   }
   return ret;
}

/*
 * If cookie path is not properly set, remedy that.
 */
static void Cookies_validate_path(CookieData_t *cookie, const char *url_path)
{
   if (!cookie->path || cookie->path[0] != '/') {
      dFree(cookie->path);

      if (url_path) {
         uint_t len = strlen(url_path);

         while (len && url_path[len] != '/')
            len--;
         cookie->path = dStrndup(url_path, len ? len : 1);
      } else {
         cookie->path = dStrdup("/");
      }
   }
}

/*
 * Check whether host name A domain-matches host name B.
 */
static bool_t Cookies_domain_matches(char *A, char *B)
{
   int diff;

   if (!A || !*A || !B || !*B)
      return FALSE;

   if (*B == '.')
      B++;

   /* Should we concern ourselves with trailing dots in matching (here or
    * elsewhere)? The HTTP State people have found that most user agents
    * don't, so: No.
    */

   if (!dStrcasecmp(A, B))
      return TRUE;

   diff = strlen(A) - strlen(B);

   if (diff > 0) {
      /* B is the tail of A, and the match is preceded by a '.' */
      return (dStrcasecmp(A + diff, B) == 0 && A[diff - 1] == '.');
   } else {
      return FALSE;
   }
}

/*
 * Based on the host, how many internal dots do we need in a cookie domain
 * to make it valid? e.g., "org" is not on the list, so dillo.org is a safe
 * cookie domain, but "uk" is on the list, so ac.uk is not safe.
 *
 * This is imperfect, but it's something. Specifically, checking for these
 * TLDs is the solution that Konqueror used once upon a time, according to
 * reports.
 */
static uint_t Cookies_internal_dots_required(const char *host)
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
         const char *const tlds[] = {"ai","au","bd","bh","ck","eg","et","fk",
                                     "il","in","kh","kr","mk","mt","na","np",
                                     "nz","pg","pk","qa","sa","sb","sg","sv",
                                    "ua","ug","uk","uy","vn","za","zw","name"};
         uint_t i, tld_num = sizeof(tlds) / sizeof(tlds[0]);

         for (i = 0; i < tld_num; i++) {
            if (strlen(tlds[i]) == (uint_t) tld_len &&
                !dStrncasecmp(tlds[i], host + start, tld_len)) {
               MSG("TLD code matched %s\n", tlds[i]);
               ret++;
               break;
            }
         }
      }
   }
   return ret;
}

/*
 * Is the domain an IP address?
 */
static bool_t Cookies_domain_is_ip(const char *domain)
{
   bool_t ipv4 = TRUE, ipv6 = TRUE;

   if (!domain)
      return FALSE;

   while (*domain) {
      if (*domain != '.' && !isdigit(*domain))
         ipv4 = FALSE;
      if (*domain != ':' && !isxdigit(*domain))
         ipv6 = FALSE;
      if (!(ipv4 || ipv6))
         return FALSE;
      domain++;
   }
   MSG("an IP address\n");
   return TRUE;
}

/*
 * Validate cookies domain against some security checks.
 */
static bool_t Cookies_validate_domain(CookieData_t *cookie, char *host)
{
   uint_t i, internal_dots;

   if (!cookie->domain) {
      cookie->domain = dStrdup(host);
      return TRUE;
   }

   if (cookie->domain[0] != '.' && !Cookies_domain_is_ip(cookie->domain)) {
      char *d = dStrconcat(".", cookie->domain, NULL);
      dFree(cookie->domain);
      cookie->domain = d;
   }

   if (!Cookies_domain_matches(host, cookie->domain))
      return FALSE;

   internal_dots = 0;
   for (i = 1; i < strlen(cookie->domain) - 1; i++) {
      if (cookie->domain[i] == '.')
         internal_dots++;
   }

   /* All of this dots business is a weak hack.
    * TODO: accept the publicsuffix.org list as an optional external file.
    */
   if (internal_dots < Cookies_internal_dots_required(host)) {
      MSG("not enough dots in %s\n", cookie->domain);
      return FALSE;
   }

   MSG("host %s and domain %s is all right\n", host, cookie->domain);
   return TRUE;
}

/*
 * Set the value corresponding to the cookie string
 */
static void Cookies_set(char *cookie_string, char *url_host,
                        char *url_path, char *server_date)
{
   CookieControlAction action;
   CookieData_t *cookie;

   if (disabled)
      return;

   action = Cookies_control_check_domain(url_host);
   if (action == COOKIE_DENY) {
      MSG("denied SET for %s\n", url_host);
      return;
   }

   _MSG("%s setting: %s\n", url_host, cookie_string);

   if ((cookie = Cookies_parse(cookie_string, server_date))) {
      if (Cookies_validate_domain(cookie, url_host)) {
         Cookies_validate_path(cookie, url_path);
         if (action == COOKIE_ACCEPT_SESSION)
            cookie->session_only = TRUE;
         Cookies_add_cookie(cookie);
      } else {
         MSG("Rejecting cookie for domain %s from host %s path %s\n",
             cookie->domain, url_host, url_path);
         Cookies_free_cookie(cookie);
      }
   }
}

/*
 * Compare the cookie with the supplied data to see whether it matches
 */
static bool_t Cookies_match(CookieData_t *cookie, const char *url_path,
                            bool_t is_ssl)
{
   /* Insecure cookies matches both secure and insecure urls, secure
      cookies matches only secure urls */
   if (cookie->secure && !is_ssl)
      return FALSE;

   if (!Cookies_path_matches(url_path, cookie->path))
      return FALSE;

   /* It's a match */
   return TRUE;
}

static void Cookies_add_matching_cookies(const char *domain,
                                         const char *url_path,
                                         Dlist *matching_cookies,
                                         bool_t is_ssl)
{
   CookieNode *node = dList_find_sorted(cookies, domain,
                                        Cookie_node_by_domain_cmp);
   if (node) {
      int i;
      CookieData_t *cookie;
      Dlist *domain_cookies = node->dlist;

      for (i = 0; (cookie = dList_nth_data(domain_cookies, i)); ++i) {
         /* Remove expired cookie. */
         if (cookie->expires_at < time(NULL)) {
            MSG("Goodbye, expired cookie %s=%s d:%s p:%s\n", cookie->name,
                cookie->value, cookie->domain, cookie->path);
            Cookies_remove_cookie(cookie);
            --i; continue;
         }
         /* Check if the cookie matches the requesting URL */
         if (Cookies_match(cookie, url_path, is_ssl)) {
            int j;
            CookieData_t *curr;
            uint_t path_length = strlen(cookie->path);

            cookie->last_used = cookies_use_counter;

            /* Longest cookies go first */
            for (j = 0;
                 (curr = dList_nth_data(matching_cookies, j)) &&
                  strlen(curr->path) >= path_length;
                 j++) ;
            dList_insert_pos(matching_cookies, cookie, j);
         }
      }
   }
}

/*
 * Return a string that contains all relevant cookies as headers.
 */
static char *Cookies_get(char *url_host, char *url_path,
                         char *url_scheme)
{
   char *domain_str, *str;
   CookieData_t *cookie;
   Dlist *matching_cookies;
   bool_t is_ssl;
   Dstr *cookie_dstring;
   int i;

   if (disabled)
      return dStrdup("");

   matching_cookies = dList_new(8);

   /* Check if the protocol is secure or not */
   is_ssl = (!dStrcasecmp(url_scheme, "https"));

   for (domain_str = (char *) url_host;
        domain_str != NULL && *domain_str;
        domain_str = strchr(domain_str+1, '.')) {
      Cookies_add_matching_cookies(domain_str, url_path, matching_cookies,
                                   is_ssl);
   }
   if (!Cookies_domain_is_ip(url_host)) {
      domain_str = dStrconcat(".", url_host, NULL);
      Cookies_add_matching_cookies(domain_str, url_path, matching_cookies,
                                   is_ssl);
      dFree(domain_str);
   }

   /* Found the cookies, now make the string */
   cookie_dstring = dStr_new("");
   if (dList_length(matching_cookies) > 0) {

      dStr_sprintfa(cookie_dstring, "Cookie: ");

      for (i = 0; (cookie = dList_nth_data(matching_cookies, i)); ++i) {
         dStr_sprintfa(cookie_dstring,
                       "%s%s%s",
                       cookie->name, *cookie->name ? "=" : "", cookie->value);
         dStr_append(cookie_dstring,
                     dList_length(matching_cookies) > i + 1 ? "; " : "\r\n");
      }
   }

   dList_free(matching_cookies);
   str = cookie_dstring->str;
   dStr_free(cookie_dstring, FALSE);

   if (*str)
      cookies_use_counter++;

   _MSG("%s gets %s\n", url_host, str);
   return str;
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
   int i, j;
   bool_t enabled = FALSE;

   /* Get a file pointer */
   filename = dStrconcat(dGethomedir(), "/.dillo/cookiesrc", NULL);
   stream = Cookies_fopen(filename, "r", "DEFAULT DENY\n");
   dFree(filename);

   if (!stream)
      return 2;

   /* Get all lines in the file */
   while (!feof(stream)) {
      line[0] = '\0';
      rc = fgets(line, LINE_MAXLEN, stream);
      if (!rc && ferror(stream)) {
         MSG("Error while reading rule from cookiesrc: %s\n",
             dStrerror(errno));
         break; /* bail out */
      }

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if (line[0] != '\0' && line[0] != '#') {
         i = 0;
         j = 0;

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

/* -- Dpi parser ----------------------------------------------------------- */

/*
 * Parse a data stream (dpi protocol)
 * Note: Buf is a zero terminated string
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int srv_parse_tok(Dsh *sh, ClientInfo *client, char *Buf)
{
   char *cmd, *cookie, *host, *path, *scheme;
   int ret = 1;
   size_t BufSize = strlen(Buf);

   cmd = a_Dpip_get_attr_l(Buf, BufSize, "cmd");

   if (!cmd) {
      /* abort */
   } else if (client->status == 0) {
      /* authenticate */
      if (a_Dpip_check_auth(Buf) == 1) {
         client->status = 1;
         ret = 0;
      }
   } else if (strcmp(cmd, "DpiBye") == 0) {
      dFree(cmd);
      MSG("(pid %d): Got DpiBye.\n", (int)getpid());
      exit(0);

   } else if (cmd && strcmp(cmd, "set_cookie") == 0) {
      char *date;

      dFree(cmd);
      cookie = a_Dpip_get_attr_l(Buf, BufSize, "cookie");
      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");
      date = a_Dpip_get_attr_l(Buf, BufSize, "date");

      Cookies_set(cookie, host, path, date);

      dFree(date);
      dFree(path);
      dFree(host);
      dFree(cookie);
      ret = 2;

   } else if (cmd && strcmp(cmd, "get_cookie") == 0) {
      dFree(cmd);
      scheme = a_Dpip_get_attr_l(Buf, BufSize, "scheme");
      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");

      cookie = Cookies_get(host, path, scheme);
      dFree(scheme);
      dFree(path);
      dFree(host);

      cmd = a_Dpip_build_cmd("cmd=%s cookie=%s", "get_cookie_answer", cookie);

      if (a_Dpip_dsh_write_str(sh, 1, cmd)) {
          ret = 1;
      } else {
          _MSG("a_Dpip_dsh_write_str: SUCCESS cmd={%s}\n", cmd);
          ret = 2;
      }
      dFree(cookie);
      dFree(cmd);
   }

   return ret;
}

/* --  Termination handlers ----------------------------------------------- */
/*
 * (was to delete the local namespace socket),
 *  but this is handled by 'dpid' now.
 */
static void cleanup(void)
{
  Cookies_save_and_free();
  MSG("cleanup\n");
  /* no more cleanup required */
}

/*
 * Perform any necessary cleanups upon abnormal termination
 */
static void termination_handler(int signum)
{
  exit(signum);
}


/*
 * -- MAIN -------------------------------------------------------------------
 */
int main(void) {
   struct sockaddr_in sin;
   socklen_t address_size;
   ClientInfo *client;
   int sock_fd, code;
   char *buf;
   Dsh *sh;

   /* Arrange the cleanup function for terminations via exit() */
   atexit(cleanup);

   /* Arrange the cleanup function for abnormal terminations */
   if (signal (SIGINT, termination_handler) == SIG_IGN)
     signal (SIGINT, SIG_IGN);
   if (signal (SIGHUP, termination_handler) == SIG_IGN)
     signal (SIGHUP, SIG_IGN);
   if (signal (SIGTERM, termination_handler) == SIG_IGN)
     signal (SIGTERM, SIG_IGN);

   Cookies_init();
   MSG("(v.1) accepting connections...\n");

   if (disabled)
      exit(1);

   /* some OSes may need this... */
   address_size = sizeof(struct sockaddr_in);

   while (1) {
      sock_fd = accept(STDIN_FILENO, (struct sockaddr *)&sin, &address_size);
      if (sock_fd == -1) {
         perror("[accept]");
         exit(1);
      }

      /* create the Dsh structure */
      sh = a_Dpip_dsh_new(sock_fd, sock_fd, 8*1024);
      client = dNew(ClientInfo,1);
      client->sh = sh;
      client->status = 0;

      while (1) {
         code = 1;
         if ((buf = a_Dpip_dsh_read_token(sh, 1)) != NULL) {
            /* Let's see what we fished... */
            _MSG(" buf = {%s}\n", buf);
            code = srv_parse_tok(sh, client, buf);
            dFree(buf);
         }

         _MSG(" code = %d %s\n", code, code == 1 ? "EXIT" : "BREAK");
         if (code == 1) {
            exit(1);
         } else if (code == 2) {
            break;
         }
      }

      _MSG("Closing Dsh\n");
      a_Dpip_dsh_close(sh);
      a_Dpip_dsh_free(sh);
      dFree(client);

   }/*while*/

   return 0;
}

#endif /* !DISABLE_COOKIES */
