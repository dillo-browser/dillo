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

#ifndef __RULES_H__
#define __RULES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Only one type of rule for now */
struct rule {
   char *action;
   char *command;
};

struct rules {
   struct rule *rule[256];
   int n;
};

int a_Rules_init(void);
struct rules *a_Rules_get(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__RULES_H__ */
