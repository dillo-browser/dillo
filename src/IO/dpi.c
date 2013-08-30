/*
 * File: dpi.c
 *
 * Copyright (C) 2002-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo plugins (small programs that interact with dillo)
 *
 * Dillo plugins are designed to handle:
 *   bookmarks, cookies, FTP, downloads, files, preferences, https,
 *   datauri and a lot of any-to-html filters.
 */


#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>           /* for errno */
#include <fcntl.h>
#include <ctype.h>           /* isxdigit */

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../msg.h"
#include "../klist.h"
#include "IO.h"
#include "Url.h"
#include "../../dpip/dpip.h"

/* This one is tricky, some sources state it should include the byte
 * for the terminating NULL, and others say it shouldn't. */
# define D_SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) \
                        + strlen ((ptr)->sun_path))

/* Solaris may not have this one... */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif


typedef struct {
   int InTag;
   int Send2EOF;

   int DataTotalSize;
   int DataRecvSize;

   Dstr *Buf;

   int BufIdx;
   int TokIdx;
   int TokSize;
   int TokIsTag;

   ChainLink *InfoRecv;
   int Key;
} dpi_conn_t;


/*
 * Local data
 */
static Klist_t *ValidConns = NULL; /* Active connections list. It holds
                                    * pointers to dpi_conn_t structures. */
static char SharedKey[32];

/*
 * Initialize local data
 */
void a_Dpi_init(void)
{
   /* empty */
}

/*
 * Create a new connection data structure
 */
static dpi_conn_t *Dpi_conn_new(ChainLink *Info)
{
   dpi_conn_t *conn = dNew0(dpi_conn_t, 1);

   conn->Buf = dStr_sized_new(8*1024);
   conn->InfoRecv = Info;
   conn->Key = a_Klist_insert(&ValidConns, conn);

   return conn;
}

/*
 * Free a connection data structure
 */
static void Dpi_conn_free(dpi_conn_t *conn)
{
   a_Klist_remove(ValidConns, conn->Key);
   dStr_free(conn->Buf, 1);
   dFree(conn);
}

/*
 * Check whether a conn is still valid.
 * Return: 1 if found, 0 otherwise
 */
static int Dpi_conn_valid(int key)
{
   return (a_Klist_get_data(ValidConns, key)) ? 1 : 0;
}

/*
 * Append the new buffer in 'dbuf' to Buf in 'conn'
 */
static void Dpi_append_dbuf(dpi_conn_t *conn, DataBuf *dbuf)
{
   if (dbuf->Code == 0 && dbuf->Size > 0) {
      dStr_append_l(conn->Buf, dbuf->Buf, dbuf->Size);
   }
}

/*
 * Split the data stream into tokens.
 * Here, a token is either:
 *    a) a dpi tag
 *    b) a raw data chunk
 *
 * Return Value: 0 upon a new token, -1 on not enough data.
 *
 * TODO: define an API and move this function into libDpip.a.
*/
static int Dpi_get_token(dpi_conn_t *conn)
{
   int i, resp = -1;
   char *buf = conn->Buf->str;

   if (conn->BufIdx == conn->Buf->len) {
      dStr_truncate(conn->Buf, 0);
      conn->BufIdx = 0;
      return resp;
   }

   if (conn->Send2EOF) {
      conn->TokIdx = conn->BufIdx;
      conn->TokSize = conn->Buf->len - conn->BufIdx;
      conn->BufIdx = conn->Buf->len;
      return 0;
   }

   _MSG("conn->BufIdx = %d; conn->Buf->len = %d\nbuf: [%s]\n",
        conn->BufIdx,conn->Buf->len, conn->Buf->str + conn->BufIdx);

   if (!conn->InTag) {
      /* search for start of tag */
      while (conn->BufIdx < conn->Buf->len && buf[conn->BufIdx] != '<')
         ++conn->BufIdx;
      if (conn->BufIdx < conn->Buf->len) {
         /* found */
         conn->InTag = 1;
         conn->TokIdx = conn->BufIdx;
      } else {
         MSG_ERR("[Dpi_get_token] Can't find token start\n");
      }
   }

   if (conn->InTag) {
      /* search for end of tag (EOT=" '>") */
      for (i = conn->BufIdx; i < conn->Buf->len; ++i)
         if (buf[i] == '>' && i >= 2 && buf[i-1] == '\'' && buf[i-2] == ' ')
            break;
      conn->BufIdx = i;

      if (conn->BufIdx < conn->Buf->len) {
         /* found EOT */
         conn->TokIsTag = 1;
         conn->TokSize = conn->BufIdx - conn->TokIdx + 1;
         ++conn->BufIdx;
         conn->InTag = 0;
         resp = 0;
      }
   }

   return resp;
}

/*
 * Parse a dpi tag and take the appropriate actions
 */
