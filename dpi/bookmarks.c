/*
 * Bookmarks server (chat version).
 *
 * NOTE: this code illustrates how to make a dpi-program.
 *
 * Copyright 2002-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 */

/* TODO: this server is not assembling the received packets.
 * This means it currently expects dillo to send full dpi tags
 * within the socket; if that fails, everything stops.
 * This is not hard to fix, mainly is a matter of expecting the
 * final '>' of a tag.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include "../dpip/dpip.h"
#include "dpiutil.h"


/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[bookmarks dpi]: " __VA_ARGS__)

#define DOCTYPE \
   "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"

/*
 * Notes on character escaping:
 *   - Basically things are saved unescaped and escaped when in memory.
 *   - &<>"' are escaped in titles and sections and saved unescaped.
 *   - ' is escaped as %27 in URLs and saved escaped.
 */
typedef struct {
   int key;
   int section;
   char *url;
   char *title;
} BmRec;

typedef struct {
   int section;
   char *title;

   int o_sec;   /* private, for normalization */
} BmSec;


/*
 * Local data
 */
static char *Header = "Content-type: text/html\n\n";
static char *BmFile = NULL;
static time_t BmFileTimeStamp = 0;
static Dlist *B_bms = NULL;
static int bm_key = 0;

static Dlist *B_secs = NULL;
static int sec_key = 0;

static int MODIFY_PAGE_NUM = 1;


/*
 * Forward declarations
 */


/* -- HTML templates ------------------------------------------------------- */

static const char *mainpage_header =
DOCTYPE
"<html>\n"
"<head>\n"
"<title>Bookmarks</title>\n"
"</head>\n"
"<body id='dillo_bm' bgcolor='#778899' link='black' vlink='brown'>\n"
"<table border='1' cellpadding='0' width='100%'>\n"
" <tr><td>\n"
"  <table width='100%' bgcolor='#b4b4b4'>\n"
"   <tr>\n"
"    <td> Bookmarks :: </td>\n"
"    <td align='right'>\n"
"     [<a href='dpi:/bm/modify'>modify</a>]\n"
"    </td></tr>\n"
"  </table></td></tr>\n"
"</table>\n"
"<br>\n";

static const char *modifypage_header =
DOCTYPE
"<html>\n"
"<head>\n"
"<title>Bookmarks</title>\n"
"</head>\n"
"<body id='dillo_bm' bgcolor='#778899' link='black' vlink='brown'>\n"
"<table border='1' cellpadding='0' width='100%'>\n"
" <tr><td>\n"
"  <table width='100%' bgcolor='#b4b4b4'>\n"
"   <tr>\n"
"    <td> Bookmarks :: modify</td>\n"
"    <td align='right'>\n"
"     [<a href='dpi:/bm/'>cancel</a>]\n"
"    </td>\n"
"   </tr>\n"
"  </table></td></tr>                            \n"
"</table>                                        \n"
"\n"
"<form action='modify'>\n"
"<table width='100%' border='1' cellpadding='0'>\n"
" <tr style='background-color: teal'>\n"
"  <td>\n"
"   <b>Select an operation</b>\n"
"   <select name='operation'>\n"
"    <option value='none' selected>--\n"
"    <option value='delete'>Delete\n"
"    <option value='move'>Move\n"
"    <option value='modify'>Modify\n"
"    <option value='add_sec'>Add Section\n"
"    <option value='add_url'>Add URL\n"
"   </select>\n"
"   <b>, mark its operands, and</b>\n"
"   <input type='submit' name='submit' value='submit.'>\n"
"  </td>\n"
" </tr>\n"
"</table>\n";

static const char *mainpage_sections_header =
"<table border='1' cellpadding='0' cellspacing='20' width='100%'>\n"
" <tr valign='top'>\n"
"  <td>\n"
"   <table bgcolor='#b4b4b4' border='2' cellpadding='4' cellspacing='1'>\n"
"    <tr><td>\n"
"     <table width='100%' bgcolor='#b4b4b4'>\n"
"      <tr><td><small>Sections:</small></td></tr></table></td></tr>\n";

static const char *modifypage_sections_header =
"<table border='1' cellpadding='0' cellspacing='20' width='100%'>\n"
" <tr valign='top'>\n"
"  <td>\n"
"   <table bgcolor='#b4b4b4' border='1'>\n"
"    <tr><td>\n"
"     <table width='100%' bgcolor='#b4b4b4'>\n"
"      <tr><td><small>Sections:</small></td></tr></table></td></tr>\n";

static const char *mainpage_sections_item =
"    <tr><td align='center'>\n"
"      <a href='#s%d'>%s</a></td></tr>\n";

static const char *modifypage_sections_item =
"    <tr><td>\n"
"     <table width='100%%'>\n"
"      <tr align='center'>"
"       <td><input type='checkbox' name='s%d'></td>\n"
"       <td width='100%%'><a href='#s%d'>%s</a></td></tr></table></td></tr>\n";

static const char *mainpage_sections_footer =
"   </table>\n";

static const char *modifypage_sections_footer =
"   </table>\n";

static const char *mainpage_middle1 =
"  </td>\n"
"  <td width='100%'>\n";

static const char *modifypage_middle1 =
"  </td>\n"
"  <td width='100%'>\n";

static const char *mainpage_section_card_header =
"   <a name='s%d'></a>\n"
"   <table bgcolor='#bfbfbf' width='100%%' cellspacing='2'>\n"
"    <tr>\n"
"     <td bgcolor='#bf0c0c'><font color='white'><b>\n"
"      &nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;&nbsp;</b></font></td>\n"
"     <td bgcolor='white' width='100%%'>&nbsp;</td></tr>\n";

static const char *modifypage_section_card_header =
"   <a name='s%d'></a>\n"
"   <table bgcolor='#bfbfbf' width='100%%' cellspacing='2'>\n"
"    <tr>\n"
"     <td bgcolor='#bf0c0c'><font color='white'><b>\n"
"      &nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;&nbsp;</b></font></td>\n"
"     <td bgcolor='white' width='100%%'>&nbsp;</td></tr>\n";

static const char *mainpage_section_card_item =
"    <tr><td colspan='2'>\n"
"      <a href='%s'>%s</a> </td></tr>\n";

