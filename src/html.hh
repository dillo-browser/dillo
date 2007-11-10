#ifndef __HTML_HH__
#define __HTML_HH__

#include "d_size.h"            // for uchar_t
#include "bw.h"                // for BrowserWindow

#include "dw/core.hh"
#include "lout/misc.hh"        // For SimpleVector

//#include "dw_image.h"        // for DwImageMapList
#include "image.hh"            // DilloImage for HtmlLinkImageReceiver

#include "form.hh"             // For receiving the "clicked" signal

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _DilloLinkImage   DilloLinkImage;

typedef struct _DilloHtmlClass   DilloHtmlClass;
typedef struct _DilloHtmlState   DilloHtmlState;
typedef struct _DilloHtmlForm    DilloHtmlForm;
typedef struct _DilloHtmlOption  DilloHtmlOption;
typedef struct _DilloHtmlSelect  DilloHtmlSelect;
typedef struct _DilloHtmlInput   DilloHtmlInput;


struct _DilloLinkImage {
   DilloUrl *url;
   DilloImage *image;
};


typedef enum {
   DT_NONE,           
   DT_HTML,           
   DT_XHTML
} DilloHtmlDocumentType;

typedef enum {
   DILLO_HTML_PARSE_MODE_INIT = 0,
   DILLO_HTML_PARSE_MODE_STASH,
   DILLO_HTML_PARSE_MODE_STASH_AND_BODY,
   DILLO_HTML_PARSE_MODE_VERBATIM,
   DILLO_HTML_PARSE_MODE_BODY,
   DILLO_HTML_PARSE_MODE_PRE
} DilloHtmlParseMode;

typedef enum {
   SEEK_ATTR_START,
   MATCH_ATTR_NAME,
   SEEK_TOKEN_START,
   SEEK_VALUE_START,
   SKIP_VALUE,
   GET_VALUE,
   FINISHED
} DilloHtmlTagParsingState;

typedef enum {
   HTML_LeftTrim      = 1 << 0,
   HTML_RightTrim     = 1 << 1,
   HTML_ParseEntities = 1 << 2
} DilloHtmlTagParsingFlags;

typedef enum {
   DILLO_HTML_TABLE_MODE_NONE,  /* no table at all */
   DILLO_HTML_TABLE_MODE_TOP,   /* outside of <tr> */
   DILLO_HTML_TABLE_MODE_TR,    /* inside of <tr>, outside of <td> */
   DILLO_HTML_TABLE_MODE_TD     /* inside of <td> */
} DilloHtmlTableMode;

typedef enum {
   HTML_LIST_NONE,
   HTML_LIST_UNORDERED,
   HTML_LIST_ORDERED
} DilloHtmlListMode;

enum DilloHtmlProcessingState {
   IN_NONE        = 0,
   IN_HTML        = 1 << 0,
   IN_HEAD        = 1 << 1,
   IN_BODY        = 1 << 2,
   IN_FORM        = 1 << 3,
   IN_SELECT      = 1 << 4,
   IN_TEXTAREA    = 1 << 5,
   IN_MAP         = 1 << 6,
   IN_PRE         = 1 << 7,
   IN_BUTTON      = 1 << 8
};


struct _DilloHtmlState {
   char *tag_name;
   //DwStyle *style, *table_cell_style;
   dw::core::style::Style *style, *table_cell_style;
   DilloHtmlParseMode parse_mode;
   DilloHtmlTableMode table_mode;
   bool_t cell_text_align_set;
   DilloHtmlListMode list_type;
   int list_number;

   /* TagInfo index for the tag that's being processed */
   int tag_idx;

   dw::core::Widget *textblock, *table;

   /* This is used to align list items (especially in enumerated lists) */
   dw::core::Widget *ref_list_item;

   /* This makes image processing faster than a function
      a_Dw_widget_get_background_color. */
   int32_t current_bg_color;

   /* This is used for list items etc; if it is set to TRUE, breaks
      have to be "handed over" (see Html_add_indented and
      Html_eventually_pop_dw). */
   bool_t hand_over_break;
};

typedef enum {
   DILLO_HTML_METHOD_UNKNOWN,
   DILLO_HTML_METHOD_GET,
   DILLO_HTML_METHOD_POST
} DilloHtmlMethod;

typedef enum {
   DILLO_HTML_ENC_URLENCODING
} DilloHtmlEnc;

struct _DilloHtmlForm {
   DilloHtmlMethod method;
   DilloUrl *action;
   DilloHtmlEnc enc;

   misc::SimpleVector<DilloHtmlInput> *inputs;

   int num_entry_fields;
   int num_submit_buttons;

   form::Form *form_receiver;
};

struct _DilloHtmlOption {
   //GtkWidget *menuitem;
   char *value;
   bool_t init_val;
};

struct _DilloHtmlSelect {
   //GtkWidget *menu;
   int size;

   DilloHtmlOption *options;
   int num_options;
   int num_options_max;
};

