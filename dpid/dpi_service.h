/*! \file
 * \todo
 * This module should be removed because its original functions
 * have been removed or modified.
 * Put these functions in dpid.c
 */

#ifndef DPI_SERVICE_H
#define DPI_SERVICE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dpid.h"

char *get_dpi_dir(char *dpidrc);

int send_service_list(int sock, struct dp *dpi_attr_list, int srv_num);

#endif