static const char *modifypage_section_card_item =
"    <tr>\n"
"     <td colspan='2'><input type='checkbox' name='url%d'>\n"
"      <a href='%s'>%s</a></td></tr>\n";

static const char *mainpage_section_card_footer =
"   </table>\n"
"   <hr>\n";

static const char *modifypage_section_card_footer =
"   </table>\n"
"   <hr>\n";

static const char *mainpage_footer =
"  </td>\n"
" </tr>\n"
"</table>\n"
"</body>\n"
"</html>\n";

static const char *modifypage_footer =
"  </td>\n"
" </tr>\n"
"</table>\n"
"</form>\n"
"</body>\n"
"</html>\n";

/* ------------------------------------------------------------------------- */
static const char *modifypage_add_section_page =
DOCTYPE
"<html>\n"
"<head>\n"
"<title>Bookmarks</title>\n"
"</head>\n"
"<body id='dillo_bm' bgcolor='#778899' link='black' vlink='brown'>\n"
"<table border='1' cellpadding='0' width='100%'>\n"
" <tr><td colspan='2'>\n"
"  <table bgcolor='#b4b4b4' width='100%'>\n"
"   <tr>\n"
"    <td bgcolor='#b4b4b4'>\n"
"     Modify bookmarks :: add section\n"
"    </td>\n"
"    <td align='right'>\n"
"     [<a href='dpi:/bm/'>cancel</a>]\n"
"    </td>\n"
"   </tr>\n"
"  </table></td></tr>\n"
"</table>\n"
"<br>\n"
"<form action='modify'>\n"
" <input type='hidden' name='operation' value='add_section'>\n"
"<table border='1' width='100%'>\n"
" <tr>\n"
"  <td bgcolor='olive'><b>New&nbsp;section:</b></td>\n"
"  <td bgcolor='white' width='100%'></td></tr>\n"
"</table>\n"
"<table width='100%' cellpadding='10'>\n"
"<tr><td>\n"
" <table width='100%' bgcolor='teal'>\n"
"  <tr>\n"
"   <td>Title:</td>\n"
"   <td><input type='text' name='title' size='64'></td></tr>\n"
" </table>\n"
" </td></tr>\n"
"</table>\n"
"<table width='100%' cellpadding='4' border='0'>\n"
"<tr><td bgcolor='#a0a0a0'>\n"
" <input type='submit' name='submit' value='submit.'></td></tr>\n"
"</table>\n"
"</form>\n"
"</body>\n"
"</html>\n"
"\n";

/* ------------------------------------------------------------------------- */
static const char *modifypage_update_header =
DOCTYPE
"<html>\n"
"<head>\n"
"<title>Bookmarks</title>\n"
"</head>\n"
"<body id='dillo_bm' bgcolor='#778899' link='black' vlink='brown'>\n"
"<table border='1' cellpadding='0' width='100%'>\n"
" <tr><td colspan='2'>\n"
"  <table bgcolor='#b4b4b4' width='100%'>\n"
"   <tr><td bgcolor='#b4b4b4'> Modify bookmarks :: update\n"
"    </td>\n"
"    <td align='right'>\n"
"     [<a href='dpi:/bm/'>cancel</a>]\n"
"    </td>\n"
"   </tr>\n"
"  </table></td></tr>\n"
"</table>\n"
"<br>\n"
"<form action='modify'>\n"
"<input type='hidden' name='operation' value='modify2'>\n";

static const char *modifypage_update_title =
"<table border='1' width='100%%'>\n"
" <tr>\n"
"  <td bgcolor='olive'><b>%s</b></td>\n"
"  <td bgcolor='white' width='100%%'></td></tr>\n"
"</table>\n";

static const char *modifypage_update_item_header =
"<table width='100%' cellpadding='10'>\n";

static const char *modifypage_update_item =
"<tr><td>\n"
" <table width='100%%' bgcolor='teal'>\n"
"  <tr>\n"
"   <td>Title:</td>\n"
"   <td><input type='text' name='title%d' size='64'\n"
"        value='%s'></td></tr>\n"
"  <tr>\n"
"   <td>URL:</td>\n"
"   <td>%s</td></tr>\n"
" </table>\n"
" </td></tr>\n";

static const char *modifypage_update_item2 =
"<tr><td>\n"
" <table width='100%%' bgcolor='teal'>\n"
"  <tr>\n"
"   <td>Title:</td>\n"
"   <td><input type='text' name='s%d' size='64'\n"
"        value='%s'></td></tr>\n"
" </table>\n"
" </td></tr>\n";

static const char *modifypage_update_item_footer =
"</table>\n";

static const char *modifypage_update_footer =
"<table width='100%' cellpadding='4' border='0'>\n"
"<tr><td bgcolor='#a0a0a0'>\n"
" <input type='submit' name='submit' value='submit.'></td></tr>\n"
"</table>\n"
"</form>\n"
"</body>\n"
"</html>\n";

/* ------------------------------------------------------------------------- */
static const char *modifypage_add_url =
DOCTYPE
"<html>\n"
"<head>\n"
"<title>Bookmarks</title>\n"
"</head>\n"
"<body id='dillo_bm' bgcolor='#778899' link='black' vlink='brown'>\n"
"<table border='1' cellpadding='0' width='100%'>\n"
" <tr><td colspan='2'>\n"
"  <table bgcolor='#b4b4b4' width='100%'>\n"
"   <tr><td bgcolor='#b4b4b4'> Modify bookmarks :: add url\n"
"    </td>\n"
"    <td align='right'>\n"
"     [<a href='dpi:/bm/'>cancel</a>]\n"
"    </td>\n"
"   </tr>\n"
"  </table></td></tr>\n"
"</table>\n"
"<br>\n"
"<form action='modify'>\n"
"<input type='hidden' name='operation' value='add_url2'>\n"
"<table border='1' width='100%'>\n"
" <tr>\n"
"  <td bgcolor='olive'><b>Add&nbsp;url:</b></td>\n"
"  <td bgcolor='white' width='100%'></td></tr>\n"
"</table>\n"
"<table width='100%' cellpadding='10'>\n"
"<tr><td>\n"
" <table width='100%' bgcolor='teal'>\n"
"  <tr>\n"
"   <td>Title:</td>\n"
"   <td><input type='text' name='title' size='64'></td></tr>\n"
"  <tr>\n"
"   <td>URL:</td>\n"
"   <td><input type='text' name='url' size='64'></td></tr>\n"
" </table>\n"
" </td></tr>\n"
"</table>\n"
"<table width='100%' cellpadding='4' border='0'>\n"
"<tr><td bgcolor='#a0a0a0'>\n"
" <input type='submit' name='submit' value='submit.'></td></tr>\n"
"</table>\n"
"</form>\n"
"</body>\n"
"</html>\n";


