/*
 * File: file.c :)
 *
 * Copyright (C) 2000-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Directory scanning is no longer streamed, but it gets sorted instead!
 * Directory entries on top, files next.
 * With new HTML layout.
 */

#include <ctype.h>           /* for isspace */
#include <errno.h>           /* for errno */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <netinet/in.h>

#include "../dpip/dpip.h"
#include "dpiutil.h"
#include "d_size.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[file dpi]: " __VA_ARGS__)
#define _MSG_RAW(...)
#define MSG_RAW(...)  printf(__VA_ARGS__)


#define MAXNAMESIZE 30
#define HIDE_DOTFILES TRUE

/*
 * Communication flags
 */
#define FILE_AUTH_OK     1     /* Authentication done */
#define FILE_READ        2     /* Waiting data */
#define FILE_WRITE       4     /* Sending data */
#define FILE_DONE        8     /* Operation done */
#define FILE_ERR        16     /* Operation error */


typedef enum {
   st_start = 10,
   st_dpip,
   st_http,
   st_content,
   st_done,
   st_err
} FileState;

typedef struct {
   char *full_path;
   const char *filename;
   off_t size;
   mode_t mode;
   time_t mtime;
} FileInfo;

typedef struct {
   char *dirname;
   Dlist *flist;       /* List of files and subdirectories (for sorting) */
} DilloDir;

typedef struct {
   Dsh *sh;
   char *orig_url;
   char *filename;
   int file_fd;
   off_t file_sz;
   DilloDir *d_dir;
   FileState state;
   int err_code;
   int flags;
   int old_style;
} ClientInfo;

/*
 * Forward references
 */
static const char *File_content_type(const char *filename);

/*
 * Global variables
 */
static int DPIBYE = 0;
static int OLD_STYLE = 0;
/* A list for the clients we are serving */
static Dlist *Clients;
/* Set of filedescriptors we're working on */
fd_set read_set, write_set;


/*
 * Close a file descriptor, but handling EINTR
 */
static void File_close(int fd)
{
   while (fd >= 0 && close(fd) < 0 && errno == EINTR)
      ;
}

/*
 * Detects 'Content-Type' when the server does not supply one.
 * It uses the magic(5) logic from file(1). Currently, it
 * only checks the few mime types that Dillo supports.
 *
 * 'Data' is a pointer to the first bytes of the raw data.
 * (this is based on a_Misc_get_content_type_from_data())
 */
static const char *File_get_content_type_from_data(void *Data, size_t Size)
{
   static const char *Types[] = {
      "application/octet-stream",
      "text/html", "text/plain",
      "image/gif", "image/png", "image/jpeg",
   };
   int Type = 0;
   char *p = Data;
   size_t i, non_ascci;

   _MSG("File_get_content_type_from_data:: Size = %d\n", Size);

   /* HTML try */
   for (i = 0; i < Size && dIsspace(p[i]); ++i);
   if ((Size - i >= 5  && !dStrnAsciiCasecmp(p+i, "<html", 5)) ||
       (Size - i >= 5  && !dStrnAsciiCasecmp(p+i, "<head", 5)) ||
       (Size - i >= 6  && !dStrnAsciiCasecmp(p+i, "<title", 6)) ||
       (Size - i >= 14 && !dStrnAsciiCasecmp(p+i, "<!doctype html", 14)) ||
       /* this line is workaround for FTP through the Squid proxy */
       (Size - i >= 17 && !dStrnAsciiCasecmp(p+i, "<!-- HTML listing", 17))) {

      Type = 1;

   /* Images */
   } else if (Size >= 4 && !strncmp(p, "GIF8", 4)) {
      Type = 3;
   } else if (Size >= 4 && !strncmp(p, "\x89PNG", 4)) {
      Type = 4;
   } else if (Size >= 2 && !strncmp(p, "\xff\xd8", 2)) {
      /* JPEG has the first 2 bytes set to 0xffd8 in BigEndian - looking
       * at the character representation should be machine independent. */
      Type = 5;

   /* Text */
   } else {
      /* We'll assume "text/plain" if the set of chars above 127 is <= 10
       * in a 256-bytes sample.  Better heuristics are welcomed! :-) */
      non_ascci = 0;
      Size = MIN (Size, 256);
      for (i = 0; i < Size; i++)
         if ((uchar_t) p[i] > 127)
            ++non_ascci;
      if (Size == 256) {
         Type = (non_ascci > 10) ? 0 : 2;
      } else {
         Type = (non_ascci > 0) ? 0 : 2;
      }
   }

   return (Types[Type]);
}

