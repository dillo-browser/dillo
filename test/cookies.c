/*
 * Dillo cookies test
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This has a big blob of the current src/IO/dpi.c in it.
 * I hope there's a better way.
 */

#include <stdlib.h> /* malloc, etc. */
#include <unistd.h> /* read, etc. */
#include <stdio.h>
#include <stdarg.h>  /* va_list */
#include <string.h> /* strchr */
#include <errno.h>
#include <ctype.h>
#include <time.h>
/* net */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define _MSG(...)

#define MSG_INNARDS(prefix, ...)                   \
   D_STMT_START {                                  \
      printf(prefix __VA_ARGS__);               \
      fflush (stdout);                          \
   } D_STMT_END

#define MSG(...) MSG_INNARDS("", __VA_ARGS__)
#define MSG_WARN(...) MSG_INNARDS("** WARNING **: ", __VA_ARGS__)
#define MSG_ERR(...) MSG_INNARDS("** ERROR **: ", __VA_ARGS__)


#include "../dlib/dlib.h"
#include "../dpip/dpip.h"

static uint_t failed = 0;
static uint_t passed = 0;

static char SharedKey[32];

/*
 * Read all the available data from a filedescriptor.
 * This is intended for short answers, i.e. when we know the server
 * will write it all before being preempted. For answers that may come
 * as an stream with delays, non-blocking is better.
 * Return value: read data, or NULL on error and no data.
 */
static char *Dpi_blocking_read(int fd)
{
   int st;
   const int buf_sz = 8*1024;
   char buf[buf_sz], *msg = NULL;
   Dstr *dstr = dStr_sized_new(buf_sz);

   do {
      st = read(fd, buf, buf_sz);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            MSG_ERR("[Dpi_blocking_read] %s\n", dStrerror(errno));
            break;
         }
      } else if (st > 0) {
         dStr_append_l(dstr, buf, st);
      }
   } while (st == buf_sz);

   msg = (dstr->len > 0) ? dstr->str : NULL;
   dStr_free(dstr, (dstr->len > 0) ? FALSE : TRUE);
   return msg;
}

static void Dpi_close_fd(int fd)
{
   int st;

   dReturn_if (fd < 0);
   do
      st = close(fd);
   while (st < 0 && errno == EINTR);
}

static int Dpi_make_socket_fd()
{
   int fd, ret = -1;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
      ret = fd;
   }
   return ret;
}

/*
 * Read dpid's communication keys from its saved file.
 * Return value: 1 on success, -1 on error.
 */
static int Dpi_read_comm_keys(int *port)
{
   FILE *In;
   char *fname, *rcline = NULL, *tail;
   int i, ret = -1;

   fname = dStrconcat(dGethomedir(), "/.dillo/dpid_comm_keys", NULL);
   if ((In = fopen(fname, "r")) == NULL) {
      MSG_ERR("[Dpi_read_comm_keys] %s\n", dStrerror(errno));
   } else if ((rcline = dGetline(In)) == NULL) {
      MSG_ERR("[Dpi_read_comm_keys] empty file: %s\n", fname);
   } else {
      *port = strtol(rcline, &tail, 10);
      for (i = 0; *tail && isxdigit(tail[i+1]); ++i)
         SharedKey[i] = tail[i+1];
      SharedKey[i] = 0;
      ret = 1;
   }
   if (In)
      fclose(In);
   dFree(rcline);
   dFree(fname);

   return ret;
}

static int Dpi_check_dpid_ids()
{
   struct sockaddr_in sin;
   const socklen_t sin_sz = sizeof(sin);
   int sock_fd, dpid_port, ret = -1;

   /* socket connection test */
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   if (Dpi_read_comm_keys(&dpid_port) != -1) {
      sin.sin_port = htons(dpid_port);
      if ((sock_fd = Dpi_make_socket_fd()) == -1) {
         MSG("Dpi_check_dpid_ids: sock_fd=%d %s\n", sock_fd, dStrerror(errno));
      } else if (connect(sock_fd, (struct sockaddr *)&sin, sin_sz) == -1) {
         MSG("Dpi_check_dpid_ids: %s\n", dStrerror(errno));
      } else {
         Dpi_close_fd(sock_fd);
         ret = 1;
      }
   }
   return ret;
}

static int Dpi_blocking_write(int fd, const char *msg, int msg_len)
{
   int st, sent = 0;

   while (sent < msg_len) {
      st = write(fd, msg + sent, msg_len - sent);
      if (st < 0) {
         if (errno == EINTR) {
            continue;
         } else {
            MSG_ERR("[Dpi_blocking_write] %s\n", dStrerror(errno));
            break;
         }
      }
      sent += st;
   }

   return (sent == msg_len) ? 1 : -1;
}

