#ifndef __HTML_HH__
#define __HTML_HH__

#include "url.h"               // for DilloUrl
#include "form.hh"             // for form::Form

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Exported functions
 */
void a_Html_form_event_handler(void *data,
                               form::Form *form_receiver,
                               void *v_resource);
void a_Html_free(void *data);
void a_Html_load_images(void *v_html, DilloUrl *pattern);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HTML_HH__ */
