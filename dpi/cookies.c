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

/* The current standard for cookies is RFC 6265.
 *
 * Info from 2009 on cookies in the wild:
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

#define MAX_DOMAIN_COOKIES 20
#define MAX_TOTAL_COOKIES 1200

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
   Dlist *cookies;
} DomainNode;

typedef struct {
   char *name;
   char *value;
   char *domain;
   char *path;
   time_t expires_at;
   bool_t host_only;
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

static Dlist *all_cookies;

/* List of DomainNode. Each node holds a domain and its list of cookies */
static Dlist *domains;

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
"# [domain  subdomains  path  secure  expiry_time  name  value]\n\n";

/* The epoch is Jan 1, 1970. When there is difficulty in representing future
 * dates, use the (by far) most likely last representable time in Jan 19, 2038.
 */
static struct tm cookies_epoch_tm = {0, 0, 0, 1, 0, 70, 0, 0, 0, 0, 0};
static time_t cookies_epoch_time, cookies_future_time;

/*
 * Forward declarations
 */

static CookieControlAction Cookies_control_check_domain(const char *domain);
static int Cookie_control_init(void);
static void Cookies_add_cookie(CookieData_t *cookie);
static int Cookies_cmp(const void *a, const void *b);

/*
 * Compare function for searching a domain node
 */
static int Domain_node_cmp(const void *v1, const void *v2)
{
   const DomainNode *n1 = v1, *n2 = v2;

   return dStrAsciiCasecmp(n1->domain, n2->domain);
}

/*
 * Compare function for searching a domain node by domain
 */
static int Domain_node_by_domain_cmp(const void *v1, const void *v2)
{
   const DomainNode *node = v1;
   const char *domain = v2;

   return dStrAsciiCasecmp(node->domain, domain);
}

/*
 * Delete node. This will not free any cookies that might be in node->cookies.
 */