/*
 * Start dpid.
 * Return: 0 starting now, 1 Error.
 */
static int Dpi_start_dpid(void)
{
   pid_t pid;
   int st_pipe[2], ret = 1;
   char *answer;

   /* create a pipe to track our child's status */
   if (pipe(st_pipe))
      return 1;

   pid = fork();
   if (pid == 0) {
      /* This is the child process.  Execute the command. */
      char *path1 = dStrconcat(dGethomedir(), "/.dillo/dpid", NULL);
      Dpi_close_fd(st_pipe[0]);
      if (execl(path1, "dpid", (char*)NULL) == -1) {
         dFree(path1);
         if (execlp("dpid", "dpid", (char*)NULL) == -1) {
            MSG("Dpi_start_dpid (child): %s\n", dStrerror(errno));
            if (Dpi_blocking_write(st_pipe[1], "ERROR", 5) == -1) {
               MSG("Dpi_start_dpid (child): can't write to pipe.\n");
            }
            Dpi_close_fd(st_pipe[1]);
            _exit (EXIT_FAILURE);
         }
      }
   } else if (pid < 0) {
      /* The fork failed.  Report failure.  */
      MSG("Dpi_start_dpid: %s\n", dStrerror(errno));
      /* close the unused pipe */
      Dpi_close_fd(st_pipe[0]);
      Dpi_close_fd(st_pipe[1]);

   } else {
      /* This is the parent process, check our child status... */
      Dpi_close_fd(st_pipe[1]);
      if ((answer = Dpi_blocking_read(st_pipe[0])) != NULL) {
         MSG("Dpi_start_dpid: can't start dpid\n");
         dFree(answer);
      } else {
         ret = 0;
      }
      Dpi_close_fd(st_pipe[0]);
   }

   return ret;
}

/*
 * Confirm that the dpid is running. If not, start it.
 * Return: 0 running OK, 1 starting (EAGAIN), 2 Error.
 */
static int Dpi_check_dpid(int num_tries)
{
   static int starting = 0;
   int check_st = 1, ret = 2;

   check_st = Dpi_check_dpid_ids();
   _MSG("Dpi_check_dpid: check_st=%d\n", check_st);

   if (check_st == 1) {
      /* connection test with dpi server passed */
      starting = 0;
      ret = 0;
   } else {
      if (!starting) {
         /* start dpid */
         if (Dpi_start_dpid() == 0) {
            starting = 1;
            ret = 1;
         }
      } else if (++starting < num_tries) {
         /* starting */
         ret = 1;
      } else {
         /* we waited too much, report an error... */
         starting = 0;
      }
   }

   _MSG("Dpi_check_dpid:: %s\n",
        (ret == 0) ? "OK" : (ret == 1 ? "EAGAIN" : "ERROR"));
   return ret;
}


static int Dpi_blocking_start_dpid(void)
{
   int cst, try = 0,
       n_tries = 12; /* 3 seconds */

   /* test the dpid, and wait a bit for it to start if necessary */
   while ((cst = Dpi_check_dpid(n_tries)) == 1) {
      MSG("Dpi_blocking_start_dpid: try %d\n", ++try);
      usleep(250000); /* 1/4 sec */
   }
   return cst;
}


/*
 * Return the dpi server's port number, or -1 on error.
 * (A query is sent to dpid and then its answer parsed)
 * note: as the available servers and/or the dpi socket directory can
 *       change at any time, we'll ask each time. If someday we find
 *       that connecting each time significantly degrades performance,
 *       an optimized approach can be tried.
 */
static int Dpi_get_server_port(const char *server_name)
{
   int sock_fd = -1, dpi_port = -1;
   int dpid_port, ok = 0;
   struct sockaddr_in sin;
   char *cmd, *request, *rply = NULL, *port_str;
   socklen_t sin_sz;

   dReturn_val_if_fail (server_name != NULL, dpi_port);
   _MSG("Dpi_get_server_port:: server_name = [%s]\n", server_name);

   /* Read dpid's port from saved file */
   if (Dpi_read_comm_keys(&dpid_port) != -1) {
      ok = 1;
   }
   if (ok) {
      /* Connect a socket with dpid */
      ok = 0;
      sin_sz = sizeof(sin);
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      sin.sin_port = htons(dpid_port);
      if ((sock_fd = Dpi_make_socket_fd()) == -1 ||
          connect(sock_fd, (struct sockaddr *)&sin, sin_sz) == -1) {
         MSG("Dpi_get_server_port: %s\n", dStrerror(errno));
      } else {
         ok = 1;
      }
   }
   if (ok) {
      /* ask dpid to check the dpi and send its port number back */
      ok = 0;
      request = a_Dpip_build_cmd("cmd=%s msg=%s", "check_server", server_name);
      _MSG("[%s]\n", request);

      if (Dpi_blocking_write(sock_fd, request, strlen(request)) == -1) {
         MSG("Dpi_get_server_port: %s\n", dStrerror(errno));
      } else {
         ok = 1;
      }
      dFree(request);
   }
   if (ok) {
      /* Get the reply */
      ok = 0;
      if ((rply = Dpi_blocking_read(sock_fd)) == NULL) {
         MSG("Dpi_get_server_port: can't read server port from dpid.\n");
      } else {
         ok = 1;
      }
   }
   if (ok) {
      /* Parse reply */
      ok = 0;
      cmd = a_Dpip_get_attr(rply, "cmd");
      if (strcmp(cmd, "send_data") == 0) {
         port_str = a_Dpip_get_attr(rply, "msg");
         _MSG("Dpi_get_server_port: rply=%s\n", rply);
         _MSG("Dpi_get_server_port: port_str=%s\n", port_str);
         dpi_port = strtol(port_str, NULL, 10);
         dFree(port_str);
         ok = 1;
      }
      dFree(cmd);
   }
   dFree(rply);
   Dpi_close_fd(sock_fd);

   return ok ? dpi_port : -1;
}