/* ------------------------------------------------------------------------- */

/*
 * Return a new string with spaces changed with &nbsp;
 */
static char *make_one_line_str(char *str)
{
   char *new_str;
   int i, j, n;

   for (i = 0, n = 0; str[i]; ++i)
      if (str[i] == ' ')
         ++n;

   new_str = dNew(char, strlen(str) + 6*n + 1);
   new_str[0] = 0;

   for (i = 0, j = 0; str[i]; ++i) {
      if (str[i] == ' ') {
         strcpy(new_str + j, "&nbsp;");
         j += 6;
      } else {
         new_str[j] = str[i];
         new_str[++j] = 0;
      }
   }

   return new_str;
}

/*
 * Given an urlencoded string, return it to the original version.
 */
static void Unencode_str(char *e_str)
{
   char *p, *e;

   for (p = e = e_str; *e; e++, p++) {
      if (*e == '+') {
         *p = ' ';
      } else if (*e == '%') {
         if (dStrnAsciiCasecmp(e, "%0D%0A", 6) == 0) {
            *p = '\n';
            e += 5;
         } else {
            *p = (isdigit(e[1]) ? (e[1] - '0') : (e[1] - 'A' + 10)) * 16 +
                 (isdigit(e[2]) ? (e[2] - '0') : (e[2] - 'A' + 10));
            e += 2;
         }
      } else {
         *p = *e;
      }
   }
   *p = 0;
}

/*
 * Send a short message to dillo's status bar.
 */
static int Bmsrv_dpi_send_status_msg(Dsh *sh, char *str)
{
   int st;
   char *d_cmd;

   d_cmd = a_Dpip_build_cmd("cmd=%s msg=%s", "send_status_message", str);
   st = a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);
   return st;
}

/* -- ADT for bookmarks ---------------------------------------------------- */
/*
 * Compare function for searching a bookmark by its key
 */
static int Bms_node_by_key_cmp(const void *node, const void *key)
{
   return ((BmRec *)node)->key - VOIDP2INT(key);
}

/*
 * Compare function for searching a bookmark by section
 */
static int Bms_node_by_section_cmp(const void *node, const void *key)
{
   return ((BmRec *)node)->section - VOIDP2INT(key);
}

/*
 * Compare function for searching a section by its number
 */
static int Bms_sec_by_number_cmp(const void *node, const void *key)
{
   return ((BmSec *)node)->section - VOIDP2INT(key);
}

/*
 * Return the Bm record by key
 */
static BmRec *Bms_get(int key)
{
   return dList_find_custom(B_bms, INT2VOIDP(key), Bms_node_by_key_cmp);
}

/*
 * Return the Section record by key
 */
static BmSec *Bms_get_sec(int key)
{
   return dList_find_custom(B_secs, INT2VOIDP(key), Bms_sec_by_number_cmp);
}

/*
 * Add a bookmark
 */
static void Bms_add(int section, char *url, char *title)
{
   BmRec *bm_node;

   bm_node = dNew(BmRec, 1);
   bm_node->key = ++bm_key;
   bm_node->section = section;
   bm_node->url = Escape_uri_str(url, "'");
   bm_node->title = Escape_html_str(title);
   dList_append(B_bms, bm_node);
}

/*
 * Add a section
 */
static void Bms_sec_add(char *title)
{
   BmSec *sec_node;

   sec_node = dNew(BmSec, 1);
   sec_node->section = sec_key++;
   sec_node->title = Escape_html_str(title);
   dList_append(B_secs, sec_node);
}

/*
 * Delete a bookmark by its key
 */
static void Bms_del(int key)
{
   BmRec *bm_node;

   bm_node = dList_find_custom(B_bms, INT2VOIDP(key), Bms_node_by_key_cmp);
   if (bm_node) {
      dList_remove(B_bms, bm_node);
      dFree(bm_node->title);
      dFree(bm_node->url);
      dFree(bm_node);
   }
   if (dList_length(B_bms) == 0)
      bm_key = 0;
}

/*
 * Delete a section and its bookmarks by section number
 */
static void Bms_sec_del(int section)
{
   BmSec *sec_node;
   BmRec *bm_node;

   sec_node = dList_find_custom(B_secs, INT2VOIDP(section),
                                Bms_sec_by_number_cmp);
   if (sec_node) {
      dList_remove(B_secs, sec_node);
      dFree(sec_node->title);
      dFree(sec_node);

      /* iterate B_bms and remove those that match the section */
      while ((bm_node = dList_find_custom(B_bms, INT2VOIDP(section),
                                          Bms_node_by_section_cmp))) {
         Bms_del(bm_node->key);
      }
   }
   if (dList_length(B_secs) == 0)
      sec_key = 0;
}

/*
 * Move a bookmark to another section
 */
static void Bms_move(int key, int target_section)
{
   BmRec *bm_node;

   bm_node = dList_find_custom(B_bms, INT2VOIDP(key), Bms_node_by_key_cmp);
   if (bm_node) {
      bm_node->section = target_section;
   }
}

/*
 * Update a bookmark title by key
 */
static void Bms_update_title(int key, char *n_title)
{
   BmRec *bm_node;

   bm_node = dList_find_custom(B_bms, INT2VOIDP(key), Bms_node_by_key_cmp);
   if (bm_node) {
      dFree(bm_node->title);
      bm_node->title = Escape_html_str(n_title);
   }
}

/*
 * Update a section title by key
 */
static void Bms_update_sec_title(int key, char *n_title)
{
   BmSec *sec_node;

   sec_node = dList_find_custom(B_secs, INT2VOIDP(key), Bms_sec_by_number_cmp);
   if (sec_node) {
      dFree(sec_node->title);
      sec_node->title = Escape_html_str(n_title);
   }
}

