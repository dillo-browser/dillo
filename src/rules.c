/*
 * File: rules.c
 *
 * Copyright (C) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "rules.h"
#include "msg.h"
#include "../dlib/dlib.h"
#include <stdio.h>
#include <errno.h>

#define LINE_MAXLEN 4096

static struct rules rules;

int
parse_string(char *line, char *buf, int n, char **next)
{
   char *p = line;
   if (*p != '"') {
      MSG("expecting string\n");
      return -1;
   }

   p++; /* Skip quote */

   int i = 0;
   while (1) {
      if (p[0] == '\\' && p[1] == '"') {
         if (i >= n) {
            MSG("too big\n");
            return -1;
         }
         buf[i++] = '"';
         p += 2;
         continue;
      }

      if (*p == '\0') {
         MSG("premature end\n");
         return -1;
      }

      if (p[0] == '"') {
         p++;
         break;
      }

      if (i >= n) {
         MSG("too big\n");
         return -1;
      }
      buf[i++] = *p;
      p++;
   }

   if (i >= n) {
      MSG("too big\n");
      return -1;
   }
   buf[i] = '\0';

   *next = p;

   return 0;
}

int
parse_keyword(char *p, char *keyword, char **next)
{
   while (*p == ' ')
      p++;

   int n = strlen(keyword);

   if (strncmp(p, keyword, n) != 0) {
      MSG("doesn't match keyword '%s' at p=%s\n", keyword, p);
      return -1;
   }

   p += n;

   /* Skip spaces after action */
   while (*p == ' ')
      p++;

   *next = p;
   return 0;
}

struct rule *
parse_rule(char *line)
{
   char *p;
   if (parse_keyword(line, "action", &p) != 0) {
      MSG("cannot find keyword action\n");
      return NULL;
   }

   char name[LINE_MAXLEN];
   if (parse_string(p, name, LINE_MAXLEN, &p) != 0) {
      MSG("failed parsing action name\n");
      return NULL;
   }

   line = p;
   if (parse_keyword(line, "shell", &p) != 0) {
      MSG("cannot find keyword shell\n");
      return NULL;
   }

   char cmd[LINE_MAXLEN];
   if (parse_string(p, cmd, LINE_MAXLEN, &p) != 0) {
      MSG("failed parsing shell command\n");
      return NULL;
   }

   MSG("name = '%s', command = '%s'\n", name, cmd);

   struct rule *rule = dNew(struct rule, 1);
   rule->action = dStrdup(name);
   rule->command = dStrdup(cmd);

   return rule;
}

int
a_Rules_init(void)
{
   memset(&rules, 0, sizeof(rules));

   char *filename = dStrconcat(dGethomedir(), "/.dillo/rulesrc", NULL);
   if (!filename) {
      MSG("dStrconcat failed\n");
      return -1;
   }

   FILE *f = fopen(filename, "r");
   if (!f) {
      /* No rules to parse */
      return 0;
   }

   char line[LINE_MAXLEN];
   while (!feof(f)) {
      line[0] = '\0';
      char *rc = fgets(line, LINE_MAXLEN, f);
      if (!rc && ferror(f)) {
         MSG("Error while reading rule from rulesrc: %s\n",
             dStrerror(errno));
         break; /* bail out */
      }

      /* Remove leading and trailing whitespaces */
      dStrstrip(line);

      if ((line[0] == '\0') || (line[0] == '#'))
         continue;

      MSG("RULE: %s\n", line);
      struct rule *rule = parse_rule(line);
      if (rule == NULL) {
         MSG("Cannot parse rule: %s\n", line);
         break;
      }

      if (rules.n < 256)
         rules.rule[rules.n++] = rule;
   }

   fclose(f);
   return 0;
}

struct rules *
a_Rules_get(void)
{
   return &rules;
}
