#ifndef DPID_COMMON_H
#define DPID_COMMON_H

/*! \file
 * Declares common functions, global variables, and types.
 *
 * \todo
 * The dpid error codes will be used in
 * the next patch
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include "../dlib/dlib.h"

/*
 * Debugging macros
 */
#define _MSG(...)
#define MSG(...)  printf("[dpid]: " __VA_ARGS__)
#define _MSG_ERR(...)
#define MSG_ERR(...)  fprintf(stderr, "[dpid]: " __VA_ARGS__)

#define dotDILLO_DPI ".dillo/dpi"
#define dotDILLO_DPIDRC ".dillo/dpidrc"
#define ERRMSG(CALLER, CALLED, ERR)\
 errmsg(CALLER, CALLED, ERR, __FILE__, __LINE__)
#define _ERRMSG(CALLER, CALLED, ERR)


/*!
 * Macro for calling the ckd_write function
 */
#define CKD_WRITE(fd, msg) ckd_write(fd, msg, __FILE__, __LINE__)


/*! Error codes for dpid */
enum {
   no_errors,
   dpid_srs_addrinuse /* dpid service request socket address already in use */
} dpi_errno;

/*! Intended for identifying dillo plugins
 * and related files
 */
enum file_type {
   DPI_FILE,                     /*! Any file name containing .dpi */
   UNKNOWN_FILE
};


void errmsg(char *caller, char *called, int errornum, char *file, int line);

int no_dotfiles(const struct dirent *filedat);

ssize_t ckd_write(int fd, char *msg, char *file, int line);

#endif
