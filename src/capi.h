#ifndef __CAPI_H__
#define __CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "cache.h"
#include "web.hh"

/*
 * Flag defines
 */
#define CAPI_IsCached       (0x1)
#define CAPI_IsEmpty        (0x2)
#define CAPI_InProgress     (0x4)
#define CAPI_Aborted        (0x8)
#define CAPI_Completed     (0x10)

/*
 * Function prototypes
 */
void a_Capi_init(void);
int a_Capi_open_url(DilloWeb *web, CA_Callback_t Call, void *CbData);
int a_Capi_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize);
void a_Capi_unref_buf(const DilloUrl *Url);
const char *a_Capi_get_content_type(const DilloUrl *url);
const char *a_Capi_set_content_type(const DilloUrl *url, const char *ctype,
                                    const char *from);
int a_Capi_get_flags(const DilloUrl *Url);
int a_Capi_get_flags_with_redirection(const DilloUrl *Url);
int a_Capi_dpi_verify_request(BrowserWindow *bw, DilloUrl *url);
int a_Capi_dpi_send_data(const DilloUrl *url, void *bw,
                         char *data, int data_sz, char *server, int flags);
int a_Capi_dpi_send_cmd(DilloUrl *url, void *bw, char *cmd, char *server,
                         int flags);
void a_Capi_set_vsource_url(const DilloUrl *url);
void a_Capi_stop_client(int Key, int force);
void a_Capi_conn_abort_by_url(const DilloUrl *url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAPI_H__ */