static void Dpi_parse_token(dpi_conn_t *conn)
{
   char *tag, *cmd, *msg, *urlstr;
   DataBuf *dbuf;
   char *Tok = conn->Buf->str + conn->TokIdx;

   if (conn->Send2EOF) {
      /* we're receiving data chunks from a HTML page */
      dbuf = a_Chain_dbuf_new(Tok, conn->TokSize, 0);
      a_Chain_fcb(OpSend, conn->InfoRecv, dbuf, "send_page_2eof");
      dFree(dbuf);
      return;
   }

   tag = dStrndup(Tok, (size_t)conn->TokSize);
   _MSG("Dpi_parse_token: {%s}\n", tag);

   cmd = a_Dpip_get_attr_l(Tok, conn->TokSize, "cmd");
   if (strcmp(cmd, "send_status_message") == 0) {
      msg = a_Dpip_get_attr_l(Tok, conn->TokSize, "msg");
      a_Chain_fcb(OpSend, conn->InfoRecv, msg, cmd);
      dFree(msg);

   } else if (strcmp(cmd, "chat") == 0) {
      msg = a_Dpip_get_attr_l(Tok, conn->TokSize, "msg");
      a_Chain_fcb(OpSend, conn->InfoRecv, msg, cmd);
      dFree(msg);

   } else if (strcmp(cmd, "dialog") == 0) {
      /* For now will send the dpip tag... */
      a_Chain_fcb(OpSend, conn->InfoRecv, tag, cmd);

   } else if (strcmp(cmd, "start_send_page") == 0) {
      conn->Send2EOF = 1;
      urlstr = a_Dpip_get_attr_l(Tok, conn->TokSize, "url");
      a_Chain_fcb(OpSend, conn->InfoRecv, urlstr, cmd);
      dFree(urlstr);
      /* TODO: a_Dpip_get_attr_l(Tok, conn->TokSize, "send_mode") */

   } else if (strcmp(cmd, "reload_request") == 0) {
      urlstr = a_Dpip_get_attr_l(Tok, conn->TokSize, "url");
      a_Chain_fcb(OpSend, conn->InfoRecv, urlstr, cmd);
      dFree(urlstr);
   }
   dFree(cmd);

   dFree(tag);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Write data into a file descriptor taking care of EINTR
 * and possible data splits.
 * Return value: 1 on success, -1 on error.
 */
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Get a new data buffer (within a 'dbuf'), save it into local data,
 * split in tokens and parse the contents.
 */
static void Dpi_process_dbuf(int Op, void *Data1, dpi_conn_t *conn)
{
   DataBuf *dbuf = Data1;
   int key = conn->Key;

   /* Very useful for debugging: show the data stream as received. */
   /* fwrite(dbuf->Buf, dbuf->Size, 1, stdout); */

   if (Op == IORead) {
      Dpi_append_dbuf(conn, dbuf);
      /* 'conn' has to be validated because Dpi_parse_token() MAY call abort */
      while (Dpi_conn_valid(key) && Dpi_get_token(conn) != -1) {
         Dpi_parse_token(conn);
      }

   } else if (Op == IOClose) {
      /* unused */
   }
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
      dClose(st_pipe[0]);
      if (execl(path1, "dpid", (char*)NULL) == -1) {
         dFree(path1);
         path1 = dStrconcat(DILLO_BINDIR, "dpid", NULL);
         if (execl(path1, "dpid", (char*)NULL) == -1) {
            dFree(path1);
            if (execlp("dpid", "dpid", (char*)NULL) == -1) {
               MSG("Dpi_start_dpid (child): %s\n", dStrerror(errno));
               if (Dpi_blocking_write(st_pipe[1], "ERROR", 5) == -1) {
                  MSG("Dpi_start_dpid (child): can't write to pipe.\n");
               }
               dClose(st_pipe[1]);
               _exit (EXIT_FAILURE);
            }
         }
      }
   } else if (pid < 0) {
      /* The fork failed.  Report failure.  */
      MSG("Dpi_start_dpid: %s\n", dStrerror(errno));
      /* close the unused pipe */
      dClose(st_pipe[0]);
      dClose(st_pipe[1]);
   } else {
      /* This is the parent process, check our child status... */
      dClose(st_pipe[1]);
      if ((answer = Dpi_blocking_read(st_pipe[0])) != NULL) {
         MSG("Dpi_start_dpid: can't start dpid\n");
         dFree(answer);
      } else {
         ret = 0;
      }
      dClose(st_pipe[0]);
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

/*
 * Return a socket file descriptor
 */
static int Dpi_make_socket_fd()
{
   int fd, one = 1, ret = -1;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
      /* avoid delays when sending small pieces of data */
      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      ret = fd;
   }
   return ret;
}

/*
 * Make a connection test for a IDS.
 * Return: 1 OK, -1 Not working.
 */
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
         dClose(sock_fd);
         ret = 1;
      }
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

   _MSG("Dpi_check_dpid: %s\n",
        (ret == 0) ? "OK" : (ret == 1 ? "EAGAIN" : "ERROR"));
   return ret;
}

