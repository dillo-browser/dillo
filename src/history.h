
#ifndef __DILLO_HISTORY_H__
#define __DILLO_HISTORY_H__

#include "url.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int a_History_add_url(DilloUrl *url);
void a_History_set_title_by_url(const DilloUrl *url, const char *title);
const DilloUrl *a_History_get_url(int idx);
const char *a_History_get_title(int idx, int force);
const char *a_History_get_title_by_url(const DilloUrl *url, int force);
void a_History_freeall(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DILLO_HISTORY_H__ */