/*
 * Compare two FileInfo pointers
 * This function is used for sorting directories
 */
static int File_comp(const FileInfo *f1, const FileInfo *f2)
{
   if (S_ISDIR(f1->mode)) {
      if (S_ISDIR(f2->mode)) {
         return strcmp(f1->filename, f2->filename);
      } else {
         return -1;
      }
   } else {
      if (S_ISDIR(f2->mode)) {
         return 1;
      } else {
         return strcmp(f1->filename, f2->filename);
      }
   }
}

/*
 * Allocate a DilloDir structure, set safe values in it and sort the entries.
 */
static DilloDir *File_dillodir_new(char *dirname)
{
   struct stat sb;
   struct dirent *de;
   DIR *dir;
   DilloDir *Ddir;
   FileInfo *finfo;
   char *fname;
   int dirname_len;

   if (!(dir = opendir(dirname)))
      return NULL;

   Ddir = dNew(DilloDir, 1);
   Ddir->dirname = dStrdup(dirname);
   Ddir->flist = dList_new(512);

   dirname_len = strlen(Ddir->dirname);

   /* Scan every name and sort them */
   while ((de = readdir(dir)) != 0) {
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
         continue;              /* skip "." and ".." */

      if (HIDE_DOTFILES) {
         /* Don't add hidden files or backup files to the list */
         if (de->d_name[0] == '.' ||
             de->d_name[0] == '#' ||
             (de->d_name[0] != '\0' &&
              de->d_name[strlen(de->d_name) - 1] == '~'))
            continue;
      }

      fname = dStrconcat(Ddir->dirname, de->d_name, NULL);
      if (stat(fname, &sb) == -1) {
         dFree(fname);
         continue;              /* ignore files we can't stat */
      }

      finfo = dNew(FileInfo, 1);
      finfo->full_path = fname;
      finfo->filename = fname + dirname_len;
      finfo->size = sb.st_size;
      finfo->mode = sb.st_mode;
      finfo->mtime = sb.st_mtime;

      dList_append(Ddir->flist, finfo);
   }

   closedir(dir);

   /* sort the entries */
   dList_sort(Ddir->flist, (dCompareFunc)File_comp);

   return Ddir;
}

/*
 * Deallocate a DilloDir structure.
 */
static void File_dillodir_free(DilloDir *Ddir)
{
   int i;
   FileInfo *finfo;

   dReturn_if (Ddir == NULL);

   for (i = 0; i < dList_length(Ddir->flist); ++i) {
      finfo = dList_nth_data(Ddir->flist, i);
      dFree(finfo->full_path);
      dFree(finfo);
   }

   dList_free(Ddir->flist);
   dFree(Ddir->dirname);
   dFree(Ddir);
}

/*
 * Output the string for parent directory
 */
static void File_print_parent_dir(ClientInfo *client, const char *dirname)
{
   if (strcmp(dirname, "/") != 0) {        /* Not the root dir */
      char *p, *parent, *HUparent, *Uparent;

      parent = dStrdup(dirname);
      /* cut trailing slash */
      parent[strlen(parent) - 1] = '\0';
      /* make 'parent' have the parent dir path */
      if ((p = strrchr(parent, '/')))
         *(p + 1) = '\0';

      Uparent = Escape_uri_str(parent, NULL);
      HUparent = Escape_html_str(Uparent);
      a_Dpip_dsh_printf(client->sh, 0,
         "<a href='file:%s'>Parent directory</a>", HUparent);
      dFree(HUparent);
      dFree(Uparent);
      dFree(parent);
   }
}

/*
 * Given a timestamp, output an HTML-formatted date string.
 */
