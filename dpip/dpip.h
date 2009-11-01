/*
 * Library for dealing with dpip tags (dillo plugin protocol tags).
 */

#ifndef __DPIP_H__
#define __DPIP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "../dlib/dlib.h"

/*
 * Communication mode flags
 */
#define   DPIP_TAG        1   /* Dpip tags in the socket */
#define   DPIP_LAST_TAG   2   /* Dpip mode-switching tag */
#define   DPIP_RAW        4   /* Raw data in the socket  */
#define   DPIP_NONBLOCK   8   /* Nonblocking IO          */

typedef enum {
   DPIP_EAGAIN,
   DPIP_ERROR,
   DPIP_EOF
} DpipDshStatus;

/*
 * Dpip socket handler type.
 */
typedef struct _DpipSocketHandler Dsh;
struct _DpipSocketHandler {
   int fd_in;
   int fd_out;
   /* FILE *in;    --Unused. The stream functions block when reading. */
   FILE *out;

   Dstr *dbuf;     /* write buffer */
   Dstr *rd_dbuf;  /* read buffer */
   int flush_sz;   /* max size before flush */

   int mode;       /* mode flags: DPIP_TAG | DPIP_LAST_TAG | DPIP_RAW */
   int status;     /* status code: DPIP_EAGAIN | DPIP_ERROR | DPIP_EOF */
};


/*
 * Printf like function for building dpip commands.
 * It takes care of dpip escaping of its arguments.
 * NOTE : It ONLY accepts string parameters, and
 *        only one %s per parameter.
 */
char *a_Dpip_build_cmd(const char *format, ...);

/*
 * Task: given a tag and an attribute name, return its value.
 *       (dpip character escaping is removed here)
 * Return value: the attribute value, or NULL if not present or malformed.
 */
char *a_Dpip_get_attr(const char *tag, const char *attrname);
char *a_Dpip_get_attr_l(const char *tag, size_t tagsize, const char *attrname);

int a_Dpip_check_auth(const char *auth);

/*
 * Dpip socket API
 */
Dsh *a_Dpip_dsh_new(int fd_in, int fd_out, int flush_sz);
int a_Dpip_dsh_write(Dsh *dsh, int flush, const char *Data, int DataSize);
int a_Dpip_dsh_write_str(Dsh *dsh, int flush, const char *str);
char *a_Dpip_dsh_read_token(Dsh *dsh);
void a_Dpip_dsh_close(Dsh *dsh);
void a_Dpip_dsh_free(Dsh *dsh);

#define a_Dpip_dsh_printf(sh, flush, ...)                   \
   D_STMT_START {                                           \
      Dstr *dstr = dStr_sized_new(128);                     \
      dStr_sprintf(dstr, __VA_ARGS__);                      \
      a_Dpip_dsh_write(sh, flush, dstr->str, dstr->len);    \
      dStr_free(dstr, 1);                                   \
   } D_STMT_END

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DPIP_H__ */

