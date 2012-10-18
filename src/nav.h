#ifndef __NAV_H__
#define __NAV_H__

#include "bw.h"

/*
 * useful macros for the navigation stack
 */
#define NAV_UIDX(bw, i)       a_Nav_get_uidx(bw, i)
#define NAV_TOP_UIDX(bw)      a_Nav_get_top_uidx(bw)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Nav_redirection0(BrowserWindow *bw, const DilloUrl *new_url);
void a_Nav_push(BrowserWindow *bw, const DilloUrl *url,
                const DilloUrl *requester);
void a_Nav_repush(BrowserWindow *bw);
void a_Nav_back(BrowserWindow *bw);
void a_Nav_forw(BrowserWindow *bw);
void a_Nav_home(BrowserWindow *bw);
void a_Nav_reload(BrowserWindow *bw);
void a_Nav_jump(BrowserWindow *bw, int offset, int new_bw);
void a_Nav_free(BrowserWindow *bw);
void a_Nav_cancel_expect (BrowserWindow *bw);
void a_Nav_cancel_expect_if_eq(BrowserWindow *bw, const DilloUrl *url);
void a_Nav_expect_done(BrowserWindow *bw);
int a_Nav_stack_ptr(BrowserWindow *bw);
int a_Nav_stack_size(BrowserWindow *bw);
int a_Nav_get_uidx(BrowserWindow *bw, int i);
int a_Nav_get_top_uidx(BrowserWindow *bw);

void a_Nav_save_url(BrowserWindow *bw,
                    const DilloUrl *url, const char *filename);
int a_Nav_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize);
void a_Nav_unref_buf(const DilloUrl *Url);
const char *a_Nav_get_content_type(const DilloUrl *url);
void a_Nav_set_vsource_url(const DilloUrl *Url);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NAV_H__ */