typedef enum {
   DILLO_HTML_INPUT_UNKNOWN,
   DILLO_HTML_INPUT_TEXT,
   DILLO_HTML_INPUT_PASSWORD,
   DILLO_HTML_INPUT_CHECKBOX,
   DILLO_HTML_INPUT_RADIO,
   DILLO_HTML_INPUT_IMAGE,
   DILLO_HTML_INPUT_FILE,
   DILLO_HTML_INPUT_BUTTON,
   DILLO_HTML_INPUT_HIDDEN,
   DILLO_HTML_INPUT_SUBMIT,
   DILLO_HTML_INPUT_RESET,
   DILLO_HTML_INPUT_BUTTON_SUBMIT,
   DILLO_HTML_INPUT_BUTTON_RESET,
   DILLO_HTML_INPUT_SELECT,
   DILLO_HTML_INPUT_SEL_LIST,
   DILLO_HTML_INPUT_TEXTAREA,
   DILLO_HTML_INPUT_INDEX
} DilloHtmlInputType;

struct _DilloHtmlInput {
   DilloHtmlInputType type;
   void *widget;      /* May be a FLTKWidget or a Dw Widget. */
   void *embed;       /* May be NULL */
   char *name;
   char *init_str;    /* note: some overloading - for buttons, init_str
                         is simply the value of the button; for text
                         entries, it is the initial value */
   DilloHtmlSelect *select;
   bool_t init_val;   /* only meaningful for buttons */
};

class DilloHtml {
private:
   class HtmlLinkReceiver: public dw::core::Widget::LinkReceiver {
   public:
      DilloHtml *html;

      bool enter (dw::core::Widget *widget, int link, int img, int x, int y);
      bool press (dw::core::Widget *widget, int link, int img, int x, int y,
                  dw::core::EventButton *event);
      bool click (dw::core::Widget *widget, int link, int img, int x, int y,
                  dw::core::EventButton *event);
   };
   HtmlLinkReceiver linkReceiver;

public:  //BUG: for now everything is public

   BrowserWindow *bw;
   DilloUrl *base_url;
   dw::core::Widget *dw;    /* this is duplicated in the stack */

   /* -------------------------------------------------------------------*/
   /* Variables required at parsing time                                 */
   /* -------------------------------------------------------------------*/
   char *Start_Buf;
   size_t Start_Ofs;
   size_t CurrTagOfs;
   size_t OldTagOfs, OldTagLine;

   DilloHtmlDocumentType DocType; /* as given by DOCTYPE tag */
   float DocTypeVersion;          /* HTML or XHTML version number */

   misc::SimpleVector<DilloHtmlState> *stack;

   int InFlags; /* tracks which elements we are in */

   Dstr *Stash;
   bool_t StashSpace;

   char *SPCBuf;          /* Buffer for white space */

   int pre_column;        /* current column, used in PRE tags with tabs */
   bool_t PreFirstChar;   /* used to skip the first CR or CRLF in PRE tags */
   bool_t PrevWasCR;      /* Flag to help parsing of "\r\n" in PRE tags */
   bool_t PrevWasOpenTag; /* Flag to help deferred parsing of white space */
   bool_t SPCPending;     /* Flag to help deferred parsing of white space */
   bool_t InVisitedLink;  /* used to 'contrast_visited_colors' */
   bool_t ReqTagClose;    /* Flag to help handling bad-formed HTML */
   bool_t CloseOneTag;    /* Flag to help Html_tag_cleanup_at_close() */
   bool_t TagSoup;        /* Flag to enable the parser's cleanup functions */
   char *NameVal;         /* used for validation of "NAME" and "ID" in <A> */

   /* element counters: used for validation purposes */
   uchar_t Num_HTML, Num_HEAD, Num_BODY, Num_TITLE;

   Dstr *attr_data;       /* Buffer for attribute value */

   /* -------------------------------------------------------------------*/
   /* Variables required after parsing (for page functionality)          */
   /* -------------------------------------------------------------------*/
   misc::SimpleVector<DilloHtmlForm> *forms;
   misc::SimpleVector<DilloUrl*> *links;
   misc::SimpleVector<DilloLinkImage*> *images;
   //DwImageMapList maps;

   int32_t link_color;
   int32_t visited_color;

private:
   void initDw();  /* Used by the constructor */

public:
   DilloHtml(BrowserWindow *bw, const DilloUrl *url);
   ~DilloHtml();
   void connectSignals(dw::core::Widget *dw);
   void write(char *Buf, int BufSize, int Eof);
   void closeParser(int ClientKey);
   int formNew(DilloHtmlMethod method, const DilloUrl *action,
               DilloHtmlEnc enc);
   void loadImages (const DilloUrl *pattern);
};

/*
 * Exported functions
 */
void a_Html_form_event_handler(void *data,
                               form::Form *form_receiver,
                               void *v_resource);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HTML_HH__ */
