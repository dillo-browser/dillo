#ifndef __BW_H__
#define __BW_H__

#include "url.h"     /* for DilloUrl */

/*
 * Flag Defines for a_Bw_stop_clients()
 */
#define BW_Root            (1)  /* Root URLs */
#define BW_Img             (2)  /* Image URLs */
#define BW_Force           (4)  /* Stop connection too */


/* browser_window contains the specific data for a single window */
typedef struct {
   /* Pointer to the UI object this bw belongs to */
   void *ui;

   /* All the rendering is done by this.
    * It is defined as a void pointer to avoid C++ in this structure.
    * C++ sources have to include "dw/core.hh" and cast it into an object. */
   void *render_layout;

   /* Root document(s). Currently only used by DilloHtml */
   Dlist *Docs;

   /* A list of active cache clients in the window (The primary Key) */
   Dlist *RootClients;
   /* Image Keys for all active connections in the window */
   Dlist *ImageClients;
   /* Number of images in the page */
   int NumImages;
   /* Number of images already loaded */
   int NumImagesGot;
   /* Number of not yet arrived style sheets */
   int NumPendingStyleSheets;
   /* List of all Urls requested by this page (and its types) */
   Dlist *PageUrls;

   /* The navigation stack (holds indexes to history list) */
   Dlist *nav_stack;
   /* 'nav_stack_ptr' refers to what's being displayed */
   int nav_stack_ptr;        /* [0 based; -1 = empty] */
   /* When the user clicks a link, the URL isn't pushed directly to history;
    * nav_expect_url holds it until a dw is assigned to it. Only then an entry
    * is made in history and referenced at the top of nav_stack */
   DilloUrl *nav_expect_url;

   /* Counter for the number of hops on a redirection. Used to stop
    * redirection loops (accounts for WEB_RootUrl only) */
   int redirect_level;

   /* Url for zero-delay redirections in the META element */
   int meta_refresh_status;
   DilloUrl *meta_refresh_url;

   /* HTML-bugs detected at parse time */
   int num_page_bugs;
   Dstr *page_bugs;
} BrowserWindow;


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void a_Bw_init(void);
BrowserWindow *a_Bw_new();
void a_Bw_free(BrowserWindow *bw);
BrowserWindow *a_Bw_get(int i);
int a_Bw_num();

void a_Bw_add_client(BrowserWindow *bw, int Key, int Root);
int a_Bw_remove_client(BrowserWindow *bw, int ClientKey);
void a_Bw_close_client(BrowserWindow *bw, int ClientKey);
void a_Bw_stop_clients(BrowserWindow *bw, int flags);
void a_Bw_add_doc(BrowserWindow *bw, void *vdoc);
void *a_Bw_get_current_doc(BrowserWindow *bw);
void *a_Bw_get_url_doc(BrowserWindow *bw, const DilloUrl *Url);
void a_Bw_remove_doc(BrowserWindow *bw, void *vdoc);
void a_Bw_add_url(BrowserWindow *bw, const DilloUrl *Url);
void a_Bw_cleanup(BrowserWindow *bw);
/* expect API */
void a_Bw_expect(BrowserWindow *bw, const DilloUrl *Url);
void a_Bw_cancel_expect(BrowserWindow *bw);
bool_t a_Bw_expecting(BrowserWindow *bw);
const DilloUrl *a_Bw_expected_url(BrowserWindow *bw);

typedef void (*BwCallback_t)(BrowserWindow *bw, const void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BROWSER_H__ */

