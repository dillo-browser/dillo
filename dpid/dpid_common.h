#ifndef DPID_COMMON_H
#define DPID_COMMON_H

/*! \file
 * Declares common functions, global variables, and types.
 *
 * \todo
 * The dpid error codes will be used in
 * the next patch
 */

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
#define dotDILLO_DPID_COMM_KEYS ".dillo/dpid_comm_keys"

#define ERRMSG(CALLER, CALLED, ERR)\
 errmsg(CALLER, CALLED, ERR, __FILE__, __LINE__)
#define _ERRMSG(CALLER, CALLED, ERR)


/*!
 * Macros for calling ckd_write and ckd_close functions
 */
#define CKD_WRITE(fd, msg) ckd_write(fd, msg, __FILE__, __LINE__)
#define CKD_CLOSE(fd)      ckd_close(fd, __FILE__, __LINE__)


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

ssize_t ckd_write(int fd, char *msg, char *file, int line);
ssize_t ckd_close(int fd, char *file, int line);

#endif