/*
 * Free all the bookmarks data (bookmarks and sections)
 */
static void Bms_free(void)
{
   BmRec *bm_node;
   BmSec *sec_node;

   /* free B_bms */
   while ((bm_node = dList_nth_data(B_bms, 0))) {
      Bms_del(bm_node->key);
   }
   /* free B_secs */
   while ((sec_node = dList_nth_data(B_secs, 0))) {
      Bms_sec_del(sec_node->section);
   }
}

/*
 * Enforce increasing correlative section numbers with no jumps.
 */
static void Bms_normalize(void)
{
   BmRec *bm_node;
   BmSec *sec_node;
   int i, j;

   /* we need at least one section */
   if (dList_length(B_secs) == 0)
      Bms_sec_add("Unclassified");

   /* make correlative section numbers */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      sec_node->o_sec = sec_node->section;
      sec_node->section = i;
   }

   /* iterate B_secs and make the changes in B_bms */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      if (sec_node->section != sec_node->o_sec) {
         /* update section numbers */
         for (j = 0; (bm_node = dList_nth_data(B_bms, j)); ++j) {
            if (bm_node->section == sec_node->o_sec)
               bm_node->section = sec_node->section;
         }
      }
   }
}

/* -- Load bookmarks file -------------------------------------------------- */

/*
 * If there's no "bm.txt", create one from "bookmarks.html".
 */
static void Bms_check_import(void)
{
   char *OldBmFile;
   char *cmd1 =
      "echo \":s0: Unclassified\" > %s";
   char *cmd2 =
      "grep -i \"href\" %s | "
      "sed -e 's/<li><A HREF=\"/s0 /' -e 's/\">/ /' -e 's/<.*$//' >> %s";
   Dstr *dstr = dStr_new("");
   int rc;


   if (access(BmFile, F_OK) != 0) {
      OldBmFile = dStrconcat(dGethomedir(), "/.dillo/bookmarks.html", NULL);
      if (access(OldBmFile, F_OK) == 0) {
         dStr_sprintf(dstr, cmd1, BmFile);
         rc = system(dstr->str);
         if (rc == 127) {
            MSG("Bookmarks: /bin/sh could not be executed\n");
         } else if (rc == -1) {
            MSG("Bookmarks: process creation failure: %s\n",
                dStrerror(errno));
         }
         dStr_sprintf(dstr, cmd2, OldBmFile, BmFile);
         rc = system(dstr->str);
         if (rc == 127) {
            MSG("Bookmarks: /bin/sh could not be executed\n");
         } else if (rc == -1) {
            MSG("Bookmarks: process creation failure: %s\n",
                dStrerror(errno));
         }

         dStr_free(dstr, TRUE);
         dFree(OldBmFile);
      }
   }
}

/*
 * Load bookmarks data from a file
 */
static int Bms_load(void)
{
   FILE *BmTxt;
   char *buf, *p, *url, *title, *u_title;
   int section;
   struct stat TimeStamp;

   /* clear current bookmarks */
   Bms_free();

   /* open bm file */
   if (!(BmTxt = fopen(BmFile, "r"))) {
      perror("[fopen]");
      return 1;
   }

   /* load bm file into memory */
   while ((buf = dGetline(BmTxt)) != NULL) {
      if (buf[0] == 's') {
         /* get section, url and title */
         section = strtol(buf + 1, NULL, 10);
         p = strchr(buf, ' '); *p = 0;
         url = ++p;
         p = strchr(p, ' '); *p = 0;
         title = ++p;
         p = strchr(p, '\n'); *p = 0;
         u_title = Unescape_html_str(title);
         Bms_add(section, url, u_title);
         dFree(u_title);

      } else if (buf[0] == ':' && buf[1] == 's') {
         /* section = strtol(buf + 2, NULL, 10); */
         p = strchr(buf + 2, ' ');
         title = ++p;
         p = strchr(p, '\n'); *p = 0;
         Bms_sec_add(title);

      } else {
         MSG("Syntax error in bookmarks file:\n %s", buf);
      }
      dFree(buf);
   }
   fclose(BmTxt);

   /* keep track of the timestamp */
   stat(BmFile, &TimeStamp);
   BmFileTimeStamp = TimeStamp.st_mtime;

   return 0;
}

/*
 * Load bookmarks data if:
 *   - file timestamp is newer than ours  or
 *   - we haven't loaded anything yet :)
 */
static int Bms_cond_load(void)
{
   int st = 0;
   struct stat TimeStamp;

   if (stat(BmFile, &TimeStamp) != 0) {
      /* try to import... */
      Bms_check_import();
      if (stat(BmFile, &TimeStamp) != 0)
         TimeStamp.st_mtime = 0;
   }

   if (!BmFileTimeStamp || !dList_length(B_bms) || !dList_length(B_secs) ||
       BmFileTimeStamp < TimeStamp.st_mtime) {
      Bms_load();
      st = 1;
   }
   return st;
}

/* -- Save bookmarks file -------------------------------------------------- */

/*
 * Update the bookmarks file from memory contents
 * Return code: { 0:OK, 1:Abort }
 */
static int Bms_save(void)
{
   FILE *BmTxt;
   BmRec *bm_node;
   BmSec *sec_node;
   struct stat BmStat;
   char *u_title;
   int i, j;
   Dstr *dstr = dStr_new("");

   /* make a safety backup */
   if (stat(BmFile, &BmStat) == 0 && BmStat.st_size > 256) {
      char *BmFileBak = dStrconcat(BmFile, ".bak", NULL);
      rename(BmFile, BmFileBak);
      dFree(BmFileBak);
   }

   /* open bm file */
   if (!(BmTxt = fopen(BmFile, "w"))) {
      perror("[fopen]");
      return 1;
   }

   /* normalize */
   Bms_normalize();

   /* save sections */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      u_title = Unescape_html_str(sec_node->title);
      dStr_sprintf(dstr, ":s%d: %s\n", sec_node->section, u_title);
      fwrite(dstr->str, (size_t)dstr->len, 1, BmTxt);
      dFree(u_title);
   }

   /* save bookmarks  (section url title) */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      for (j = 0; (bm_node = dList_nth_data(B_bms, j)); ++j) {
         if (bm_node->section == sec_node->section) {
            u_title = Unescape_html_str(bm_node->title);
            dStr_sprintf(dstr, "s%d %s %s\n",
                         bm_node->section, bm_node->url, u_title);
            fwrite(dstr->str, (size_t)dstr->len, 1, BmTxt);
            dFree(u_title);
         }
      }
   }

   dStr_free(dstr, TRUE);
   fclose(BmTxt);

   /* keep track of the timestamp */
   stat(BmFile, &BmStat);
   BmFileTimeStamp = BmStat.st_mtime;

   return 0;
}