static void File_print_mtime(ClientInfo *client, time_t mtime)
{
   char *ds = ctime(&mtime);

   /* Month, day and {hour or year} */
   if (client->old_style) {
      a_Dpip_dsh_printf(client->sh, 0, " %.3s %.2s", ds + 4, ds + 8);
      if (time(NULL) - mtime > 15811200) {
         a_Dpip_dsh_printf(client->sh, 0, "  %.4s", ds + 20);
      } else {
         a_Dpip_dsh_printf(client->sh, 0, " %.5s", ds + 11);
      }
   } else {
      a_Dpip_dsh_printf(client->sh, 0,
         "<td>%.3s&nbsp;%.2s&nbsp;%.5s", ds + 4, ds + 8,
         /* (more than 6 months old) ? year : hour; */
         (time(NULL) - mtime > 15811200) ? ds + 20 : ds + 11);
   }
}

/*
 * Return a HTML-line from file info.
 */
static void File_info2html(ClientInfo *client, FileInfo *finfo, int n)
{
   int size;
   char *sizeunits;
   char namebuf[MAXNAMESIZE + 1];
   char *Uref, *HUref, *Hname;
   const char *ref, *filecont, *name = finfo->filename;

   if (finfo->size <= 9999) {
      size = finfo->size;
      sizeunits = "bytes";
   } else if (finfo->size / 1024 <= 9999) {
      size = finfo->size / 1024 + (finfo->size % 1024 >= 1024 / 2);
      sizeunits = "KB";
   } else {
      size = finfo->size / 1048576 + (finfo->size % 1048576 >= 1048576 / 2);
      sizeunits = "MB";
   }

   /* we could note if it's a symlink... */
   if S_ISDIR (finfo->mode) {
      filecont = "Directory";
   } else if (finfo->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
      filecont = "Executable";
   } else {
      filecont = File_content_type(finfo->full_path);
      if (!filecont || !strcmp(filecont, "application/octet-stream"))
         filecont = "unknown";
   }

   ref = name;

   if (strlen(name) > MAXNAMESIZE) {
      memcpy(namebuf, name, MAXNAMESIZE - 3);
      strcpy(namebuf + (MAXNAMESIZE - 3), "...");
      name = namebuf;
   }

   /* escape problematic filenames */
   Uref = Escape_uri_str(ref, NULL);
   HUref = Escape_html_str(Uref);
   Hname = Escape_html_str(name);

   if (client->old_style) {
      char *dots = ".. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..";
      int ndots = MAXNAMESIZE - strlen(name);
      a_Dpip_dsh_printf(client->sh, 0,
         "%s<a href='%s'>%s</a>"
         " %s"
         " %-11s%4d %-5s",
         S_ISDIR (finfo->mode) ? ">" : " ", HUref, Hname,
         dots + 50 - (ndots > 0 ? ndots : 0),
         filecont, size, sizeunits);

   } else {
      a_Dpip_dsh_printf(client->sh, 0,
         "<tr align=center %s><td>%s<td align=left><a href='%s'>%s</a>"
         "<td>%s<td>%d&nbsp;%s",
         (n & 1) ? "bgcolor=#dcdcdc" : "",
         S_ISDIR (finfo->mode) ? ">" : " ", HUref, Hname,
         filecont, size, sizeunits);
   }
   File_print_mtime(client, finfo->mtime);
   a_Dpip_dsh_write_str(client->sh, 0, "\n");

   dFree(Hname);
   dFree(HUref);
   dFree(Uref);
}

/*
 * Send the HTML directory page in HTTP.
 */
