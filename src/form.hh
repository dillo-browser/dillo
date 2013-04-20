#ifndef __FORM_HH__
#define __FORM_HH__

#include "url.h"

/*
 * Typedefs
 */

typedef enum {
   DILLO_HTML_METHOD_UNKNOWN,
   DILLO_HTML_METHOD_GET,
   DILLO_HTML_METHOD_POST
} DilloHtmlMethod;

typedef enum {
   DILLO_HTML_ENC_URLENCODED,
   DILLO_HTML_ENC_MULTIPART
} DilloHtmlEnc;

/*
 * Classes
 */

class DilloHtmlForm;
class DilloHtmlInput;
class DilloHtml;

/*
 * Form API
 */

DilloHtmlForm *a_Html_form_new(DilloHtml *html,
                               DilloHtmlMethod method,
                               const DilloUrl *action,
                               DilloHtmlEnc enc,
                               const char *charset, bool enabled);

void a_Html_form_delete(DilloHtmlForm* form);
void a_Html_input_delete(DilloHtmlInput* input);
void a_Html_form_submit2(void *v_form);
void a_Html_form_reset2(void *v_form);
void a_Html_form_display_hiddens2(void *v_form, bool display);


/*
 * Form parsing functions
 */

void Html_tag_open_form(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_form(DilloHtml *html);
void Html_tag_open_input(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_open_isindex(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_open_textarea(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_content_textarea(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_textarea(DilloHtml *html);
void Html_tag_open_select(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_select(DilloHtml *html);
void Html_tag_open_option(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_option(DilloHtml *html);
void Html_tag_open_optgroup(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_optgroup(DilloHtml *html);
void Html_tag_open_button(DilloHtml *html, const char *tag, int tagsize);
void Html_tag_close_button(DilloHtml *html);

#endif /* __FORM_HH__ */