/* -- Add bookmark --------------------------------------------------------- */

/*
 * Add a new bookmark to DB :)
 */
static int Bmsrv_add_bm(Dsh *sh, char *url, char *title)
{
   char *u_title;
   char *msg="Added bookmark!";
   int section = 0;

   /* Add in memory */
   u_title = Unescape_html_str(title);
   Bms_add(section, url, u_title);
   dFree(u_title);

   /* Write to file */
   Bms_save();

   if (Bmsrv_dpi_send_status_msg(sh, msg))
      return 1;

   return 0;
}

/* -- Modify --------------------------------------------------------------- */

/*
 * Count how many sections and urls were marked in a request
 */
static void Bmsrv_count_urls_and_sections(char *url, int *n_sec, int *n_url)
{
   char *p, *q;
   int i;

   /* Check marked urls and sections */
   *n_sec = *n_url = 0;
   if ((p = strchr(url, '?'))) {
      for (q = p; (q = strstr(q, "&url")); ++q) {
         for (i = 0; isdigit(q[4+i]); ++i);
         *n_url += (q[4+i] == '=') ? 1 : 0;
      }
      for (q = p; (q = strstr(q, "&s")); ++q) {
         for (i = 0; isdigit(q[2+i]); ++i);
         *n_sec += (q[2+i] == '=') ? 1 : 0;
      }
   }
}

/*
 * Send a dpi reload request
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_reload_request(Dsh *sh, char *url)
{
   int st;
   char *d_cmd;

   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "reload_request", url);
   st = a_Dpip_dsh_write_str(sh, 1, d_cmd) ? 1 : 0;
   dFree(d_cmd);
   return st;
}

/*
 * Send the HTML for the modify page
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_modify_page(Dsh *sh)
{
   static Dstr *dstr = NULL;
   char *l_title;
   BmSec *sec_node;
   BmRec *bm_node;
   int i, j;

   if (!dstr)
      dstr = dStr_new("");

   /* send modify page header */
   if (a_Dpip_dsh_write_str(sh, 0, modifypage_header))
      return 1;

   /* write sections header */
   if (a_Dpip_dsh_write_str(sh, 0, modifypage_sections_header))
      return 1;
   /* write sections */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      dStr_sprintf(dstr, modifypage_sections_item,
                   sec_node->section, sec_node->section, sec_node->title);
      if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
         return 1;
   }
   /* write sections footer */
   if (a_Dpip_dsh_write_str(sh, 0, modifypage_sections_footer))
      return 1;

   /* send page middle */
   if (a_Dpip_dsh_write_str(sh, 0, modifypage_middle1))
      return 1;

   /* send bookmark cards */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      /* send card header */
      l_title = make_one_line_str(sec_node->title);
      dStr_sprintf(dstr, modifypage_section_card_header,
                   sec_node->section, l_title);
      dFree(l_title);
      if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
         return 1;

      /* send section's bookmarks */
      for (j = 0; (bm_node = dList_nth_data(B_bms, j)); ++j) {
         if (bm_node->section == sec_node->section) {
            dStr_sprintf(dstr, modifypage_section_card_item,
                         bm_node->key, bm_node->url, bm_node->title);
            if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
               return 1;
         }
      }

      /* send card footer */
      if (a_Dpip_dsh_write_str(sh, 0, modifypage_section_card_footer))
         return 1;
   }

   /* finish page */
   if (a_Dpip_dsh_write_str(sh, 1, modifypage_footer))
      return 1;

   return 2;
}

/*
 * Send the HTML for the modify page for "add section"
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_modify_page_add_section(Dsh *sh)
{
   /* send modify page2 */
   if (a_Dpip_dsh_write_str(sh, 1, modifypage_add_section_page))
      return 1;

   return 2;
}

/*
 * Send the HTML for the modify page for "add url"
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_modify_page_add_url(Dsh *sh)
{
   if (a_Dpip_dsh_write_str(sh, 1, modifypage_add_url))
      return 1;
   return 2;
}

/*
 * Parse a modify urls request and either:
 *   - make a local copy of the url
 *     or
 *   - send the modify page for the marked urls and sections
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_modify_update(Dsh *sh, char *url)
{
   static char *url1 = NULL;
   static Dstr *dstr = NULL;
   char *p, *q;
   int i, key, n_sec, n_url;
   BmRec *bm_node;
   BmSec *sec_node;

   /* bookmarks were loaded before */

   if (!dstr)
      dstr = dStr_new("");

   if (sh == NULL) {
      /* just copy url */
      dFree(url1);
      url1 = dStrdup(url);
      return 0;
   }

   /* send HTML here */
   if (a_Dpip_dsh_write_str(sh, 0, modifypage_update_header))
      return 1;

   /* Count number of marked urls and sections */
   Bmsrv_count_urls_and_sections(url1, &n_sec, &n_url);

   if (n_sec) {
      dStr_sprintf(dstr, modifypage_update_title, "Update&nbsp;sections:");
      a_Dpip_dsh_write_str(sh, 0, dstr->str);
      a_Dpip_dsh_write_str(sh, 0, modifypage_update_item_header);
      /* send items here */
      p = strchr(url1, '?');
      for (q = p; (q = strstr(q, "&s")); ++q) {
         for (i = 0; isdigit(q[2+i]); ++i);
         if (q[2+i] == '=') {
            key = strtol(q + 2, NULL, 10);
            if ((sec_node = Bms_get_sec(key))) {
               dStr_sprintf(dstr, modifypage_update_item2,
                            sec_node->section, sec_node->title);
               a_Dpip_dsh_write_str(sh, 0, dstr->str);
            }
         }
      }
      a_Dpip_dsh_write_str(sh, 0, modifypage_update_item_footer);
   }

   if (n_url) {
      dStr_sprintf(dstr, modifypage_update_title, "Update&nbsp;titles:");
      a_Dpip_dsh_write_str(sh, 0, dstr->str);
      a_Dpip_dsh_write_str(sh, 0, modifypage_update_item_header);
      /* send items here */
      p = strchr(url1, '?');
      for (q = p; (q = strstr(q, "&url")); ++q) {
         for (i = 0; isdigit(q[4+i]); ++i);
         if (q[4+i] == '=') {
            key = strtol(q + 4, NULL, 10);
            bm_node = Bms_get(key);
            dStr_sprintf(dstr, modifypage_update_item,
                         bm_node->key, bm_node->title, bm_node->url);
            a_Dpip_dsh_write_str(sh, 0, dstr->str);
         }
      }
      a_Dpip_dsh_write_str(sh, 0, modifypage_update_item_footer);
   }

   a_Dpip_dsh_write_str(sh, 1, modifypage_update_footer);

   return 2;
}