static void File_send_dir(ClientInfo *client)
{
   int n;
   char *d_cmd, *Hdirname, *Udirname, *HUdirname;
   DilloDir *Ddir = client->d_dir;

   if (client->state == st_start) {
      /* Send DPI command */
      d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page",
                               client->orig_url);
      a_Dpip_dsh_write_str(client->sh, 1, d_cmd);
      dFree(d_cmd);
      client->state = st_dpip;

   } else if (client->state == st_dpip) {
      /* send HTTP header and HTML top part */

      /* Send page title */
      Udirname = Escape_uri_str(Ddir->dirname, NULL);
      HUdirname = Escape_html_str(Udirname);
      Hdirname = Escape_html_str(Ddir->dirname);

      a_Dpip_dsh_printf(client->sh, 0,
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/html\r\n"
         "\r\n"
         "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
         "<HTML>\n<HEAD>\n <BASE href='file:%s'>\n"
         " <TITLE>file:%s</TITLE>\n</HEAD>\n"
         "<BODY><H1>Directory listing of %s</H1>\n",
         HUdirname, Hdirname, Hdirname);
      dFree(Hdirname);
      dFree(HUdirname);
      dFree(Udirname);

      if (client->old_style) {
         a_Dpip_dsh_write_str(client->sh, 0, "<pre>\n");
      }

      /* Output the parent directory */
      File_print_parent_dir(client, Ddir->dirname);

      /* HTML style toggle */
      a_Dpip_dsh_write_str(client->sh, 0,
         "&nbsp;&nbsp;<a href='dpi:/file/toggle'>%</a>\n");

      if (dList_length(Ddir->flist)) {
         if (client->old_style) {
            a_Dpip_dsh_write_str(client->sh, 0, "\n\n");
         } else {
            a_Dpip_dsh_write_str(client->sh, 0,
               "<br><br>\n"
               "<table border=0 cellpadding=1 cellspacing=0"
               " bgcolor=#E0E0E0 width=100%>\n"
               "<tr align=center>\n"
               "<td>\n"
               "<td width=60%><b>Filename</b>"
               "<td><b>Type</b>"
               "<td><b>Size</b>"
               "<td><b>Modified&nbsp;at</b>\n");
         }
      } else {
         a_Dpip_dsh_write_str(client->sh, 0, "<br><br>Directory is empty...");
      }
      client->state = st_http;

   } else if (client->state == st_http) {
      /* send directories as HTML contents */
      for (n = 0; n < dList_length(Ddir->flist); ++n) {
         File_info2html(client, dList_nth_data(Ddir->flist,n), n+1);
      }

      if (client->old_style) {
         a_Dpip_dsh_write_str(client->sh, 0, "</pre>\n");
      } else if (dList_length(Ddir->flist)) {
         a_Dpip_dsh_write_str(client->sh, 0, "</table>\n");
      }

      a_Dpip_dsh_write_str(client->sh, 1, "</BODY></HTML>\n");
      client->state = st_content;
      client->flags |= FILE_DONE;
   }
}

/*
 * Return a content type based on the extension of the filename.
 */
static const char *File_ext(const char *filename)
{
   char *e;

   if (!(e = strrchr(filename, '.')))
      return NULL;

   e++;

   if (!dStrAsciiCasecmp(e, "gif")) {
      return "image/gif";
   } else if (!dStrAsciiCasecmp(e, "jpg") ||
              !dStrAsciiCasecmp(e, "jpeg")) {
      return "image/jpeg";
   } else if (!dStrAsciiCasecmp(e, "png")) {
      return "image/png";
   } else if (!dStrAsciiCasecmp(e, "html") ||
              !dStrAsciiCasecmp(e, "htm") ||
              !dStrAsciiCasecmp(e, "shtml")) {
      return "text/html";
   } else if (!dStrAsciiCasecmp(e, "txt")) {
      return "text/plain";
   } else {
      return NULL;
   }
}

/*
 * Based on the extension, return the content_type for the file.
 * (if there's no extension, analyze the data and try to figure it out)
 */
static const char *File_content_type(const char *filename)
{
   int fd;
   struct stat sb;
   const char *ct;
   char buf[256];
   ssize_t buf_size;

   if (!(ct = File_ext(filename))) {
      /* everything failed, let's analyze the data... */
      if ((fd = open(filename, O_RDONLY | O_NONBLOCK)) != -1) {
         if ((buf_size = read(fd, buf, 256)) == 256 ) {
            ct = File_get_content_type_from_data(buf, (size_t)buf_size);

         } else if (stat(filename, &sb) != -1 &&
                    buf_size > 0 && buf_size == sb.st_size) {
            ct = File_get_content_type_from_data(buf, (size_t)buf_size);
         }
         File_close(fd);
      }
   }
   _MSG("File_content_type: name=%s ct=%s\n", filename, ct);
   return ct;
}

/*
 * Send an error page
 */
static void File_prepare_send_error_page(ClientInfo *client, int res,
                                         const char *orig_url)
{
   client->state = st_err;
   client->err_code = res;
   client->orig_url = dStrdup(orig_url);
   client->flags &= ~FILE_READ;
   client->flags |= FILE_WRITE;
}

/*
 * Send an error page
 */
