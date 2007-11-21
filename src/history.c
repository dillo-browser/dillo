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
 * Add a new H_Item at the end of the history list
 * (taking care of not making a duplicate entry)
 */
int a_History_add_url(DilloUrl *url)
{
   int i, idx;

   for (i = 0; i < history_size; ++i)
      if (!a_Url_cmp(history[i].url, url) &&
          !strcmp(URL_FRAGMENT(history[i].url), URL_FRAGMENT(url)))
         return i;

   idx = history_size;
   a_List_add(history, history_size, history_size_max);
   history[idx].url = a_Url_dup(url);
   history[idx].title = NULL;
   ++history_size;
   return idx;
}

/*
 * Set the page-title for a given URL (by idx)
 * (this is known when the first chunks of HTML data arrive)
 */
int a_History_set_title(int idx, const char *title)
{
   dReturn_val_if_fail(idx >= 0 && idx < history_size, 0);

   dFree(history[idx].title);
   history[idx].title = dStrdup(title);
   return 1;
}

/*
 * Return the DilloUrl field (by index)
 */
DilloUrl *a_History_get_url(int idx)
{
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
 * Free all the memory used by this module
 */
void a_History_free()
{
   int i;

   for (i = 0; i < history_size; ++i) {
      a_Url_free(history[i].url);
      dFree(history[i].title);
   }
   dFree(history);
}
