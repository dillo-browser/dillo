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

/* Handling of cookies takes place here.
 * This implementation aims to follow RFC 2965:
 * http://www.ietf.org/rfc/rfc2965.txt
 */

/*
 * TODO: Cleanup this code. Shorten some functions, order things,
 *       add comments, remove leaks, etc.
 */

/* TODO: this server is not assembling the received packets.
 * This means it currently expects dillo to send full dpi tags
 * within the socket; if that fails, everything stops.
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
   uint_t version;
   char *comment;
   char *comment_url;
   bool_t secure;
   bool_t session_only;
   Dlist *ports;
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

static bool_t disabled;
static FILE *file_stream;
static char *cookies_txt_header_str =
"# HTTP Cookie File\n"
"# http://wp.netscape.com/newsref/std/cookie_spec.html\n"
"# This is a generated file!  Do not edit.\n\n";


/*
 * Forward declarations
 */

static CookieControlAction Cookies_control_check_domain(const char *domain);
static int Cookie_control_init(void);
static void Cookies_parse_ports(int url_port, CookieData_t *cookie,
                                const char *port_str);
static char *Cookies_build_ports_str(CookieData_t *cookie);
static char *Cookies_strip_path(const char *path);
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
                           char *init_str)
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
   dFree(cookie->comment);
   dFree(cookie->comment_url);
   dList_free(cookie->ports);
   dFree(cookie);
}

/*
 * Initialize the cookies module
 * (The 'disabled' variable is writable only within Cookies_init)
 */