/*
 * Make the modify-page and send it back
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_send_modify_answer(Dsh *sh, char *url)
{
   char *d_cmd;
   int st;

   d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
   st = a_Dpip_dsh_write_str(sh, 1, d_cmd);
   dFree(d_cmd);
   if (st != 0)
      return 1;

   /* Send HTTP header */
   if (a_Dpip_dsh_write_str(sh, 0, Header) != 0) {
      return 1;
   }

   if (MODIFY_PAGE_NUM == 2) {
      MODIFY_PAGE_NUM = 1;
      return Bmsrv_send_modify_page_add_section(sh);
   } else if (MODIFY_PAGE_NUM == 3) {
      MODIFY_PAGE_NUM = 1;
      return Bmsrv_send_modify_update(sh, NULL);
   } else if (MODIFY_PAGE_NUM == 4) {
      MODIFY_PAGE_NUM = 1;
      return Bmsrv_send_modify_page_add_url(sh);
   } else {
      return Bmsrv_send_modify_page(sh);
   }
}


/* Operations */

/*
 * Parse a delete bms request, delete them, and update bm file.
 * Return code: { 0:OK, 1:Abort }
 */
static int Bmsrv_modify_delete(char *url)
{
   char *p;
   int nb, ns, key;

   /* bookmarks were loaded before */

   /* Remove marked sections */
   p = strchr(url, '?');
   for (ns = 0; (p = strstr(p, "&s")); ++p) {
      if (isdigit(p[2])) {
         key = strtol(p + 2, NULL, 10);
         Bms_sec_del(key);
         ++ns;
      }
   }

   /* Remove marked urls */
   p = strchr(url, '?');
   for (nb = 0; (p = strstr(p, "&url")); ++p) {
      if (isdigit(p[4])) {
         key = strtol(p + 4, NULL, 10);
         Bms_del(key);
         ++nb;
      }
   }

/* -- This doesn't work because dillo erases the message upon the
 *    receipt of the first data stream.
 *
   sprintf(msg, "Deleted %d bookmark%s!>", n, (n > 1) ? "s" : "");
   if (Bmsrv_dpi_send_status_msg(sh, msg))
      return 1;
*/

   /* Write new bookmarks file */
   if (nb || ns)
      Bms_save();

   return 0;
}

/*
 * Parse a move urls request, move and update bm file.
 * Return code: { 0:OK, 1:Abort }
 */
static int Bmsrv_modify_move(char *url)
{
   char *p;
   int n, section = 0, key;

   /* bookmarks were loaded before */

   /* get target section */
   for (p = url; (p = strstr(p, "&s")); ++p) {
      if (isdigit(p[2])) {
         section = strtol(p + 2, NULL, 10);
         break;
      }
   }
   if (!p)
      return 1;

   /* move marked urls */
   p = strchr(url, '?');
   for (n = 0; (p = strstr(p, "&url")); ++p) {
      if (isdigit(p[4])) {
         key = strtol(p + 4, NULL, 10);
         Bms_move(key, section);
         ++n;
      }
   }

   /* Write new bookmarks file */
   if (n) {
      Bms_save();
   }

   return 0;
}

/*
 * Parse a modify request: update urls and sections, then save.
 * Return code: { 0:OK, 1:Abort }
 */
static int Bmsrv_modify_update(char *url)
{
   char *p, *q, *title;
   int i, key;

   /* bookmarks were loaded before */

   p = strchr(url, '?');
   for (  ; (p = strstr(p, "s")); ++p) {
      if (p[-1] == '&' || p[-1] == '?' ) {
         for (i = 0; isdigit(p[1 + i]); ++i);
         if (i && p[1 + i] == '=') {
            /* we have a title/key to change */
            key = strtol(p + 1, NULL, 10);
            if ((q = strchr(p + 1, '&')))
               title = dStrndup(p + 2 + i, (uint_t)(q - (p + 2 + i)));
            else
               title = dStrdup(p + 2 + i);

            Unencode_str(title);
            Bms_update_sec_title(key, title);
            dFree(title);
         }
      }
   }

   p = strchr(url, '?');
   for (  ; (p = strstr(p, "title")); ++p) {
      if (p[-1] == '&' || p[-1] == '?' ) {
         for (i = 0; isdigit(p[5 + i]); ++i);
         if (i && p[5 + i] == '=') {
            /* we have a title/key to change */
            key = strtol(p + 5, NULL, 10);
            if ((q = strchr(p + 5, '&')))
               title = dStrndup(p + 6 + i, (uint_t)(q - (p + 6 + i)));
            else
               title = dStrdup(p + 6 + i);

            Unencode_str(title);
            Bms_update_title(key, title);
            dFree(title);
         }
      }
   }

   /* Write new bookmarks file */
   Bms_save();

   return 0;
}

/*
 * Parse an "add section" request, and update the bm file.
 * Return code: { 0:OK, 1:Abort }
 */
static int Bmsrv_modify_add_section(char *url)
{
   char *p, *title = NULL;

   /* bookmarks were loaded before */

   /* get new section's title */
   if ((p = strstr(url, "&title="))) {
      title = dStrdup (p + 7);
      if ((p = strchr(title, '&')))
         *p = 0;
      Unencode_str(title);
   } else
      return 1;

   Bms_sec_add(title);
   dFree(title);

   /* Write new bookmarks file */
   Bms_save();

   return 0;
}