static void Cookies_delete_node(DomainNode *node)
{
   dList_remove(domains, node);
   dFree(node->domain);
   dList_free(node->cookies);
   dFree(node);
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

static void Cookies_tm_init(struct tm *tm)
{
   tm->tm_sec = cookies_epoch_tm.tm_sec;
   tm->tm_min = cookies_epoch_tm.tm_min;
   tm->tm_hour = cookies_epoch_tm.tm_hour;
   tm->tm_mday = cookies_epoch_tm.tm_mday;
   tm->tm_mon = cookies_epoch_tm.tm_mon;
   tm->tm_year = cookies_epoch_tm.tm_year;
   tm->tm_isdst = cookies_epoch_tm.tm_isdst;
}

/*
 * Read in cookies from 'stream' (cookies.txt)
 */
static void Cookies_load_cookies(FILE *stream)
{
   char line[LINE_MAXLEN];

   all_cookies = dList_new(32);
   domains = dList_new(32);

   /* Get all lines in the file */
   while (!feof(stream)) {
      line[0] = '\0';
      if ((fgets(line, LINE_MAXLEN, stream) == NULL) && ferror(stream)) {
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
         CookieData_t *cookie = dNew0(CookieData_t, 1);

         cookie->session_only = FALSE;
         cookie->domain = dStrdup(dStrsep(&line_marker, "\t"));
         piece = dStrsep(&line_marker, "\t");
         if (piece != NULL && piece[0] == 'F')
            cookie->host_only = TRUE;
         cookie->path = dStrdup(dStrsep(&line_marker, "\t"));
         piece = dStrsep(&line_marker, "\t");
         if (piece != NULL && piece[0] == 'T')
            cookie->secure = TRUE;
         piece = dStrsep(&line_marker, "\t");
         if (piece != NULL) {
            /* There is some problem with simply putting the maximum value
             * into tm.tm_sec (although a value close to it works).
             */
            long seconds = strtol(piece, NULL, 10);
            struct tm tm;
            Cookies_tm_init(&tm);
            tm.tm_min += seconds / 60;
            tm.tm_sec += seconds % 60;
            cookie->expires_at = mktime(&tm);
         } else {
            cookie->expires_at = (time_t) -1;
         }
         cookie->name = dStrdup(dStrsep(&line_marker, "\t"));
         cookie->value = dStrdup(line_marker ? line_marker : "");

         if (!cookie->domain || cookie->domain[0] == '\0' ||
             !cookie->path || cookie->path[0] != '/' ||
             !cookie->name || !cookie->value) {
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
   MSG("Cookies loaded: %d.\n", dList_length(all_cookies));
}

/*
 * Initialize the cookies module
 * (The 'disabled' variable is writeable only within Cookies_init)
 */
static void Cookies_init()
{
   char *filename;
#ifndef HAVE_LOCKF
   struct flock lck;
#endif
   struct tm future_tm = {7, 14, 3, 19, 0, 138, 0, 0, 0, 0, 0};

   /* Default setting */
   disabled = TRUE;

   cookies_epoch_time = mktime(&cookies_epoch_tm);
   cookies_future_time = mktime(&future_tm);

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

   Cookies_load_cookies(file_stream);
}

/*
 * Flush cookies to disk and free all the memory allocated.
 */
static void Cookies_save_and_free()
{
   int i, fd, saved = 0;
   DomainNode *node;
   CookieData_t *cookie;
   time_t now;

#ifndef HAVE_LOCKF
   struct flock lck;
#endif

   if (disabled)
      return;

   now = time(NULL);

   rewind(file_stream);
   fd = fileno(file_stream);
   if (ftruncate(fd, 0) == -1)
      MSG("Cookies: Truncate file stream failed: %s\n", dStrerror(errno));
   fprintf(file_stream, "%s", cookies_txt_header_str);

   /* Iterate cookies per domain, saving and freeing */
   while ((node = dList_nth_data(domains, 0))) {
      for (i = 0; (cookie = dList_nth_data(node->cookies, i)); ++i) {
         if (!cookie->session_only && difftime(cookie->expires_at, now) > 0) {
            int len;
            char buf[LINE_MAXLEN];

            len = snprintf(buf, LINE_MAXLEN, "%s\t%s\t%s\t%s\t%ld\t%s\t%s\n",
                           cookie->domain,
                           cookie->host_only ? "FALSE" : "TRUE",
                           cookie->path,
                           cookie->secure ? "TRUE" : "FALSE",
                           (long) difftime(cookie->expires_at,
                                           cookies_epoch_time),
                           cookie->name,
                           cookie->value);
            if (len < LINE_MAXLEN) {
               fprintf(file_stream, "%s", buf);
               saved++;
            } else {
               MSG("Not saving overly long cookie for %s.\n", cookie->domain);
            }
         }
         Cookies_free_cookie(cookie);
      }
      Cookies_delete_node(node);
   }
   dList_free(domains);
   dList_free(all_cookies);

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

   MSG("Cookies saved: %d.\n", saved);
}

/*
 * Take a month's name and return a number between 0-11.
 * E.g. 'April' -> 3
 */
static int Cookies_get_month(const char *month_name)
{
   static const char *const months[] =
   { "Jan", "Feb", "Mar",
     "Apr", "May", "Jun",
     "Jul", "Aug", "Sep",
     "Oct", "Nov", "Dec"
   };
   int i;

   for (i = 0; i < 12; i++) {
      if (!dStrnAsciiCasecmp(months[i], month_name, 3))
         return i;
   }
   return -1;
}

/*
 * Accept: RFC-1123 | RFC-850 | ANSI asctime | Old Netscape format date string.
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
 * Return a pointer to a struct tm, or NULL on error.
 *
 * NOTE that the RFC wants user agents to be more flexible in what
 * they accept. For now, let's hack in special cases when they're encountered.
 * Why? Because this function is currently understandable, and I don't want to
 * abandon that (or at best decrease that -- see section 5.1.1) until there
 * is known to be good reason.
 */
static struct tm *Cookies_parse_date(const char *date)
{
   struct tm *tm;
   char *cp = strchr(date, ',');

   if (!cp && strlen(date)>20 && date[13] == ':' && date[16] == ':') {
      /* Looks like ANSI asctime format... */
      tm = dNew0(struct tm, 1);

      cp = (char *)date;
      tm->tm_mon = Cookies_get_month(cp + 4);
      tm->tm_mday = strtol(cp + 8, NULL, 10);
      tm->tm_hour = strtol(cp + 11, NULL, 10);
      tm->tm_min = strtol(cp + 14, NULL, 10);
      tm->tm_sec = strtol(cp + 17, NULL, 10);
      tm->tm_year = strtol(cp + 20, NULL, 10) - 1900;

   } else if (cp && (cp - date == 3 || cp - date > 5) &&
                    (strlen(cp) == 24 || strlen(cp) == 26)) {
      /* RFC-1123 | RFC-850 format | Old Netscape format */
      tm = dNew0(struct tm, 1);

      tm->tm_mday = strtol(cp + 2, NULL, 10);
      tm->tm_mon = Cookies_get_month(cp + 5);
      tm->tm_year = strtol(cp + 9, &cp, 10);
      /* tm_year is the number of years since 1900 */
      if (tm->tm_year < 70)
         tm->tm_year += 100;
      else if (tm->tm_year > 100)
         tm->tm_year -= 1900;
      tm->tm_hour = strtol(cp + 1, NULL, 10);
      tm->tm_min = strtol(cp + 4, NULL, 10);
      tm->tm_sec = strtol(cp + 7, NULL, 10);

   } else {
      tm = NULL;
      MSG("In date \"%s\", format not understood.\n", date);
   }

   /* Error checks. This may be overkill. */
   if (tm &&
       !(tm->tm_mday > 0 && tm->tm_mday < 32 && tm->tm_mon >= 0 &&
         tm->tm_mon < 12 && tm->tm_year >= 70 && tm->tm_hour >= 0 &&
         tm->tm_hour < 24 && tm->tm_min >= 0 && tm->tm_min < 60 &&
         tm->tm_sec >= 0 && tm->tm_sec < 60)) {
      MSG("Date \"%s\" values not in range.\n", date);
      dFree(tm);
      tm = NULL;
   }

   return tm;
}

/*
 * Find the least recently used cookie among those in the provided list.
 */
static CookieData_t *Cookies_get_LRU(Dlist *cookies)
{
   int i, n = dList_length(cookies);
   CookieData_t *lru = dList_nth_data(cookies, 0);

   for (i = 1; i < n; i++) {
      CookieData_t *curr = dList_nth_data(cookies, i);

      if (curr->last_used < lru->last_used)
         lru = curr;
   }
   return lru;
}

/*
 * Delete expired cookies.
 * If node is given, only check those cookies.
 * Note that nodes can disappear if all of their cookies were expired.
 *
 * Return the number of cookies that were expired.
 */
static int Cookies_rm_expired_cookies(DomainNode *node)
{
   Dlist *cookies = node ? node->cookies : all_cookies;
   int removed = 0;
   int i = 0, n = dList_length(cookies);
   time_t now = time(NULL);

   while (i < n) {
      CookieData_t *c = dList_nth_data(cookies, i);

      if (difftime(c->expires_at, now) < 0) {
         DomainNode *currnode = node ? node :
              dList_find_sorted(domains, c->domain, Domain_node_by_domain_cmp);
         dList_remove(currnode->cookies, c);
         if (dList_length(currnode->cookies) == 0)
            Cookies_delete_node(currnode);
         dList_remove_fast(all_cookies, c);
         Cookies_free_cookie(c);
         n--;
         removed++;
      } else {
         i++;
      }
   }
   return removed;
}

/*
 * There are too many cookies. Choose one to remove and delete.
 * If node is given, select from among its cookies only.
 */
static void Cookies_too_many(DomainNode *node)
{
   CookieData_t *lru = Cookies_get_LRU(node ? node->cookies : all_cookies);

   MSG("Too many cookies!\n"
       "Removing LRU cookie for \'%s\': \'%s=%s\'\n", lru->domain,
       lru->name, lru->value);
   if (!node)
      node = dList_find_sorted(domains, lru->domain,Domain_node_by_domain_cmp);

   dList_remove(node->cookies, lru);
   dList_remove_fast(all_cookies, lru);
   Cookies_free_cookie(lru);
   if (dList_length(node->cookies) == 0)
      Cookies_delete_node(node);
}

static void Cookies_add_cookie(CookieData_t *cookie)
{
   Dlist *domain_cookies;
   CookieData_t *c;
   DomainNode *node;

   node = dList_find_sorted(domains, cookie->domain,Domain_node_by_domain_cmp);
   domain_cookies = (node) ? node->cookies : NULL;

   if (domain_cookies) {
      /* Remove any cookies with the same name, path, and host-only values. */
      while ((c = dList_find_custom(domain_cookies, cookie, Cookies_cmp))) {
         dList_remove(domain_cookies, c);
         dList_remove_fast(all_cookies, c);
         Cookies_free_cookie(c);
      }
   }

   if ((cookie->expires_at == (time_t) -1) ||
       (difftime(cookie->expires_at, time(NULL)) <= 0)) {
      /*
       * Don't add an expired cookie. Whether expiring now == expired, exactly,
       * is arguable, but we definitely do not want to add a Max-Age=0 cookie.
       */
      _MSG("Goodbye, cookie %s=%s d:%s p:%s\n", cookie->name,
           cookie->value, cookie->domain, cookie->path);
      Cookies_free_cookie(cookie);
   } else {
      if (domain_cookies && dList_length(domain_cookies) >=MAX_DOMAIN_COOKIES){
         int removed = Cookies_rm_expired_cookies(node);

         if (removed == 0) {
            Cookies_too_many(node);
         } else if (removed >= MAX_DOMAIN_COOKIES) {
            /* So many were removed that the node might have been deleted. */
            node = dList_find_sorted(domains, cookie->domain,
                                                    Domain_node_by_domain_cmp);
            domain_cookies = (node) ? node->cookies : NULL;
         }
      }
      if (dList_length(all_cookies) >= MAX_TOTAL_COOKIES) {
         if (Cookies_rm_expired_cookies(NULL) == 0) {
            Cookies_too_many(NULL);
         } else if (domain_cookies) {
            /* Our own node might have just been deleted. */
            node = dList_find_sorted(domains, cookie->domain,
                                                    Domain_node_by_domain_cmp);
            domain_cookies = (node) ? node->cookies : NULL;
         }
      }

      cookie->last_used = cookies_use_counter++;

      /* Actually add the cookie! */
      dList_append(all_cookies, cookie);

      if (!domain_cookies) {
         domain_cookies = dList_new(5);
         dList_append(domain_cookies, cookie);
         node = dNew(DomainNode, 1);
         node->domain = dStrdup(cookie->domain);
         node->cookies = domain_cookies;
         dList_insert_sorted(domains, node, Domain_node_cmp);
      } else {
         dList_append(domain_cookies, cookie);
      }
   }
   if (domain_cookies && (dList_length(domain_cookies) == 0))
      Cookies_delete_node(node);
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
 * Return the number of seconds by which our clock is ahead of the server's
 * clock.
 */
static double Cookies_server_timediff(const char *server_date)
{
   double ret = 0;

   if (server_date) {
      struct tm *server_tm = Cookies_parse_date(server_date);

      if (server_tm) {
         time_t server_time = mktime(server_tm);

         if (server_time != (time_t) -1)
            ret = difftime(time(NULL), server_time);
         dFree(server_tm);
      }
   }
   return ret;
}

static void Cookies_unquote_string(char *str)
{
   if (str && str[0] == '\"') {
      uint_t len = strlen(str);

      if (len > 1 && str[len - 1] == '\"') {
         str[len - 1] = '\0';
         while ((*str = str[1]))
            str++;
      }
   }
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
         time_t now;
         struct tm *tm;

         if (*str != '=' || *attr == '\0') {
            /* disregard nameless cookie */
            dFree(attr);
            return NULL;
         }
         cookie = dNew0(CookieData_t, 1);
         cookie->name = attr;
         cookie->value = Cookies_parse_value(&str);

         /* let's arbitrarily initialise with a year for now */
         now = time(NULL);
         tm = gmtime(&now);
         ++tm->tm_year;
         cookie->expires_at = mktime(tm);
         if (cookie->expires_at == (time_t) -1)
            cookie->expires_at = cookies_future_time;
      } else if (dStrAsciiCasecmp(attr, "Path") == 0) {
         value = Cookies_parse_value(&str);
         dFree(cookie->path);
         cookie->path = value;
      } else if (dStrAsciiCasecmp(attr, "Domain") == 0) {
         value = Cookies_parse_value(&str);
         dFree(cookie->domain);
         cookie->domain = value;
      } else if (dStrAsciiCasecmp(attr, "Max-Age") == 0) {
         value = Cookies_parse_value(&str);
         if (isdigit(*value) || *value == '-') {
            time_t now = time(NULL);
            long age = strtol(value, NULL, 10);
            struct tm *tm = gmtime(&now);

            tm->tm_sec += age;
            cookie->expires_at = mktime(tm);
            if (age > 0 && cookie->expires_at == (time_t) -1) {
               cookie->expires_at = cookies_future_time;
            }
            _MSG("Cookie to expire at %s", ctime(&cookie->expires_at));
            expires = max_age = TRUE;
         }
         dFree(value);
      } else if (dStrAsciiCasecmp(attr, "Expires") == 0) {
         if (!max_age) {
            struct tm *tm;

            value = Cookies_parse_value(&str);
            Cookies_unquote_string(value);
            _MSG("Expires attribute gives %s\n", value);
            tm = Cookies_parse_date(value);
            if (tm) {
               tm->tm_sec += Cookies_server_timediff(server_date);
               cookie->expires_at = mktime(tm);
               if (cookie->expires_at == (time_t) -1 && tm->tm_year >= 138) {
                  /* Just checking tm_year does not ensure that the problem was
                   * inability to represent a distant date...
                   */
                  cookie->expires_at = cookies_future_time;
               }
               _MSG("Cookie to expire at %s", ctime(&cookie->expires_at));
               dFree(tm);
            } else {
               cookie->expires_at = (time_t) -1;
            }
            expires = TRUE;
            dFree(value);
         } else {
            Cookies_eat_value(&str);
         }
      } else if (dStrAsciiCasecmp(attr, "Secure") == 0) {
         cookie->secure = TRUE;
         Cookies_eat_value(&str);
      } else if (dStrAsciiCasecmp(attr, "HttpOnly") == 0) {
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
 * Compare cookies by host_only, name, and path. Return 0 if equal.
 */
static int Cookies_cmp(const void *a, const void *b)
{
   const CookieData_t *ca = a, *cb = b;

   return (ca->host_only != cb->host_only) ||
          (strcmp(ca->name, cb->name) != 0) ||
          (strcmp(ca->path, cb->path) != 0);
}

/*
 * Is the domain an IP address?
 */
static bool_t Cookies_domain_is_ip(const char *domain)
{
   uint_t len;

   if (!domain)
      return FALSE;

   len = strlen(domain);

   if (len == strspn(domain, "0123456789.")) {
      _MSG("an IPv4 address\n");
      return TRUE;
   }
   if (strchr(domain, ':') &&
       (len == strspn(domain, "0123456789abcdefABCDEF:."))) {
      /* The precise format is shown in section 3.2.2 of rfc 3986 */
      MSG("an IPv6 address\n");
      return TRUE;
   }
   return FALSE;
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

   if (!dStrAsciiCasecmp(A, B))
      return TRUE;

   if (Cookies_domain_is_ip(B))
      return FALSE;

   diff = strlen(A) - strlen(B);

   if (diff > 0) {
      /* B is the tail of A, and the match is preceded by a '.' */
      return (dStrAsciiCasecmp(A + diff, B) == 0 && A[diff - 1] == '.');
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
         /* These TLDs were chosen by examining the current publicsuffix list
          * in October 2014 and picking out those where it was simplest for
          * them to describe the situation by beginning with a "*.[tld]" rule
          * or every rule was "[something].[tld]".
          */
         const char *const tlds[] = {"bd","bn","ck","cy","er","fj","fk",
                                     "gu","il","jm","ke","kh","kw","mm","mz",
                                     "ni","np","pg","ye","za","zm","zw"};
         uint_t i, tld_num = sizeof(tlds) / sizeof(tlds[0]);

         for (i = 0; i < tld_num; i++) {
            if (strlen(tlds[i]) == (uint_t) tld_len &&
                !dStrnAsciiCasecmp(tlds[i], host + start, tld_len)) {
               _MSG("TLD code matched %s\n", tlds[i]);
               ret++;
               break;
            }
         }
      }
   }
   return ret;
}

/*
 * Validate cookies domain against some security checks.
 */
static bool_t Cookies_validate_domain(CookieData_t *cookie, char *host)
{
   uint_t i, internal_dots;

   if (!cookie->domain) {
      cookie->domain = dStrdup(host);
      cookie->host_only = TRUE;
      return TRUE;
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

   _MSG("host %s and domain %s is all right\n", host, cookie->domain);
   return TRUE;
}

/*
 * Set the value corresponding to the cookie string
 * Return value: 0 set OK, -1 disabled, -2 denied, -3 rejected.
 */
static int Cookies_set(char *cookie_string, char *url_host,
                       char *url_path, char *server_date)
{
   CookieControlAction action;
   CookieData_t *cookie;
   int ret = -1;

   if (disabled)
      return ret;

   action = Cookies_control_check_domain(url_host);
   if (action == COOKIE_DENY) {
      MSG("denied SET for %s\n", url_host);
      ret = -2;

   } else {
      MSG("%s SETTING: %s\n", url_host, cookie_string);
      ret = -3;
      if ((cookie = Cookies_parse(cookie_string, server_date))) {
         if (Cookies_validate_domain(cookie, url_host)) {
            Cookies_validate_path(cookie, url_path);
            if (action == COOKIE_ACCEPT_SESSION)
               cookie->session_only = TRUE;
            Cookies_add_cookie(cookie);
            ret = 0;
         } else {
            MSG("Rejecting cookie for domain %s from host %s path %s\n",
                cookie->domain, url_host, url_path);
            Cookies_free_cookie(cookie);
         }
      }
   }

   return ret;
}

/*
 * Compare the cookie with the supplied data to see whether it matches
 */
static bool_t Cookies_match(CookieData_t *cookie, const char *url_path,
                            bool_t host_only_val, bool_t is_ssl)
{
   if (cookie->host_only != host_only_val)
      return FALSE;

   /* Insecure cookies match both secure and insecure urls, secure
      cookies match only secure urls */
   if (cookie->secure && !is_ssl)
      return FALSE;

   if (!Cookies_path_matches(url_path, cookie->path))
      return FALSE;

   /* It's a match */
   return TRUE;
}

static void Cookies_add_matching_cookies(const char *domain,
                                         const char *url_path,
                                         bool_t host_only_val,
                                         Dlist *matching_cookies,
                                         bool_t is_ssl)
{
   DomainNode *node = dList_find_sorted(domains, domain,
                                        Domain_node_by_domain_cmp);
   if (node) {
      int i;
      CookieData_t *cookie;
      Dlist *domain_cookies = node->cookies;

      for (i = 0; (cookie = dList_nth_data(domain_cookies, i)); ++i) {
         /* Remove expired cookie. */
         if (difftime(cookie->expires_at, time(NULL)) < 0) {
            _MSG("Goodbye, expired cookie %s=%s d:%s p:%s\n", cookie->name,
                 cookie->value, cookie->domain, cookie->path);
            dList_remove(domain_cookies, cookie);
            dList_remove_fast(all_cookies, cookie);
            Cookies_free_cookie(cookie);
            --i; continue;
         }
         /* Check if the cookie matches the requesting URL */
         if (Cookies_match(cookie, url_path, host_only_val, is_ssl)) {
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

      if (dList_length(domain_cookies) == 0)
         Cookies_delete_node(node);
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
   bool_t is_ssl, is_ip_addr, host_only_val;

   Dstr *cookie_dstring;
   int i;

   if (disabled)
      return dStrdup("");

   matching_cookies = dList_new(8);

   /* Check if the protocol is secure or not */
   is_ssl = (!dStrAsciiCasecmp(url_scheme, "https"));

   is_ip_addr = Cookies_domain_is_ip(url_host);

   /* If a cookie is set that lacks a Domain attribute, its domain is set to
    * the server's host and the host_only flag is set for that cookie. Such a
    * cookie can only be sent back to that host. Cookies with Domain attrs do
    * not have the host_only flag set, and may be sent to subdomains. Domain
    * attrs can have leading dots, which should be ignored for matching
    * purposes.
    */
   host_only_val = FALSE;
   if (!is_ip_addr) {
      /* e.g., sub.example.com set a cookie with domain ".sub.example.com". */
      domain_str = dStrconcat(".", url_host, NULL);
      Cookies_add_matching_cookies(domain_str, url_path, host_only_val,
                                   matching_cookies, is_ssl);
      dFree(domain_str);
   }
   host_only_val = TRUE;
   /* e.g., sub.example.com set a cookie with no domain attribute. */
   Cookies_add_matching_cookies(url_host, url_path, host_only_val,
                                matching_cookies, is_ssl);
   host_only_val = FALSE;
   /* e.g., sub.example.com set a cookie with domain "sub.example.com". */
   Cookies_add_matching_cookies(url_host, url_path, host_only_val,
                                matching_cookies, is_ssl);

   if (!is_ip_addr) {
      for (domain_str = strchr(url_host+1, '.');
           domain_str != NULL && *domain_str;
           domain_str = strchr(domain_str+1, '.')) {
         /* e.g., sub.example.com set a cookie with domain ".example.com". */
         Cookies_add_matching_cookies(domain_str, url_path, host_only_val,
                                      matching_cookies, is_ssl);
         if (domain_str[1]) {
            domain_str++;
            /* e.g., sub.example.com set a cookie with domain "example.com".*/
            Cookies_add_matching_cookies(domain_str, url_path, host_only_val,
                                         matching_cookies, is_ssl);
         }
      }
   }

   /* Found the cookies, now make the string */
   cookie_dstring = dStr_new("");
   if (dList_length(matching_cookies) > 0) {

      dStr_sprintfa(cookie_dstring, "Cookie: ");

      for (i = 0; (cookie = dList_nth_data(matching_cookies, i)); ++i) {
         dStr_sprintfa(cookie_dstring, "%s=%s", cookie->name, cookie->value);
         dStr_append(cookie_dstring,
                     dList_length(matching_cookies) > i + 1 ? "; " : "\r\n");
      }
   }

   dList_free(matching_cookies);
   str = cookie_dstring->str;
   dStr_free(cookie_dstring, FALSE);

   if (*str) {
      MSG("%s GETTING: %s", url_host, str);
      cookies_use_counter++;
   }
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

/* -- Dpi parser ----------------------------------------------------------- */

/*
 * Parse a data stream (dpi protocol)
 * Note: Buf is a zero terminated string
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int srv_parse_tok(Dsh *sh, ClientInfo *client, char *Buf)
{
   char *cmd, *cookie, *host, *path;
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

   } else if (strcmp(cmd, "set_cookie") == 0) {
      int st;
      char *date;

      cookie = a_Dpip_get_attr_l(Buf, BufSize, "cookie");
      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");
      date = a_Dpip_get_attr_l(Buf, BufSize, "date");

      st = Cookies_set(cookie, host, path, date);

      dFree(cmd);
      cmd = a_Dpip_build_cmd("cmd=%s msg=%s", "set_cookie_answer",
                             st == 0 ? "ok" : "not set");
      a_Dpip_dsh_write_str(sh, 1, cmd);

      dFree(date);
      dFree(path);
      dFree(host);
      dFree(cookie);
      ret = 2;

   } else if (strcmp(cmd, "get_cookie") == 0) {
      char *scheme = a_Dpip_get_attr_l(Buf, BufSize, "scheme");

      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");

      cookie = Cookies_get(host, path, scheme);
      dFree(scheme);
      dFree(path);
      dFree(host);

      dFree(cmd);
      cmd = a_Dpip_build_cmd("cmd=%s cookie=%s", "get_cookie_answer", cookie);

      if (a_Dpip_dsh_write_str(sh, 1, cmd)) {
          ret = 1;
      } else {
          _MSG("a_Dpip_dsh_write_str: SUCCESS cmd={%s}\n", cmd);
          ret = 2;
      }
      dFree(cookie);
   }
   dFree(cmd);

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