static int Dpi_connect_socket(const char *server_name)
{
   struct sockaddr_in sin;
   int sock_fd, dpi_port, ret = -1;
   char *cmd = NULL;

   /* Query dpid for the port number for this server */
   if ((dpi_port = Dpi_get_server_port(server_name)) == -1) {
      _MSG("Dpi_connect_socket:: can't get port number for %s\n", server_name);
      return -1;
   }
   _MSG("Dpi_connect_socket: server=%s port=%d\n", server_name, dpi_port);

   /* connect with this server's socket */
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sin.sin_port = htons(dpi_port);

   if ((sock_fd = Dpi_make_socket_fd()) == -1) {
      perror("[dpi::socket]");
   } else if (connect(sock_fd, (void*)&sin, sizeof(sin)) == -1) {
      MSG("[dpi::connect] errno:%d %s\n", errno, dStrerror(errno));

   /* send authentication Key (the server closes sock_fd on auth error) */
   } else if (!(cmd = a_Dpip_build_cmd("cmd=%s msg=%s", "auth", SharedKey))) {
      MSG_ERR("[Dpi_connect_socket] Can't make auth message.\n");
   } else if (Dpi_blocking_write(sock_fd, cmd, strlen(cmd)) == -1) {
      MSG_ERR("[Dpi_connect_socket] Can't send auth message.\n");
   } else {
      ret = sock_fd;
   }
   dFree(cmd);
   if (sock_fd != -1 && ret == -1) /* can't send cmd? */
      Dpi_close_fd(sock_fd);

   return ret;
}


char *a_Dpi_send_blocking_cmd(const char *server_name, const char *cmd)
{
   int cst, sock_fd;
   char *ret = NULL;

   /* test the dpid, and wait a bit for it to start if necessary */
   if ((cst = Dpi_blocking_start_dpid()) != 0) {
      return ret;
   }

   if ((sock_fd = Dpi_connect_socket(server_name)) == -1) {
      MSG_ERR("[a_Dpi_send_blocking_cmd] Can't connect to server.\n");
   } else if (Dpi_blocking_write(sock_fd, cmd, strlen(cmd)) == -1) {
      MSG_ERR("[a_Dpi_send_blocking_cmd] Can't send message.\n");
   } if ((ret = Dpi_blocking_read(sock_fd)) == NULL) {
      MSG_ERR("[a_Dpi_send_blocking_cmd] Can't read message.\n");
   }
   Dpi_close_fd(sock_fd);

   return ret;
}



void a_Cookies_set(const char *cookie, const char *host, const char *path,
                   const char *date)
{
   char *cmd, *dpip_tag;

   if (date)
      cmd = a_Dpip_build_cmd("cmd=%s cookie=%s host=%s path=%s date=%s",
                             "set_cookie", cookie,
                             host, path, date);
   else
      cmd = a_Dpip_build_cmd("cmd=%s cookie=%s host=%s path=%s",
                             "set_cookie", cookie,
                             host, path);

   dpip_tag = a_Dpi_send_blocking_cmd("cookies", cmd);
   _MSG("a_Cookies_set: dpip_tag = {%s}\n", dpip_tag);
   dFree(dpip_tag);
   dFree(cmd);
}


