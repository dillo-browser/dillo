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

#include <pthread.h>

#include <ctype.h>           /* for tolower */
#include <errno.h>           /* for errno */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include "../dpip/dpip.h"
#include "dpiutil.h"
#include "d_size.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[file dpi]: " __VA_ARGS__)


#define MAXNAMESIZE 30
#define HIDE_DOTFILES TRUE

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
   int status;
   int old_style;
   pthread_t thrID;
   int done;
} ClientInfo;

/*
 * Forward references
 */
static const char *File_content_type(const char *filename);
static int File_get_file(ClientInfo *Client,
                         const char *filename,
                         struct stat *sb,
                         const char *orig_url);
static int File_get_dir(ClientInfo *Client,
                        const char *DirName,
                        const char *orig_url);

/*
 * Global variables
 */
static volatile int DPIBYE = 0;
static volatile int ThreadRunning = 0;
static int OLD_STYLE = 0;
/* A list for the clients we are serving */
static Dlist *Clients;
/* a mutex for operations on clients */
static pthread_mutex_t ClMut;

/*
 * Close a file descriptor, but handling EINTR
 */
static void File_close(int fd)
{
   while (close(fd) < 0 && errno == EINTR)
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
   if ((Size - i >= 5  && !dStrncasecmp(p+i, "<html", 5)) ||
       (Size - i >= 5  && !dStrncasecmp(p+i, "<head", 5)) ||
       (Size - i >= 6  && !dStrncasecmp(p+i, "<title", 6)) ||
       (Size - i >= 14 && !dStrncasecmp(p+i, "<!doctype html", 14)) ||
       /* this line is workaround for FTP through the Squid proxy */
       (Size - i >= 17 && !dStrncasecmp(p+i, "<!-- HTML listing", 17))) {

      Type = 1;

   /* Images */
   } else if (Size >= 4 && !dStrncasecmp(p, "GIF8", 4)) {
      Type = 3;
   } else if (Size >= 4 && !dStrncasecmp(p, "\x89PNG", 4)) {
      Type = 4;
   } else if (Size >= 2 && !dStrncasecmp(p, "\xff\xd8", 2)) {
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
static void File_print_parent_dir(ClientInfo *Client, const char *dirname)
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
      a_Dpip_dsh_printf(Client->sh, 0,
         "<a href='file:%s'>Parent directory</a>", HUparent);
      dFree(HUparent);
      dFree(Uparent);
      dFree(parent);
   }
}

/*
 * Given a timestamp, output an HTML-formatted date string.
 */
static void File_print_mtime(ClientInfo *Client, time_t mtime)
{
   char *ds = ctime(&mtime);

   /* Month, day and {hour or year} */
   if (Client->old_style) {
      a_Dpip_dsh_printf(Client->sh, 0, " %.3s %.2s", ds + 4, ds + 8);
      if (time(NULL) - mtime > 15811200) {
         a_Dpip_dsh_printf(Client->sh, 0, "  %.4s", ds + 20);
      } else {
         a_Dpip_dsh_printf(Client->sh, 0, " %.5s", ds + 11);
      }
   } else {
      a_Dpip_dsh_printf(Client->sh, 0,
         "<td>%.3s&nbsp;%.2s&nbsp;%.5s", ds + 4, ds + 8,
         /* (more than 6 months old) ? year : hour; */
         (time(NULL) - mtime > 15811200) ? ds + 20 : ds + 11);
   }
}

/*
 * Return a HTML-line from file info.
 */
static void File_info2html(ClientInfo *Client, FileInfo *finfo, int n)
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
      sizeunits = "Kb";
   } else {
      size = finfo->size / 1048576 + (finfo->size % 1048576 >= 1048576 / 2);
      sizeunits = "Mb";
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

   if (Client->old_style) {
      char *dots = ".. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..";
      int ndots = MAXNAMESIZE - strlen(name);
      a_Dpip_dsh_printf(Client->sh, 0,
         "%s<a href='%s'>%s</a>"
         " %s"
         " %-11s%4d %-5s",
         S_ISDIR (finfo->mode) ? ">" : " ", HUref, Hname,
         dots + 50 - (ndots > 0 ? ndots : 0),
         filecont, size, sizeunits);

   } else {
      a_Dpip_dsh_printf(Client->sh, 0,
         "<tr align=center %s><td>%s<td align=left><a href='%s'>%s</a>"
         "<td>%s<td>%d&nbsp;%s",
         (n & 1) ? "bgcolor=#dcdcdc" : "",
         S_ISDIR (finfo->mode) ? ">" : " ", HUref, Hname,
         filecont, size, sizeunits);
   }
   File_print_mtime(Client, finfo->mtime);
   a_Dpip_dsh_write_str(Client->sh, 0, "\n");

   dFree(Hname);
   dFree(HUref);
   dFree(Uref);
}

