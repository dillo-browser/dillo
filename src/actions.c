/*
 * File: actions.c
 *
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "actions.h"
#include "msg.h"
#include "../dlib/dlib.h"
#include <errno.h>

static Dlist *link_actions = NULL;

void
action_parse(char *line)
{
   char *label = strtok(line, ":");

   if (label == NULL || strlen(label) == 0) {
      MSG("Missing action label, ignoring '%s'\n", line);
      return;
   }

   //MSG("Got label='%s'\n", label);

   char *cmd = strtok(NULL, "");

   if (cmd == NULL || strlen(cmd) == 0) {
      MSG("Missing action command, ignoring '%s'\n", line);
      return;
   }

   //MSG("Got action label='%s' cmd='%s'\n", label, cmd);

   Action *action = dMalloc(sizeof(Action));
   action->label = dStrdup(label);
   action->cmd = dStrdup(cmd);

   dList_append(link_actions, action);
}

void
a_Actions_init(void)
{
   int n = dList_length(prefs.link_actions);

   link_actions = dList_new(n);

   for (int i = 0; i < n; i++) {
      char *line = dList_nth_data(prefs.link_actions, i);
      if (line)
         action_parse(line);
   }
}

Dlist *
a_Actions_link_get(void)
{
   return link_actions;
}
