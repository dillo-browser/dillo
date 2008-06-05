#ifndef __FORM_HH__
#define __FORM_HH__

/*
 * Typedefs 
 */

typedef enum {
   DILLO_HTML_METHOD_UNKNOWN,
   DILLO_HTML_METHOD_GET,
   DILLO_HTML_METHOD_POST
} DilloHtmlMethod;

typedef enum {
   DILLO_HTML_ENC_URLENCODING,
   DILLO_HTML_ENC_MULTIPART
} DilloHtmlEnc;

/*
 * Classes 
 */

class DilloHtmlForm;

#endif /* __FORM_HH__ */