static void File_send_error_page(ClientInfo *client)
{
   const char *status;
   char *d_cmd;
   Dstr *body = dStr_sized_new(128);

   if (client->err_code == EACCES) {
      status = "403 Forbidden";
   } else if (client->err_code == ENOENT) {
      status = "404 Not Found";
   } else {
      /* good enough */
      status = "500 Internal Server Error";
   }
   dStr_append(body, status);
   dStr_append(body, "\n");
   dStr_append(body, dStrerror(client->err_code));

   /* Send DPI command */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page",
                            client->orig_url);
   a_Dpip_dsh_write_str(client->sh, 0, d_cmd);
   dFree(d_cmd);

   a_Dpip_dsh_printf(client->sh, 0,
                     "HTTP/1.1 %s\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %d\r\n"
                     "\r\n"
                     "%s",
                     status, body->len, body->str);
   dStr_free(body, TRUE);

   client->flags |= FILE_DONE;
}

/*
 * Scan the directory, sort and prepare to send it enclosed in HTTP.
 */
static int File_prepare_send_dir(ClientInfo *client,
                                 const char *DirName, const char *orig_url)
{
   Dstr *ds_dirname;
   DilloDir *Ddir;

   /* Let's make sure this directory url has a trailing slash */
   ds_dirname = dStr_new(DirName);
   if (ds_dirname->str[ds_dirname->len - 1] != '/')
      dStr_append(ds_dirname, "/");

   /* Let's get a structure ready for transfer */
   Ddir = File_dillodir_new(ds_dirname->str);
   dStr_free(ds_dirname, TRUE);
   if (Ddir) {
      /* looks ok, set things accordingly */
      client->orig_url = dStrdup(orig_url);
      client->d_dir = Ddir;
      client->state = st_start;
      client->flags &= ~FILE_READ;
      client->flags |= FILE_WRITE;
      return 0;
   } else
      return EACCES;
}

/*
 * Prepare to send HTTP headers and then the file itself.
 */
static int File_prepare_send_file(ClientInfo *client,
                                  const char *filename,
                                  const char *orig_url)
{
   int fd, res = -1;
   struct stat sb;

   if (stat(filename, &sb) != 0) {
      /* prepare a file-not-found error */
      res = ENOENT;
   } else if ((fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0) {
      /* prepare an error message */
      res = errno;
   } else {
      /* looks ok, set things accordingly */
      client->file_fd = fd;
      client->file_sz = sb.st_size;
      client->d_dir = NULL;
      client->state = st_start;
      client->filename = dStrdup(filename);
      client->orig_url = dStrdup(orig_url);
      client->flags &= ~FILE_READ;
      client->flags |= FILE_WRITE;
      res = 0;
   }
   return res;
}

/*
 * Try to stat the file and determine if it's readable.
 */
static void File_get(ClientInfo *client, const char *filename,
                     const char *orig_url)
{
   int res;
   struct stat sb;

   if (stat(filename, &sb) != 0) {
      /* stat failed, prepare a file-not-found error. */
      res = ENOENT;
   } else if (S_ISDIR(sb.st_mode)) {
      /* set up for reading directory */
      res = File_prepare_send_dir(client, filename, orig_url);
   } else {
      /* set up for reading a file */
      res = File_prepare_send_file(client, filename, orig_url);
   }
   if (res != 0) {
      File_prepare_send_error_page(client, res, orig_url);
   }
}

/*
 * Send HTTP headers and then the file itself.
 */
static int File_send_file(ClientInfo *client)
{
//#define LBUF 1
#define LBUF 16*1024

   const char *ct;
   const char *unknown_type = "application/octet-stream";
   char buf[LBUF], *d_cmd, *name;
   int st, st2, namelen;
   bool_t gzipped = FALSE;

   if (client->state == st_start) {
      /* Send DPI command */
      d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page",
                               client->orig_url);
      a_Dpip_dsh_write_str(client->sh, 1, d_cmd);
      dFree(d_cmd);
      client->state = st_dpip;

   } else if (client->state == st_dpip) {
      /* send HTTP header */

      /* Check for gzipped file */
      namelen = strlen(client->filename);
      if (namelen > 3 &&
          !dStrAsciiCasecmp(client->filename + namelen - 3, ".gz")) {
         gzipped = TRUE;
         namelen -= 3;
      }
      /* Content-Type info is based on filename extension (with ".gz" removed).
       * If there's no known extension, perform data sniffing.
       * If this doesn't lead to a conclusion, use "application/octet-stream".
       */
      name = dStrndup(client->filename, namelen);
      if (!(ct = File_content_type(name)))
         ct = unknown_type;
      dFree(name);

      /* Send HTTP headers */
      a_Dpip_dsh_write_str(client->sh, 0, "HTTP/1.1 200 OK\r\n");
      if (gzipped) {
         a_Dpip_dsh_write_str(client->sh, 0, "Content-Encoding: gzip\r\n");
      }
      if (!gzipped || strcmp(ct, unknown_type)) {
         a_Dpip_dsh_printf(client->sh, 0, "Content-Type: %s\r\n", ct);
      } else {
         /* If we don't know type for gzipped data, let dillo figure it out. */
      }
      a_Dpip_dsh_printf(client->sh, 1,
                        "Content-Length: %ld\r\n"
                        "\r\n",
                        client->file_sz);
      client->state = st_http;

   } else if (client->state == st_http) {
      /* Send body -- raw file contents */
      if ((st = a_Dpip_dsh_tryflush(client->sh)) < 0) {
         client->flags |= (st == -3) ? FILE_ERR : 0;
      } else {
         /* no pending data, let's send new data */
         do {
            st2 = read(client->file_fd, buf, LBUF);
         } while (st2 < 0 && errno == EINTR);
         if (st2 < 0) {
            MSG("\nERROR while reading from file '%s': %s\n\n",
                client->filename, dStrerror(errno));
            client->flags |= FILE_ERR;
         } else if (st2 == 0) {
            client->state = st_content;
            client->flags |= FILE_DONE;
         } else {
            /* ok to write */
            st = a_Dpip_dsh_trywrite(client->sh, buf, st2);
            client->flags |= (st == -3) ? FILE_ERR : 0;
         }
      }
   }

   return 0;
}