/*
 * Parse an "add url" request, and update the bm file.
 * Return code: { 0:OK, 1:Abort }
 */
static int Bmsrv_modify_add_url(Dsh *sh, char *s_url)
{
   char *p, *q, *title, *u_title, *url;
   int i;
   static int section = 0;

   /* bookmarks were loaded before */

   if (sh == NULL) {
      /* look for section */
      for (q = s_url; (q = strstr(q, "&s")); ++q) {
         for (i = 0; isdigit(q[2+i]); ++i);
         if (q[2+i] == '=')
            section = strtol(q + 2, NULL, 10);
      }
      return 1;
   }

   if (!(p = strstr(s_url, "&title=")) ||
       !(q = strstr(s_url, "&url=")))
      return 1;

   title = dStrdup (p + 7);
   if ((p = strchr(title, '&')))
      *p = 0;
   url = dStrdup (q + 5);
   if ((p = strchr(url, '&')))
      *p = 0;
   if (strlen(title) && strlen(url)) {
      Unencode_str(title);
      Unencode_str(url);
      u_title = Unescape_html_str(title);
      Bms_add(section, url, u_title);
      dFree(u_title);
   }
   dFree(title);
   dFree(url);
   section = 0;

   /* TODO: we should send an "Bookmark added" message, but the
      msg-after-HTML functionallity is still pending, not hard though. */

   /* Write new bookmarks file */
   Bms_save();

   return 0;
}

/*
 * Check the parameters of a modify request, and return an error message
 * when it's wrong.
 * Return code: { 0:OK, 2:Close }
 */
static int Bmsrv_check_modify_request(Dsh *sh, char *url)
{
   char *p, *msg;
   int n_sec, n_url;

   /* Count number of marked urls and sections */
   Bmsrv_count_urls_and_sections(url, &n_sec, &n_url);

   p = strchr(url, '?');
   if (strstr(p, "operation=delete&")) {
      if (n_url || n_sec)
         return 0;
      msg = "Delete: you must mark what to delete!";

   } else if (strstr(url, "operation=move&")) {
      if (n_url && n_sec)
         return 0;
      else if (n_url)
         msg = "Move: you must mark a target section!";
      else if (n_sec)
         msg = "Move: can not move a section (yet).";
      else
         msg = "Move: you must mark some urls, and a target section!";

   } else if (strstr(url, "operation=modify&")) {
      if (n_url || n_sec)
         return 0;
      msg = "Modify: you must mark what to update!";

   } else if (strstr(url, "operation=modify2&")) {
      /* nothing to check here */
      return 0;

   } else if (strstr(url, "operation=add_sec&")) {
      /* nothing to check here */
      return 0;

   } else if (strstr(url, "operation=add_section&")) {
      /* nothing to check here */
      return 0;

   } else if (strstr(url, "operation=add_url&")) {
      if (n_sec <= 1)
         return 0;
      msg = "Add url: only one target section is allowed!";

   } else if (strstr(url, "operation=add_url2&")) {
      /* nothing to check here */
      return 0;

   } else if (strstr(url, "operation=none&")) {
      msg = "No operation, just do nothing!";

   } else {
      msg = "Sorry, not implemented yet.";
   }

   Bmsrv_dpi_send_status_msg(sh, msg);
   return 2;
}

/*
 * Parse a and process a modify request.
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_process_modify_request(Dsh *sh, char *url)
{
   /* check the provided parameters */
   if (Bmsrv_check_modify_request(sh, url) != 0)
      return 2;

   if (strstr(url, "operation=delete&")) {
      if (Bmsrv_modify_delete(url) == 1)
         return 1;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=move&")) {
      if (Bmsrv_modify_move(url) == 1)
         return 1;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=modify&")) {
      /* make a local copy of 'url' */
      Bmsrv_send_modify_update(NULL, url);
      MODIFY_PAGE_NUM = 3;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=modify2&")) {
      if (Bmsrv_modify_update(url) == 1)
         return 1;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=add_sec&")) {
      /* this global variable tells which page to send  (--hackish...) */
      MODIFY_PAGE_NUM = 2;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=add_section&")) {
      if (Bmsrv_modify_add_section(url) == 1)
         return 1;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=add_url&")) {
      /* this global variable tells which page to send  (--hackish...) */
      MODIFY_PAGE_NUM = 4;
      /* parse section if present */
      Bmsrv_modify_add_url(NULL, url);
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;

   } else if (strstr(url, "operation=add_url2&")) {
      if (Bmsrv_modify_add_url(sh, url) == 1)
         return 1;
      if (Bmsrv_send_reload_request(sh, "dpi:/bm/modify") == 1)
         return 1;
   }

   return 2;
}

/* -- Bookmarks ------------------------------------------------------------ */

/*
 * Send the current bookmarks page (in HTML)
 */
static int send_bm_page(Dsh *sh)
{
   static Dstr *dstr = NULL;
   char *l_title;
   BmSec *sec_node;
   BmRec *bm_node;
   int i, j;

   if (!dstr)
      dstr = dStr_new("");

   if (a_Dpip_dsh_write_str(sh, 0, mainpage_header))
      return 1;

   /* write sections header */
   if (a_Dpip_dsh_write_str(sh, 0, mainpage_sections_header))
      return 1;
   /* write sections */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      dStr_sprintf(dstr, mainpage_sections_item,
                   sec_node->section, sec_node->title);
      if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
         return 1;
   }
   /* write sections footer */
   if (a_Dpip_dsh_write_str(sh, 0, mainpage_sections_footer))
      return 1;

   /* send page middle */
   if (a_Dpip_dsh_write_str(sh, 0, mainpage_middle1))
      return 1;

   /* send bookmark cards */
   for (i = 0; (sec_node = dList_nth_data(B_secs, i)); ++i) {
      /* send card header */
      l_title = make_one_line_str(sec_node->title);
      dStr_sprintf(dstr, mainpage_section_card_header,
                   sec_node->section, l_title);
      dFree(l_title);
      if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
         return 1;

      /* send section's bookmarks */
      for (j = 0; (bm_node = dList_nth_data(B_bms, j)); ++j) {
         if (bm_node->section == sec_node->section) {
            dStr_sprintf(dstr, mainpage_section_card_item,
                         bm_node->url, bm_node->title);
            if (a_Dpip_dsh_write_str(sh, 0, dstr->str))
               return 1;
         }
      }

      /* send card footer */
      if (a_Dpip_dsh_write_str(sh, 0, mainpage_section_card_footer))
         return 1;
   }

   /* finish page */
   if (a_Dpip_dsh_write_str(sh, 1, mainpage_footer))
      return 1;

   return 0;
}