char *a_Cookies_get_query(const char *scheme, const char *host,
                          const char *path)
{
   char *cmd, *dpip_tag, *query;

   cmd = a_Dpip_build_cmd("cmd=%s scheme=%s host=%s path=%s",
                          "get_cookie", scheme,
                          host, path);

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

static void expect(int lineno, const char *exp_reply,
                      const char *scheme, const char *host, const char *path)
{
   char *reply = a_Cookies_get_query(scheme, host, path);

   if (strcmp(reply, exp_reply)) {
      MSG("line %d: EXPECTED: %s GOT: %s\n", lineno, exp_reply, reply);
      failed++;
   } else {
      passed++;
   }
}

static void toomany()
{
   a_Cookies_set("1=1", "toomany.com", "/", NULL);
   a_Cookies_set("2=1", "toomany.com", "/", NULL);
   a_Cookies_set("3=1", "toomany.com", "/", NULL);
   a_Cookies_set("4=1", "toomany.com", "/", NULL);
   a_Cookies_set("5=1", "toomany.com", "/", NULL);
   a_Cookies_set("6=1", "toomany.com", "/", NULL);
   a_Cookies_set("7=1", "toomany.com", "/path/", NULL);
   a_Cookies_set("8=1", "toomany.com", "/", NULL);
   a_Cookies_set("9=1", "toomany.com", "/", NULL);
   a_Cookies_set("10=1", "toomany.com", "/", NULL);
   a_Cookies_set("11=1", "toomany.com", "/", NULL);
   a_Cookies_set("12=1", "toomany.com", "/", NULL);
   a_Cookies_set("13=1", "toomany.com", "/", NULL);
   a_Cookies_set("14=1", "toomany.com", "/", NULL);
   a_Cookies_set("15=1", "toomany.com", "/", NULL);
   a_Cookies_set("16=1", "toomany.com", "/", NULL);
   a_Cookies_set("17=1", "toomany.com", "/", NULL);
   a_Cookies_set("18=1", "toomany.com", "/", NULL);
   a_Cookies_set("19=1", "toomany.com", "/", NULL);
   a_Cookies_set("20=1", "toomany.com", "/", NULL);
   a_Cookies_set("21=1", "toomany.com", "/", NULL);
   /* 1 was oldest and discarded */
   expect(__LINE__, "Cookie: 7=1; 2=1; 3=1; 4=1; 5=1; 6=1; 8=1; 9=1; 10=1; "
                    "11=1; 12=1; 13=1; 14=1; 15=1; 16=1; 17=1; 18=1; 19=1; "
                    "20=1; 21=1\r\n", "http", "toomany.com", "/path/");
   sleep(1);
   /* touch all of them except #7 (path matching) */
   expect(__LINE__, "Cookie: 2=1; 3=1; 4=1; 5=1; 6=1; 8=1; 9=1; 10=1; "
                    "11=1; 12=1; 13=1; 14=1; 15=1; 16=1; 17=1; 18=1; 19=1; "
                    "20=1; 21=1\r\n", "http", "toomany.com", "/");
   a_Cookies_set("22=1", "toomany.com", "/", NULL);
   /* 7 was oldest and discarded */
   expect(__LINE__, "Cookie: 2=1; 3=1; 4=1; 5=1; 6=1; 8=1; 9=1; 10=1; "
                    "11=1; 12=1; 13=1; 14=1; 15=1; 16=1; 17=1; 18=1; 19=1; "
                    "20=1; 21=1; 22=1\r\n", "http", "toomany.com", "/path/");
}

static void maxage()
{
   time_t t = time(NULL)+1000;
   char *server_date = dStrdup(ctime(&t));

   a_Cookies_set("name=val; max-age=0", "maxage0.com", "/", NULL);
   expect(__LINE__, "", "http", "maxage0.com", "/");

   a_Cookies_set("name=val; max-age=100", "maxage100.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "maxage100.com", "/");

   a_Cookies_set("name=val; max-age=-100", "maxage-100.com", "/", NULL);
   expect(__LINE__, "", "http", "maxage-100.com", "/");

   a_Cookies_set("name=val; max-age=2000000000", "maxage-huge.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "maxage-huge.com", "/");
   /* just having a server date shouldn't matter */

   a_Cookies_set("name=val; max-age=0", "maxage0s.com", "/", server_date);
   expect(__LINE__, "", "http", "maxage0s.com", "/");

   a_Cookies_set("name=val; max-age=100", "maxage100s.com", "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "maxage100s.com", "/");

   a_Cookies_set("name=val; max-age=-100", "maxage-100s.com", "/",server_date);
   expect(__LINE__, "", "http", "maxage-100s.com", "/");

   /* MAX-AGE and EXPIRES */
   a_Cookies_set("name=val; max-age=90; expires=Wed Jan 20 01:26:32 2010",
                 "maxagelater.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "maxagelater.com", "/");

   a_Cookies_set("name=val; max-age=90; expires=Wed Jan 20 01:26:32 2010",
                 "maxagelaters.com", "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "maxagelaters.com", "/");

   dFree(server_date);
}

static void expires_server_ahead()
{
   char *string;
   time_t t = time(NULL)+1000;
   char *server_date = dStrdup(ctime(&t));
   time_t expt = t + 1000;
   char *exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e2000s1000.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e2000s1000.com", "/");

   a_Cookies_set(string, "e2000s1000s.com", "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e2000s1000s.com", "/");

   expt = t - 500; /* past for the server, future for us */
   dFree(exp_date);
   exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e500s1000.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e500s1000.com", "/");

   a_Cookies_set(string, "e500s1000s.com", "/", server_date);
   expect(__LINE__, "", "http", "e500s1000s.com", "/");

   expt = t; /* expire at future-for-us server date */
   dFree(exp_date);
   exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e1000s1000.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e1000s1000.com", "/");

   a_Cookies_set(string, "e1000s1000s.com", "/", server_date);
   expect(__LINE__, "", "http", "e1000s1000s.com", "/");

   expt = time(NULL); /* now */
   dFree(exp_date);
   exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e0s1000.com", "/", NULL);
   expect(__LINE__, "", "http", "e0s1000.com", "/");

   a_Cookies_set(string, "e0s1000s.com", "/", server_date);
   expect(__LINE__, "", "http", "e0s1000s.com", "/");

   dFree(exp_date);
   dFree(server_date);
}

static void expires_server_behind()
{
   char *string;
   time_t t = time(NULL)-1000;
   char *server_date = dStrdup(ctime(&t));

   time_t expt = t + 1000;
   char *exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e0s-1000.com", "/", NULL);
   expect(__LINE__, "", "http", "e0s-1000.com", "/");

   a_Cookies_set(string, "e0s-1000s.com", "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e0s-1000s.com","/");

   expt = t + 500; /* future for the server, past for us */
   dFree(exp_date);
   exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e-500s-1000.com", "/", NULL);
   expect(__LINE__, "", "http", "e-500s-1000.com", "/");

   a_Cookies_set(string, "e-500s-1000s.com", "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "e-500s-1000s.com", "/");

   expt = t; /* expire at past-for-us server date */
   dFree(exp_date);
   exp_date = dStrdup(ctime(&expt));

   string = dStrconcat("name=val; expires=", exp_date, NULL);
   a_Cookies_set(string, "e-1000s-1000.com", "/", NULL);
   expect(__LINE__, "", "http", "e-1000s-1000.com", "/");

   a_Cookies_set(string, "e-1000s-1000s.com", "/", server_date);
   expect(__LINE__, "", "http", "e-1000s-1000s.com", "/");

   dFree(server_date);
   dFree(exp_date);
}

static void expires_extremes()
{
   time_t t;
   char *server_date;

   a_Cookies_set("name=val; expires=Fri Dec 13 20:45:52 1901", "expmin.com",
                 "/", NULL);
   expect(__LINE__, "", "http", "expmin.com", "/");

   a_Cookies_set("name=val; expires=Wed Dec 31 23:59:59 1969", "expneg.com",
                 "/", NULL);
   expect(__LINE__, "", "http", "expneg.com", "/");

   a_Cookies_set("name=val; expires=Thu Jan  1 00:00:00 1970", "expepoch.com",
                 "/", NULL);
   expect(__LINE__, "", "http", "expepoch.com", "/");

   /* TODO: revisit these tests in a few decades */
   a_Cookies_set("name=val; expires=Tue Jan 19 03:14:07 2038", "expmax.com",
                 "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "expmax.com", "/");

   a_Cookies_set("name=val; expires=Sun Jan  1 00:00:00 2040", "pastmax.com",
                 "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "pastmax.com", "/");

   t = time(NULL)+1000;
   server_date = dStrdup(ctime(&t));

   a_Cookies_set("name=val; expires=Fri Dec 13 20:45:52 1901", "expmina.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expmina.com", "/");

   a_Cookies_set("name=val; expires=Wed Dec 31 23:59:59 1969", "expnega.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expnega.com", "/");

   a_Cookies_set("name=val; expires=Thu Jan  1 00:00:00 1970", "expepocha.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expepocha.com", "/");

   a_Cookies_set("name=val; expires=Tue Jan 19 03:14:07 2038", "expmaxa.com",
                 "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "expmaxa.com", "/");

   a_Cookies_set("name=val; expires=Sun Jan  1 00:00:00 2040", "pastmaxa.com",
                 "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "pastmaxa.com", "/");

   t = time(NULL)-1000;
   dFree(server_date);
   server_date = dStrdup(ctime(&t));

   a_Cookies_set("name=val; expires=Fri Dec 13 20:45:52 1901", "expminb.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expminb.com", "/");

   a_Cookies_set("name=val; expires=Wed Dec 31 23:59:59 1969", "expnegb.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expnegb.com", "/");

   a_Cookies_set("name=val; expires=Thu Jan  1 00:00:00 1970", "expepochb.com",
                 "/", server_date);
   expect(__LINE__, "", "http", "expepochb.com", "/");

   a_Cookies_set("name=val; expires=Tue Jan 19 03:14:07 2038", "expmaxb.com",
                 "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "expmaxb.com", "/");

   a_Cookies_set("name=val; expires=Sun Jan  1 00:00:00 2040", "pastmaxb.com",
                 "/", server_date);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "pastmaxb.com", "/");

   dFree(server_date);
}

static void path()
{
   a_Cookies_set("name=val; path=/", "p1.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p1.com", "/");

   a_Cookies_set("name=val; path=/dir1", "p2.com", "/dir2", NULL);
   expect(__LINE__, "", "http", "p2.com", "/");
   expect(__LINE__, "", "http", "p2.com", "/d");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p2.com", "/dir1");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p2.com", "/dir1/");
   expect(__LINE__, "", "http", "p2.com", "/dir2");
   expect(__LINE__, "", "http", "p2.com", "/dir11");

   a_Cookies_set("name=val; path=dir1", "p3.com", "/dir2", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p3.com", "/");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p3.com", "/dir1");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p3.com", "/dir2");

   a_Cookies_set("name=val; path=/dir1/", "p4.com", "/dir2", NULL);
   expect(__LINE__, "", "http", "p4.com", "/");
   /* this next one strikes me as a bit odd, personally, but I suppose it's not
    * a big deal */
   expect(__LINE__, "", "http", "p4.com", "/dir1");
   expect(__LINE__, "", "http", "p4.com", "/dir11");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p4.com", "/dir1/");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p4.com", "/dir1/sub");

   a_Cookies_set("name=val", "p5.com", "/dir/subdir", NULL);
   expect(__LINE__, "", "http", "p5.com", "/");
   expect(__LINE__, "", "http", "p5.com", "/bir");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p5.com", "/dir");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p5.com", "/dir/");

   a_Cookies_set("name=val", "p6.com", "/dir/subdir/", NULL);
   expect(__LINE__, "", "http", "p6.com", "/dir/");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p6.com", "/dir/subdir");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "p6.com", "/dir/subdir/s");
}

int Cookies_rc_check()
{
   const int line_maxlen = 4096;
   FILE *stream;
   char *filename;
   char line[line_maxlen];
   bool_t default_deny = TRUE;

   /* Get a file pointer */
   filename = dStrconcat(dGethomedir(), "/.dillo/cookiesrc", NULL);
   stream = fopen(filename, "r");
   dFree(filename);

   if (!stream) {
      MSG_ERR("Cannot run test; cannot open cookiesrc.\n");
      return 1;
   }

   /* Get all lines in the file */
   while (!feof(stream)) {
      char *rc;

      line[0] = '\0';
      rc = fgets(line, line_maxlen, stream);
      if (!rc && ferror(stream)) {
         MSG_ERR("Error while reading rule from cookiesrc: %s\n",
             dStrerror(errno));
         fclose(stream);
         return 2;
      }

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if (line[0] != '\0' && line[0] != '#') {
         int domain_end, i = 0;
         const char *rule;

         /* Get the domain */
         while (line[i] != '\0' && !dIsspace(line[i]))
            i++;
         domain_end = i;

         /* Skip past whitespace */
         while (dIsspace(line[i]))
            i++;
         line[domain_end] = '\0';

         /* Get the rule */
         rule = line + i;
         while (line[i] != '\0' && !dIsspace(line[i]))
            i++;
         line[i] = '\0';

         if (!dStrAsciiCasecmp(line, "DEFAULT")) {
            if (!dStrAsciiCasecmp(rule, "ACCEPT") ||
                !dStrAsciiCasecmp(rule, "ACCEPT_SESSION"))
               default_deny = FALSE;
         } else {
            if (!dStrAsciiCasecmp(rule, "DENY"))
               MSG_WARN("DENY rules in cookiesrc can interfere with test.\n");
         }
      }
   }
   fclose(stream);

   if (default_deny) {
      MSG_ERR("Cannot run test with cookiesrc default of deny.\n");
      return 1;
   } else {
      return 0;
   }
}

int main()
{
   if (Cookies_rc_check()) {
      MSG("If you change cookiesrc, remember to stop the DPIs via dpidc.\n");
      return 1;
   }

   a_Cookies_set("name=val", "ordinary.com", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "ordinary.com", "/");

   toomany();
   maxage();
   expires_server_ahead();
   expires_server_behind();
   expires_extremes();

   a_Cookies_set("name=val; expires=\"Sun Jan 10 00:00:00 2038\"",
                 "quoted-date.org", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "quoted-date.org", "/");

   a_Cookies_set("name=val; expires=\"Sun Jan 11 00:00:00 1970\"",
                 "quoted-pastdate.org", "/", NULL);
   expect(__LINE__, "", "http", "quoted-pastdate.org", "/");

   path();

   /* LEADING/TRAILING DOTS AND A LITTLE PUBLIC SUFFIX */
   a_Cookies_set("name=val; domain=co.il", "www.co.il", "/", NULL);
   expect(__LINE__, "", "http", "www.co.il", "/");

   a_Cookies_set("name=val; domain=.co.il", "www.co.il", "/", NULL);
   expect(__LINE__, "", "http", "www.co.il", "/");

   a_Cookies_set("name=val; domain=co.il.", "www.co.il.", "/", NULL);
   expect(__LINE__, "", "http", "www.co.il.", "/");

   a_Cookies_set("name=val; domain=.co.il.", "www.co.il.", "/", NULL);
   expect(__LINE__, "", "http", ".www.co.il.", "/");

   a_Cookies_set("name=val; domain=co.org", "www.co.org", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "www.co.org", "/");

   a_Cookies_set("name=val; domain=.cp.org", "www.cp.org", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "www.cp.org", "/");


   /* DOTDOMAIN */
   a_Cookies_set("name=val; domain=.dotdomain.org", "dotdomain.org", "/",
                 NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "dotdomain.org", "/");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "www.dotdomain.org", "/");

   /* HOST_ONLY */
   a_Cookies_set("name=val; domain=.hostonly.org", "hostonly.org", "/", NULL);
   a_Cookies_set("name2=val2", "hostonly.org", "/", NULL);
   a_Cookies_set("name3=val3; domain=hostonly.org", "hostonly.org", "/", NULL);
   expect(__LINE__, "Cookie: name=val; name2=val2; name3=val3\r\n", "http",
          "hostonly.org", "/");
   a_Cookies_set("name=new; domain=.hostonly.org", "hostonly.org", "/", NULL);
   expect(__LINE__, "Cookie: name=new; name2=val2; name3=val3\r\n", "http",
          "hostonly.org", "/");
   a_Cookies_set("name2=new2", "hostonly.org", "/", NULL);
   expect(__LINE__, "Cookie: name=new; name2=new2; name3=val3\r\n", "http",
          "hostonly.org", "/");
   a_Cookies_set("name3=new3; domain=hostonly.org", "hostonly.org", "/", NULL);
   expect(__LINE__, "Cookie: name=new; name2=new2; name3=new3\r\n", "http",
          "hostonly.org", "/");

   /* SUBDOMAIN */
   a_Cookies_set("name=val; domain=www.subdomain.com", "subdomain.com", "/",
                 NULL);
   a_Cookies_set("name=val; domain=.www.subdomain.com", "subdomain.com", "/",
                 NULL);
   expect(__LINE__, "", "http", "subdomain.com", "/");
   expect(__LINE__, "", "http", "www.subdomain.com", "/");

   /* SUPERDOMAIN(?) */
   a_Cookies_set("name=val; domain=.supdomain.com", "www.supdomain.com", "/",
                 NULL);
   a_Cookies_set("name2=val2; domain=supdomain.com", "www.supdomain.com", "/",
                 NULL);
   expect(__LINE__, "Cookie: name=val; name2=val2\r\n", "http",
          "sub2.sub.supdomain.com", "/");
   expect(__LINE__, "Cookie: name=val; name2=val2\r\n", "http",
          "www.supdomain.com", "/");
   expect(__LINE__, "Cookie: name=val; name2=val2\r\n", "http",
          "supdomain.com", "/");

   /* UNRELATED */
   a_Cookies_set("name=val; domain=another.com", "unrelated.com", "/", NULL);
   expect(__LINE__, "", "http", "another.com", "/");
   a_Cookies_set("name=val; domain=another.com", "a.org", "/", NULL);
   expect(__LINE__, "", "http", "another.com", "/");
   a_Cookies_set("name=val; domain=another.com", "badguys.com", "/", NULL);
   expect(__LINE__, "", "http", "another.com", "/");
   a_Cookies_set("name=val; domain=another.com", "more.badguys.com", "/",
                 NULL);
   expect(__LINE__, "", "http", "another.com", "/");
   a_Cookies_set("name=val; domain=another.com", "verybadguys.com", "/", NULL);
   expect(__LINE__, "", "http", "another.com", "/");

   a_Cookies_set("name=val; domain=similar.com", "imilar.com", "/", NULL);
   a_Cookies_set("name2=val2; domain=similar.com", "ssimilar.com", "/", NULL);
   a_Cookies_set("name3=val3; domain=.similar.com", "imilar.com", "/", NULL);
   a_Cookies_set("name4=val4; domain=.similar.com", "timilar.com", "/", NULL);
   a_Cookies_set("name4=val4; domain=.similar.com", "tiimilar.com", "/", NULL);
   expect(__LINE__, "", "http", "similar.com", "/");

   /* SECURE */
   a_Cookies_set("name=val; secure", "secure.com", "/", NULL);
   expect(__LINE__, "", "http", "secure.com", "/");
   expect(__LINE__, "Cookie: name=val\r\n", "https", "secure.com", "/");

   /* HTTPONLY */
   a_Cookies_set("name=val; HttpOnly", "httponly.net", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "httponly.net", "/");

   /* GIBBERISH ATTR IGNORED */
   a_Cookies_set("name=val; ldkfals", "gibberish.net", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "gibberish.net", "/");

   /* WHITESPACE/DELIMITERS */
   a_Cookies_set(" name=val ", "whitespace.net", "/", NULL);
   a_Cookies_set("name2=val2;", "whitespace.net", "/", NULL);
   expect(__LINE__, "Cookie: name=val; name2=val2\r\n", "http",
          "whitespace.net", "/");

   /* NAMELESS/VALUELESS */
   a_Cookies_set("value", "nonameval.org", "/", NULL);
   a_Cookies_set("name=", "nonameval.org", "/", NULL);
   a_Cookies_set("name2= ", "nonameval.org", "/", NULL);
   expect(__LINE__, "Cookie: name=; name2=\r\n", "http", "nonameval.org", "/");
   a_Cookies_set("=val2", "nonameval.org", "/", NULL);
   expect(__LINE__, "Cookie: name=; name2=\r\n", "http", "nonameval.org", "/");


   /* SOME IP ADDRS */

   a_Cookies_set("name=val", "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210",
                 "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http",
          "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", "/");

   a_Cookies_set("name=val", "::FFFF:129.144.52.38", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "::FFFF:129.144.52.38",
          "/");

   a_Cookies_set("name=val", "127.0.0.1", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "127.0.0.1", "/");

   a_Cookies_set("name=val; domain=128.0.0.1", "128.0.0.1", "/", NULL);
   expect(__LINE__, "Cookie: name=val\r\n", "http", "128.0.0.1", "/");

   a_Cookies_set("name=val; domain=130.0.0.1", "129.0.0.1", "/", NULL);
   expect(__LINE__, "", "http", "129.0.0.1", "/");
   expect(__LINE__, "", "http", "130.0.0.1", "/");

   a_Cookies_set("name=val", "2.0.0.1", "/", NULL);
   a_Cookies_set("name=bad; domain=22.0.0.1", "2.0.0.1", "/", NULL);
   a_Cookies_set("name=bad; domain=.0.0.1", "2.0.0.1", "/", NULL);
   a_Cookies_set("name=bad; domain=not-ip.org", "2.0.0.1", "/", NULL);
   expect(__LINE__, "", "http", "22.0.0.1", "/");
   expect(__LINE__, "", "http", "not-ip.org", "/");
   expect(__LINE__, "Cookie: name=val\r\n", "http", "2.0.0.1", "/");

#if 0
HAD BEEN PLAYING AROUND WITH REAL PUBLIC SUFFIX
a_Cookies_set("name=val;domain=sub.sub.yokohama.jp", "sub.sub.yokohama.jp", "/", NULL);
MSG("sub sub yokohama should work: %s\n",
    a_Cookies_get_query("http", "sub.sub.yokohama.jp", "/"));
a_Cookies_set("name=val; domain=sub.tokyo.jp", "sub.sub.tokyo.jp", "/", NULL);
MSG("sub tokyo jp should fail: %s\n",
    a_Cookies_get_query("http", "sub.sub.tokyo.jp", "/"));
a_Cookies_set("name=val; domain=pref.chiba.jp", "sub.pref.chiba.jp", "/", NULL);
MSG("pref chiba jp should succeed: %s\n",
    a_Cookies_get_query("http", "sub.pref.chiba.jp", "/"));
a_Cookies_set("name=val; domain=org", "www.dillo.org", "/", NULL);
a_Cookies_set("name=val; domain=org", "dillo.org", "/", NULL);
a_Cookies_set("name=val; domain=org", ".dillo.org", "/", NULL);
a_Cookies_set("name=val; domain=org.", ".dillo.org", "/", NULL);
a_Cookies_set("name=val; domain=org.", ".dillo.org.", "/", NULL);
MSG("org should fail: %s\n",
    a_Cookies_get_query("http", "www.dillo.org", "/"));
#endif

   MSG("TESTS: passed: %u failed: %u\n", passed, failed);

   MSG("Now that everything is full of fake cookies, you should run "
       "'dpidc stop', plus delete cookies.txt if necessary.\n");

   return 0;
}
