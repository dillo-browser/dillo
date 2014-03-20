#ifndef __WEB_H__
#define __WEB_H__

#include <stdio.h>     /* for FILE */
#include "bw.h"        /* for BrowserWindow */
#include "cache.h"     /* for CA_Callback_t */
#include "image.hh"    /* for DilloImage */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Flag defines
 */
#define WEB_RootUrl  1
#define WEB_Image    2
#define WEB_Stylesheet 4
#define WEB_Download 8   /* Half implemented... */


typedef struct _DilloWeb DilloWeb;

struct _DilloWeb {
  DilloUrl *url;              /* Requested URL */
  DilloUrl *requester;        /* URL that caused this request, or
                               * NULL if user-initiated. */
  BrowserWindow *bw;          /* The requesting browser window [reference] */
  int flags;                  /* Additional info */

  DilloImage *Image;          /* For image urls [reference] */

  int32_t bgColor;            /* for image backgrounds */
  char *filename;             /* Variables for Local saving */
  FILE *stream;
  int SavedBytes;
};

void a_Web_init(void);
DilloWeb* a_Web_new (BrowserWindow *bw, const DilloUrl* url,
                     const DilloUrl *requester);
int a_Web_valid(DilloWeb *web);
void a_Web_free (DilloWeb*);
int a_Web_dispatch_by_type (const char *Type, DilloWeb *web,
                             CA_Callback_t *Call, void **Data);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __WEB_H__ */