/* -- Dpi parser ----------------------------------------------------------- */

/*
 * Parse a data stream (dpi protocol)
 * Note: Buf is a dpip token (zero terminated string)
 * Return code: { 0:OK, 1:Abort, 2:Close }
 */
static int Bmsrv_parse_token(Dsh *sh, char *Buf)
{
   static char *msg1=NULL, *msg2=NULL, *msg3=NULL;
   char *cmd, *d_cmd, *url, *title, *msg;
   size_t BufSize;
   int st;

   if (!msg1) {
     /* Initialize data for the "chat" command. */
     msg1 = a_Dpip_build_cmd("cmd=%s msg=%s", "chat", "Hi browser");
     msg2 = a_Dpip_build_cmd("cmd=%s msg=%s", "chat", "Is it worth?");
     msg3 = a_Dpip_build_cmd("cmd=%s msg=%s", "chat", "Ok, send it");
   }

   if (sh->mode & DPIP_RAW) {
      MSG("ERROR: Unhandled DPIP_RAW mode!\n");
      return 1;
   }

   BufSize = strlen(Buf);
   cmd = a_Dpip_get_attr_l(Buf, BufSize, "cmd");

   if (cmd && strcmp(cmd, "chat") == 0) {
      dFree(cmd);
      msg = a_Dpip_get_attr_l(Buf, BufSize, "msg");
      if (*msg == 'H') {
         /* "Hi server" */
         if (a_Dpip_dsh_write_str(sh, 1, msg1))
            return 1;
      } else if (*msg == 'I') {
         /* "I want to set abookmark" */
         if (a_Dpip_dsh_write_str(sh, 1, msg2))
            return 1;
      } else if (*msg == 'S') {
         /* "Sure" */
         if (a_Dpip_dsh_write_str(sh, 1, msg3))
            return 1;
      }
      dFree(msg);
      return 0;
   }

   /* sync with the bookmarks file */
   Bms_cond_load();

   if (cmd && strcmp(cmd, "DpiBye") == 0) {
      MSG("(pid %d): Got DpiBye.\n", (int)getpid());
      exit(0);

   } else if (cmd && strcmp(cmd, "add_bookmark") == 0) {
      dFree(cmd);
      url = a_Dpip_get_attr_l(Buf, BufSize, "url");
      title = a_Dpip_get_attr_l(Buf, BufSize, "title");
      if (strlen(title) == 0) {
         dFree(title);
         title = dStrdup("(Untitled)");
      }
      if (url && title)
         Bmsrv_add_bm(sh, url, title);
      dFree(url);
      dFree(title);
      return 2;

   } else if (cmd && strcmp(cmd, "open_url") == 0) {
      dFree(cmd);
      url = a_Dpip_get_attr_l(Buf, BufSize, "url");

      if (dStrnAsciiCasecmp(url, "dpi:", 4) == 0) {
         if (strcmp(url+4, "/bm/modify") == 0) {
            st = Bmsrv_send_modify_answer(sh, url);
            dFree(url);
            return st;
         } else if (strncmp(url+4, "/bm/modify?", 11) == 0) {
            /* process request */
            st = Bmsrv_process_modify_request(sh, url);
            dFree(url);
            return st;
         }
      }


      d_cmd = a_Dpip_build_cmd("cmd=%s url=%s", "start_send_page", url);
      dFree(url);
      st = a_Dpip_dsh_write_str(sh, 1, d_cmd);
      dFree(d_cmd);
      if (st != 0)
         return 1;

      /* Send HTTP header */
      if (a_Dpip_dsh_write_str(sh, 1, Header) != 0) {
         return 1;
      }

      st = send_bm_page(sh);
      if (st != 0) {
         char *err =
            DOCTYPE
            "<HTML><body id='dillo_bm'> Error on the bookmarks server..."
            "      </body></html>";
         if (a_Dpip_dsh_write_str(sh, 1, err) != 0) {
            return 1;
         }
      }
      return 2;
   }

   return 0;
}

/* --  Termination handlers ----------------------------------------------- */
/*
 * (was to delete the local namespace socket),
 *  but this is handled by 'dpid' now.
 */
static void cleanup(void)
{
  /* no cleanup required */
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
   struct sockaddr_un spun;
   int sock_fd, code;
   socklen_t address_size;
   char *tok;
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

   /* We may receive SIGPIPE (e.g. socket is closed early by our client) */
   signal(SIGPIPE, SIG_IGN);

   /* Initialize local data */
   B_bms = dList_new(512);
   B_secs = dList_new(32);
   BmFile = dStrconcat(dGethomedir(), "/.dillo/bm.txt", NULL);
   /* some OSes may need this... */
   address_size = sizeof(struct sockaddr_un);

   MSG("(v.13): accepting connections...\n");

   while (1) {
      sock_fd = accept(STDIN_FILENO, (struct sockaddr *)&spun, &address_size);
      if (sock_fd == -1) {
         perror("[accept]");
         exit(1);
      }

      /* create the Dsh structure */
      sh = a_Dpip_dsh_new(sock_fd, sock_fd, 8*1024);

      /* Authenticate our client... */
      if (!(tok = a_Dpip_dsh_read_token(sh, 1)) ||
          a_Dpip_check_auth(tok) < 0) {
         MSG("can't authenticate request: %s\n", dStrerror(errno));
         a_Dpip_dsh_close(sh);
         exit(1);
      }
      dFree(tok);

      while (1) {
         code = 1;
         if ((tok = a_Dpip_dsh_read_token(sh, 1)) != NULL) {
            /* Let's see what we fished... */
            code = Bmsrv_parse_token(sh, tok);
         }
         dFree(tok);

         if (code != 0) {
            /* socket is not operative (e.g. closed by client) */
            break;
         }
      }

      a_Dpip_dsh_close(sh);
      a_Dpip_dsh_free(sh);

   }/*while*/
}
