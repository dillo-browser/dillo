#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

#include "bw.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void a_Bookmarks_add(BrowserWindow *bw, const DilloUrl *url);

/* TODO: this is for testing purposes */
void a_Bookmarks_chat_add(BrowserWindow *Bw, char *Cmd, char *answer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BOOKMARK_H__ */
