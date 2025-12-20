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
#include <unistd.h>
#include <stdlib.h>

static Dlist *link_actions = NULL;
static Dlist *page_actions = NULL;

void
action_parse(Dlist *actions, char *line)
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

   dList_append(actions, action);
}

void
actions_parse(Dlist **actions, Dlist *pref_actions)
{
   int n = dList_length(pref_actions);
   *actions = dList_new(n);
   for (int i = 0; i < n; i++) {
      char *line = dList_nth_data(pref_actions, i);
      if (line)
         action_parse(*actions, line);
   }
}

void
a_Actions_init(void)
{
   actions_parse(&link_actions, prefs.link_actions);
   actions_parse(&page_actions, prefs.page_actions);
}

Dlist *
a_Actions_link_get(void)
{
   return link_actions;
}

Dlist *
a_Actions_page_get(void)
{
   return page_actions;
}

int
a_Actions_run(Action *action)
{
   char pid[64];
   snprintf(pid, 64, "%d", (int) getpid());

   MSG("Running action '%s': %s\n", action->label, action->cmd);

   int f = fork();
   if (f == -1) {
      MSG("Cannot run action '%s', fork failed: %s\n",
            action->label, strerror(errno));
      return -1;
   }

   if (f == 0) {
      /* Set the pid of the dillo browser */
      setenv("DILLO_PID", pid, 1);

      /* Child */
      errno = 0;
      int ret = system(action->cmd);
      if (ret == -1) {
         MSG("Cannot run '%s': %s\n", action->cmd, strerror(errno));
         exit(1);
      } else if (ret != 0) {
         MSG("Command exited with '%d': %s\n", ret, action->cmd);
         exit(1);
      } else {
         /* All good, terminate the child */
         exit(0);
      }
   }

   return 0;
}