static void Cookies_init()
{
   CookieData_t *cookie;
   char *filename, *rc = NULL;
   char line[LINE_MAXLEN];
#ifndef HAVE_LOCKF
   struct flock lck;
#endif
   FILE *old_cookies_file_stream;

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
      MSG("ERROR: Can't open ~/.dillo/cookies.txt, disabling cookies\n");
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
      MSG("The cookies file has a file lock: disabling cookies!\n");
      fclose(file_stream);
      return;
   }

   MSG("Enabling cookies as from cookiesrc...\n");

   cookies = dList_new(32);

   /* Get all lines in the file */
   while (!feof(file_stream)) {
      line[0] = '\0';
      rc = fgets(line, LINE_MAXLEN, file_stream);
      if (!rc && ferror(file_stream)) {
         MSG("Cookies1: Error while reading from cookies.txt: %s\n",
             dStrerror(errno));
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
          * pieces[3] Is the cookie unsecure or secure (TRUE/FALSE)
          * pieces[4] Timestamp of expire date
          * pieces[5] Name of the cookie
          * pieces[6] Value of the cookie
          */
         CookieControlAction action;
         char *piece;
         char *line_marker = line;

         cookie = dNew0(CookieData_t, 1);

         cookie->session_only = FALSE;
         cookie->version = 0;
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

   filename = dStrconcat(dGethomedir(), "/.dillo/cookies", NULL);
   if ((old_cookies_file_stream = fopen(filename, "r")) != NULL) {
      MSG("WARNING: Reading old cookies file ~/.dillo/cookies too\n");

      /* Get all lines in the file */
      while (!feof(old_cookies_file_stream)) {
         line[0] = '\0';
         rc = fgets(line, LINE_MAXLEN, old_cookies_file_stream);
         if (!rc && ferror(old_cookies_file_stream)) {
            MSG("Cookies2: Error while reading from cookies file: %s\n",
                dStrerror(errno));
            break; /* bail out */
         }

         /* Remove leading and trailing whitespaces */
         dStrstrip(line);

         if (line[0] != '\0') {
            /*
             * Split the row into pieces using a tab as the delimiter.
             * pieces[0] The version this cookie was set as (0 / 1)
             * pieces[1] The domain name
             * pieces[2] A comma separated list of accepted ports
             * pieces[3] The path
             * pieces[4] Is the cookie unsecure or secure (0 / 1)
             * pieces[5] Timestamp of expire date
             * pieces[6] Name of the cookie
             * pieces[7] Value of the cookie
             * pieces[8] Comment
             * pieces[9] Comment url
             */
            CookieControlAction action;
            char *piece;
            char *line_marker = line;

            cookie = dNew0(CookieData_t, 1);

            cookie->session_only = FALSE;
            piece = dStrsep(&line_marker, "\t");
            if (piece != NULL)
            cookie->version = strtol(piece, NULL, 10);
            cookie->domain  = dStrdup(dStrsep(&line_marker, "\t"));
            Cookies_parse_ports(0, cookie, dStrsep(&line_marker, "\t"));
            cookie->path = dStrdup(dStrsep(&line_marker, "\t"));
            piece = dStrsep(&line_marker, "\t");
            if (piece != NULL && piece[0] == '1')
               cookie->secure = TRUE;
            piece = dStrsep(&line_marker, "\t");
            if (piece != NULL)
               cookie->expires_at = (time_t) strtol(piece, NULL, 10);
            cookie->name = dStrdup(dStrsep(&line_marker, "\t"));
            cookie->value = dStrdup(dStrsep(&line_marker, "\t"));
            cookie->comment = dStrdup(dStrsep(&line_marker, "\t"));
            cookie->comment_url = dStrdup(dStrsep(&line_marker, "\t"));

            if (!cookie->domain || cookie->domain[0] == '\0' ||
                !cookie->path || cookie->path[0] != '/' ||
                !cookie->name || cookie->name[0] == '\0' ||
                !cookie->value) {
               MSG("Malformed line in cookies file!\n");
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
      fclose(old_cookies_file_stream);
   }
   dFree(filename);
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
            /* char * ports_str = Cookies_build_ports_str(cookie); */
            fprintf(file_stream, "%s\tTRUE\t%s\t%s\t%ld\t%s\t%s\n",
                    cookie->domain,
                    cookie->path,
                    cookie->secure ? "TRUE" : "FALSE",
                    (long)cookie->expires_at,
                    cookie->name,
                    cookie->value);
            /* dFree(ports_str); */
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

static char *months[] =
{ "",
  "Jan", "Feb", "Mar",
  "Apr", "May", "Jun",
  "Jul", "Aug", "Sep",
  "Oct", "Nov", "Dec"
};

/*
 * Take a months name and return a number between 1-12.
 * E.g. 'April' -> 4
 */
static int Cookies_get_month(const char *month_name)
{
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
 * (return 0 on malformed date string syntax)
 */
static time_t Cookies_create_timestamp(const char *expires)
{
   time_t ret;
   int day, month, year, hour, minutes, seconds;
   char *cp;
   char *E_msg =
      "Expire date is malformed!\n"
      " (should be RFC-1123 | RFC-850 | ANSI asctime)\n"
      " Ignoring cookie: ";

   cp = strchr(expires, ',');
   if (!cp && (strlen(expires) == 24 || strlen(expires) == 25)) {
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

   MSG("Expires in %ld seconds, at %s",
       (long)ret - time(NULL), ctime(&ret));

   return ret;
}

/*
 * Parse a string containing a list of port numbers.
 */
static void Cookies_parse_ports(int url_port, CookieData_t *cookie,
                                const char *port_str)
{
   if ((!port_str || !port_str[0]) && url_port != 0) {
      /* There was no list, so only the calling urls port should be allowed. */
      if (!cookie->ports)
         cookie->ports = dList_new(1);
      dList_append(cookie->ports, INT2VOIDP(url_port));
   } else if (port_str[0] == '"' && port_str[1] != '"') {
      char *tok, *str;
      int port;

      str = dStrdup(port_str + 1);
      while ((tok = dStrsep(&str, ","))) {
         port = strtol(tok, NULL, 10);
         if (port > 0) {
            if (!cookie->ports)
               cookie->ports = dList_new(1);
            dList_append(cookie->ports, INT2VOIDP(port));
         }
      }
      dFree(str);
   }
}

/*
 * Build a string of the ports in 'cookie'.
 */
static char *Cookies_build_ports_str(CookieData_t *cookie)
{
   Dstr *dstr;
   char *ret;
   void *data;
   int i;

   dstr = dStr_new("\"");
   for (i = 0; (data = dList_nth_data(cookie->ports, i)); ++i) {
      dStr_sprintfa(dstr, "%d,", VOIDP2INT(data));
   }
   /* Remove any trailing comma */
   if (dstr->len > 1)
      dStr_erase(dstr, dstr->len - 1, 1);
   dStr_append(dstr, "\"");

   ret = dstr->str;
   dStr_free(dstr, FALSE);

   return ret;
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
         Cookies_free_cookie(cookie);
         return;
      }

   }

   /* Don't add an expired cookie. Strictly speaking, max-age cookies should
    * only be discarded when "the age is _greater_ than delta-seconds seconds"
    * (my emphasis), but "A value of zero means the cookie SHOULD be discarded
    * immediately", and there's no compelling reason to distinguish between
    * these cases. */
   if (cookie->expires_at <= time(NULL)) {
      Cookies_free_cookie(cookie);
      return;
   }

   /* add the cookie into the respective domain list */
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
 * Return the attribute that is present at *cookie_str. This function
 * will also attempt to advance cookie_str past any equal-sign.
 */
static char *Cookies_parse_attr(char **cookie_str)
{
   char *str = *cookie_str;
   uint_t i, end = 0;
   bool_t got_attr = FALSE;

   for (i = 0; ; i++) {
      switch (str[i]) {
      case ' ':
      case '\t':
      case '=':
      case ';':
         got_attr = TRUE;
         if (end == 0)
            end = i;
         break;
      case ',':
         *cookie_str = str + i;
         return dStrndup(str, i);
         break;
      case '\0':
         if (!got_attr) {
            end = i;
            got_attr = TRUE;
         }
         /* fall through! */
      default:
         if (got_attr) {
            *cookie_str = str + i;
            return dStrndup(str, end);
         }
         break;
      }
   }

   return NULL;
}

/*
 * Get the value starting at *cookie_str.
 * broken_syntax: watch out for stupid syntax (comma in unquoted string...)
 */
static char *Cookies_parse_value(char **cookie_str,
                                 bool_t broken_syntax,
                                 bool_t keep_quotes)
{
   uint_t i, end;
   char *str = *cookie_str;

   for (i = end = 0; !end; ++i) {
      switch (str[i]) {
      case ' ':
      case '\t':
         if (!broken_syntax && str[0] != '\'' && str[0] != '"') {
            end = 1;
            *cookie_str = str + i + 1;
            while (**cookie_str == ' ' || **cookie_str == '\t')
               *cookie_str += 1;
            if (**cookie_str == ';')
               *cookie_str += 1;
         }
         break;
      case '\'':
      case '"':
         if (i != 0 && str[i] == str[0]) {
            char *tmp = str + i;

            while (*tmp != '\0' && *tmp != ';' && *tmp != ',')
               tmp++;

            *cookie_str = (*tmp == ';') ? tmp + 1 : tmp;

            if (keep_quotes)
               i++;
            end = 1;
         }
         break;
      case '\0':
         *cookie_str = str + i;
         end = 1;
         break;
      case ',':
         if (str[0] != '\'' && str[0] != '"' && !broken_syntax) {
            /* A new cookie starts here! */
            *cookie_str = str + i;
            end = 1;
         }
         break;
      case ';':
         if (str[0] != '\'' && str[0] != '"') {
            *cookie_str = str + i + 1;
            end = 1;
         }
         break;
      default:
         break;
      }
   }
   /* keep i as an index to the last char */
   --i;

   if ((str[0] == '\'' || str[0] == '"') && !keep_quotes) {
      return i > 1 ? dStrndup(str + 1, i - 1) : NULL;
   } else {
      return dStrndup(str, i);
   }
}

/*
 * Parse one cookie...
 */
static CookieData_t *Cookies_parse_one(int url_port, char **cookie_str)
{
   CookieData_t *cookie;
   char *str = *cookie_str;
   char *attr;
   char *value;
   int num_attr = 0;
   bool_t max_age = FALSE;
   bool_t expires = FALSE;
   bool_t discard = FALSE;
   bool_t error = FALSE;

   cookie = dNew0(CookieData_t, 1);

   /* let's arbitrarily choose a year for now */
   cookie->expires_at = time(NULL) + 60 * 60 * 24 * 365;

   /* Iterate until there is nothing left of the string OR we come
    * across a comma representing the start of another cookie */
   while (*str != '\0' && *str != ',') {
      if (error) {
         str++;
         continue;
      }
      /* Skip whitespace */
      while (dIsspace(*str))
         str++;

      /* Get attribute */
      attr = Cookies_parse_attr(&str);
      if (!attr) {
         MSG("Cannot parse cookie attribute!\n");
         error = TRUE;
         continue;
      }

      /* Get the value for the attribute and store it */
      if (num_attr == 0) {
         /* The first attr, which always is the user supplied attr, may
          * have the same name as an ordinary attr. Hence this workaround. */
         cookie->name = dStrdup(attr);
         cookie->value = Cookies_parse_value(&str, FALSE, TRUE);
      } else if (dStrcasecmp(attr, "Path") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);
         cookie->path = value;
      } else if (dStrcasecmp(attr, "Domain") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);
         cookie->domain = value;
      } else if (dStrcasecmp(attr, "Discard") == 0) {
         discard = TRUE;
      } else if (dStrcasecmp(attr, "Max-Age") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);
         if (value) {
            cookie->expires_at = time(NULL) + strtol(value, NULL, 10);
            expires = max_age = TRUE;
            dFree(value);
         } else {
            MSG("Cannot parse cookie Max-Age value!\n");
            dFree(attr);
            error = TRUE;
            continue;
         }
      } else if (dStrcasecmp(attr, "Expires") == 0) {
         if (!max_age) {
            MSG("Old Netscape-style cookie...\n");
            value = Cookies_parse_value(&str, TRUE, FALSE);
            if (value) {
               cookie->expires_at = Cookies_create_timestamp(value);
               expires = TRUE;
               dFree(value);
            } else {
               MSG("Cannot parse cookie Expires value!\n");
               dFree(attr);
               error = TRUE;
               continue;
            }
         } else {
            MSG("Cookie cannot contain Max-Age and Expires.\n");
            dFree(attr);
            error = TRUE;
            continue;
         }
      } else if (dStrcasecmp(attr, "Port") == 0) {
         value = Cookies_parse_value(&str, FALSE, TRUE);
         Cookies_parse_ports(url_port, cookie, value);
         dFree(value);
      } else if (dStrcasecmp(attr, "Comment") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);
         cookie->comment = value;
      } else if (dStrcasecmp(attr, "CommentURL") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);
         cookie->comment_url = value;
      } else if (dStrcasecmp(attr, "Version") == 0) {
         value = Cookies_parse_value(&str, FALSE, FALSE);

         if (value) {
            cookie->version = strtol(value, NULL, 10);
            dFree(value);
         } else {
            MSG("Cannot parse cookie Version value!\n");
            dFree(attr);
            error = TRUE;
            continue;
         }
      } else if (dStrcasecmp(attr, "Secure") == 0) {
         cookie->secure = TRUE;
      } else if (dStrcasecmp(attr, "HttpOnly") == 0) {
         // this case is intentionally left blank, because we do not
         // do client-side scripting (yet).
      } else {
         /* Oops! this can't be good... */
         MSG("Cookie contains unknown attribute: '%s'\n", attr);
         dFree(attr);
         error = TRUE;
         continue;
      }

      dFree(attr);
      num_attr++;
   }

   /*
    * Netscape cookie spec: "expires is an optional attribute. If not
    * specified, the cookie will expire when the user's session ends."
    * rfc 2965: (in the absence of) "Max-Age The default behavior is to
    * discard the cookie when the user agent exits."
    * "The Discard attribute instructs the user agent to discard the
    * cookie unconditionally when the user agent terminates."
    */
   cookie->session_only = discard == TRUE || expires == FALSE;

   *cookie_str = (*str == ',') ? str + 1 : str;

   if (!error && (!cookie->name || !cookie->value)) {
      MSG("Cookie missing name and/or value!\n");
      error = TRUE;
   }
   if (error) {
      Cookies_free_cookie(cookie);
      cookie = NULL;
   }
   return cookie;
}

/*
 * Iterate the cookie string until we catch all cookies.
 * Return Value: a list with all the cookies! (or NULL upon error)
 */
static Dlist *Cookies_parse_string(int url_port, char *cookie_string)
{
   CookieData_t *cookie;
   Dlist *ret = NULL;
   char *str = cookie_string;

   /* The string may contain several cookies separated by comma.
    * We'll iterate until we've caught them all */
   while (*str) {
      cookie = Cookies_parse_one(url_port, &str);

      if (cookie) {
         if (!ret)
            ret = dList_new(4);
         dList_append(ret, cookie);
      } else {
         MSG("Malformed cookie field, ignoring cookie: %s\n", cookie_string);
      }
   }

   return ret;
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
 * Check whether 'prefix' is a prefix of 'path'.
 */
static bool_t Cookies_path_is_prefix(const char *prefix, const char *path)
{
   bool_t ret = TRUE;

   if (!prefix || !path)
      return FALSE;

   /*
    * The original Netscape cookie spec states 'The path "/foo" would match
    * "/foobar" and "/foo/bar.html"', so when the RFCs say "prefix", they
    * apparently really do mean prefix, however utterly bizarre that might be.
    *
    * (On the other hand, http://testsuites.opera.com/cookies/302/302.php
    * takes quite some interest in the prefix as a genuine path.)
    */
   if (strncmp(prefix, path, strlen(prefix)))
      ret = FALSE;

   return ret;
}

/*
 * Check whether host name A domain-matches host name B.
 */
static bool_t Cookies_domain_matches(char *A, char *B)
{
   int diff;

   if (!A || !*A || !B || !*B)
      return FALSE;

   /* "Host A's name domain-matches host B's if their host name strings
    * string-compare equal; or"...
    */
   if (!dStrcasecmp(A, B))
      return TRUE;

   /* ..."A is a HDN [host domain name] string and has the form NB, where N
    * is a non-empty name string, B has the form .B', and B' is a HDN string.
    */

   if (*B != '.') {
      return FALSE;
   }

   diff = strlen(A) - strlen(B);

   if (diff > 0) {
      return (dStrcasecmp(A + diff, B) == 0);
   } else {
      /* Consider A to domain-match B if B is of the form .A
       * CONTRARY TO RFC
       */
      return (dStrcasecmp(A, B + 1) == 0);
   }
}

/*
 * Validate cookies domain against some security checks.
 */
static bool_t Cookies_validate_domain(CookieData_t *cookie, char *host,
                                      char *url_path)
{
   int dots, diff, i;
   bool_t is_ip;

   /* Make sure that the path is set to something */
   if (!cookie->path || cookie->path[0] != '/') {
      dFree(cookie->path);
      cookie->path = Cookies_strip_path(url_path);
   }

   if (!Cookies_path_is_prefix(cookie->path, url_path))
      return FALSE;

   /* If the server never set a domain, or set one without a leading
    * dot (which isn't allowed), we use the calling URL's hostname. */
   if (cookie->domain == NULL || cookie->domain[0] != '.') {
      if (cookie->domain) {
         /* It may be necessary to handle these old-style domains. */
         MSG("Ignoring cookie domain \'%s\' without leading dot.\n",
             cookie->domain);
      }
      dFree(cookie->domain);
      cookie->domain = dStrdup(host);
      return TRUE;
   }

   if (!Cookies_domain_matches(host, cookie->domain))
      return FALSE;

   /* Count the number of dots and also find out if it is an IP-address */
   is_ip = TRUE;
   for (i = 0, dots = 0; cookie->domain[i] != '\0'; i++) {
      if (cookie->domain[i] == '.')
         dots++;
      else if (!isdigit(cookie->domain[i]))
         is_ip = FALSE;
   }

   if (i > 0 && cookie->domain[i - 1] == '.') {
       /* A trailing dot is a sneaky trick, but we won't fall for it. */
      dots--;
   }

   /* A valid domain must have at least two dots in it
    * NOTE: this breaks cookies on localhost...
    *
    * TODO: accept the publicsuffix.org list as an optional external file.
    */
   if (dots < 2) {
      return FALSE;
   }

   /* Reject a cookie if the "request-host is a FQDN [fully-qualified domain
    * name] (not IP address) and has the form HD, where D is the value of the
    * Domain attribute, and H is a string that contains one or more dots.
    */
   diff = strlen(host) - i;
   if (diff > 0) {
      if (dStrcasecmp(host + diff, cookie->domain))
         return FALSE;

      if (!is_ip) {
         /* "x.y.test.com" is not allowed to set cookies for ".test.com";
          *  only an url of the form "y.test.com" would be. */
         while ( diff-- )
            if (host[diff] == '.')
               return FALSE;
      }
   }

   return TRUE;
}

/*
 * For default path attributes, RFC 2109 gives: "Defaults to the path of the
 * request URL that generated the Set-Cookie response, up to, but not
 * including, the right-most /".
 *
 * (RFC 2965 keeps the '/', but is not so widely followed.)
 */
static char *Cookies_strip_path(const char *path)
{
   char *ret;
   uint_t len;

   if (path) {
      len = strlen(path);

      while (len && path[len] != '/')
         len--;
      ret = dStrndup(path, len ? len : 1);
   } else {
      ret = dStrdup("/");
   }

   return ret;
}

/*
 * Set the value corresponding to the cookie string
 */
static void Cookies_set(char *cookie_string, char *url_host,
                        char *url_path, int url_port)
{
   CookieControlAction action;
   CookieData_t *cookie;
   Dlist *list;
   int i;

   if (disabled)
      return;

   action = Cookies_control_check_domain(url_host);
   if (action == COOKIE_DENY) {
      MSG("denied SET for %s\n", url_host);
      return;
   }

   _MSG("%s setting: %s\n", url_host, cookie_string);

   if ((list = Cookies_parse_string(url_port, cookie_string))) {
      for (i = 0; (cookie = dList_nth_data(list, i)); ++i) {
         if (Cookies_validate_domain(cookie, url_host, url_path)) {
            if (action == COOKIE_ACCEPT_SESSION)
               cookie->session_only = TRUE;
            Cookies_add_cookie(cookie);
         } else {
            MSG("Rejecting cookie for %s from host %s path %s\n",
                cookie->domain, url_host, url_path);
            Cookies_free_cookie(cookie);
         }
      }
      dList_free(list);
   }
}

/*
 * Compare the cookie with the supplied data to see if it matches
 */
static bool_t Cookies_match(CookieData_t *cookie, int port,
                              const char *path, bool_t is_ssl)
{
   void *data;
   int i;

   /* Insecure cookies matches both secure and insecure urls, secure
      cookies matches only secure urls */
   if (cookie->secure && !is_ssl)
      return FALSE;

   /* Check that the cookie path is a prefix of the current path */
   if (!Cookies_path_is_prefix(cookie->path, path))
      return FALSE;

   /* Check if the port of the request URL matches any
    * of those set in the cookie */
   if (cookie->ports) {
      for (i = 0; (data = dList_nth_data(cookie->ports, i)); ++i) {
         if (VOIDP2INT(data) == port)
            return TRUE;
      }
      return FALSE;
   }

   /* It's a match */
   return TRUE;
}

/*
 * Return a string that contains all relevant cookies as headers.
 */
static char *Cookies_get(char *url_host, char *url_path,
                         char *url_scheme, int url_port)
{
   char *domain_str, *q, *str;
   CookieData_t *cookie;
   Dlist *matching_cookies;
   CookieNode *node;
   Dlist *domain_cookies;
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

      node = dList_find_sorted(cookies, domain_str, Cookie_node_by_domain_cmp);
      domain_cookies = (node) ? node->dlist : NULL;

      for (i = 0; (cookie = dList_nth_data(domain_cookies, i)); ++i) {
         /* Remove expired cookie. */
         if (cookie->expires_at < time(NULL)) {
            Cookies_remove_cookie(cookie);
            --i; continue;
         }
         /* Check if the cookie matches the requesting URL */
         if (Cookies_match(cookie, url_port, url_path, is_ssl)) {
            int j;
            CookieData_t *curr;
            uint_t path_length = strlen(cookie->path);

            /* "If multiple cookies satisfy the criteria [to be sent in a
             * query], they are ordered in the Cookie header such that those
             * with more specific Path attributes precede those with less
             * specific."
             */
            for (j = 0;
                 (curr = dList_nth_data(matching_cookies, j)) &&
                  strlen(curr->path) >= path_length;
                 j++) ;
            dList_insert_pos(matching_cookies, cookie, j);
         }
      }
   }

   /* Found the cookies, now make the string */
   cookie_dstring = dStr_new("");
   if (dList_length(matching_cookies) > 0) {
      CookieData_t *first_cookie = dList_nth_data(matching_cookies, 0);

      dStr_sprintfa(cookie_dstring, "Cookie: ");

      if (first_cookie->version != 0)
         dStr_sprintfa(cookie_dstring, "$Version=\"%d\"; ",
                       first_cookie->version);


      for (i = 0; (cookie = dList_nth_data(matching_cookies, i)); ++i) {
         q = (cookie->version == 0 ? "" : "\"");
         dStr_sprintfa(cookie_dstring,
                       "%s=%s; $Path=%s%s%s; $Domain=%s%s%s",
                       cookie->name, cookie->value,
                       q, cookie->path, q, q, cookie->domain, q);
         if (cookie->ports) {
            char *ports_str = Cookies_build_ports_str(cookie);
            dStr_sprintfa(cookie_dstring, "; $Port=%s", ports_str);
            dFree(ports_str);
         }

         dStr_append(cookie_dstring,
                     dList_length(matching_cookies) > i + 1 ? "; " : "\r\n");
      }
   }

   dList_free(matching_cookies);
   str = cookie_dstring->str;
   dStr_free(cookie_dstring, FALSE);
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
         MSG("Cookies3: Error while reading rule from cookiesrc: %s\n",
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
   char *p, *cmd, *cookie, *host, *path, *scheme;
   int port, ret = 1;
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
      dFree(cmd);
      cookie = a_Dpip_get_attr_l(Buf, BufSize, "cookie");
      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");
      p = a_Dpip_get_attr_l(Buf, BufSize, "port");
      port = strtol(p, NULL, 10);
      dFree(p);

      Cookies_set(cookie, host, path, port);

      dFree(path);
      dFree(host);
      dFree(cookie);
      ret = 2;

   } else if (cmd && strcmp(cmd, "get_cookie") == 0) {
      dFree(cmd);
      scheme = a_Dpip_get_attr_l(Buf, BufSize, "scheme");
      host = a_Dpip_get_attr_l(Buf, BufSize, "host");
      path = a_Dpip_get_attr_l(Buf, BufSize, "path");
      p = a_Dpip_get_attr_l(Buf, BufSize, "port");
      port = strtol(p, NULL, 10);
      dFree(p);

      cookie = Cookies_get(host, path, scheme, port);
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
