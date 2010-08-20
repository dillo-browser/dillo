/*
 * File: history.c
 *
 * Copyright (C) 2001-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Linear history (it also provides indexes for the navigation stack)
 */

#include "msg.h"
#include "list.h"
#include "history.h"


typedef struct {
   DilloUrl *url;
   char *title;
} H_Item;


/* Global history list */
static H_Item *history = NULL;
static int history_size = 0;        /* [1 based] */
static int history_size_max = 16;


/*
 * Debug procedure.
 */
void History_show()
{
   int i;

   MSG("  {");
   for (i = 0; i < history_size; ++i)
      MSG(" %s", URL_STR(history[i].url));
   MSG(" }\n");
}

/*
 * Add a new H_Item at the end of the history list
 * (taking care of not making a duplicate entry)
 */
int a_History_add_url(DilloUrl *url)
{
   int i, idx;

   _MSG("a_History_add_url: '%s' ", URL_STR(url));
   for (i = 0; i < history_size; ++i)
      if (!a_Url_cmp(history[i].url, url) &&
          !strcmp(URL_FRAGMENT(history[i].url), URL_FRAGMENT(url)))
         break;

   if (i < history_size) {
      idx = i;
      _MSG("FOUND at idx=%d\n", idx);
   } else {
      idx = history_size;
      a_List_add(history, history_size, history_size_max);
      history[idx].url = a_Url_dup(url);
      history[idx].title = NULL;
      ++history_size;
      _MSG("ADDED at idx=%d\n", idx);
   }

   /* History_show(); */

   return idx;
}

/*
 * Return the DilloUrl field (by index)
 */
const DilloUrl *a_History_get_url(int idx)
{
   _MSG("a_History_get_url: ");
   /* History_show(); */

   dReturn_val_if_fail(idx >= 0 && idx < history_size, NULL);

   return history[idx].url;
}

/*
 * Return the title field (by index)
 * ('force' returns URL_STR when there's no title)
 */
const char *a_History_get_title(int idx, int force)
{
   dReturn_val_if_fail(idx >= 0 && idx < history_size, NULL);

   if (history[idx].title)
      return history[idx].title;
   else if (force)
      return URL_STR(history[idx].url);
   else
      return NULL;
}

/*
 * Return the title field (by url)
 * ('force' returns URL_STR when there's no title)
 */
const char *a_History_get_title_by_url(const DilloUrl *url, int force)
{
   int i;

   dReturn_val_if_fail(url != NULL, NULL);

   for (i = 0; i < history_size; ++i)
      if (a_Url_cmp(url, history[i].url) == 0)
         break;

   if (i < history_size && history[i].title)
      return history[i].title;
   else if (force)
      return URL_STR_(url);
   return NULL;
}

/*
 * Set the page-title for a given URL
 */
void a_History_set_title_by_url(const DilloUrl *url, const char *title)
{
   int i;

   dReturn_if (url == NULL);

   for (i = history_size - 1; i >= 0; --i)
      if (a_Url_cmp(url, history[i].url) == 0)
         break;

   if (i >= 0) {
      dFree(history[i].title);
      history[i].title = dStrdup(title);
   } else {
      MSG_ERR("a_History_set_title_by_url: %s not found\n", URL_STR(url));
   }
}


/*
 * Free all the memory used by this module
 */
void a_History_freeall()
{
   int i;

   for (i = 0; i < history_size; ++i) {
      a_Url_free(history[i].url);
      dFree(history[i].title);
   }
   dFree(history);
}
