#ifndef __NAV_H__
#define __NAV_H__

#include "bw.h"


/* useful macros for the navigation stack */
#define NAV_IDX(bw, i)   (bw)->nav_stack[i]
#define NAV_TOP(bw)      (bw)->nav_stack[(bw)->nav_stack_ptr]


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Nav_push(BrowserWindow *bw, const DilloUrl *url);
void a_Nav_push_nw(BrowserWindow *bw, const DilloUrl *url);
void a_Nav_vpush(void *vbw, const DilloUrl *url);
void a_Nav_back(BrowserWindow *bw);
void a_Nav_forw(BrowserWindow *bw);
void a_Nav_home(BrowserWindow *bw);
void a_Nav_reload(BrowserWindow *bw);
void a_Nav_jump(BrowserWindow *bw, int offset, int new_bw);
void a_Nav_free(BrowserWindow *bw);
void a_Nav_cancel_expect (BrowserWindow *bw);
void a_Nav_expect_done(BrowserWindow *bw);
int a_Nav_stack_ptr(BrowserWindow *bw);
int a_Nav_stack_size(BrowserWindow *bw);

void a_Nav_save_url(BrowserWindow *bw,
                    const DilloUrl *url, const char *filename);
int a_Nav_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NAV_H__ */