/*
 * Given a hex octet (e3, 2F, 20), return the corresponding
 * character if the octet is valid, and -1 otherwise
 */
static int File_parse_hex_octet(const char *s)
{
   int hex_value;
   char *tail, hex[3];

   if ((hex[0] = s[0]) && (hex[1] = s[1])) {
      hex[2] = 0;
      hex_value = strtol(hex, &tail, 16);
      if (tail - hex == 2)
        return hex_value;
   }

   return -1;
}

/*
 * Make a file URL into a human (and machine) readable path.
 * The idea is to always have a path that starts with only one slash.
 * Embedded slashes are ignored.
 */
static char *File_normalize_path(const char *orig)
{
   char *str = (char *) orig, *basename = NULL, *ret = NULL, *p;

   dReturn_val_if (orig == NULL, ret);

   /* Make sure the string starts with "file:/" */
   if (dStrnAsciiCasecmp(str, "file:/", 5) != 0)
      return ret;
   str += 5;

   /* Skip "localhost" */
   if (dStrnAsciiCasecmp(str, "//localhost/", 12) == 0)
      str += 11;

   /* Skip packed slashes, and leave just one */
   while (str[0] == '/' && str[1] == '/')
      str++;

   {
      int i, val;
      Dstr *ds = dStr_sized_new(32);
      dStr_sprintf(ds, "%s%s%s",
                   basename ? basename : "",
                   basename ? "/" : "",
                   str);
      dFree(basename);

      /* Parse possible hexadecimal octets in the URI path */
      for (i = 0; ds->str[i]; ++i) {
         if (ds->str[i] == '%' &&
             (val = File_parse_hex_octet(ds->str + i+1)) != -1) {
            ds->str[i] = val;
            dStr_erase(ds, i+1, 2);
         }
      }
      /* Remove the fragment if not a part of the filename */
      if ((p = strrchr(ds->str, '#')) != NULL && access(ds->str, F_OK) != 0)
         dStr_truncate(ds, p - ds->str);
      ret = ds->str;
      dStr_free(ds, 0);
   }

   return ret;
}

/*
 * Set the style flag and ask for a reload, so it shows immediately.
 */