/*
 * Read a local directory and translate it to html.
 */
static void File_transfer_dir(ClientInfo *Client,
                              DilloDir *Ddir, const char *orig_url)
{
   int n;
   char *d_cmd, *Hdirname, *Udirname, *HUdirname;

   /* Send DPI header */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", orig_url);
   a_Dpip_dsh_write_str(Client->sh, 1, d_cmd);
   dFree(d_cmd);

   /* Send page title */
   Udirname = Escape_uri_str(Ddir->dirname, NULL);
   HUdirname = Escape_html_str(Udirname);
   Hdirname = Escape_html_str(Ddir->dirname);

   a_Dpip_dsh_printf(Client->sh, 0,
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

   if (Client->old_style) {
      a_Dpip_dsh_write_str(Client->sh, 0, "<pre>\n");
   }

   /* Output the parent directory */
   File_print_parent_dir(Client, Ddir->dirname);

   /* HTML style toggle */
   a_Dpip_dsh_write_str(Client->sh, 0,
      "&nbsp;&nbsp;<a href='dpi:/file/toggle'>%</a>\n");

   if (dList_length(Ddir->flist)) {
      if (Client->old_style) {
         a_Dpip_dsh_write_str(Client->sh, 0, "\n\n");
      } else {
         a_Dpip_dsh_write_str(Client->sh, 0,
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
      a_Dpip_dsh_write_str(Client->sh, 0, "<br><br>Directory is empty...");
   }

   /* Output entries */
   for (n = 0; n < dList_length(Ddir->flist); ++n) {
      File_info2html(Client, dList_nth_data(Ddir->flist,n), n+1);
   }

   if (dList_length(Ddir->flist)) {
      if (Client->old_style) {
         a_Dpip_dsh_write_str(Client->sh, 0, "</pre>\n");
      } else {
         a_Dpip_dsh_write_str(Client->sh, 0, "</table>\n");
      }
   }

   a_Dpip_dsh_write_str(Client->sh, 0, "</BODY></HTML>\n");
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

   if (!dStrcasecmp(e, "gif")) {
      return "image/gif";
   } else if (!dStrcasecmp(e, "jpg") ||
              !dStrcasecmp(e, "jpeg")) {
      return "image/jpeg";
   } else if (!dStrcasecmp(e, "png")) {
      return "image/png";
   } else if (!dStrcasecmp(e, "html") ||
              !dStrcasecmp(e, "htm") ||
              !dStrcasecmp(e, "shtml")) {
      return "text/html";
   } else if (!dStrcasecmp(e, "txt")) {
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

   return ct;
}

/*
 * Send an error page
 */
static void File_send_error_page(ClientInfo *Client, int res,
                                 const char *orig_url)
{
   const char *status;
   const char *body;
   char *d_cmd;

   switch (res) {
   case EACCES:
      status = "403 Forbidden";
      break;
   case ENOENT:
      status = "404 Not Found";
      break;
   default:
      /* good enough */
      status = "500 Internal Server Error";
   }

   body = status; /* it's simple */

   /* Send DPI command */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", orig_url);
   a_Dpip_dsh_write_str(Client->sh, 1, d_cmd);
   dFree(d_cmd);

   a_Dpip_dsh_printf(Client->sh, 0, 
                     "HTTP/1.1 %s\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %ld\r\n"
                     "\r\n"
                     "%s",
                     status, strlen(body), body);
}

/*
 * Try to stat the file and determine if it's readable.
 */
static void File_get(ClientInfo *Client, const char *filename,
                     const char *orig_url)
{
   int res;
   struct stat sb;

   if (stat(filename, &sb) != 0) {
      /* stat failed, prepare a file-not-found error. */
      res = ENOENT;
   } else if (S_ISDIR(sb.st_mode)) {
      /* set up for reading directory */
      res = File_get_dir(Client, filename, orig_url);
   } else {
      /* set up for reading a file */
      res = File_get_file(Client, filename, &sb, orig_url);
   }
   if (res != 0) {
      File_send_error_page(Client, res, orig_url);
   }
}

/*
 *
 */
static int File_get_dir(ClientInfo *Client,
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
      File_transfer_dir(Client, Ddir, orig_url);
      File_dillodir_free(Ddir);
      return 0;
   } else
      return EACCES;
}

/*
 * Send HTTP headers and then send the file itself.
 */
static int File_get_file(ClientInfo *Client,
                         const char *filename,
                         struct stat *sb,
                         const char *orig_url)
{
#define LBUF 16*1024

   const char *ct;
   const char *unknown_type = "application/octet-stream";
   char buf[LBUF], *d_cmd, *name;
   int fd, st, namelen;
   bool_t gzipped = FALSE;

   if ((fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0)
      return errno;

   /* Send DPI command */
   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", orig_url);
   a_Dpip_dsh_write_str(Client->sh, 1, d_cmd);
   dFree(d_cmd);

   /* Check for gzipped file */
   namelen = strlen(filename);
   if (namelen > 3 && !dStrcasecmp(filename + namelen - 3, ".gz")) {
      gzipped = TRUE;
      namelen -= 3;
   }

   /* Content-Type info is based on filename extension (with ".gz" removed).
    * If there's no known extension, perform data sniffing.
    * If this doesn't lead to a conclusion, use "application/octet-stream".
    */
   name = dStrndup(filename, namelen);
   if (!(ct = File_content_type(name)))
      ct = unknown_type;
   dFree(name);

   /* Send HTTP headers */
   a_Dpip_dsh_write_str(Client->sh, 0, "HTTP/1.1 200 OK\r\n");
   if (gzipped) {
      a_Dpip_dsh_write_str(Client->sh, 0, "Content-Encoding: gzip\r\n");
   }
   if (!gzipped || strcmp(ct, unknown_type)) {
      a_Dpip_dsh_printf(Client->sh, 0, "Content-Type: %s\r\n", ct);
   } else {
      /* If we don't know type for gzipped data, let dillo figure it out. */
   }
   a_Dpip_dsh_printf(Client->sh, 0,
                     "Content-Length: %ld\r\n"
                     "\r\n",
                     sb->st_size);

   /* Send body -- raw file contents */
   do {
      if ((st = read(fd, buf, LBUF)) > 0) {
         if (a_Dpip_dsh_write(Client->sh, 0, buf, (size_t)st) != 0)
            break;
      } else if (st < 0) {
         perror("[read]");
         if (errno == EINTR || errno == EAGAIN)
            continue;
      }
   } while (st > 0);

   /* TODO: It may be better to send an error report to dillo instead of
    *       calling exit() */
   if (st == -1) {
      MSG("ERROR while reading from file \"%s\", error was \"%s\"\n",
          filename, dStrerror(errno));
      exit(1);
   }

   File_close(fd);

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

   /* Make sure the string starts with "file:/" */
   if (strncmp(str, "file:/", 5) != 0)
      return ret;
   str += 5;

   /* Skip "localhost" */
   if (dStrncasecmp(str, "//localhost/", 12) == 0)
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
static void File_toggle_html_style(ClientInfo *Client)
{
   char *d_cmd;

   OLD_STYLE = !OLD_STYLE;
   d_cmd = a_Dpip_build_cmd("cmd=%s", "reload_request");
   a_Dpip_dsh_write_str(Client->sh, 1, d_cmd);
   dFree(d_cmd);
}

/*
 * Perform any necessary cleanups upon abnormal termination
 */
static void termination_handler(int signum)
{
  exit(signum);
}


/* Client handling ----------------------------------------------------------*/

/*
 * Add a new client to the list.
 */
static ClientInfo *File_add_client(int sock_fd)
{
   ClientInfo *NewClient;

   NewClient = dNew(ClientInfo, 1);
   NewClient->sh = a_Dpip_dsh_new(sock_fd, sock_fd, 8*1024);
   NewClient->status = 0;
   NewClient->done = 0;
   NewClient->old_style = OLD_STYLE;
  pthread_mutex_lock(&ClMut);
   dList_append(Clients, NewClient);
  pthread_mutex_unlock(&ClMut);
   return NewClient;
}

/*
 * Get client record by number
 */
static void *File_get_client_n(uint_t n)
{
   void *client;

  pthread_mutex_lock(&ClMut);
   client = dList_nth_data(Clients, n);
  pthread_mutex_unlock(&ClMut);

   return client;
}

/*
 * Remove a client from the list.
 */
static void File_remove_client_n(uint_t n)
{
   ClientInfo *Client;

  pthread_mutex_lock(&ClMut);
   Client = dList_nth_data(Clients, n);
   dList_remove(Clients, (void *)Client);
  pthread_mutex_unlock(&ClMut);

   _MSG("Closing Socket Handler\n");
   a_Dpip_dsh_close(Client->sh);
   a_Dpip_dsh_free(Client->sh);
   dFree(Client);
}

/*
 * Return the number of clients.
 */
static int File_num_clients(void)
{
   uint_t n;

  pthread_mutex_lock(&ClMut);
   n = dList_length(Clients);
  pthread_mutex_unlock(&ClMut);

   return n;
}

/*
 * Serve this client.
 * (this function runs on its own thread)
 */
static void File_serve_client(void *data)
{
   char *dpip_tag = NULL, *cmd = NULL, *url = NULL, *path;
   ClientInfo *Client = data;
   int st;

   /* Authenticate our client... */
   do {
      dpip_tag = a_Dpip_dsh_read_token(Client->sh);
   } while (!dpip_tag && Client->sh->status == DPIP_EAGAIN);
   _MSG("auth={%s}\n", dpip_tag);
   st = a_Dpip_check_auth(dpip_tag);
   dFree(dpip_tag);
   _MSG("a_Dpip_check_auth returned %d\n", st);
   dReturn_if (st == -1);

   /* Read the dpi command */
   do {
      dpip_tag = a_Dpip_dsh_read_token(Client->sh);
   } while (!dpip_tag && Client->sh->status == DPIP_EAGAIN);
   _MSG("dpip_tag={%s}\n", dpip_tag);

   if (dpip_tag) {
      cmd = a_Dpip_get_attr(dpip_tag, "cmd");
      if (cmd) {
         if (strcmp(cmd, "DpiBye") == 0) {
            DPIBYE = 1;
            MSG("file.dpi:: (pid %d): Got DpiBye.\n", (int)getpid());
         } else {
            url = a_Dpip_get_attr(dpip_tag, "url");
            if (!url)
               MSG("file.dpi:: Failed to parse 'url'\n");
         }
      }
      dFree(cmd);
   }
   dFree(dpip_tag);

   if (!DPIBYE && url) {
      _MSG("url = '%s'\n", url);

      path = File_normalize_path(url);
      if (path) {
         _MSG("path = '%s'\n", path);
         File_get(Client, path, url);
      } else if (strcmp(url, "dpi:/file/toggle") == 0) {
         File_toggle_html_style(Client);
      } else {
         MSG("ERROR: URL path was %s\n", url);
      }
      dFree(path);
   }
   dFree(url);

   /* flag the the transfer finished */
   Client->done = 1;
}

/*
 * Serve the client queue.
 * (this function runs on its own thread)
 */
static void *File_serve_clients(void *client)
{
   /* switch to detached state */
   pthread_detach(pthread_self());

   while (File_num_clients()) {
      client = File_get_client_n((uint_t)0);
      File_serve_client(client);
      File_remove_client_n((uint_t)0);
   }
   ThreadRunning = 0;

   return NULL;
}

/* --------------------------------------------------------------------------*/

/*
 * Check a fd for activity, with a max timeout.
 * return value: 0 if timeout, 1 if input available, -1 if error.
 */
static int File_check_fd(int filedes, uint_t seconds)
{
  int st;
  fd_set set;
  struct timeval timeout;

  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  do {
     st = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
  } while (st == -1 && errno == EINTR);

  return st;
}


int main(void)
{
   ClientInfo *NewClient;
   struct sockaddr_un spun;
   int temp_sock_descriptor;
   socklen_t address_size;
   int c_st, st = 1;
   uint_t i;

   /* Arrange the cleanup function for abnormal terminations */
   if (signal (SIGINT, termination_handler) == SIG_IGN)
     signal (SIGINT, SIG_IGN);
   if (signal (SIGHUP, termination_handler) == SIG_IGN)
     signal (SIGHUP, SIG_IGN);
   if (signal (SIGTERM, termination_handler) == SIG_IGN)
     signal (SIGTERM, SIG_IGN);

   MSG("(v.1) accepting connections...\n");

   /* initialize mutex */
   pthread_mutex_init(&ClMut, NULL);

   /* initialize Clients list */
   Clients = dList_new(512);

   /* some OSes may need this... */
   address_size = sizeof(struct sockaddr_un);

   /* start the service loop */
   while (!DPIBYE) {
      /* wait for a connection */
      do {
         c_st = File_check_fd(STDIN_FILENO, 1);
      } while (c_st == 0 && !DPIBYE);
      if (c_st < 0) {
         perror("[select]");
         break;
      }
      if (DPIBYE)
         break;

      temp_sock_descriptor =
         accept(STDIN_FILENO, (struct sockaddr *)&spun, &address_size);

      if (temp_sock_descriptor == -1) {
         perror("[accept]");
         break;
      }

      /* Create and initialize a new client */
      NewClient = File_add_client(temp_sock_descriptor);

      if (!ThreadRunning) {
         ThreadRunning = 1;
         /* Serve the client from a thread (avoids deadlocks) */
         if (pthread_create(&NewClient->thrID, NULL,
                            File_serve_clients, NewClient) != 0) {
            perror("[pthread_create]");
            ThreadRunning = 0;
            break;
         }
      }
   }

   /* TODO: handle a running thread better. */
   for (i = 0; i < 5 && ThreadRunning; ++i) {
      MSG("sleep i=%u", i);
      sleep(i);
   }

   if (DPIBYE)
      st = 0;
   return st;
}