/*
 * Confirm that the dpid is running. If not, start it.
 * Return: 0 running OK, 2 Error.
 */
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
   _MSG("Dpi_get_server_port: server_name = [%s]\n", server_name);

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
   dClose(sock_fd);

   return ok ? dpi_port : -1;
}


/*
 * Connect a socket to a dpi server and return the socket's FD.
 * We have to ask 'dpid' (dpi daemon) for the port of the target dpi server.
 * Once we have it, then the proper file descriptor is returned (-1 on error).
 */
static int Dpi_connect_socket(const char *server_name)
{
   struct sockaddr_in sin;
   int sock_fd, dpi_port, ret = -1;
   char *cmd = NULL;

   /* Query dpid for the port number for this server */
   if ((dpi_port = Dpi_get_server_port(server_name)) == -1) {
      _MSG("Dpi_connect_socket: can't get port number for %s\n", server_name);
      return -1;
   }
   _MSG("Dpi_connect_socket: server=%s port=%d\n", server_name, dpi_port);

   /* connect with this server's socket */
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sin.sin_port = htons(dpi_port);

   if ((sock_fd = Dpi_make_socket_fd()) == -1) {
      MSG_ERR("[Dpi_connect_socket] %s\n", dStrerror(errno));
   } else if (connect(sock_fd, (void*)&sin, sizeof(sin)) == -1) {
      MSG_ERR("[Dpi_connect_socket] errno:%d %s\n", errno, dStrerror(errno));

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
      dClose(sock_fd);

   return ret;
}

/*
 * CCC function for the Dpi module
 */
void a_Dpi_ccc(int Op, int Branch, int Dir, ChainLink *Info,
               void *Data1, void *Data2)
{
   dpi_conn_t *conn;
   int SockFD = -1, st;

   dReturn_if_fail( a_Chain_check("a_Dpi_ccc", Op, Branch, Dir, Info) );

   if (Branch == 1) {
      if (Dir == BCK) {
         /* Send commands to dpi-server */
         switch (Op) {
         case OpStart:
            if ((st = Dpi_blocking_start_dpid()) == 0) {
               if ((SockFD = Dpi_connect_socket(Data1)) != -1) {
                  int *fd = dNew(int, 1);
                  *fd = SockFD;
                  Info->LocalKey = fd;
                  a_Chain_link_new(Info, a_Dpi_ccc, BCK, a_IO_ccc, 1, 1);
                  a_Chain_bcb(OpStart, Info, NULL, NULL);
                  /* Let the FD known and tracked */
                  a_Chain_bcb(OpSend, Info, &SockFD, "FD");
                  a_Chain_fcb(OpSend, Info, &SockFD, "FD");
                  a_Chain_fcb(OpSend, Info, NULL, "DpidOK");
               } else {
                  a_Dpi_ccc(OpAbort, 1, FWD, Info, NULL, NULL);
               }
            } else {
               MSG_ERR("dpi.c: can't start dpi daemon\n");
               a_Dpi_ccc(OpAbort, 1, FWD, Info, NULL, "DpidERROR");
            }
            break;
         case OpSend:
            a_Chain_bcb(OpSend, Info, Data1, NULL);
            break;
         case OpEnd:
            a_Chain_bcb(OpEnd, Info, NULL, NULL);
            dFree(Info->LocalKey);
            dFree(Info);
            break;
         case OpAbort:
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            dFree(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* 1 FWD */
         /* Send commands to dpi-server (status) */
         switch (Op) {
         case OpAbort:
            a_Chain_fcb(OpAbort, Info, NULL, Data2);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }

   } else if (Branch == 2) {
      if (Dir == FWD) {
         /* Receiving from server */
         switch (Op) {
         case OpSend:
            /* Data1 = dbuf */
            Dpi_process_dbuf(IORead, Data1, Info->LocalKey);
            break;
         case OpEnd:
            a_Chain_fcb(OpEnd, Info, NULL, NULL);
            Dpi_conn_free(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* 2 BCK */
         switch (Op) {
         case OpStart:
            conn = Dpi_conn_new(Info);
            Info->LocalKey = conn;

            /* Hack: for receiving HTTP through the DPI framework */
            if (strcmp(Data2, "http") == 0) {
               conn->Send2EOF = 1;
            }

            a_Chain_link_new(Info, a_Dpi_ccc, BCK, a_IO_ccc, 2, 2);
            a_Chain_bcb(OpStart, Info, NULL, NULL); /* IORead */
            break;
         case OpSend:
            if (Data2 && !strcmp(Data2, "FD")) {
               a_Chain_bcb(OpSend, Info, Data1, Data2);
            }
            break;
         case OpAbort:
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Dpi_conn_free(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}

/*! Let dpid know dillo is no longer running.
 * Note: currently disabled. It may serve to let the cookies dpi know
 * when to expire session cookies.
 */
void a_Dpi_dillo_exit()
{

}

/*
 * Send a command to a dpi server, and block until the answer is got.
 * Return value: the dpip tag answer as an string, NULL on error.
 */
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
   dClose(sock_fd);

   return ret;
}