static void File_toggle_html_style(ClientInfo *client)
{
   char *d_cmd;

   OLD_STYLE = !OLD_STYLE;
   d_cmd = a_Dpip_build_cmd("cmd=%s", "reload_request");
   a_Dpip_dsh_write_str(client->sh, 1, d_cmd);
   dFree(d_cmd);
}

/*
 * Perform any necessary cleanups upon abnormal termination
 */
static void termination_handler(int signum)
{
  MSG("\nexit(signum), signum=%d\n\n", signum);
  exit(signum);
}


/* Client handling ----------------------------------------------------------*/

/*
 * Add a new client to the list.
 */
static ClientInfo *File_add_client(int sock_fd)
{
   ClientInfo *new_client;

   new_client = dNew(ClientInfo, 1);
   new_client->sh = a_Dpip_dsh_new(sock_fd, sock_fd, 8*1024);
   new_client->orig_url = NULL;
   new_client->filename = NULL;
   new_client->file_fd = -1;
   new_client->file_sz = 0;
   new_client->d_dir = NULL;
   new_client->state = 0;
   new_client->err_code = 0;
   new_client->flags = FILE_READ;
   new_client->old_style = OLD_STYLE;

   dList_append(Clients, new_client);
   return new_client;
}

/*
 * Remove a client from the list.
 */
static void File_remove_client(ClientInfo *client)
{
   dList_remove(Clients, (void *)client);

   _MSG("Closing Socket Handler\n");
   a_Dpip_dsh_close(client->sh);
   a_Dpip_dsh_free(client->sh);
   File_close(client->file_fd);
   dFree(client->orig_url);
   dFree(client->filename);
   File_dillodir_free(client->d_dir);

   dFree(client);
}

/*
 * Serve this client.
 */
static void File_serve_client(void *data, int f_write)
{
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *path;
   ClientInfo *client = data;
   int st;

   while (1) {
      _MSG("File_serve_client %p, flags=%d state=%d\n",
          client, client->flags, client->state);
      if (client->flags & (FILE_DONE | FILE_ERR))
         break;
      if (client->flags & FILE_READ) {
         dpip_tag = a_Dpip_dsh_read_token(client->sh, 0);
         _MSG("dpip_tag={%s}\n", dpip_tag);
         if (!dpip_tag)
            break;
      }

      if (client->flags & FILE_READ) {
         if (!(client->flags & FILE_AUTH_OK)) {
            /* Authenticate our client... */
            st = a_Dpip_check_auth(dpip_tag);
            _MSG("a_Dpip_check_auth returned %d\n", st);
            client->flags |= (st == 1) ? FILE_AUTH_OK : FILE_ERR;
         } else {
            /* Get file request */
            cmd = a_Dpip_get_attr(dpip_tag, "cmd");
            url = a_Dpip_get_attr(dpip_tag, "url");
            path = File_normalize_path(url);
            if (cmd) {
               if (strcmp(cmd, "DpiBye") == 0) {
                  DPIBYE = 1;
                  MSG("(pid %d): Got DpiBye.\n", (int)getpid());
                  client->flags |= FILE_DONE;
               } else if (url && dStrnAsciiCasecmp(url, "dpi:", 4) == 0 &&
                          strcmp(url+4, "/file/toggle") == 0) {
                  File_toggle_html_style(client);
               } else if (path) {
                  File_get(client, path, url);
               } else {
                  client->flags |= FILE_ERR;
                  MSG("ERROR: URL was %s\n", url);
               }
            }
            dFree(path);
            dFree(url);
            dFree(cmd);
            dFree(dpip_tag);
            break;
         }
         dFree(dpip_tag);

      } else if (f_write) {
         /* send our answer */
         if (client->state == st_err)
            File_send_error_page(client);
         else if (client->d_dir)
            File_send_dir(client);
         else
            File_send_file(client);
         break;
      }
   } /*while*/

   client->flags |= (client->sh->status & DPIP_ERROR) ? FILE_ERR : 0;
   client->flags |= (client->sh->status & DPIP_EOF) ? FILE_DONE : 0;
}

/*
 * Serve the client queue.
 */
static void File_serve_clients()
{
   int i, f_read, f_write;
   ClientInfo *client;

   for (i = 0; (client = dList_nth_data(Clients, i)); ++i) {
      f_read = FD_ISSET(client->sh->fd_in, &read_set);
      f_write = FD_ISSET(client->sh->fd_out, &write_set);
      if (!f_read && !f_write)
         continue;
      File_serve_client(client, f_write);
      if (client->flags & (FILE_DONE | FILE_ERR)) {
         File_remove_client(client);
         --i;
      }
   }
}

/* --------------------------------------------------------------------------*/

/*
 * Check the fd sets for activity, with a max timeout.
 * return value: 0 if timeout, 1 if input available, -1 if error.
 */
static int File_check_fds(uint_t seconds)
{
   int i, st;
   ClientInfo *client;
   struct timeval timeout;

   /* initialize observed file descriptors */
   FD_ZERO (&read_set);
   FD_ZERO (&write_set);
   FD_SET (STDIN_FILENO, &read_set);
   for (i = 0; (client = dList_nth_data(Clients, i)); ++i) {
      if (client->flags & FILE_READ)
         FD_SET (client->sh->fd_in, &read_set);
      if (client->flags & FILE_WRITE)
         FD_SET (client->sh->fd_out, &write_set);
   }
   _MSG("Watching %d fds\n", dList_length(Clients) + 1);

   /* Initialize the timeout data structure. */
   timeout.tv_sec = seconds;
   timeout.tv_usec = 0;

   do {
      st = select(FD_SETSIZE, &read_set, &write_set, NULL, &timeout);
   } while (st == -1 && errno == EINTR);
/*
   MSG_RAW(" (%d%s%s)", STDIN_FILENO,
           FD_ISSET(STDIN_FILENO, &read_set) ? "R" : "",
           FD_ISSET(STDIN_FILENO, &write_set) ? "W" : "");
   for (i = 0; (client = dList_nth_data(Clients, i)); ++i) {
      MSG_RAW(" (%d%s%s)", client->sh->fd_in,
              FD_ISSET(client->sh->fd_in, &read_set) ? "R" : "",
              FD_ISSET(client->sh->fd_out, &write_set) ? "W" : "");
   }
   MSG_RAW("\n");
*/
   return st;
}


int main(void)
{
   struct sockaddr_in sin;
   socklen_t sin_sz;
   int sock_fd, c_st, st = 1;

   /* Arrange the cleanup function for abnormal terminations */
   if (signal (SIGINT, termination_handler) == SIG_IGN)
     signal (SIGINT, SIG_IGN);
   if (signal (SIGHUP, termination_handler) == SIG_IGN)
     signal (SIGHUP, SIG_IGN);
   if (signal (SIGTERM, termination_handler) == SIG_IGN)
     signal (SIGTERM, SIG_IGN);

   MSG("(v.2) accepting connections...\n");
   //sleep(20);

   /* initialize observed file descriptors */
   FD_ZERO (&read_set);
   FD_ZERO (&write_set);
   FD_SET (STDIN_FILENO, &read_set);

   /* Set STDIN socket nonblocking (to ensure accept() never blocks) */
   fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK | fcntl(STDIN_FILENO, F_GETFL));

   /* initialize Clients list */
   Clients = dList_new(512);

   /* some OSes may need this... */
   sin_sz = sizeof(sin);

   /* start the service loop */
   while (!DPIBYE) {
      /* wait for activity */
      do {
         c_st = File_check_fds(10);
      } while (c_st == 0 && !DPIBYE);
      if (c_st < 0) {
         MSG(" select() %s\n", dStrerror(errno));
         break;
      }
      if (DPIBYE)
         break;

      if (FD_ISSET(STDIN_FILENO, &read_set)) {
         /* accept the incoming connection */
         do {
            sock_fd = accept(STDIN_FILENO, (struct sockaddr *)&sin, &sin_sz);
         } while (sock_fd < 0 && errno == EINTR);
         if (sock_fd == -1) {
            if (errno == EAGAIN)
               continue;
            MSG(" accept() %s\n", dStrerror(errno));
            break;
         } else {
            _MSG(" accept() fd=%d\n", sock_fd);
            /* Set nonblocking */
            fcntl(sock_fd, F_SETFL, O_NONBLOCK | fcntl(sock_fd, F_GETFL));
            /* Create and initialize a new client */
            File_add_client(sock_fd);
         }
         continue;
      }

      File_serve_clients();
   }

   if (DPIBYE)
      st = 0;
   return st;
}

