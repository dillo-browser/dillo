/*
 * File: html.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo HTML parsing routines
 */

/*-----------------------------------------------------------------------------
 * Includes
 *---------------------------------------------------------------------------*/
#include <ctype.h>      /* for isspace and tolower */
#include <string.h>     /* for memcpy and memmove */
#include <stdlib.h>
#include <stdio.h>      /* for sprintf */
#include <math.h>       /* for rint */
#include <errno.h>

#include <fltk/utf.h>   /* for utf8encode */

#define DEBUG_LEVEL 10
#include "debug.h"

#include "bw.h"         /* for BrowserWindow */
#include "msg.h"
#include "binaryconst.h"
#include "colors.h"

#include "uicmd.hh"
#include "history.h"
#include "nav.h"
#include "menu.hh"
#include "prefs.h"
#include "capi.h"
#include "html.hh"

#include "dw/textblock.hh"
#include "dw/bullet.hh"
#include "dw/table.hh"
#include "dw/tablecell.hh"
#include "dw/listitem.hh"
#include "dw/image.hh"
#include "dw/ruler.hh"

/*-----------------------------------------------------------------------------
 * Defines 
 *---------------------------------------------------------------------------*/
/* Undefine if you want to unroll tables. For instance for PDAs */
#define USE_TABLES

/* Define to 1 to ignore white space immediately after an open tag,
 * and immediately before a close tag. */
#define SGML_SPCDEL 0

#define TAB_SIZE 8
#define dillo_dbg_rendering 0

// Dw to Textblock
#define DW2TB(dw)  ((Textblock*)dw)
// "html struct" to "Layout"
#define HT2LT(html)  ((Layout*)html->bw->render_layout)
// "Image" to "Dw Widget"
#define IM2DW(Image)  ((Widget*)Image->dw)
// Top of the parsing stack
#define S_TOP(html)  (html->stack->getRef(html->stack->size()-1))

/*-----------------------------------------------------------------------------
 * Name spaces
 *---------------------------------------------------------------------------*/
using namespace dw;
using namespace dw::core;
using namespace dw::core::ui;
using namespace dw::core::style;

/*-----------------------------------------------------------------------------
 * Typedefs
 *---------------------------------------------------------------------------*/
class DilloHtml;
typedef void (*TagOpenFunct) (DilloHtml *Html, const char *Tag, int Tagsize);
typedef void (*TagCloseFunct) (DilloHtml *Html, int TagIdx);
typedef struct _DilloLinkImage   DilloLinkImage;
typedef struct _DilloHtmlClass   DilloHtmlClass;
typedef struct _DilloHtmlState   DilloHtmlState;
typedef struct _DilloHtmlForm    DilloHtmlForm;
typedef struct _DilloHtmlInput   DilloHtmlInput;
typedef struct _DilloHtmlSelect  DilloHtmlSelect;
typedef struct _DilloHtmlOption  DilloHtmlOption;

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

typedef enum {
   DILLO_HTML_METHOD_UNKNOWN,
   DILLO_HTML_METHOD_GET,
   DILLO_HTML_METHOD_POST
} DilloHtmlMethod;

typedef enum {
   DILLO_HTML_ENC_URLENCODING
} DilloHtmlEnc;

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

typedef enum {
   IN_NONE        = 0,
   IN_HTML        = 1 << 0,
   IN_HEAD        = 1 << 1,
   IN_BODY        = 1 << 2,
   IN_FORM        = 1 << 3,
   IN_SELECT      = 1 << 4,
   IN_OPTION      = 1 << 5,
   IN_TEXTAREA    = 1 << 6,
   IN_MAP         = 1 << 7,
   IN_PRE         = 1 << 8,
   IN_BUTTON      = 1 << 9,
} DilloHtmlProcessingState;

/*-----------------------------------------------------------------------------
 * Data Structures
 *---------------------------------------------------------------------------*/
struct _DilloLinkImage {
   DilloUrl *url;
   DilloImage *image;
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
   char *value, *content;
   bool selected, enabled;
};

struct _DilloHtmlSelect {
   misc::SimpleVector<DilloHtmlOption *> *options;
};

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

/*-----------------------------------------------------------------------------
 * Classes
 *---------------------------------------------------------------------------*/
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
 * Exported function with C linkage.
 */
extern "C" {
void *a_Html_text(const char *type, void *P, CA_Callback_t *Call,void **Data);
}

/*-----------------------------------------------------------------------------
 * Forward declarations
 *---------------------------------------------------------------------------*/
static const char *Html_get_attr(DilloHtml *html,
                                 const char *tag,
                                 int tagsize,
                                 const char *attrname);
static const char *Html_get_attr2(DilloHtml *html,
                                  const char *tag,
                                  int tagsize,
                                  const char *attrname,
                                  int tag_parsing_flags);
static char *Html_get_attr_wdef(DilloHtml *html,
                                const char *tag,
                                int tagsize,
                                const char *attrname,
                                const char *def);
static void Html_add_widget(DilloHtml *html, Widget *widget,
                            char *width_str, char *height_str,
                            StyleAttrs *style_attrs);
static int Html_write_raw(DilloHtml *html, char *buf, int bufsize, int Eof);
static void Html_load_image(BrowserWindow *bw, DilloUrl *url,
                            DilloImage *image);
static void Html_callback(int Op, CacheClient_t *Client);
static void Html_tag_open_input(DilloHtml *html, const char *tag, int tagsize);
static void Html_add_input(DilloHtmlForm *form,
                           DilloHtmlInputType type,
                           Widget *widget,
                           Embed *embed,
                           const char *name,
                           const char *init_str,
                           DilloHtmlSelect *select,
                           bool_t init_val);
//static void Html_reset_form(GtkWidget *reset, DilloHtmlLB *html_lb);
static int Html_tag_index(const char *tag);
static void Html_tag_cleanup_at_close(DilloHtml *html, int TagIdx);

/*-----------------------------------------------------------------------------
 * Local Data
 *---------------------------------------------------------------------------*/
/* The following array of font sizes has to be _strictly_ crescent */
static const int FontSizes[] = {8, 10, 12, 14, 18, 24};
static const int FontSizesNum = 6;
static const int FontSizesBase = 2;

/* Parsing table structure */
typedef struct {
   const char *name;      /* element name */
   unsigned char Flags;   /* flags (explained near the table data) */
   char EndTag;           /* Is it Required, Optional or Forbidden */
   uchar_t TagLevel;      /* Used to heuristically parse bad HTML  */
   TagOpenFunct open;     /* Open function */
   TagCloseFunct close;   /* Close function */
} TagInfo;
extern const TagInfo Tags[];


/*-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 * Main Code
 *-----------------------------------------------------------------------------
 *---------------------------------------------------------------------------*/

/*
 * Return the line number of the tag being processed by the parser.
 */
static int Html_get_line_number(DilloHtml *html)
{
   int i, ofs, line;
   const char *p = html->Start_Buf;

   dReturn_val_if_fail(p != NULL, -1);

   ofs = html->CurrTagOfs;
   line = html->OldTagLine;
   for (i = html->OldTagOfs; i < ofs; ++i)
      if (p[i] == '\n')
         ++line;
   html->OldTagOfs = html->CurrTagOfs;
   html->OldTagLine = line;
   return line;
}

/*
 * Collect HTML error strings.
 */
static void Html_msg(DilloHtml *html, const char *format, ... )
{
   va_list argp;

   dStr_sprintfa(html->bw->page_bugs,
                 "HTML warning: line %d, ",
                 Html_get_line_number(html));
   va_start(argp, format);
   dStr_vsprintfa(html->bw->page_bugs, format, argp);
   va_end(argp);
   a_UIcmd_set_bug_prog(html->bw, ++html->bw->num_page_bugs);
}

/*
 * Wrapper for a_Url_new that adds an error detection message.
 * (if use_base_url is TRUE, html->base_url is used)
 */
static DilloUrl *Html_url_new(DilloHtml *html,
                              const char *url_str, const char *base_url,
                              int flags, int32_t posx, int32_t posy,
                              int use_base_url)
{
   DilloUrl *url;
   int n_ic, n_ic_spc;

   url = a_Url_new(
            url_str,
            (use_base_url) ? base_url : URL_STR_(html->base_url),
            flags, posx, posy);
   if ((n_ic = URL_ILLEGAL_CHARS(url)) != 0) {
      const char *suffix = (n_ic) > 1 ? "s" : "";
      n_ic_spc = URL_ILLEGAL_CHARS_SPC(url);
      if (n_ic == n_ic_spc) {
         MSG_HTML("URL has %d illegal character%s [%d space%s]\n",
                   n_ic, suffix, n_ic_spc, suffix);
      } else if (n_ic_spc == 0) {
         MSG_HTML("URL has %d illegal character%s [%d in (00-1F or 7F)]\n",
                   n_ic, suffix, n_ic);
      } else {
         MSG_HTML("URL has %d illegal character%s "
                  "[%d space%s and %d in (00-1F or 7F)]\n",
                  n_ic, suffix, n_ic_spc, n_ic_spc ? "s" : "", n_ic-n_ic_spc);
      }
   }
   return url;
}

/*
 * Set callback function and callback data for the "html/text" MIME type.
 */
void *a_Html_text(const char *Type, void *P, CA_Callback_t *Call, void **Data)
{
   DilloWeb *web = (DilloWeb*)P;
   DilloHtml *html = new DilloHtml(web->bw, web->url);

   *Data = (void*)html;
   *Call = (CA_Callback_t)Html_callback;

   return (void*)html->dw;
}

void a_Html_free(void *data)
{
   delete ((DilloHtml*)data);
}

/*
 * Used bye the "Load images" page menuitem.
 */ 
void a_Html_load_images(void *v_html, DilloUrl *pattern)
{
   DilloHtml *html = (DilloHtml*)v_html;

   html->loadImages(pattern);
}

/*
 * Set the URL data for image maps.
 */
//static void Html_set_link_coordinates(DilloHtmlLB *lb,
//                                      int link, int x, int y)
//{
//   char data[64];
//
//   if (x != -1) {
//      snprintf(data, 64, "?%d,%d", x, y);
//      a_Url_set_ismap_coords(lb->links->get(link), data);
//   }
//}

/*
 * Create a new link, set it as the url's parent
 * and return the index.
 */
static int Html_set_new_link(DilloHtml *html, DilloUrl **url)
{
   int nl = html->links->size();
   html->links->increase();
   html->links->set(nl, (*url) ? *url : NULL);
   return nl;
}

/*
 * Add a new image.
 */
static int Html_add_new_linkimage(DilloHtml *html,
                                  DilloUrl *url, DilloImage *image)
{
   DilloLinkImage *li = dNew(DilloLinkImage, 1);
   li->url = url;
   li->image = image;

   int ni = html->images->size();
   html->images->increase();
   html->images->set(ni, li);
   return ni;
}


/*
 * Change one toplevel attribute. var should be an identifier. val is
 * only evaluated once, so you can safely use a function call for it.
 */
#define HTML_SET_TOP_ATTR(html, var, val) \
   do { \
      StyleAttrs style_attrs; \
      Style *old_style; \
       \
      old_style = S_TOP(html)->style; \
      style_attrs = *old_style; \
      style_attrs.var = (val); \
      S_TOP(html)->style = \
         Style::create (HT2LT(html), &style_attrs); \
      old_style->unref (); \
   } while (FALSE)



/*
 * Set the font at the top of the stack. BImask specifies which
 * attributes in BI should be changed.
 */
static void Html_set_top_font(DilloHtml *html, const char *name, int size,
                              int BI, int BImask)
{
   FontAttrs font_attrs;

   font_attrs = *S_TOP(html)->style->font;
   if (name)
      font_attrs.name = name;
   if (size)
      font_attrs.size = size;
   if (BImask & 1)
      font_attrs.weight = (BI & 1) ? 700 : 400;
   if (BImask & 2)
      font_attrs.style = (BI & 2) ?
         (prefs.use_oblique ?
          FONT_STYLE_OBLIQUE : FONT_STYLE_ITALIC) :
         FONT_STYLE_NORMAL;

   HTML_SET_TOP_ATTR (html, font,
                      Font::create (HT2LT(html), &font_attrs));
}

/*
 * Evaluates the ALIGN attribute (left|center|right|justify) and
 * sets the style at the top of the stack.
 */
static void Html_tag_set_align_attr(DilloHtml *html, const char *tag, 
                                    int tagsize)
{
   const char *align, *charattr;

   if ((align = Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrcasecmp (align, "left") == 0)
         HTML_SET_TOP_ATTR (html, textAlign, TEXT_ALIGN_LEFT);
      else if (dStrcasecmp (align, "right") == 0)
         HTML_SET_TOP_ATTR (html, textAlign, TEXT_ALIGN_RIGHT);
      else if (dStrcasecmp (align, "center") == 0)
         HTML_SET_TOP_ATTR (html, textAlign, TEXT_ALIGN_CENTER);
      else if (dStrcasecmp (align, "justify") == 0)
         HTML_SET_TOP_ATTR (html, textAlign, TEXT_ALIGN_JUSTIFY);
      else if (dStrcasecmp (align, "char") == 0) {
         /* todo: Actually not supported for <p> etc. */
         HTML_SET_TOP_ATTR (html, textAlign, TEXT_ALIGN_STRING);
         if ((charattr = Html_get_attr(html, tag, tagsize, "char"))) {
            if (charattr[0] == 0)
               /* todo: ALIGN=" ", and even ALIGN="&32;" will reult in
                * an empty string (don't know whether the latter is
                * correct, has to be clarified with the specs), so
                * that for empty strings, " " is assumed. */
               HTML_SET_TOP_ATTR (html, textAlignChar, ' ');
            else
               HTML_SET_TOP_ATTR (html, textAlignChar, charattr[0]);
         } else
            /* todo: Examine LANG attr of <html>. */
            HTML_SET_TOP_ATTR (html, textAlignChar, '.');
      }
   }
}

/*
 * Evaluates the VALIGN attribute (top|bottom|middle|baseline) and
 * sets the style in style_attrs. Returns TRUE when set.
 */
static bool_t Html_tag_set_valign_attr(DilloHtml *html, const char *tag,
                                         int tagsize, StyleAttrs *style_attrs)
{
   const char *attr;

   if ((attr = Html_get_attr(html, tag, tagsize, "valign"))) {
      if (dStrcasecmp (attr, "top") == 0)
         style_attrs->valign = VALIGN_TOP;
      else if (dStrcasecmp (attr, "bottom") == 0)
         style_attrs->valign = VALIGN_BOTTOM;
      else if (dStrcasecmp (attr, "baseline") == 0)
         style_attrs->valign = VALIGN_BASELINE;
      else
         style_attrs->valign = VALIGN_MIDDLE;
      return TRUE;
   } else
      return FALSE;
}


/*
 * Add a new DwPage into the current DwPage, for indentation.
 * left and right are the horizontal indentation amounts, space is the
 * vertical space around the block.
 */
static void Html_add_indented_widget(DilloHtml *html, Widget *textblock,
                                     int left, int right, int space)
{
   StyleAttrs style_attrs;
   Style *style;

   style_attrs = *S_TOP(html)->style;

   style_attrs.margin.setVal (0);
   style_attrs.borderWidth.setVal (0);
   style_attrs.padding.setVal(0);

   /* Activate this for debugging */
#if 0
   style_attrs.borderWidth.setVal (1);
   style_attrs.setBorderColor (
      Color::createShaded (HT2LT(html), style_attrs.color->getColor());
   style_attrs.setBorderStyle (BORDER_DASHED);
#endif

   style_attrs.margin.left = left;
   style_attrs.margin.right = right;
   style = Style::create (HT2LT(html), &style_attrs);

   DW2TB(html->dw)->addParbreak (space, style);
   DW2TB(html->dw)->addWidget (textblock, style);
   DW2TB(html->dw)->addParbreak (space, style);
   S_TOP(html)->textblock = html->dw = textblock;
   S_TOP(html)->hand_over_break = TRUE;
   style->unref ();

   /* Handle it when the user clicks on a link */
   html->connectSignals(textblock);
}

/*
 * Create and add a new indented DwPage to the current DwPage
 */
static void Html_add_indented(DilloHtml *html, int left, int right, int space)
{
   Textblock *textblock = new Textblock (prefs.limit_text_width);
   Html_add_indented_widget (html, textblock, left, right, space);
}

/*
 * Given a font_size, this will return the correct 'level'.
 * (or the closest, if the exact level isn't found).
 */
static int Html_fontsize_to_level(int fontsize)
{
   int i, level;
   double normalized_size = fontsize / prefs.font_factor,
          approximation   = FontSizes[FontSizesNum-1] + 1;

   for (i = level = 0; i < FontSizesNum; i++)
      if (approximation >= fabs(normalized_size - FontSizes[i])) {
         approximation = fabs(normalized_size - FontSizes[i]);
         level = i;
      } else {
         break;
      }

   return level;
}

/*
 * Given a level of a font, this will return the correct 'size'.
 */
static int Html_level_to_fontsize(int level)
{
   level = MAX(0, level);
   level = MIN(FontSizesNum - 1, level);

   return (int)rint(FontSizes[level]*prefs.font_factor);
}

/*
 * Create and initialize a new DilloHtml class
 */
DilloHtml::DilloHtml(BrowserWindow *p_bw, const DilloUrl *url)
{
   /* Init event receiver */
   linkReceiver.html = this;

   /* Init main variables */
   bw = p_bw;
   base_url = a_Url_dup(url);
   dw = NULL;

   a_Bw_add_doc(p_bw, this);

   /* Init for-parsing variables */
   Start_Buf = NULL;
   Start_Ofs = 0;
   CurrTagOfs = 0;
   OldTagOfs = 0;
   OldTagLine = 1;

   DocType = DT_NONE;    /* assume Tag Soup 0.0!   :-) */
   DocTypeVersion = 0.0f;

   stack = new misc::SimpleVector <DilloHtmlState> (16);
   stack->increase();
   stack->getRef(0)->tag_name = dStrdup("none");
   stack->getRef(0)->style = NULL;
   stack->getRef(0)->table_cell_style = NULL;
   stack->getRef(0)->parse_mode = DILLO_HTML_PARSE_MODE_INIT;
   stack->getRef(0)->table_mode = DILLO_HTML_TABLE_MODE_NONE;
   stack->getRef(0)->cell_text_align_set = FALSE;
   stack->getRef(0)->list_type = HTML_LIST_NONE;
   stack->getRef(0)->list_number = 0;
   stack->getRef(0)->tag_idx = -1;               /* MUST not be used */
   stack->getRef(0)->textblock = NULL;
   stack->getRef(0)->table = NULL;
   stack->getRef(0)->ref_list_item = NULL;
   stack->getRef(0)->current_bg_color = prefs.bg_color;
   stack->getRef(0)->hand_over_break = FALSE;

   InFlags = IN_NONE;

   Stash = dStr_new("");
   StashSpace = FALSE;

   SPCBuf = NULL;

   pre_column = 0;
   PreFirstChar = FALSE;
   PrevWasCR = FALSE;
   PrevWasOpenTag = FALSE;
   SPCPending = FALSE;
   InVisitedLink = FALSE;
   ReqTagClose = FALSE;
   CloseOneTag = FALSE;
   TagSoup = TRUE;
   NameVal = NULL;

   Num_HTML = Num_HEAD = Num_BODY = Num_TITLE = 0;

   attr_data = dStr_sized_new(1024);

   /* Init page-handling variables */
   forms = new misc::SimpleVector <DilloHtmlForm> (1);
   links = new misc::SimpleVector <DilloUrl*> (64);
   images = new misc::SimpleVector <DilloLinkImage*> (16);
   //a_Dw_image_map_list_init(&maps);

   link_color = prefs.link_color;
   visited_color = prefs.visited_color;

   /* Initialize the main widget */
   initDw();
   /* Hook destructor to the dw delete call */
   dw->setDeleteCallback(a_Html_free, this);
}

/*
 * Miscelaneous initializations for Dw
 */
void DilloHtml::initDw()
{
   StyleAttrs style_attrs;
   FontAttrs font_attrs;

   dReturn_if_fail (dw == NULL);

   /* Create the main widget */
   dw = stack->getRef(0)->textblock = new Textblock (prefs.limit_text_width);

   /* Create a dummy font, attribute, and tag for the bottom of the stack. */
   font_attrs.name = prefs.vw_fontname;
   font_attrs.size = Html_level_to_fontsize(FontSizesBase);
   font_attrs.weight = 400;
   font_attrs.style = FONT_STYLE_NORMAL;

   style_attrs.initValues ();
   style_attrs.font = Font::create (HT2LT(this), &font_attrs);
   style_attrs.color = Color::createSimple (HT2LT(this), prefs.text_color);
   stack->getRef(0)->style = Style::create (HT2LT(this), &style_attrs);

   stack->getRef(0)->table_cell_style = NULL;

   /* Handle it when the user clicks on a link */
   connectSignals(dw);

   bw->num_page_bugs = 0;
   dStr_truncate(bw->page_bugs, 0);
}

/*
 * Free memory used by the DilloHtml class.
 */
DilloHtml::~DilloHtml()
{
   _MSG("::~DilloHtml(this=%p)\n", this);

   a_Bw_remove_doc(bw, this);

   a_Url_free(base_url);

   for (int i = 0; i < forms->size(); i++) {
      DilloHtmlForm *form = forms->getRef(i);
      a_Url_free(form->action);
      for (int j = 0; j < form->inputs->size(); j++) {
         DilloHtmlInput *input = form->inputs->getRef(j);
         dFree(input->name);
         dFree(input->init_str);

         if (input->type == DILLO_HTML_INPUT_SELECT ||
             input->type == DILLO_HTML_INPUT_SEL_LIST) {

            int size = input->select->options->size ();
            for (int k = 0; k < size; k++) {
               DilloHtmlOption *option =
                  input->select->options->get (k);
               dFree(option->value);
               dFree(option->content);
               delete(option);
            }
            delete(input->select->options);
            delete(input->select);
         }
      }
      delete(form->inputs);
      delete(form->form_receiver);
   }
   delete(forms);

   for (int i = 0; i < links->size(); i++)
      if (links->get(i))
         a_Url_free(links->get(i));
   delete (links);

   for (int i = 0; i < images->size(); i++) {
      DilloLinkImage *li = images->get(i);
      a_Url_free(li->url);
      if (li->image)
         a_Image_unref(li->image);
      dFree(li);
   }
   delete (images);

   //a_Dw_image_map_list_free(&maps);
}

/*
 * Connect all signals of a textblock or an image.
 */
void DilloHtml::connectSignals(Widget *dw)
{
   dw->connectLink (&linkReceiver);
}

/*
 * Process the newly arrived html and put it into the page structure.
 * (This function is called by Html_callback whenever there's new data)
 */
void DilloHtml::write(char *Buf, int BufSize, int Eof)
{
   int token_start;
   char *buf = Buf + Start_Ofs;
   int bufsize = BufSize - Start_Ofs;

   dReturn_if_fail (dw != NULL);

   Start_Buf = Buf;
   token_start = Html_write_raw(this, buf, bufsize, Eof);
   Start_Ofs += token_start;

   if (bw)
      a_UIcmd_set_page_prog(bw, Start_Ofs, 1);
}

/*
 * Finish parsing a HTML page.
 * i.e. Free parsing data, close the client and update the page progress bar.
 * The class is not deleted here, it remains until the widget is destroyed.
 */
void DilloHtml::closeParser(int ClientKey)
{
   int si;

   /* force the close of elements left open (todo: not for XHTML) */
   while ((si = stack->size() - 1)) {
      if (stack->getRef(si)->tag_idx != -1) {
         Html_tag_cleanup_at_close(this, stack->getRef(si)->tag_idx);
      }
   }
   dFree(stack->getRef(0)->tag_name);    /* "none" */
   (stack->getRef(0)->style)->unref ();  /* template style */

   delete (stack);

   dStr_free(Stash, TRUE);
   dFree(SPCBuf);
   dStr_free(attr_data, TRUE);

   /* Remove this client from our active list */
   a_Bw_close_client(bw, ClientKey);

   /* Set progress bar insensitive */
   a_UIcmd_set_page_prog(bw, 0, 0);
}

/*
 * Allocate and insert form information.
 */
int DilloHtml::formNew(DilloHtmlMethod method, const DilloUrl *action,
                       DilloHtmlEnc enc)
{
   DilloHtmlForm *form;

   forms->increase();
   form = forms->getRef (forms->size() - 1);
   form->method = method;
   form->action = a_Url_dup(action);
   form->enc = enc;
   form->inputs = new misc::SimpleVector <DilloHtmlInput> (4);
   form->num_entry_fields = 0;
   form->num_submit_buttons = 0;
   form->form_receiver = new form::Form(this);

   _MSG("Html formNew: action=%s nform=%d\n", action, nf);
   return forms->size();
}

/*
 * Load images if they were disabled.
 */
void DilloHtml::loadImages (const DilloUrl *pattern)
{
   for (int i = 0; i < images->size(); i++) {
      if (images->get(i)->image) {
         if ((!pattern) || (!a_Url_cmp(images->get(i)->url, pattern))) {
            Html_load_image(bw, images->get(i)->url, images->get(i)->image);
            images->get(i)->image = NULL;  // web owns it now
         }
      }
   }
}

bool DilloHtml::HtmlLinkReceiver::enter (Widget *widget, int link, int img,
                                         int x, int y)
{
   BrowserWindow *bw = html->bw;

   _MSG(" ** ");
   if (link == -1 && x == -1 && y == -1) {
      _MSG(" Link  LEAVE  notify...\n");
      a_UIcmd_set_msg(bw, "");
   } else {
      _MSG(" Link  ENTER  notify...\n");
      a_UIcmd_set_msg(bw, "%s", URL_STR(html->links->get(link)));
   }
   return true;
}

/*
 * Handle the "press" signal.
 */
bool DilloHtml::HtmlLinkReceiver::press (Widget *widget, int link, int img,
                                         int x, int y, EventButton *event)
{
   BrowserWindow *bw = html->bw;
   int ret = false;
   DilloUrl *linkurl = NULL;

   _MSG("pressed button %d\n", event->button);
   if (event->button == 3) {
      // popup menus
      if (img != -1) {
         // image menu
         if (link != -1)
            linkurl = html->links->get(link);
         a_UIcmd_image_popup(bw, html->images->get(img)->url, linkurl);
         ret = true;
      } else {
         if (link == -1) {
            a_UIcmd_page_popup(bw, a_History_get_url(NAV_TOP_UIDX(bw)),
                               bw->num_page_bugs ? bw->page_bugs->str:NULL,
                               prefs.load_images);
            ret = true;
         } else {
            a_UIcmd_link_popup(bw, html->links->get(link));
            ret = true;
         }
      }
   }
   return ret;
}

/*
 * Handle the "click" signal.
 */
bool DilloHtml::HtmlLinkReceiver::click (Widget *widget, int link, int img,
                                         int x, int y, EventButton *event)
{
   BrowserWindow *bw = html->bw;

   if ((img != -1) && (html->images->get(img)->image)) {
      // clicked an image that has not already been loaded
      DilloUrl *pattern;

      if (event->button == 1){
         // load all instances of this image
         pattern = html->images->get(img)->url;
      } else {
         if (event->button == 2){
            // load all images
            pattern = NULL;
         } else {
            return false;
         }
      }

      html->loadImages(pattern);
      return true;
   }

   if (link != -1) {
      DilloUrl *url = html->links->get(link);
      _MSG("clicked on URL %d: %s\n", link, a_Url_str (url));

      if (x != -1) {
         char data[64];
         snprintf(data, 64, "?%d,%d", x, y);
         a_Url_set_ismap_coords(url, data);
      }

      if (event->button == 1) {
         a_Nav_push(bw, url);
      } else if (event->button == 2) {
         a_Nav_push_nw(bw, url);
      } else {
         return false;
      }

      /* Change the link color to "visited" as visual feedback */
      for (Widget *w = widget; w; w = w->getParent()) {
         _MSG("  ->%s\n", w->getClassName());
         if (w->instanceOf(dw::Textblock::CLASS_ID)) {
            ((Textblock*)w)->changeLinkColor (link, html->visited_color);
            break;
         }
      }
   }
   return true;
}

/*
 * Initialize the stash buffer
 */
static void Html_stash_init(DilloHtml *html)
{
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_STASH;
   html->StashSpace = FALSE;
   dStr_truncate(html->Stash, 0);
}

/* Entities list from the HTML 4.01 DTD */
typedef struct {
   const char *entity;
   int isocode;
} Ent_t;

#define NumEnt 252
static const Ent_t Entities[NumEnt] = {
   {"AElig",0306}, {"Aacute",0301}, {"Acirc",0302},  {"Agrave",0300},
   {"Alpha",01621},{"Aring",0305},  {"Atilde",0303}, {"Auml",0304},
   {"Beta",01622}, {"Ccedil",0307}, {"Chi",01647},   {"Dagger",020041},
   {"Delta",01624},{"ETH",0320},    {"Eacute",0311}, {"Ecirc",0312},
   {"Egrave",0310},{"Epsilon",01625},{"Eta",01627},  {"Euml",0313},
   {"Gamma",01623},{"Iacute",0315}, {"Icirc",0316},  {"Igrave",0314},
   {"Iota",01631}, {"Iuml",0317},   {"Kappa",01632}, {"Lambda",01633},
   {"Mu",01634},   {"Ntilde",0321}, {"Nu",01635},    {"OElig",0522},
   {"Oacute",0323},{"Ocirc",0324},  {"Ograve",0322}, {"Omega",01651},
   {"Omicron",01637},{"Oslash",0330},{"Otilde",0325},{"Ouml",0326},
   {"Phi",01646},  {"Pi",01640},    {"Prime",020063},{"Psi",01650},
   {"Rho",01641},  {"Scaron",0540}, {"Sigma",01643}, {"THORN",0336},
   {"Tau",01644},  {"Theta",01630}, {"Uacute",0332}, {"Ucirc",0333},
   {"Ugrave",0331},{"Upsilon",01645},{"Uuml",0334},  {"Xi",01636},
   {"Yacute",0335},{"Yuml",0570},   {"Zeta",01626},  {"aacute",0341},
   {"acirc",0342}, {"acute",0264},  {"aelig",0346},  {"agrave",0340},
   {"alefsym",020465},{"alpha",01661},{"amp",38},    {"and",021047},
   {"ang",021040}, {"aring",0345},  {"asymp",021110},{"atilde",0343},
   {"auml",0344},  {"bdquo",020036},{"beta",01662},  {"brvbar",0246},
   {"bull",020042},{"cap",021051},  {"ccedil",0347}, {"cedil",0270},
   {"cent",0242},  {"chi",01707},   {"circ",01306},  {"clubs",023143},
   {"cong",021105},{"copy",0251},   {"crarr",020665},{"cup",021052},
   {"curren",0244},{"dArr",020723}, {"dagger",020040},{"darr",020623},
   {"deg",0260},   {"delta",01664}, {"diams",023146},{"divide",0367},
   {"eacute",0351},{"ecirc",0352},  {"egrave",0350}, {"empty",021005},
   {"emsp",020003},{"ensp",020002}, {"epsilon",01665},{"equiv",021141},
   {"eta",01667},  {"eth",0360},    {"euml",0353},   {"euro",020254},
   {"exist",021003},{"fnof",0622},  {"forall",021000},{"frac12",0275},
   {"frac14",0274},{"frac34",0276}, {"frasl",020104},{"gamma",01663},
   {"ge",021145},  {"gt",62},       {"hArr",020724}, {"harr",020624},
   {"hearts",023145},{"hellip",020046},{"iacute",0355},{"icirc",0356},
   {"iexcl",0241}, {"igrave",0354}, {"image",020421},{"infin",021036},
   {"int",021053}, {"iota",01671},  {"iquest",0277}, {"isin",021010},
   {"iuml",0357},  {"kappa",01672}, {"lArr",020720}, {"lambda",01673},
   {"lang",021451},{"laquo",0253},  {"larr",020620}, {"lceil",021410},
   {"ldquo",020034},{"le",021144},  {"lfloor",021412},{"lowast",021027},
   {"loz",022712}, {"lrm",020016},  {"lsaquo",020071},{"lsquo",020030},
   {"lt",60},      {"macr",0257},   {"mdash",020024},{"micro",0265},
   {"middot",0267},{"minus",021022},{"mu",01674},    {"nabla",021007},
   {"nbsp",32},    {"ndash",020023},{"ne",021140},   {"ni",021013},
   {"not",0254},   {"notin",021011},{"nsub",021204}, {"ntilde",0361},
   {"nu",01675},   {"oacute",0363}, {"ocirc",0364},  {"oelig",0523},
   {"ograve",0362},{"oline",020076},{"omega",01711}, {"omicron",01677},
   {"oplus",021225},{"or",021050},  {"ordf",0252},   {"ordm",0272},
   {"oslash",0370},{"otilde",0365}, {"otimes",021227},{"ouml",0366},
   {"para",0266},  {"part",021002}, {"permil",020060},{"perp",021245},
   {"phi",01706},  {"pi",01700},    {"piv",01726},   {"plusmn",0261},
   {"pound",0243}, {"prime",020062},{"prod",021017}, {"prop",021035},
   {"psi",01710},  {"quot",34},     {"rArr",020722}, {"radic",021032},
   {"rang",021452},{"raquo",0273},  {"rarr",020622}, {"rceil",021411},
   {"rdquo",020035},{"real",020434},{"reg",0256},    {"rfloor",021413},
   {"rho",01701},  {"rlm",020017},  {"rsaquo",020072},{"rsquo",020031},
   {"sbquo",020032},{"scaron",0541},{"sdot",021305}, {"sect",0247},
   {"shy",0255},   {"sigma",01703}, {"sigmaf",01702},{"sim",021074},
   {"spades",023140},{"sub",021202},{"sube",021206}, {"sum",021021},
   {"sup",021203}, {"sup1",0271},   {"sup2",0262},   {"sup3",0263},
   {"supe",021207},{"szlig",0337},  {"tau",01704},   {"there4",021064},
   {"theta",01670},{"thetasym",01721},{"thinsp",020011},{"thorn",0376},
   {"tilde",01334},{"times",0327},  {"trade",020442},{"uArr",020721},
   {"uacute",0372},{"uarr",020621}, {"ucirc",0373},  {"ugrave",0371},
   {"uml",0250},   {"upsih",01722}, {"upsilon",01705},{"uuml",0374},
   {"weierp",020430},{"xi",01676},  {"yacute",0375}, {"yen",0245},
   {"yuml",0377},  {"zeta",01666},  {"zwj",020015},  {"zwnj",020014}
};


/*
 * Comparison function for binary search
 */
static int Html_entity_comp(const void *a, const void *b)
{
   return strcmp(((Ent_t *)a)->entity, ((Ent_t *)b)->entity);
}

/*
 * Binary search of 'key' in entity list
 */
static int Html_entity_search(char *key)
{
   Ent_t *res, EntKey;

   EntKey.entity = key;
   res = (Ent_t*) bsearch(&EntKey, Entities, NumEnt,
                          sizeof(Ent_t), Html_entity_comp);
   if (res)
     return (res - Entities);
   return -1;
}

/*
 * This is M$ non-standard "smart quotes" (w1252). Now even deprecated by them!
 *
 * SGML for HTML4.01 defines c >= 128 and c <= 159 as UNUSED.
 * TODO: Probably I should remove this hack, and add a HTML warning. --Jcid
 */
static int Html_ms_stupid_quotes_2ucs(int isocode)
{
   int ret;
   switch (isocode) {
      case 145:
      case 146: ret = '\''; break;
      case 147:
      case 148: ret = '"'; break;
      case 149: ret = 176; break;
      case 150:
      case 151: ret = '-'; break;
      default:  ret = isocode; break;
   }
   return ret;
}

/*
 * Given an entity, return the UCS character code.
 * Returns a negative value (error code) if not a valid entity.
 *
 * The first character *token is assumed to be == '&'
 *
 * For valid entities, *entsize is set to the length of the parsed entity.
 */
static int Html_parse_entity(DilloHtml *html, const char *token,
                             int toksize, int *entsize)
{
   int isocode, i;
   char *tok, *s, c;

   token++;
   tok = s = toksize ? dStrndup(token, (uint_t)toksize) : dStrdup(token);

   isocode = -1;

   if (*s == '#') {
      /* numeric character reference */
      errno = 0;
      if (*++s == 'x' || *s == 'X') {
         if (isxdigit(*++s)) {
            /* strtol with base 16 accepts leading "0x" - we don't */
            if (*s == '0' && s[1] == 'x') {
               s++;
               isocode = 0; 
            } else {
               isocode = strtol(s, &s, 16);
            }
         }
      } else if (isdigit(*s)) {
         isocode = strtol(s, &s, 10);
      }

      if (!isocode || errno || isocode > 0xffff) {
         /* this catches null bytes, errors and codes >= 0xFFFF */
         MSG_HTML("numeric character reference out of range\n");
         isocode = -2;
      }

      if (isocode != -1) {
         if (*s == ';')
            s++;
         else if (prefs.show_extra_warnings)
            MSG_HTML("numeric character reference without trailing ';'\n");
      }

   } else if (isalpha(*s)) {
      /* character entity reference */
      while (isalnum(*++s) || strchr(":_.-", *s));
      c = *s;
      *s = 0;

      if ((i = Html_entity_search(tok)) == -1) {
         if ((html->DocType == DT_HTML && html->DocTypeVersion == 4.01f) ||
             html->DocType == DT_XHTML)
            MSG_HTML("undefined character entity '%s'\n", tok);
         isocode = -3;
      } else
         isocode = Entities[i].isocode;

      if (c == ';')
         s++;
      else if (prefs.show_extra_warnings)
         MSG_HTML("character entity reference without trailing ';'\n");
   }

   *entsize = s-tok+1;
   dFree(tok);

   if (isocode >= 145 && isocode <= 151) {
      /* TODO: remove this hack. */
      isocode = Html_ms_stupid_quotes_2ucs(isocode);
   } else if (isocode == -1 && prefs.show_extra_warnings)
      MSG_HTML("literal '&'\n");

   return isocode;
}

/*
 * Convert all the entities in a token to utf8 encoding. Takes
 * a token and its length, and returns a newly allocated string.
 */
static char *
 Html_parse_entities(DilloHtml *html, const char *token, int toksize)
{
   const char *esc_set = "&\xE2\xC2";
   char *new_str, buf[4];
   int i, j, k, n, isocode, entsize;

   new_str = dStrndup(token, toksize);
   if (new_str[strcspn(new_str, esc_set)] == 0)
      return new_str;

   for (i = j = 0; i < toksize; i++) {
      if (token[i] == '&' &&
          (isocode = Html_parse_entity(html, token+i,
                                       toksize-i, &entsize)) >= 0) {
         if (isocode >= 128) {
            /* multibyte encoding */
            n = utf8encode(isocode, buf);
            for (k = 0; k < n; ++k)
               new_str[j++] = buf[k];
         } else {
            new_str[j++] = (char) isocode;
         }
         i += entsize-1;
      } else {
         new_str[j++] = token[i];
      }
   }
   new_str[j] = '\0';
   return new_str;
}

/*
 * Parse spaces
 */
static void Html_process_space(DilloHtml *html, const char *space, 
                               int spacesize)
{
   int i, offset;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH) {
      html->StashSpace = (html->Stash->len > 0);
      html->SPCPending = FALSE;

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      char *Pword = dStrndup(space, spacesize);
      dStr_append(html->Stash, Pword);
      dFree(Pword);
      html->SPCPending = FALSE;

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_PRE) {
      /* re-scan the string for characters that cause line breaks */
      for (i = 0; i < spacesize; i++) {
         /* Support for "\r", "\n" and "\r\n" line breaks (skips the first) */
         if (!html->PreFirstChar &&
             (space[i] == '\r' || (space[i] == '\n' && !html->PrevWasCR))) {
            DW2TB(html->dw)->addLinebreak (S_TOP(html)->style);
            html->pre_column = 0;
         }
         html->PreFirstChar = FALSE;

         /* cr and lf should not be rendered -- they appear as a break */
         switch (space[i]) {
         case '\r':
         case '\n':
            break;
         case '\t':
            if (prefs.show_extra_warnings)
               MSG_HTML("TAB character inside <PRE>\n");
            offset = TAB_SIZE - html->pre_column % TAB_SIZE;
            DW2TB(html->dw)->addText (dStrnfill(offset, ' '),
                                      S_TOP(html)->style);
            html->pre_column += offset;
            break;
         default:
            DW2TB(html->dw)->addText (dStrndup(space + i, 1),
                                      S_TOP(html)->style);
            html->pre_column++;
            break;
         }

         html->PrevWasCR = (space[i] == '\r');
      }
      html->SPCPending = FALSE;

   } else {
      if (SGML_SPCDEL && html->PrevWasOpenTag) {
         /* SGML_SPCDEL ignores white space inmediately after an open tag */
         html->SPCPending = FALSE;
      } else {
         dFree(html->SPCBuf);
         html->SPCBuf = dStrndup(space, spacesize);
         html->SPCPending = TRUE;
      }

      if (parse_mode == DILLO_HTML_PARSE_MODE_STASH_AND_BODY)
         html->StashSpace = (html->Stash->len > 0);
   }
}

/*
 * Handles putting the word into its proper place
 *  > STASH and VERBATIM --> html->Stash
 *  > otherwise it goes through addText()
 *
 * Entities are parsed (or not) according to parse_mode.
 */
static void Html_process_word(DilloHtml *html, const char *word, int size)
{
   int i, j, start;
   char *Pword;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH ||
       parse_mode == DILLO_HTML_PARSE_MODE_STASH_AND_BODY) {
      if (html->StashSpace) {
         dStr_append_c(html->Stash, ' ');
         html->StashSpace = FALSE;
      }
      Pword = Html_parse_entities(html, word, size);
      dStr_append(html->Stash, Pword);
      dFree(Pword);

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      /* word goes in untouched, it is not processed here. */
      Pword = dStrndup(word, size);
      dStr_append(html->Stash, Pword);
      dFree(Pword);
   }

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH  ||
       parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      /* skip until the closing instructions */

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_PRE) {
      /* all this overhead is to catch white-space entities */
      Pword = Html_parse_entities(html, word, size);
      for (start = i = 0; Pword[i]; start = i)
         if (isspace(Pword[i])) {
            while (Pword[++i] && isspace(Pword[i]));
            Html_process_space(html, Pword + start, i - start);
         } else {
            while (Pword[++i] && !isspace(Pword[i]));
            DW2TB(html->dw)->addText(
                               dStrndup(Pword + start, i - start),
                               S_TOP(html)->style);
            html->pre_column += i - start;
            html->PreFirstChar = FALSE;
         }
      dFree(Pword);

   } else {
      /* add pending space if present */
      if (html->SPCPending && (!SGML_SPCDEL || !html->PrevWasOpenTag))
         /* SGML_SPCDEL ignores space after an open tag */
         DW2TB(html->dw)->addSpace (S_TOP(html)->style);

      /* Collapse white-space entities inside the word (except &nbsp;) */
      Pword = Html_parse_entities(html, word, size);
      for (i = 0; Pword[i]; ++i)
         if (strchr("\t\f\n\r", Pword[i]))
            for (j = i; (Pword[j] = Pword[j+1]); ++j);

      DW2TB(html->dw)->addText(Pword, S_TOP(html)->style);
   }

   html->PrevWasOpenTag = FALSE;
   html->SPCPending = FALSE;
}

/*
 * Does the tag in tagstr (e.g. "p") match the tag in the tag, tagsize
 * structure, with the initial < skipped over (e.g. "P align=center>")
 */
static bool_t Html_match_tag(const char *tagstr, char *tag, int tagsize)
{
   int i;

   for (i = 0; i < tagsize && tagstr[i] != '\0'; i++) {
      if (tolower(tagstr[i]) != tolower(tag[i]))
         return FALSE;
   }
   /* The test for '/' is for xml compatibility: "empty/>" will be matched. */
   if (i < tagsize && (isspace(tag[i]) || tag[i] == '>' || tag[i] == '/'))
      return TRUE;
   return FALSE;
}

/*
 * This function is called after popping the stack, to
 * handle nested DwPage widgets.
 */
static void Html_eventually_pop_dw(DilloHtml *html, bool_t hand_over_break)
{
   if (html->dw != S_TOP(html)->textblock) {
      if (hand_over_break)
         DW2TB(html->dw)->handOverBreak (S_TOP(html)->style);
      DW2TB(html->dw)->flush ();
      html->dw = S_TOP(html)->textblock;
   }
}

/*
 * Push the tag (copying attributes from the top of the stack)
 */
static void Html_push_tag(DilloHtml *html, int tag_idx)
{
   char *tagstr;
   int n_items;

   /* Save the element's name (no parameters) into tagstr. */
   tagstr = dStrdup(Tags[tag_idx].name);

   n_items = html->stack->size ();
   html->stack->increase ();
   /* We'll copy the former stack item and just change the tag and its index
    * instead of copying all fields except for tag.  --Jcid */
   *html->stack->getRef(n_items) = *html->stack->getRef(n_items - 1);
   html->stack->getRef(n_items)->tag_name = tagstr;
   html->stack->getRef(n_items)->tag_idx = tag_idx;
   /* proper memory management, may be unref'd later */
   (S_TOP(html)->style)->ref ();
   if (S_TOP(html)->table_cell_style)
      (S_TOP(html)->table_cell_style)->ref ();
   html->dw = S_TOP(html)->textblock;
}

/*
 * Push the tag (used to force en element with optional open into the stack)
 * Note: now it's the same as Html_push_tag(), but things may change...
 */
static void Html_force_push_tag(DilloHtml *html, int tag_idx)
{
   Html_push_tag(html, tag_idx);
}

/*
 * Pop the top tag in the stack
 */
static void Html_real_pop_tag(DilloHtml *html)
{
   bool_t hand_over_break;

   (S_TOP(html)->style)->unref ();
   if (S_TOP(html)->table_cell_style)
      (S_TOP(html)->table_cell_style)->unref ();
   dFree(S_TOP(html)->tag_name);
   hand_over_break = S_TOP(html)->hand_over_break;
   html->stack->setSize (html->stack->size() - 1);
   Html_eventually_pop_dw(html, hand_over_break);
}

/*
 * Default close function for tags.
 * (conditional cleanup of the stack)
 * There're several ways of doing it. Considering the HTML 4.01 spec
 * which defines optional close tags, and the will to deliver useful diagnose
 * messages for bad-formed HTML, it'll go as follows:
 *   1.- Search the stack for the first tag that requires a close tag.
 *   2.- If it matches, clean all the optional-close tags in between.
 *   3.- Cleanup the matching tag. (on error, give a warning message)
 *
 * If 'w3c_mode' is NOT enabled:
 *   1.- Search the stack for a matching tag based on tag level.
 *   2.- If it exists, clean all the tags in between.
 *   3.- Cleanup the matching tag. (on error, give a warning message)
 */
static void Html_tag_cleanup_at_close(DilloHtml *html, int TagIdx)
{
   int w3c_mode = !prefs.w3c_plus_heuristics;
   int stack_idx, cmp = 1;
   int new_idx = TagIdx;

   if (html->CloseOneTag) {
      Html_real_pop_tag(html);
      html->CloseOneTag = FALSE;
      return;
   }

   /* Look for the candidate tag to close */
   stack_idx = html->stack->size() - 1;
   while (stack_idx &&
          (cmp = (new_idx != html->stack->getRef(stack_idx)->tag_idx)) &&
          ((w3c_mode &&
            Tags[html->stack->getRef(stack_idx)->tag_idx].EndTag == 'O') ||
           (!w3c_mode &&
            Tags[html->stack->getRef(stack_idx)->tag_idx].TagLevel <
            Tags[new_idx].TagLevel))) {
      --stack_idx;
   }

   /* clean, up to the matching tag */
   if (cmp == 0 && stack_idx > 0) {
      /* There's a valid matching tag in the stack */
      while (html->stack->size() > stack_idx) {
         int toptag_idx = S_TOP(html)->tag_idx;
         /* Warn when we decide to close an open tag (for !w3c_mode) */
         if (html->stack->size() > stack_idx + 1 &&
             Tags[toptag_idx].EndTag != 'O')
            MSG_HTML("  - forcing close of open tag: <%s>\n",
                     Tags[toptag_idx].name);

         /* Close this and only this tag */
         html->CloseOneTag = TRUE;
         Tags[toptag_idx].close (html, toptag_idx);
      }

   } else {
      MSG_HTML("unexpected closing tag: </%s>. -- expected </%s>\n",
               Tags[new_idx].name, html->stack->getRef(stack_idx)->tag_name);
   }
}

/*
 * Cleanup (conditional), and Pop the tag (if it matches)
 */
static void Html_pop_tag(DilloHtml *html, int TagIdx)
{
   Html_tag_cleanup_at_close(html, TagIdx);
}

/*
 * Some parsing routines.
 */

/*
 * Used by Html_parse_length
 */
static Length Html_parse_length_or_multi_length (const char *attr,
                                                 char **endptr)
{
   Length l;
   double v;
   char *end;

   v = strtod (attr, &end);
   switch (*end) {
   case '%':
      end++;
      l = createPerLength (v / 100);
      break;

   case '*':
      end++;
      l = createRelLength (v);
      break;
/*
   The "px" suffix seems not allowed by HTML4.01 SPEC.
   case 'p':
      if (end[1] == 'x')
         end += 2;
*/
   default:
      l = createAbsLength ((int)v);
      break;
   }

   if (endptr)
      *endptr = end;
   return l;
}


/*
 * Returns a length or a percentage, or UNDEF_LENGTH in case
 * of an error, or if attr is NULL.
 */
static Length Html_parse_length (DilloHtml *html, const char *attr)
{
   Length l;
   char *end;

   l = Html_parse_length_or_multi_length (attr, &end);
   if (isRelLength (l))
      /* not allowed as &Length; */
      return LENGTH_AUTO;
   else {
      /* allow only whitespaces */
      if (*end && !isspace (*end)) {
         MSG_HTML("Garbage after length: %s\n", attr);
         return LENGTH_AUTO;
      }
   }

   _MSG("Html_parse_length: \"%s\" %d\n", attr, absLengthVal(l));
   return l;
}

/*
 * Parse a color attribute.
 * Return value: parsed color, or default_color (+ error msg) on error.
 */
static int32_t
 Html_color_parse(DilloHtml *html, const char *subtag, int32_t default_color)
{
   int err = 1;
   int32_t color = a_Color_parse(subtag, default_color, &err);

   if (err) {
      MSG_HTML("color is not in \"#RRGGBB\" format\n");
   }
   return color;
}

/*
 * Check that 'val' is composed of characters inside [A-Za-z0-9:_.-]
 * Note: ID can't have entities, but this check is enough (no '&').
 * Return value: 1 if OK, 0 otherwise.
 */
static int
 Html_check_name_val(DilloHtml *html, const char *val, const char *attrname)
{
   int i;

   for (i = 0; val[i]; ++i)
      if (!(isalnum(val[i]) || strchr(":_.-", val[i])))
         break;

   if (val[i] || !isalpha(val[0]))
      MSG_HTML("'%s' value is not of the form "
               "[A-Za-z][A-Za-z0-9:_.-]*\n", attrname);

   return !(val[i]);
}

/*
 * Handle DOCTYPE declaration
 *
 * Follows the convention that HTML 4.01
 * doctypes which include a full w3c DTD url are treated as
 * standards-compliant, but 4.01 without the url and HTML 4.0 and
 * earlier are not. XHTML doctypes are always standards-compliant
 * whether or not an url is present.
 *
 * Note: I'm not sure about this convention. The W3C validator
 * recognizes the "HTML Level" with or without the URL. The convention
 * comes from mozilla (see URLs below), but Dillo doesn't have the same
 * rendering modes, so it may be better to chose another behaviour. --Jcid
 * 
 * http://www.mozilla.org/docs/web-developer/quirks/doctypes.html
 * http://lists.auriga.wearlab.de/pipermail/dillo-dev/2004-October/002300.html
 *
 * This is not a full DOCTYPE parser, just enough for what Dillo uses.
 */
static void Html_parse_doctype(DilloHtml *html, const char *tag, int tagsize)
{
   static const char HTML_sig   [] = "<!DOCTYPE HTML PUBLIC ";
   static const char HTML20     [] = "-//IETF//DTD HTML//EN";
   static const char HTML32     [] = "-//W3C//DTD HTML 3.2";
   static const char HTML40     [] = "-//W3C//DTD HTML 4.0";
   static const char HTML401    [] = "-//W3C//DTD HTML 4.01";
   static const char HTML401_url[] = "http://www.w3.org/TR/html4/";
   static const char XHTML1     [] = "-//W3C//DTD XHTML 1.0";
   static const char XHTML1_url [] = "http://www.w3.org/TR/xhtml1/DTD/";
   static const char XHTML11    [] = "-//W3C//DTD XHTML 1.1";
   static const char XHTML11_url[] = "http://www.w3.org/TR/xhtml11/DTD/";

   int i, quote;
   char *p, *ntag = dStrndup(tag, tagsize);

   /* Tag sanitization: Collapse whitespace between tokens
    * and replace '\n' and '\r' with ' ' inside quoted strings. */
   for (i = 0, p = ntag; *p; ++p) {
      if (isspace(*p)) {
         for (ntag[i++] = ' '; isspace(p[1]); ++p);
      } else if ((quote = *p) == '"' || *p == '\'') {
         for (ntag[i++] = *p++; (ntag[i++] = *p) && *p != quote; ++p) {
            if (*p == '\n' || *p == '\r')
               ntag[i - 1] = ' ';
            p += (p[0] == '\r' && p[1] == '\n') ? 1 : 0;
         }
      } else {
         ntag[i++] = *p;
      }
      if (!*p)
         break;
   }
   ntag[i] = 0;

   _MSG("New: {%s}\n", ntag);

   /* The default DT_NONE type is TagSoup */
   if (!dStrncasecmp(ntag, HTML_sig, strlen(HTML_sig))) {
      p = ntag + strlen(HTML_sig) + 1;
      if (!strncmp(p, HTML401, strlen(HTML401)) &&
          dStristr(p + strlen(HTML401), HTML401_url)) {
         html->DocType = DT_HTML;
         html->DocTypeVersion = 4.01f;
      } else if (!strncmp(p, XHTML1, strlen(XHTML1)) &&
                 dStristr(p + strlen(XHTML1), XHTML1_url)) {
         html->DocType = DT_XHTML;
         html->DocTypeVersion = 1.0f;
      } else if (!strncmp(p, XHTML11, strlen(XHTML11)) &&
                 dStristr(p + strlen(XHTML11), XHTML11_url)) {
         html->DocType = DT_XHTML;
         html->DocTypeVersion = 1.1f;
      } else if (!strncmp(p, HTML40, strlen(HTML40))) {
         html->DocType = DT_HTML;
         html->DocTypeVersion = 4.0f;
      } else if (!strncmp(p, HTML32, strlen(HTML32))) {
         html->DocType = DT_HTML;
         html->DocTypeVersion = 3.2f;
      } else if (!strncmp(p, HTML20, strlen(HTML20))) {
         html->DocType = DT_HTML;
         html->DocTypeVersion = 2.0f;
      }
   }

   dFree(ntag);
}

/*
 * Handle open HTML element
 */
static void Html_tag_open_html(DilloHtml *html, const char *tag, int tagsize)
{
   if (!(html->InFlags & IN_HTML))
      html->InFlags |= IN_HTML;
   ++html->Num_HTML;

   if (html->Num_HTML > 1) {
      MSG_HTML("HTML element was already open\n");
   }
}

/*
 * Handle close HTML element
 */
static void Html_tag_close_html(DilloHtml *html, int TagIdx)
{
   /* todo: may add some checks here */
   if (html->Num_HTML == 1) {
      /* beware of pages with multiple HTML close tags... :-P */
      html->InFlags &= ~IN_HTML;
   }
   Html_pop_tag(html, TagIdx);
}

/*
 * Handle open HEAD element
 */
static void Html_tag_open_head(DilloHtml *html, const char *tag, int tagsize)
{
   if (html->InFlags & IN_BODY) {
      MSG_HTML("HEAD element must go before the BODY section\n");
      html->ReqTagClose = TRUE;
      return;
   }

   if (!(html->InFlags & IN_HEAD))
      html->InFlags |= IN_HEAD;
   ++html->Num_HEAD;

   if (html->Num_HEAD > 1) {
      MSG_HTML("HEAD element was already open\n");
   }
}

/*
 * Handle close HEAD element
 * Note: as a side effect of Html_test_section() this function is called
 *       twice when the head element is closed implicitly.
 */
static void Html_tag_close_head(DilloHtml *html, int TagIdx)
{
   if (html->InFlags & IN_HEAD) {
      if (html->Num_TITLE == 0)
         MSG_HTML("HEAD section lacks the TITLE element\n");
   
      html->InFlags &= ~IN_HEAD;
   }
   Html_pop_tag(html, TagIdx);
}

/*
 * Handle open TITLE
 * calls stash init, where the title string will be stored
 */
static void Html_tag_open_title(DilloHtml *html, const char *tag, int tagsize)
{
   ++html->Num_TITLE;
   Html_stash_init(html);
}

/*
 * Handle close TITLE
 * set page-title in the browser window and in the history.
 */
static void Html_tag_close_title(DilloHtml *html, int TagIdx)
{
   if (html->InFlags & IN_HEAD) {
      /* title is only valid inside HEAD */
      a_UIcmd_set_page_title(html->bw, html->Stash->str);
      a_History_set_title(NAV_TOP_UIDX(html->bw),html->Stash->str);
   } else {
      MSG_HTML("the TITLE element must be inside the HEAD section\n");
   }
   Html_pop_tag(html, TagIdx);
}

/*
 * Handle open SCRIPT
 * initializes stash, where the embedded code will be stored.
 * MODE_VERBATIM is used because MODE_STASH catches entities.
 */
static void Html_tag_open_script(DilloHtml *html, const char *tag, int tagsize)
{
   Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
}

/*
 * Handle close SCRIPT
 */
static void Html_tag_close_script(DilloHtml *html, int TagIdx)
{
   /* eventually the stash will be sent to an interpreter for parsing */
   Html_pop_tag(html, TagIdx);
}

/*
 * Handle open STYLE
 * store the contents to the stash where (in the future) the style
 * sheet interpreter can get it.
 */
static void Html_tag_open_style(DilloHtml *html, const char *tag, int tagsize)
{
   Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
}

/*
 * Handle close STYLE
 */
static void Html_tag_close_style(DilloHtml *html, int TagIdx)
{
   /* eventually the stash will be sent to an interpreter for parsing */
   Html_pop_tag(html, TagIdx);
}

/*
 * <BODY>
 */
static void Html_tag_open_body(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   Textblock *textblock;
   StyleAttrs style_attrs;
   Style *style;
   int32_t color;

   if (!(html->InFlags & IN_BODY))
      html->InFlags |= IN_BODY;
   ++html->Num_BODY;

   if (html->Num_BODY > 1) {
      MSG_HTML("BODY element was already open\n");
      return;
   }
   if (html->InFlags & IN_HEAD) {
      /* if we're here, it's bad XHTML, no need to recover */
      MSG_HTML("unclosed HEAD element\n");
   }

   textblock = DW2TB(html->dw);

   if (!prefs.force_my_colors) {
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         color = Html_color_parse(html, attrbuf, prefs.bg_color);
         if ((color == 0xffffff && !prefs.allow_white_bg) ||
             prefs.force_my_colors)
            color = prefs.bg_color;

         style_attrs = *html->dw->getStyle ();
         style_attrs.backgroundColor =
            Color::createSimple (HT2LT(html), color);
         style = Style::create (HT2LT(html), &style_attrs);
         html->dw->setStyle (style);
         style->unref ();
         S_TOP(html)->current_bg_color = color;
      }

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "text"))) {
         color = Html_color_parse(html, attrbuf, prefs.text_color);
         HTML_SET_TOP_ATTR (html, color,
                            Color::createSimple (HT2LT(html),color));
      }

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "link")))
         html->link_color = Html_color_parse(html, attrbuf, prefs.link_color);

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "vlink")))
         html->visited_color = Html_color_parse(html, attrbuf,
                                                prefs.visited_color);

      if (prefs.contrast_visited_color) {
         /* get a color that has a "safe distance" from text, link and bg */
         html->visited_color =
            a_Color_vc(html->visited_color,
                       S_TOP(html)->style->color->getColor(),
                       html->link_color,
                       S_TOP(html)->current_bg_color);
      }
   }

   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_BODY;
}

/*
 * BODY
 */
static void Html_tag_close_body(DilloHtml *html, int TagIdx)
{
   if (html->Num_BODY == 1) {
      /* some tag soup pages use multiple BODY tags... */
      html->InFlags &= ~IN_BODY;
   }
   Html_pop_tag(html, TagIdx);
}

/*
 * <P>
 * todo: what's the point between adding the parbreak before and
 *       after the push?
 */
static void Html_tag_open_p(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_tag_set_align_attr (html, tag, tagsize);
}

/*
 * <TABLE>
 */
static void Html_tag_open_table(DilloHtml *html, const char *tag, int tagsize)
{
#ifdef USE_TABLES
   Widget *table;
   StyleAttrs style_attrs;
   Style *cell_style, *old_style;
   const char *attrbuf;
   int32_t border = 0, cellspacing = 1, cellpadding = 2, bgcolor;
#endif

   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);

#ifdef USE_TABLES
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "border")))
      border = isdigit(attrbuf[0]) ? strtol (attrbuf, NULL, 10) : 1;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cellspacing")))
      cellspacing = strtol (attrbuf, NULL, 10);
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cellpadding")))
      cellpadding = strtol (attrbuf, NULL, 10);

   /* The style for the table */
   style_attrs = *S_TOP(html)->style;

   /* When dillo was started with the --debug-rendering option, there
    * is always a border around the table. */
   if (dillo_dbg_rendering)
      style_attrs.borderWidth.setVal (MIN (border, 1));
   else
      style_attrs.borderWidth.setVal (border);

   style_attrs.setBorderColor (
      Color::createShaded (HT2LT(html), S_TOP(html)->current_bg_color));
   style_attrs.setBorderStyle (BORDER_OUTSET);
   style_attrs.hBorderSpacing = cellspacing;
   style_attrs.vBorderSpacing = cellspacing;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "width")))
      style_attrs.width = Html_parse_length (html, attrbuf);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrcasecmp (attrbuf, "left") == 0)
         style_attrs.textAlign = TEXT_ALIGN_LEFT;
      else if (dStrcasecmp (attrbuf, "right") == 0)
         style_attrs.textAlign = TEXT_ALIGN_RIGHT;
      else if (dStrcasecmp (attrbuf, "center") == 0)
         style_attrs.textAlign = TEXT_ALIGN_CENTER;
   }

   if (!prefs.force_my_colors &&
       (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      bgcolor = Html_color_parse(html, attrbuf, -1);
      if (bgcolor != -1) {
         if (bgcolor == 0xffffff && !prefs.allow_white_bg)
            bgcolor = prefs.bg_color;
         S_TOP(html)->current_bg_color = bgcolor;
         style_attrs.backgroundColor =
            Color::createSimple (HT2LT(html), bgcolor);
      }
   }

   /* The style for the cells */
   cell_style = Style::create (HT2LT(html), &style_attrs);
   style_attrs = *S_TOP(html)->style;
   /* When dillo was started with the --debug-rendering option, there
    * is always a border around the cells. */
   if (dillo_dbg_rendering)
      style_attrs.borderWidth.setVal (1);
   else
      style_attrs.borderWidth.setVal (border ? 1 : 0);
   style_attrs.padding.setVal(cellpadding);
   style_attrs.setBorderColor (cell_style->borderColor.top);
   style_attrs.setBorderStyle (BORDER_INSET);

   old_style = S_TOP(html)->table_cell_style;
   S_TOP(html)->table_cell_style =
      Style::create (HT2LT(html), &style_attrs);
   if (old_style)
      old_style->unref ();

   table = new Table(prefs.limit_text_width);
   DW2TB(html->dw)->addWidget (table, cell_style);
   cell_style->unref ();

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TOP;
   S_TOP(html)->cell_text_align_set = FALSE;
   S_TOP(html)->table = table;
#endif
}


/*
 * used by <TD> and <TH>
 */
static void Html_tag_open_table_cell(DilloHtml *html,
                                     const char *tag, int tagsize,
                                     TextAlignType text_align)
{
#ifdef USE_TABLES
   Widget *col_tb;
   int colspan = 1, rowspan = 1;
   const char *attrbuf;
   StyleAttrs style_attrs;
   Style *style, *old_style;
   int32_t bgcolor;
   bool_t new_style;

   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      MSG_HTML("<td> or <th> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
      MSG_HTML("<td> or <th> outside <tr>\n");
      /* a_Dw_table_add_cell takes care that dillo does not crash. */
      /* continues */
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      /* todo: check errors? */
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "colspan")))
         colspan = MAX(1, strtol (attrbuf, NULL, 10));
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "rowspan")))
         rowspan = MAX(1, strtol (attrbuf, NULL, 10));

      /* text style */
      old_style = S_TOP(html)->style;
      style_attrs = *old_style;
      if (!S_TOP(html)->cell_text_align_set)
         style_attrs.textAlign = text_align;
      if (Html_get_attr(html, tag, tagsize, "nowrap"))
         style_attrs.whiteSpace = WHITE_SPACE_NOWRAP;
      else
         style_attrs.whiteSpace = WHITE_SPACE_NORMAL;

      S_TOP(html)->style =
         Style::create (HT2LT(html), &style_attrs);
      old_style->unref ();
      Html_tag_set_align_attr (html, tag, tagsize);

      /* cell style */
      style_attrs = *S_TOP(html)->table_cell_style;
      new_style = FALSE;

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "width"))) {
         style_attrs.width = Html_parse_length (html, attrbuf);
         new_style = TRUE;
      }

      if (Html_tag_set_valign_attr (html, tag, tagsize, &style_attrs))
         new_style = TRUE;

      if (!prefs.force_my_colors &&
          (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            new_style = TRUE;
            style_attrs.backgroundColor =
               Color::createSimple (HT2LT(html), bgcolor);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      if (S_TOP(html)->style->textAlign
          == TEXT_ALIGN_STRING)
         col_tb = new TableCell (
             ((Table*)S_TOP(html)->table)->getCellRef (),
             prefs.limit_text_width);
      else
         col_tb = new Textblock (prefs.limit_text_width);

      if (new_style) {
         style = Style::create (HT2LT(html), &style_attrs);
         col_tb->setStyle (style);
         style->unref ();
      } else
         col_tb->setStyle (S_TOP(html)->table_cell_style);

      ((Table*)S_TOP(html)->table)->addCell (col_tb, colspan, rowspan);
      S_TOP(html)->textblock = html->dw = col_tb;

      /* Handle it when the user clicks on a link */
      html->connectSignals(col_tb);
      break;

   default:
      /* compiler happiness */
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TD;
#endif
}


/*
 * <TD>
 */
static void Html_tag_open_td(DilloHtml *html, const char *tag, int tagsize)
{
   Html_tag_open_table_cell (html, tag, tagsize, TEXT_ALIGN_LEFT);
}


/*
 * <TH>
 */
static void Html_tag_open_th(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 1, 1);
   Html_tag_open_table_cell (html, tag, tagsize, TEXT_ALIGN_CENTER);
}


/*
 * <TR>
 */
static void Html_tag_open_tr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   StyleAttrs style_attrs;
   Style *style, *old_style;
   int32_t bgcolor;

#ifdef USE_TABLES
   switch (S_TOP(html)->table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      _MSG("Invalid HTML syntax: <tr> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      style = NULL;

      if (!prefs.force_my_colors &&
          (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = Html_color_parse(html, attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            style_attrs = *S_TOP(html)->style;
            style_attrs.backgroundColor =
               Color::createSimple (HT2LT(html), bgcolor);
            style = Style::create (HT2LT(html), &style_attrs);
            S_TOP(html)->current_bg_color = bgcolor;
         }
      }

      ((Table*)S_TOP(html)->table)->addRow (style);
      if (style)
         style->unref ();

      if (Html_get_attr (html, tag, tagsize, "align")) {
         S_TOP(html)->cell_text_align_set = TRUE;
         Html_tag_set_align_attr (html, tag, tagsize);
      }

      style_attrs = *S_TOP(html)->table_cell_style;
      if (Html_tag_set_valign_attr (html, tag, tagsize, &style_attrs)) {
         old_style = S_TOP(html)->table_cell_style;
         S_TOP(html)->table_cell_style =
            Style::create (HT2LT(html), &style_attrs);
         old_style->unref ();
      } else

      break;

   default:
      break;
   }

   S_TOP(html)->table_mode = DILLO_HTML_TABLE_MODE_TR;
#else
   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);
#endif
}

/*
 * <FRAME>, <IFRAME>
 * todo: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frame (DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   char *src, *buf;
   DilloUrl *url;
   Textblock *textblock;
   StyleAttrs style_attrs;
   Style *link_style;
   Widget *bullet;
   int buf_size;

   textblock = DW2TB(html->dw);

   if (!(attrbuf = Html_get_attr(html, tag, tagsize, "src")))
      return;

   if (!(url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0)))
      return;

   src = dStrdup(attrbuf);

   style_attrs = *(S_TOP(html)->style);

   if (a_Capi_get_buf(url, &buf, &buf_size)) { /* visited frame */
      style_attrs.color =
         Color::createSimple (HT2LT(html), html->visited_color);
   } else {                                    /* unvisited frame */
      style_attrs.color = Color::createSimple (HT2LT(html), html->link_color);
   }
   style_attrs.textDecoration |= TEXT_DECORATION_UNDERLINE;
   style_attrs.x_link = Html_set_new_link(html, &url);
   link_style = Style::create (HT2LT(html), &style_attrs);

   textblock->addParbreak (5, S_TOP(html)->style);

   /* The bullet will be assigned the current list style, which should
    * be "disc" by default, but may in very weird pages be different.
    * Anyway, there should be no harm. */
   bullet = new Bullet();
   textblock->addWidget(bullet, S_TOP(html)->style);
   textblock->addSpace(S_TOP(html)->style);

   if (tolower(tag[1]) == 'i') {
      /* IFRAME usually comes with very long advertising/spying URLS,
       * to not break rendering we will force name="IFRAME" */
      textblock->addText (dStrdup("IFRAME"), link_style);

   } else {
      /* FRAME:
       * If 'name' tag is present use it, if not use 'src' value */
      if (!(attrbuf = Html_get_attr(html, tag, tagsize, "name"))) {
         textblock->addText (dStrdup(src), link_style);
      } else {
         textblock->addText (dStrdup(attrbuf), link_style);
      }
   }

   textblock->addParbreak (5, S_TOP(html)->style);

   link_style->unref ();
   dFree(src);
}

/*
 * <FRAMESET>
 * todo: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frameset (DilloHtml *html,
                                    const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   DW2TB(html->dw)->addText(dStrdup("--FRAME--"),
                              S_TOP(html)->style);
   Html_add_indented(html, 40, 0, 5);
}

/*
 * <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_open_h(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);

   /* todo: combining these two would be slightly faster */
   Html_set_top_font(html, prefs.vw_fontname,
                     Html_level_to_fontsize(FontSizesNum - (tag[2] - '0')),
                     1, 3);
   Html_tag_set_align_attr (html, tag, tagsize);

   /* First finalize unclosed H tags (we test if already named anyway) */
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   a_Menu_pagemarks_add(html->bw, DW2TB(html->dw),
                        S_TOP(html)->style, (tag[2] - '0'));
   Html_stash_init(html);
   S_TOP(html)->parse_mode =
      DILLO_HTML_PARSE_MODE_STASH_AND_BODY;
}

/*
 * Handle close: <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_close_h(DilloHtml *html, int TagIdx)
{
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_pop_tag(html, TagIdx);
}

/*
 * <BIG> | <SMALL>
 */
static void Html_tag_open_big_small(DilloHtml *html,
                                    const char *tag, int tagsize)
{
   int level;

   level =
      Html_fontsize_to_level(S_TOP(html)->style->font->size) +
      ((dStrncasecmp(tag+1, "big", 3)) ? -1 : 1);
   Html_set_top_font(html, NULL, Html_level_to_fontsize(level), 0, 0);
}

/*
 * <BR>
 */
static void Html_tag_open_br(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addLinebreak (S_TOP(html)->style);
}

/*
 * <BUTTON>
 */
static void Html_tag_open_button(DilloHtml *html, const char *tag, int tagsize)
{
// // AL
// /*
//  * Buttons are rendered on one line, this is (at several levels) a
//  * bit simpler. May be changed in the future.
//  */
// StyleAttrs style_attrs;
// Style *style;
// Widget *button, *page;
// DilloHtmlForm *form;
// DilloHtmlInputType inp_type;
// char *name, *value, *type;
//
// /* Render the button */
// style_attrs = *S_TOP(html)->style;
//
// style_attrs.margin.setVal (0);
// style_attrs.borderWidth.setVal (0);
// style_attrs.padding.setVal(0);
// style = Style::create (HT2LT(html), &style_attrs);
// button = a_Dw_button_new (DW_USES_HINTS, TRUE);
//
// /* The new button is not set button-insensitive, since nested buttons
//  * (if they are anyway allowed, todo: search in spec) should all be
//  * activatable. */
// a_Dw_widget_set_button_sensitive (button, TRUE);
//
// DW2TB(html->dw)->addParbreak (5, style);
// DW2TB(html->dw)->addWidget (button, style);
// DW2TB(html->dw)->addParbreak (5, style);
// style->unref ();
//
// style_attrs.margin.setVal (5);
// style = Style::create (HT2LT(html), &style_attrs);
// page = new Textblock (prefs.limit_text_width);
// page->setStyle (style);
// style->unref ();
// a_Dw_container_add (DW_CONTAINER (button), page);
// a_Dw_widget_set_button_sensitive (DW_WIDGET (page), FALSE);
// style_attrs.margin.setVal (0);
//
// a_Dw_button_set_sensitive (DW_BUTTON (button), FALSE);
//
// S_TOP(html)->textblock = html->dw = page;
//
// /* Handle it when the user clicks on a link */
// html->connectSignals(page);
//
// /* Connect it to the form */
// form = forms->getRef (forms->size() - 1);
//
// type = Html_get_attr_wdef(html, tag, tagsize, "type", "");
//
// if (strcmp(type, "submit") == 0) {
//    inp_type = DILLO_HTML_INPUT_BUTTON_SUBMIT;
//    gtk_signal_connect(GTK_OBJECT(button), "clicked",
//                       GTK_SIGNAL_FUNC(Html_submit_form), html_lb);
// } else if (strcmp(type, "reset") == 0) {
//    inp_type = DILLO_HTML_INPUT_BUTTON_RESET;
//    gtk_signal_connect(GTK_OBJECT(button), "clicked",
//                       GTK_SIGNAL_FUNC(Html_reset_form), html_lb);
// } else
//    return;
//
// value = Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
// name = Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
//
// Html_add_input(form, inp_type, (GtkWidget*)button, name, value,
//                NULL, FALSE);
//
// dFree(type);
// dFree(name);
// dFree(value);
}


/*
 * <FONT>
 */
static void Html_tag_open_font(DilloHtml *html, const char *tag, int tagsize)
{
#if 1
   StyleAttrs style_attrs;
   Style *old_style;
   /*Font font;*/
   const char *attrbuf;
   int32_t color;

   if (!prefs.force_my_colors) {
      old_style = S_TOP(html)->style;
      style_attrs = *old_style;

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "color"))) {
         if (prefs.contrast_visited_color && html->InVisitedLink) {
            color = html->visited_color;
         } else { 
            /* use the tag-specified color */
            color = Html_color_parse(
                       html, attrbuf, style_attrs.color->getColor());
            style_attrs.color = Color::createSimple (HT2LT(html), color);
         }
      }

#if 0
    //if ((attrbuf = Html_get_attr(html, tag, tagsize, "face"))) {
    //   font = *( style_attrs.font );
    //   font.name = attrbuf;
    //   style_attrs.font = a_Dw_style_font_new_from_list (&font);
    //}
#endif

      S_TOP(html)->style =
         Style::create (HT2LT(html), &style_attrs);
      old_style->unref ();
   }

#endif
}

/*
 * <ABBR>
 */
static void Html_tag_open_abbr(DilloHtml *html, const char *tag, int tagsize)
{
// DwTooltip *tooltip;
// const char *attrbuf;
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "title"))) {
//    tooltip = a_Dw_tooltip_new_no_ref(attrbuf);
//    HTML_SET_TOP_ATTR(html, x_tooltip, tooltip);
// }
}

/*
 * <B>
 */
static void Html_tag_open_b(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <STRONG>
 */
static void Html_tag_open_strong(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <I>
 */
static void Html_tag_open_i(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <EM>
 */
static void Html_tag_open_em(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <CITE>
 */
static void Html_tag_open_cite(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <CENTER>
 */
static void Html_tag_open_center(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);
   HTML_SET_TOP_ATTR(html, textAlign, TEXT_ALIGN_CENTER);
}

/*
 * <ADDRESS>
 */
static void Html_tag_open_address(DilloHtml *html,
                                  const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <TT>
 */
static void Html_tag_open_tt(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, prefs.fw_fontname, 0, 0, 0);
}

/*
 * Read image-associated tag attributes,
 * create new image and add it to the html page (if add is TRUE).
 */
static DilloImage *Html_add_new_image(DilloHtml *html, const char *tag,
                                      int tagsize, DilloUrl *url,
                                      StyleAttrs *style_attrs, bool_t add)
{
   const int MAX_W = 6000, MAX_H = 6000;

   DilloImage *Image;
   char *width_ptr, *height_ptr, *alt_ptr;
   const char *attrbuf;
   Length l_w, l_h;
   int space, w = 0, h = 0;

// if (prefs.show_tooltip &&
//     (attrbuf = Html_get_attr(html, tag, tagsize, "title")))
//    style_attrs->x_tooltip = a_Dw_tooltip_new_no_ref(attrbuf);

   alt_ptr = Html_get_attr_wdef(html, tag, tagsize, "alt", NULL);
   width_ptr = Html_get_attr_wdef(html, tag, tagsize, "width", NULL);
   height_ptr = Html_get_attr_wdef(html, tag, tagsize, "height", NULL);
   // Check for malicious values
   // TODO: the same for percentage and relative lengths.
   if (width_ptr) {
      l_w = Html_parse_length (html, width_ptr);
      w = isAbsLength(l_w) ? absLengthVal(l_w) : 0;
   }
   if (height_ptr) {
      l_h = Html_parse_length (html, height_ptr);
      h = isAbsLength(l_h) ? absLengthVal(l_h) : 0;
   }
   if (w < 0 || h < 0 || abs(w*h) > MAX_W * MAX_H) {
      dFree(width_ptr);
      dFree(height_ptr);
      width_ptr = height_ptr = NULL;
      MSG("Html_add_new_image: suspicious image size request %dx%d\n", w, h);
   }

   /* todo: we should scale the image respecting its ratio.
    *       As the image size is not known at this time, maybe a flag
    *       can be set to scale it later.
   if ((width_ptr && !height_ptr) || (height_ptr && !width_ptr))
      [...]
   */

   /* Spacing to the left and right */
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "hspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0)
         style_attrs->margin.left = style_attrs->margin.right = space;
   }

   /* Spacing at the top and bottom */
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "vspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0)
         style_attrs->margin.top = style_attrs->margin.bottom = space;
   }

   /* Add a new image widget to this page */
   Image = a_Image_new(0, 0, alt_ptr, S_TOP(html)->current_bg_color);
   if (add) {
      Html_add_widget(html, (Widget*)Image->dw, width_ptr, height_ptr,
                      style_attrs);
   }

   dFree(width_ptr);
   dFree(height_ptr);
   dFree(alt_ptr);
   return Image;
}

/*
 * Tell cache to retrieve image
 */
static void Html_load_image(BrowserWindow *bw, DilloUrl *url, 
                            DilloImage *Image)
{
   DilloWeb *Web;
   int ClientKey;
   /* Fill a Web structure for the cache query */
   Web = a_Web_new(url);
   Web->bw = bw;
   Web->Image = Image;
   Web->flags |= WEB_Image;
   /* Request image data from the cache */
   if ((ClientKey = a_Capi_open_url(Web, NULL, NULL)) != 0) {
      a_Bw_add_client(bw, ClientKey, 0);
      a_Bw_add_url(bw, url);
   }
}

/*
 * Create a new Image struct and request the image-url to the cache
 * (If it either hits or misses, is not relevant here; that's up to the
 *  cache functions)
 */
static void Html_tag_open_img(DilloHtml *html, const char *tag, int tagsize)
{
   DilloImage *Image;
   DilloUrl *url, *usemap_url;
   Textblock *textblock;
   StyleAttrs style_attrs;
   const char *attrbuf;
   int border, load_now;
   char *buf; int buf_size; /* solely for the sake of the cache test */

   /* This avoids loading images. Useful for viewing suspicious HTML email. */
   if (URL_FLAGS(html->base_url) & URL_SpamSafe)
      return;

   if (!(attrbuf = Html_get_attr(html, tag, tagsize, "src")) ||
       !(url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0)))
      return;

   textblock = DW2TB(html->dw);

   usemap_url = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "usemap")))
      /* todo: usemap URLs outside of the document are not used. */
      usemap_url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);

   /* Set the style attributes for this image */
   style_attrs = *S_TOP(html)->style;
   if (S_TOP(html)->style->x_link != -1 ||
       usemap_url != NULL) {
      /* Images within links */
      border = 1;
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "border")))
         border = strtol (attrbuf, NULL, 10);

      if (S_TOP(html)->style->x_link != -1) {
         /* In this case we can use the text color */
         style_attrs.setBorderColor (
            Color::createShaded (HT2LT(html), style_attrs.color->getColor()));
      } else {
         style_attrs.setBorderColor (
            Color::createShaded (HT2LT(html), html->link_color));
      }
      style_attrs.setBorderStyle (BORDER_SOLID);
      style_attrs.borderWidth.setVal (border);
   }
   /* x_img is an index to a list of {url,image} pairs.
    * we know Html_add_new_linkimage() will use size() as its next index */
   style_attrs.x_img = html->images->size();

   load_now = (prefs.load_images || a_Capi_get_buf(url,&buf, &buf_size));
   Image = Html_add_new_image(html, tag, tagsize, url, &style_attrs, TRUE);
   Html_add_new_linkimage(html, url, load_now ? NULL : Image);
   if (load_now)
      Html_load_image(html->bw, url, Image);

#if 0
   /* Image maps */
   if (Html_get_attr(html, tag, tagsize, "ismap")) {
      /* BUG: if several ISMAP images follow each other without
       * being separated with a word, only the first one is ISMAPed
       */
//    a_Dw_image_set_ismap (Image->dw);
      _MSG("  Html_tag_open_img: server-side map (ISMAP)\n");
   } else if (S_TOP(html)->style->x_link != -1 &&
              usemap_url == NULL)
      /* For simple links, we have to suppress the "image_pressed" signal.
       * This is overridden for USEMAP images. */
//    a_Dw_widget_set_button_sensitive (IM2DW(Image->dw), FALSE);

   if (usemap_url) {
//    a_Dw_image_set_usemap (Image->dw, &html->maps, usemap_url);
      a_Url_free (usemap_url);
   }
#endif

   html->connectSignals((Widget*)Image->dw);
}

/*
 * <map>
 */
static void Html_tag_open_map(DilloHtml *html, const char *tag, int tagsize)
{
   char *hash_name;
   const char *attrbuf;
   DilloUrl *url;

   if (html->InFlags & IN_MAP) {
      MSG_HTML("nested <map>\n");
   } else {
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "name"))) {
         hash_name = dStrconcat("#", attrbuf, NULL);
         url = Html_url_new(html, hash_name, NULL, 0, 0, 0, 0);
         //a_Dw_image_map_list_add_map (&html->maps, url);
         a_Url_free (url);
         dFree(hash_name);
      }
      html->InFlags |= IN_MAP;
   }
}

/*
 * Handle close <MAP>
 */
static void Html_tag_close_map(DilloHtml *html, int TagIdx)
{
   html->InFlags &= ~IN_MAP;
   Html_pop_tag(html, TagIdx);
}

/*
 * <AREA>
 */
static void Html_tag_open_area(DilloHtml *html, const char *tag, int tagsize)
{
// // AL
// /* todo: point must be a dynamic array */
// GdkPoint point[1024];
// DilloUrl* url;
// const char *attrbuf;
// int type = DW_IMAGE_MAP_SHAPE_RECT;
// int nbpoints, link = -1;
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "shape"))) {
//    if (dStrcasecmp(attrbuf, "rect") == 0)
//       type = DW_IMAGE_MAP_SHAPE_RECT;
//    else if (dStrcasecmp(attrbuf, "circle") == 0)
//       type = DW_IMAGE_MAP_SHAPE_CIRCLE;
//    else if (dStrncasecmp(attrbuf, "poly", 4) == 0)
//       type = DW_IMAGE_MAP_SHAPE_POLY;
//    else
//       type = DW_IMAGE_MAP_SHAPE_RECT;
// }
// /* todo: add support for coords in % */
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "coords"))) {
//    /* Is this a valid poly ?
//     * rect = x0,y0,x1,y1               => 2
//     * circle = x,y,r                   => 2
//     * poly = x0,y0,x1,y1,x2,y2 minimum => 3 */
//    nbpoints = Html_read_coords(html, attrbuf, point);
// } else
//    return;
//
// if (Html_get_attr(html, tag, tagsize, "nohref")) {
//    link = -1;
//    _MSG("nohref");
// }
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
//    url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
//    dReturn_if_fail ( url != NULL );
//    if ((attrbuf = Html_get_attr(html, tag, tagsize, "alt")))
//       a_Url_set_alt(url, attrbuf);
//
//    link = Html_set_new_link(html, &url);
// }
//
// a_Dw_image_map_list_add_shape(&html->maps, type, link,
//                               point, nbpoints);
}


/*
 * Test and extract the link from a javascript instruction.
 */
static const char* Html_get_javascript_link(DilloHtml *html)
{
   size_t i;
   char ch, *p1, *p2;
   Dstr *Buf = html->attr_data;

   if (dStrncasecmp("javascript", Buf->str, 10) == 0) {
      i = strcspn(Buf->str, "'\"");
      ch = Buf->str[i];
      if ((ch == '"' || ch == '\'') &&
          (p2 = strchr(Buf->str + i + 1 , ch))) {
         p1 = Buf->str + i;
         MSG_HTML("link depends on javascript()\n");
         dStr_truncate(Buf, p2 - Buf->str);
         dStr_erase(Buf, 0, p1 - Buf->str + 1);
      }
   }
   return Buf->str;
}

/*
 * Register an anchor for this page.
 */
static void Html_add_anchor(DilloHtml *html, const char *name)
{
   _MSG("Registering ANCHOR: %s\n", name);
   if (!DW2TB(html->dw)->addAnchor (name, S_TOP(html)->style))
      MSG_HTML("Anchor names must be unique within the document\n");
   /*
    * According to Sec. 12.2.1 of the HTML 4.01 spec, "anchor names that
    * differ only in case may not appear in the same document", but
    * "comparisons between fragment identifiers and anchor names must be
    * done by exact (case-sensitive) match." We ignore the case issue and
    * always test for exact matches. Moreover, what does uppercase mean
    * for Unicode characters outside the ASCII range?
    */
}

/*
 * <A>
 */
static void Html_tag_open_a(DilloHtml *html, const char *tag, int tagsize)
{
   StyleAttrs style_attrs;
   Style *old_style;
   DilloUrl *url;
   const char *attrbuf;
   char *buf;
   int buf_size;

   /* todo: add support for MAP with A HREF */
   Html_tag_open_area(html, tag, tagsize);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
      /* if it's a javascript link, extract the reference. */
      if (tolower(attrbuf[0]) == 'j')
         attrbuf = Html_get_javascript_link(html);

      url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
      dReturn_if_fail ( url != NULL );

      old_style = S_TOP(html)->style;
      style_attrs = *old_style;

      if (a_Capi_get_buf(url, &buf, &buf_size)) {
         html->InVisitedLink = TRUE;
         style_attrs.color = Color::createSimple (
            HT2LT(html),
            html->visited_color
/*
            a_Color_vc(html->visited_color,
                       S_TOP(html)->style->color->getColor(),
                       html->link_color,
                       S_TOP(html)->current_bg_color),
*/
            );
      } else {
         style_attrs.color = Color::createSimple(HT2LT(html),
                                                 html->link_color);
      }

//    if ((attrbuf = Html_get_attr(html, tag, tagsize, "title")))
//       style_attrs.x_tooltip = a_Dw_tooltip_new_no_ref(attrbuf);

      style_attrs.textDecoration |= TEXT_DECORATION_UNDERLINE;
      style_attrs.x_link = Html_set_new_link(html, &url);
      style_attrs.cursor = CURSOR_POINTER;

      S_TOP(html)->style =
         Style::create (HT2LT(html), &style_attrs);
      old_style->unref ();
   }

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "name"))) {
      if (prefs.show_extra_warnings)
         Html_check_name_val(html, attrbuf, "name");
      /* html->NameVal is freed in Html_process_tag */
      html->NameVal = a_Url_decode_hex_str(attrbuf);
      Html_add_anchor(html, html->NameVal);
   }
}

/*
 * <A> close function
 */
static void Html_tag_close_a(DilloHtml *html, int TagIdx)
{
   html->InVisitedLink = FALSE;
   Html_pop_tag(html, TagIdx);
}

/*
 * Insert underlined text in the page.
 */
static void Html_tag_open_u(DilloHtml *html, const char *tag, int tagsize)
{
   Style *style;
   StyleAttrs style_attrs;

   style = S_TOP(html)->style;
   style_attrs = *style;
   style_attrs.textDecoration |= TEXT_DECORATION_UNDERLINE;
   S_TOP(html)->style =
      Style::create (HT2LT(html), &style_attrs);
   style->unref ();
}

/*
 * Insert strike-through text. Used by <S>, <STRIKE> and <DEL>.
 */
static void Html_tag_open_strike(DilloHtml *html, const char *tag, int tagsize)
{
   Style *style;
   StyleAttrs style_attrs;

   style = S_TOP(html)->style;
   style_attrs = *style;
   style_attrs.textDecoration |= TEXT_DECORATION_LINE_THROUGH;
   S_TOP(html)->style =
      Style::create (HT2LT(html), &style_attrs);
   style->unref ();
}

/*
 * <BLOCKQUOTE>
 */
static void Html_tag_open_blockquote(DilloHtml *html, 
                                     const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_add_indented(html, 40, 40, 9);
}

/*
 * Handle the <UL> tag.
 */
static void Html_tag_open_ul(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   ListStyleType list_style_type;

   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_add_indented(html, 40, 0, 9);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "type"))) {
      /* list_style_type explicitly defined */
      if (dStrncasecmp(attrbuf, "disc", 4) == 0)
         list_style_type = LIST_STYLE_TYPE_DISC;
      else if (dStrncasecmp(attrbuf, "circle", 6) == 0)
         list_style_type = LIST_STYLE_TYPE_CIRCLE;
      else if (dStrncasecmp(attrbuf, "square", 6) == 0)
         list_style_type = LIST_STYLE_TYPE_SQUARE;
      else
         /* invalid value */
         list_style_type = LIST_STYLE_TYPE_DISC;
   } else {
      if (S_TOP(html)->list_type == HTML_LIST_UNORDERED) {
         /* Nested <UL>'s. */
         /* --EG :: I changed the behavior here : types are cycling instead of
          * being forced to square. It's easier for mixed lists level counting.
          */
         switch (S_TOP(html)->style->listStyleType) {
         case LIST_STYLE_TYPE_DISC:
            list_style_type = LIST_STYLE_TYPE_CIRCLE;
            break;
         case LIST_STYLE_TYPE_CIRCLE:
            list_style_type = LIST_STYLE_TYPE_SQUARE;
            break;
         case LIST_STYLE_TYPE_SQUARE:
         default: /* this is actually a bug */
            list_style_type = LIST_STYLE_TYPE_DISC;
            break;
         }
      } else {
         /* Either first <UL>, or a <OL> before. */
         list_style_type = LIST_STYLE_TYPE_DISC;
      }
   }

   HTML_SET_TOP_ATTR(html, listStyleType, list_style_type);
   S_TOP(html)->list_type = HTML_LIST_UNORDERED;

   S_TOP(html)->list_number = 0;
   S_TOP(html)->ref_list_item = NULL;
}

/*
 * Handle the <MENU> tag.
 * (Deprecated and almost the same as <UL>)
 */
static void Html_tag_open_menu(DilloHtml *html, const char *tag, int tagsize)
{
   ListStyleType list_style_type = LIST_STYLE_TYPE_DISC;

   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_add_indented(html, 40, 0, 9);
   HTML_SET_TOP_ATTR(html, listStyleType, list_style_type);
   S_TOP(html)->list_type = HTML_LIST_UNORDERED;
   S_TOP(html)->list_number = 0;
   S_TOP(html)->ref_list_item = NULL;

   if (prefs.show_extra_warnings)
      MSG_HTML("it is strongly recommended using <UL> instead of <MENU>\n");
}

/*
 * Handle the <OL> tag.
 */
static void Html_tag_open_ol(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   ListStyleType list_style_type;
   int n = 1;

   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_add_indented(html, 40, 0, 9);

   list_style_type = LIST_STYLE_TYPE_DECIMAL;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "type"))) {
      if (*attrbuf == '1')
         list_style_type = LIST_STYLE_TYPE_DECIMAL;
      else if (*attrbuf == 'a')
         list_style_type = LIST_STYLE_TYPE_LOWER_ALPHA;
      else if (*attrbuf == 'A')
         list_style_type = LIST_STYLE_TYPE_UPPER_ALPHA;
      else if (*attrbuf == 'i')
         list_style_type = LIST_STYLE_TYPE_LOWER_ROMAN;
      else if (*attrbuf == 'I')
         list_style_type = LIST_STYLE_TYPE_UPPER_ROMAN;
   }

   HTML_SET_TOP_ATTR(html, listStyleType, list_style_type);
   S_TOP(html)->list_type = HTML_LIST_ORDERED;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "start")) &&
       (n = (int) strtol(attrbuf, NULL, 10)) < 0) {
      MSG_HTML( "illegal '-' character in START attribute; Starting from 0\n");
      n = 0;
   }
   S_TOP(html)->list_number = n;
   S_TOP(html)->ref_list_item = NULL;
}

/*
 * Handle the <LI> tag.
 */
static void Html_tag_open_li(DilloHtml *html, const char *tag, int tagsize)
{
   StyleAttrs style_attrs;
   Style *item_style, *word_style;
   Widget **ref_list_item;
   ListItem *list_item;
   int *list_number;
   const char *attrbuf;
   char buf[16];

   /* Get our parent tag's variables (used as state storage) */
   list_number = &html->stack->getRef(html->stack->size()-2)->list_number;
   ref_list_item = &html->stack->getRef(html->stack->size()-2)->ref_list_item;

   /* set the item style */
   word_style = S_TOP(html)->style;
   style_attrs = *word_style;
 //style_attrs.backgroundColor = Color::createSimple (HT2LT(html), 0xffff40);
 //style_attrs.setBorderColor (Color::createSimple (HT2LT(html), 0x000000));
 //style_attrs.setBorderStyle (BORDER_SOLID);
 //style_attrs.borderWidth.setVal (1);
   item_style = Style::create (HT2LT(html), &style_attrs);

   DW2TB(html->dw)->addParbreak (2, word_style);

   list_item = new ListItem ((ListItem*)*ref_list_item,prefs.limit_text_width);
   DW2TB(html->dw)->addWidget (list_item, item_style);
   DW2TB(html->dw)->addParbreak (2, word_style);
   *ref_list_item = list_item;
   S_TOP(html)->textblock = html->dw = list_item;
   item_style->unref();
   /* Handle it when the user clicks on a link */
   html->connectSignals(list_item);

   switch (S_TOP(html)->list_type) {
   case HTML_LIST_ORDERED:
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "value")) &&
          (*list_number = strtol(attrbuf, NULL, 10)) < 0) {
         MSG_HTML("illegal negative LIST VALUE attribute; Starting from 0\n");
          *list_number = 0;
      }
      numtostr((*list_number)++, buf, 16, S_TOP(html)->style->listStyleType);
      list_item->initWithText (dStrdup(buf), word_style);
      list_item->addSpace (word_style);
      break;
   case HTML_LIST_NONE:
      MSG_HTML("<li> outside <ul> or <ol>\n");
   default:
      list_item->initWithWidget (new Bullet(), word_style);
      list_item->addSpace (word_style);
      break;
   }

   // list_item->flush (); Looks like there's no need to flush.
   // If it MUST be done, it could be inside Html_tag_close_li().
}

/*
 * Close <LI>
 */
static void Html_tag_close_li(DilloHtml *html, int TagIdx)
{
   ((ListItem *)html->dw)->flush ();
   Html_pop_tag(html, TagIdx);
}

/*
 * <HR>
 */
static void Html_tag_open_hr(DilloHtml *html, const char *tag, int tagsize)
{
   Widget *hruler;
   StyleAttrs style_attrs;
   Style *style;
   char *width_ptr;
   const char *attrbuf;
   int32_t size = 0;
  
   style_attrs = *S_TOP(html)->style;

   width_ptr = Html_get_attr_wdef(html, tag, tagsize, "width", "100%");
   style_attrs.width = Html_parse_length (html, width_ptr);
   dFree(width_ptr);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
      size = strtol(attrbuf, NULL, 10);
  
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "align"))) {
      if (dStrcasecmp (attrbuf, "left") == 0)
         style_attrs.textAlign = TEXT_ALIGN_LEFT;
      else if (dStrcasecmp (attrbuf, "right") == 0)
         style_attrs.textAlign = TEXT_ALIGN_RIGHT;
      else if (dStrcasecmp (attrbuf, "center") == 0)
         style_attrs.textAlign = TEXT_ALIGN_CENTER;
   }
  
   /* todo: evaluate attribute */
   if (Html_get_attr(html, tag, tagsize, "noshade")) {
      style_attrs.setBorderStyle (BORDER_SOLID);
      style_attrs.setBorderColor (
         Color::createShaded (HT2LT(html), style_attrs.color->getColor()));
      if (size < 1)
         size = 1;
   } else {
      style_attrs.setBorderStyle (BORDER_INSET);
      style_attrs.setBorderColor (
         Color::createShaded (HT2LT(html),
                              (S_TOP(html)->current_bg_color)));
      if (size < 2)
         size = 2;
   }
  
   style_attrs.borderWidth.top =
      style_attrs.borderWidth.left = (size + 1) / 2;
   style_attrs.borderWidth.bottom =
      style_attrs.borderWidth.right = size / 2;
   style = Style::create (HT2LT(html), &style_attrs);

   DW2TB(html->dw)->addParbreak (5, S_TOP(html)->style);
   hruler = new Ruler();
   hruler->setStyle (style);
   DW2TB(html->dw)->addWidget (hruler, style);
   style->unref ();
   DW2TB(html->dw)->addParbreak (5, S_TOP(html)->style);
}

/*
 * <DL>
 */
static void Html_tag_open_dl(DilloHtml *html, const char *tag, int tagsize)
{
   /* may want to actually do some stuff here. */
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
}

/*
 * <DT>
 */
static void Html_tag_open_dt(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <DD>
 */
static void Html_tag_open_dd(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_add_indented(html, 40, 40, 9);
}

/*
 * <PRE>
 */
static void Html_tag_open_pre(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_set_top_font(html, prefs.fw_fontname, 0, 0, 0);

   /* Is the placement of this statement right? */
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_PRE;
   HTML_SET_TOP_ATTR (html, whiteSpace, WHITE_SPACE_PRE);
   html->pre_column = 0;
   html->PreFirstChar = TRUE;
   html->InFlags |= IN_PRE;
}

/*
 * Custom close for <PRE>
 */
static void Html_tag_close_pre(DilloHtml *html, int TagIdx)
{
   html->InFlags &= ~IN_PRE;
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_pop_tag(html, TagIdx);
}

/*
 * Check whether a tag is in the "excluding" element set for PRE
 * Excl. Set = {IMG, OBJECT, APPLET, BIG, SMALL, SUB, SUP, FONT, BASEFONT}
 */
static int Html_tag_pre_excludes(int tag_idx)
{
   const char *es_set[] = {"img", "object", "applet", "big", "small", "sub",
                           "sup", "font", "basefont", NULL};
   static int ei_set[10], i;

   /* initialize array */
   if (!ei_set[0])
      for (i = 0; es_set[i]; ++i)
         ei_set[i] = Html_tag_index(es_set[i]);

   for (i = 0; ei_set[i]; ++i)
      if (tag_idx == ei_set[i])
         return 1;
   return 0;
}

/*
 * Handle <FORM> tag
 */
static void Html_tag_open_form(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *action;
   DilloHtmlMethod method;
   DilloHtmlEnc enc;
   const char *attrbuf;

   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);

   if (html->InFlags & IN_FORM) {
      MSG_HTML("nested forms\n");
      return;
   }
   html->InFlags |= IN_FORM;
   html->InFlags &= ~IN_SELECT;
   html->InFlags &= ~IN_OPTION;
   html->InFlags &= ~IN_TEXTAREA;

   method = DILLO_HTML_METHOD_GET;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "method"))) {
      if (!dStrcasecmp(attrbuf, "post"))
         method = DILLO_HTML_METHOD_POST;
      /* todo: maybe deal with unknown methods? */
   }
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "action")))
      action = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
   else
      action = a_Url_dup(html->base_url);
   enc = DILLO_HTML_ENC_URLENCODING;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "encoding"))) {
      /* todo: maybe deal with unknown encodings? */
   }
   html->formNew(method, action, enc);
   a_Url_free(action);
}

static void Html_tag_close_form(DilloHtml *html, int TagIdx)
{
   static const char *SubmitTag =
      "<input type='submit' value='?Submit?' alt='dillo-generated-button'>";
   DilloHtmlForm *form;
// int i;
  
   if (html->InFlags & IN_FORM) {
      form = html->forms->getRef (html->forms->size() - 1);
      /* If we don't have a submit button and the user desires one,
         let's add a custom one */
      if (form->num_submit_buttons == 0) {
         if (prefs.show_extra_warnings || form->num_entry_fields != 1)
            MSG_HTML("FORM lacks a Submit button\n");
         if (prefs.generate_submit) {
            MSG_HTML(" (added a submit button internally)\n");
            Html_tag_open_input(html, SubmitTag, strlen(SubmitTag));
            form->num_submit_buttons = 0;
         }
      }
  
//    /* Make buttons sensitive again */
//    for (i = 0; i < form->inputs->size(); i++) {
//       input_i = form->inputs->getRef(i);
//       /* Check for tricky HTML (e.g. <input type=image>) */
//       if (!input_i->widget)
//          continue;
//       if (input_i->type == DILLO_HTML_INPUT_SUBMIT ||
//           input_i->type == DILLO_HTML_INPUT_RESET) {
//          gtk_widget_set_sensitive(input_i->widget, TRUE);
//       } else if (input_i->type == DILLO_HTML_INPUT_IMAGE ||
//                  input_i->type == DILLO_HTML_INPUT_BUTTON_SUBMIT ||
//                  input_i->type == DILLO_HTML_INPUT_BUTTON_RESET) {
//          a_Dw_button_set_sensitive(DW_BUTTON(input_i->widget), TRUE);
//       }
//    }
   }

   html->InFlags &= ~IN_FORM;
   html->InFlags &= ~IN_SELECT;
   html->InFlags &= ~IN_OPTION;
   html->InFlags &= ~IN_TEXTAREA;

   Html_pop_tag(html, TagIdx);
}

/*
 * Handle <META>
 * We do not support http-equiv=refresh because it's non standard,
 * (the HTML 4.01 SPEC recommends explicitily to avoid it), and it
 * can be easily abused!
 *
 * More info at:
 *   http://lists.w3.org/Archives/Public/www-html/2000Feb/thread.html#232
 *
 * todo: Note that we're sending custom HTML while still IN_HEAD. This
 * is a hackish way to put the message. A much cleaner approach is to
 * build a custom widget for it.
 */
static void Html_tag_open_meta(DilloHtml *html, const char *tag, int tagsize)
{
   const char meta_template[] =
"<table width='100%%'><tr><td bgcolor='#ee0000'>Warning:</td>\n"
" <td bgcolor='#8899aa' width='100%%'>\n"
" This page uses the NON-STANDARD meta refresh tag.<br> The HTML 4.01 SPEC\n"
" (sec 7.4.4) recommends explicitly to avoid it.</td></tr>\n"
" <tr><td bgcolor='#a0a0a0' colspan='2'>The author wanted you to go\n"
" <a href='%s'>here</a>%s</td></tr></table><br>\n";

   const char *equiv, *content;
   char delay_str[64];
   Dstr *ds_msg;
   int delay;

   /* only valid inside HEAD */
   if (!(html->InFlags & IN_HEAD)) {
      MSG_HTML("META elements must be inside the HEAD section\n");
      return;
   }

   if ((equiv = Html_get_attr(html, tag, tagsize, "http-equiv")) &&
       !dStrcasecmp(equiv, "refresh") &&
       (content = Html_get_attr(html, tag, tagsize, "content"))) {

      /* Get delay, if present, and make a message with it */
      if ((delay = strtol(content, NULL, 0)))
         snprintf(delay_str, 64, " after %d second%s.",
                    delay, (delay > 1) ? "s" : "");
      else
         sprintf(delay_str, ".");

      /* Skip to anything after "URL=" */
      while (*content && *(content++) != '=');

      /* Send a custom HTML message
       * todo: this is a hairy hack, It'd be much better to build a widget. */
      ds_msg = dStr_sized_new(256);
      dStr_sprintf(ds_msg, meta_template, content, delay_str);
      {
         int SaveFlags = html->InFlags;
         html->InFlags = IN_BODY;
         html->TagSoup = FALSE;
         Html_write_raw(html, ds_msg->str, ds_msg->len, 0);
         html->TagSoup = TRUE;
         html->InFlags = SaveFlags;
      }
      dStr_free(ds_msg, 1);
   }
}

/*
 * Set the history of the menu to be consistent with the active menuitem.
 */
//static void Html_select_set_history(DilloHtmlInput *input)
//{
// int i;
//
// for (i = 0; i < input->select->num_options; i++) {
//    if (GTK_CHECK_MENU_ITEM(input->select->options[i].menuitem)->active) {
//       gtk_option_menu_set_history(GTK_OPTION_MENU(input->widget), i);
//       break;
//    }
// }
//}

/*
 * Reset the input widget to the initial value.
 */
static void Html_reset_input(DilloHtmlInput *input)
{
   switch (input->type) {
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
      EntryResource *entryres;
      entryres = (EntryResource*)((Embed*)input->widget)->getResource();
      entryres->setText(input->init_str ? input->init_str : "");
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
      ToggleButtonResource *tb_r;
      tb_r = (ToggleButtonResource*)((Embed*)input->widget)->getResource();
      tb_r->setActivated(input->init_val);
      break;
   case DILLO_HTML_INPUT_SELECT:
      if (input->select != NULL) {
         /* this is in reverse order so that, in case more than one was
          * selected, we get the last one, which is consistent with handling
          * of multiple selected options in the layout code. */
//       for (i = input->select->num_options - 1; i >= 0; i--) {
//          if (input->select->options[i].init_val) {
//             gtk_menu_item_activate(GTK_MENU_ITEM
//                                    (input->select->options[i].menuitem));
//             Html_select_set_history(input);
//             break;
//          }
//       }
      }
      break;
   case DILLO_HTML_INPUT_SEL_LIST:
      if (!input->select)
         break;
//    for (i = 0; i < input->select->num_options; i++) {
//       if (input->select->options[i].init_val) {
//          if (input->select->options[i].menuitem->state == GTK_STATE_NORMAL)
//             gtk_list_select_child(GTK_LIST(input->select->menu),
//                                   input->select->options[i].menuitem);
//       } else {
//          if (input->select->options[i].menuitem->state==GTK_STATE_SELECTED)
//             gtk_list_unselect_child(GTK_LIST(input->select->menu),
//                                     input->select->options[i].menuitem);
//       }
//    }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      if (input->init_str != NULL) {
         MultiLineTextResource *textres;
         textres =
            (MultiLineTextResource*)
            ((Embed*)input->widget)->getResource();
         textres->setText(input->init_str ? input->init_str : "");
      }
      break;
   default:
      break;
   }
}


/*
 * Add a new input to the form data structure, setting the initial
 * values.
 */
static void Html_add_input(DilloHtmlForm *form,
                           DilloHtmlInputType type,
                           Widget *widget,
                           Embed *embed,
                           const char *name,
                           const char *init_str,
                           DilloHtmlSelect *select,
                           bool_t init_val)
{
   DilloHtmlInput *input;

   _MSG("name=[%s] init_str=[%s] init_val=[%d]\n",
        name, init_str, init_val);
   form->inputs->increase();
   input = form->inputs->getRef (form->inputs->size() - 1);
   input->type = type;
   input->widget = widget;
   input->embed = embed;
   input->name = (name) ? dStrdup(name) : NULL;
   input->init_str = (init_str) ? dStrdup(init_str) : NULL;
   input->select = select;
   input->init_val = init_val;
   Html_reset_input(input);

   /* some stats */
   if (type == DILLO_HTML_INPUT_PASSWORD ||
       type == DILLO_HTML_INPUT_TEXT ||
       type == DILLO_HTML_INPUT_TEXTAREA) {
      form->num_entry_fields++;
   } else if (type == DILLO_HTML_INPUT_SUBMIT ||
              type == DILLO_HTML_INPUT_BUTTON_SUBMIT ||
              type == DILLO_HTML_INPUT_IMAGE) {
      form->num_submit_buttons++;
   }
}


/*
 * Given a GtkWidget, find the form that contains it.
 * Return value: form_index if successful, -1 otherwise.
 */
//static int Html_find_form(GtkWidget *reset, DilloHtmlLB *html_lb)
//{
// int form_index;
// int input_idx;
// DilloHtmlForm *form;
//
// for (form_index = 0; form_index < html_lb->forms->size(); form_index++) {
//    form = html_lb->forms->getRef (form_index);
//    for (input_idx = 0; input_idx < form->inputs->size(); input_idx++) {
//       if (form->inputs->get(input_idx).widget == reset) {
//          return form_index;
//       }
//    }
// }
// return -1;
//}

/*
 * Reset all inputs in the form containing reset to their initial values.
 * In general, reset is the reset button for the form.
 */
//static void Html_reset_form(GtkWidget *reset, DilloHtmlLB *html_lb)
//{
//   int i, j;
//   DilloHtmlForm *form;
//
//   if ((i = Html_find_form(reset, html_lb)) != -1){
//      form = html_lb->forms->getRef (i);
//      for ( j = 0; j < form->inputs->size(); j++)
//         Html_reset_input(&(form->inputs[j]));
//   }
//}

/*
 * Urlencode 'val' and append it to 'str'
 */
static void Html_urlencode_append(Dstr *str, const char *val)
{
   char *enc_val = a_Url_encode_hex_str(val);
   dStr_append(str, enc_val);
   dFree(enc_val);
}

/*
 * Append a name-value pair to an existing url.
 * (name and value are urlencoded before appending them)
 */
static void
 Html_append_input(Dstr *url, const char *name, const char *value)
{
   if (name != NULL) {
      Html_urlencode_append(url, name);
      dStr_append_c(url, '=');
      Html_urlencode_append(url, value);
      dStr_append_c(url, '&');
   }
}

/*
 * Append a image button click position to an existing url.
 */
//static void Html_append_clickpos(Dstr *url, const char *name, int x, int y)
//{
//   if (name) {
//      Html_urlencode_append(url, name);
//      dStr_sprintfa(url, ".x=%d&", x);
//      Html_urlencode_append(url, name);
//      dStr_sprintfa(url, ".y=%d&", y);
//   } else
//      dStr_sprintfa(url, "x=%d&y=%d&", x, y);
//}

/*
 * Submit the form containing the submit input by making a new query URL
 * and sending it with a_Nav_push.
 * (Called by GTK+)
 * click_x and click_y are used only by input images and are set only when
 * called by Html_image_clicked. GTK+ does NOT give these arguments.
 */
static void Html_submit_form2(DilloHtml *html, DilloHtmlForm *form,
                              int e_input_idx)
{
   int input_idx;
   DilloHtmlInput *input;
   DilloUrl *new_url;
   char *url_str, *action_str, *p;
  
   if ((form->method == DILLO_HTML_METHOD_GET) ||
       (form->method == DILLO_HTML_METHOD_POST)) {
      Dstr *DataStr = dStr_sized_new(4096);
  
      _MSG("Html_submit_form2: form->action=%s\n",URL_STR_(form->action));
  
      for (input_idx = 0; input_idx < form->inputs->size(); input_idx++) {
         input = form->inputs->getRef (input_idx);
         switch (input->type) {
         case DILLO_HTML_INPUT_TEXT:
         case DILLO_HTML_INPUT_PASSWORD:
            EntryResource *entryres;
            entryres = (EntryResource*)((Embed*)input->widget)->getResource();
            Html_append_input(DataStr, input->name, entryres->getText());
            break;
         case DILLO_HTML_INPUT_TEXTAREA:
            MultiLineTextResource *textres;
            textres = (MultiLineTextResource*)((Embed*)input->widget)
                         ->getResource();
            Html_append_input(DataStr, input->name, textres->getText());
            break;
         case DILLO_HTML_INPUT_CHECKBOX:
         case DILLO_HTML_INPUT_RADIO:
            ToggleButtonResource *cb_r;
            cb_r=(ToggleButtonResource*)((Embed*)input->widget)->getResource();
            if (input->name && input->init_str && cb_r->isActivated()) {
               Html_append_input(DataStr, input->name, input->init_str);
            }
            break;
         case DILLO_HTML_INPUT_HIDDEN:
            Html_append_input(DataStr, input->name, input->init_str);
            break;
         case DILLO_HTML_INPUT_SELECT:
         case DILLO_HTML_INPUT_SEL_LIST:
         {  // brackets for compiler happiness.
            SelectionResource *sel_res =
               (SelectionResource*)((Embed*)input->widget)->getResource();
            int size = input->select->options->size ();
            for (int i = 0; i < size; i++) {
               if (sel_res->isSelected(i)) {
                  DilloHtmlOption *option =
                     input->select->options->get (i);
                  char *val = option->value ? option->value : option->content;
                  Html_append_input(DataStr, input->name, val);
               }
            }
            break;
         }
//       case DILLO_HTML_INPUT_INDEX:
//          Html_urlencode_append(DataStr,
//             gtk_entry_get_text(GTK_ENTRY(input->widget)));
//          break;
//       case DILLO_HTML_INPUT_IMAGE:
//          if (input->widget == submit) {
//             Html_append_input(DataStr, input->name, input->init_str);
//             Html_append_clickpos(DataStr, input->name, click_x, click_y);
//          }
//          break;
         case DILLO_HTML_INPUT_SUBMIT:
         case DILLO_HTML_INPUT_BUTTON_SUBMIT:
            if (input_idx == e_input_idx && form->num_submit_buttons > 0)
               Html_append_input(DataStr, input->name, input->init_str);
            break;
         default:
            break;
         } /* switch */
      } /* for (inputs) */
  
      if (DataStr->str[DataStr->len - 1] == '&')
         dStr_truncate(DataStr, DataStr->len - 1);
  
      /* form->action was previously resolved against base URL */
      action_str = dStrdup(URL_STR(form->action));
  
      if (form->method == DILLO_HTML_METHOD_POST) {
         new_url = a_Url_new(action_str, NULL, 0, 0, 0);
         a_Url_set_data(new_url, DataStr->str);
         a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Post);
      } else {
         /* remove <fragment> and <query> sections if present */
         if ((p = strchr(action_str, '#')))
            *p = 0;
         if ((p = strchr(action_str, '?')))
            *p = 0;
  
         url_str = dStrconcat(action_str, "?", DataStr->str, NULL);
         new_url = a_Url_new(url_str, NULL, 0, 0, 0);
         a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Get);
         dFree(url_str);
      }
  
      a_Nav_push(html->bw, new_url);
      dFree(action_str);
      dStr_free(DataStr, TRUE);
      a_Url_free(new_url);
   } else {
      MSG("Html_submit_form2: Method unknown\n");
   }
  
// /* now, make the rendered area have its focus back */
// gtk_widget_grab_focus(GTK_BIN(html->bw->render_main_scroll)->child);
}

/*
 * Handler for events related to forms.
 *
 * TODO: Currently there's "clicked" for buttons, we surely need "enter" for
 * textentries, and maybe the "mouseover, ...." set for Javascript.
 */
void a_Html_form_event_handler(void *data,
                               form::Form *form_receiver,
                               void *v_resource)
{
   int form_index, input_idx = -1, idx;
   DilloHtmlForm *form = NULL;
   DilloHtml *html = (DilloHtml*)data;

   MSG("Html_form_event_handler %p %p\n", html, form_receiver);

   /* Search the form that generated the submit event */
   for (form_index = 0; form_index < html->forms->size(); form_index++) {
      form = html->forms->getRef (form_index);
      if (form->form_receiver == form_receiver) {
         /* form found, let's get the input index for this event */
         for (idx = 0; idx < form->inputs->size(); idx++) {
            DilloHtmlInput *input = form->inputs->getRef(idx);
            if (input->embed &&
                v_resource == (void*)((Embed*)input->widget)->getResource()) {
               input_idx = idx;
               break;
            }
         }
         break;
      }
   }
   if (form_index == html->forms->size()) {
      MSG("a_Html_form_event_handler: ERROR, form not found!\n");
   } else {
      Html_submit_form2(html, form, input_idx);
   }
}

/*
 * Submit form if it has no submit button.
 * (Called by GTK+ when the user presses enter in a text entry within a form)
 */
//static void Html_enter_submit_form(GtkWidget *submit, DilloHtmlLB *html_lb)
//{
//   int i;
//
//   /* Search the form that generated the submit event */
//   if ((i = Html_find_form(submit, html_lb)) == -1)
//      return;
//
//   /* Submit on enterpress when there's a single text-entry only,
//    * or if the user set enter to always submit */
//   if ((html_lb->forms->getRef(i))->.num_entry_fields == 1) ||
//       prefs.enterpress_forces_submit)
//      Html_submit_form(submit, html_lb, 1, 1);
//}

/*
 * Call submit form, when input image has been clicked
 */
//static void Html_image_clicked(Widget *widget, int x, int y,
//                               DilloHtmlLB *lb)
//{
//   _MSG("Hallo! (%d, %d, %p)\n", x, y, lb);
//   Html_submit_form((GtkWidget*) widget, lb, x, y);
//}

/*
 * Create input image for the form
 */
static Widget *Html_input_image(DilloHtml *html, const char *tag, int tagsize,
                                DilloUrl *action)
{
// // AL
// DilloImage *Image;
// Widget *button;
// DilloUrl *url = NULL;
// Style style_attrs;
// const char *attrbuf;
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "src")) &&
//     (url = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0))) {
//    button = a_Dw_button_new (0, FALSE);
//    DW2TB(html->dw)->addWidget (button,
//                                S_TOP(html)->style);
//    gtk_signal_connect(GTK_OBJECT(button), "clicked_at",
//                       GTK_SIGNAL_FUNC(Html_image_clicked), html_lb);
//    a_Dw_button_set_sensitive(DW_BUTTON(button), FALSE);
//
//    /* create new image and add it to the button */
//    if ((Image = Html_add_new_image(html, tag, tagsize, &style_attrs,
//                                    FALSE))) {
//       /* By suppressing the "image_pressed" signal, the events are sent
//        * to the parent DwButton */
//       a_Dw_widget_set_button_sensitive (IM2DW(Image->dw), FALSE);
//       IM2DW(Image->dw)->setStyle (S_TOP(html)->style);
//       a_Dw_container_add(DW_CONTAINER(button), IM2DW(Image->dw));
//       IM2DW(Image->dw)->setCursor (CURSOR_HAND);
//       Html_load_image(html->bw, url, Image);
//       a_Url_free(url);
//       return button;
//    }
// }
//
// DEBUG_MSG(10, "Html_input_image: unable to create image submit.\n");
// a_Url_free(url);
   return NULL;
}

/*
 * Add a new input to current form
 */
static void Html_tag_open_input(DilloHtml *html, const char *tag, int tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlInputType inp_type;
   Widget *widget = NULL;
   Embed *embed = NULL;
   char *value, *name, *type, *init_str;
// const char *attrbuf, *label;
   bool_t init_val = FALSE;
   int input_idx;
  
   if (!(html->InFlags & IN_FORM)) {
      MSG_HTML("<input> element outside <form>\n");
      return;
   }
  
   form = html->forms->getRef (html->forms->size() - 1);
  
   /* Get 'value', 'name' and 'type' */
   value = Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
   name = Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   type = Html_get_attr_wdef(html, tag, tagsize, "type", "");
  
   init_str = NULL;
   inp_type = DILLO_HTML_INPUT_UNKNOWN;
   if (!dStrcasecmp(type, "password")) {
      inp_type = DILLO_HTML_INPUT_PASSWORD;
      EntryResource *entryResource =
         HT2LT(html)->getResourceFactory()->createEntryResource (10, true);
      embed = new Embed (entryResource);
      widget = embed;
      init_str = (value) ? value : NULL;
   } else if (!dStrcasecmp(type, "checkbox")) {
      inp_type = DILLO_HTML_INPUT_CHECKBOX;
      CheckButtonResource *check_b_r = HT2LT(html)->getResourceFactory()
         ->createCheckButtonResource(false);
      embed = new Embed (check_b_r);
      widget = embed;
      init_val = (Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : dStrdup("on");
   } else if (!dStrcasecmp(type, "radio")) {
      inp_type = DILLO_HTML_INPUT_RADIO;
      RadioButtonResource *rb_r = NULL;
      for (input_idx = 0; input_idx < form->inputs->size(); input_idx++) {
         DilloHtmlInput *input = form->inputs->getRef(input_idx);
         if (input->type == DILLO_HTML_INPUT_RADIO &&
             (input->name && !dStrcasecmp(input->name, name)) ) {
            rb_r =(RadioButtonResource*)((Embed*)input->widget)->getResource();
            break;
         }
      }
      rb_r = HT2LT(html)->getResourceFactory()
                ->createRadioButtonResource(rb_r, false);
      embed = new Embed (rb_r);
      widget = embed;
  
      init_val = (Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : NULL;
   } else if (!dStrcasecmp(type, "hidden")) {
      inp_type = DILLO_HTML_INPUT_HIDDEN;
      if (value)
         init_str = dStrdup(Html_get_attr(html, tag, tagsize, "value"));
   } else if (!dStrcasecmp(type, "submit")) {
      inp_type = DILLO_HTML_INPUT_SUBMIT;
      init_str = (value) ? value : dStrdup("submit");
      LabelButtonResource *label_b_r = HT2LT(html)->getResourceFactory()
         ->createLabelButtonResource(init_str);
      widget = embed = new Embed (label_b_r);
//    gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
      label_b_r->connectClicked (form->form_receiver);
   } else if (!dStrcasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_RESET;
      init_str = (value) ? value : dStrdup("Reset");
      LabelButtonResource *label_b_r = HT2LT(html)->getResourceFactory()
         ->createLabelButtonResource(init_str);
      widget = embed = new Embed (label_b_r);
//    gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
//    gtk_signal_connect(GTK_OBJECT(widget), "clicked",
//                       GTK_SIGNAL_FUNC(Html_reset_form), html_lb);
// } else if (!dStrcasecmp(type, "image")) {
//    if (URL_FLAGS(html->base_url) & URL_SpamSafe) {
//       /* Don't request the image, make a text submit button instead */
//       inp_type = DILLO_HTML_INPUT_SUBMIT;
//       attrbuf = Html_get_attr(html, tag, tagsize, "alt");
//       label = attrbuf ? attrbuf : value ? value : name ? name : "Submit";
//       init_str = dStrdup(label);
//       widget = gtk_button_new_with_label(init_str);
//       gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
//       gtk_signal_connect(GTK_OBJECT(widget), "clicked",
//                          GTK_SIGNAL_FUNC(Html_submit_form), html_lb);
//    } else {
//       inp_type = DILLO_HTML_INPUT_IMAGE;
//       /* use a dw_image widget */
//       widget = (GtkWidget*) Html_input_image(html, tag, tagsize,
//                                              html_lb, form->action);
//       init_str = value;
//    }
// } else if (!dStrcasecmp(type, "file")) {
//    /* todo: implement it! */
//    inp_type = DILLO_HTML_INPUT_FILE;
//    init_str = (value) ? value : NULL;
//    MSG("An input of the type \"file\" wasn't rendered!\n");
   } else if (!dStrcasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
      if (value) {
         init_str = value;
         LabelButtonResource *label_b_r = HT2LT(html)->getResourceFactory()
            ->createLabelButtonResource(init_str);
         widget = embed = new Embed (label_b_r);
      }
   } else if (!dStrcasecmp(type, "text") || !*type) {
      /* Text input, which also is the default */
      inp_type = DILLO_HTML_INPUT_TEXT;
      EntryResource *entryResource =
         HT2LT(html)->getResourceFactory()->createEntryResource (10, false);
      widget = embed = new Embed (entryResource);
      entryResource->connectActivate (form->form_receiver);
      init_str = (value) ? value : NULL;
   } else {
      /* Unknown input type */
      MSG_HTML("Unknown input type: \"%s\"\n", type);
   }

   if (inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      Html_add_input(form, inp_type, widget, embed, name,
                     (init_str) ? init_str : "", NULL, init_val);
   }
  
   if (widget != NULL && inp_type != DILLO_HTML_INPUT_IMAGE && 
       inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      if (inp_type == DILLO_HTML_INPUT_TEXT ||
          inp_type == DILLO_HTML_INPUT_PASSWORD) {
         EntryResource *entryres = (EntryResource*)embed->getResource();
         /* Readonly or not? */
         if (Html_get_attr(html, tag, tagsize, "readonly"))
            entryres->setEditable(false);

//       /* Set width of the entry */
//       if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
//          gtk_widget_set_usize(widget, (strtol(attrbuf, NULL, 10) + 1) *
//                               gdk_char_width(widget->style->font, '0'), 0);
//
//       /* Maximum length of the text in the entry */
//       if ((attrbuf = Html_get_attr(html, tag, tagsize, "maxlength")))
//          gtk_entry_set_max_length(GTK_ENTRY(widget),
//                                   strtol(attrbuf, NULL, 10));
      }

      DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);
   }
  
   dFree(type);
   dFree(name);
   if (init_str != value)
      dFree(init_str);
   dFree(value);
}

/*
 * The ISINDEX tag is just a deprecated form of <INPUT type=text> with
 * implied FORM, afaics.
 */
static void Html_tag_open_isindex(DilloHtml *html,
                                  const char *tag, int tagsize)
{
// // AL
// DilloHtmlForm *form;
// DilloHtmlLB *html_lb;
// DilloUrl *action;
// GtkWidget *widget;
// Widget *embed_gtk;
// const char *attrbuf;
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "action")))
//    action = Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
// else
//    action = a_Url_dup(html->base_url);
//
// html->formNew(DILLO_HTML_METHOD_GET, action,
//               DILLO_HTML_ENC_URLENCODING);
//
// form = html->forms->getRef (html_lb->forms->size() - 1);
//
// DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
//
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "prompt")))
//    DW2TB(html->dw)->addText(dStrdup(attrbuf),
//                             S_TOP(html)->style);
//
// widget = gtk_entry_new();
// Html_add_input(form, DILLO_HTML_INPUT_INDEX,
//                widget, NULL, NULL, NULL, FALSE);
// gtk_signal_connect(GTK_OBJECT(widget), "activate",
//                    GTK_SIGNAL_FUNC(Html_enter_submit_form),
//                    html_lb);
// gtk_widget_show(widget);
// /* compare <input type=text> */
// gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
//                          GTK_SIGNAL_FUNC(gtk_true),
//                          NULL);
//
// embed_gtk = a_Dw_embed_gtk_new();
// a_Dw_embed_gtk_add_gtk(DW_EMBED_GTK(embed_gtk), widget);
// DW2TB(html->dw)->addWidget (embed_gtk,
//                             S_TOP(html)->style);
//
// a_Url_free(action);
}

/*
 * Close  textarea
 * (TEXTAREA is parsed in VERBATIM mode, and entities are handled here)
 */
static void Html_tag_close_textarea(DilloHtml *html, int TagIdx)
{
   char *str;
   DilloHtmlForm *form;
   Widget *widget;
   int i;

   if (html->InFlags & IN_FORM &&
       html->InFlags & IN_TEXTAREA) {
      /* Remove the line ending that follows the opening tag */
      if (html->Stash->str[0] == '\r')
         dStr_erase(html->Stash, 0, 1);
      if (html->Stash->str[0] == '\n')
         dStr_erase(html->Stash, 0, 1);
   
      /* As the spec recommends to canonicalize line endings, it is safe
       * to replace '\r' with '\n'. It will be canonicalized anyway! */
      for (i = 0; i < html->Stash->len; ++i) {
         if (html->Stash->str[i] == '\r') {
            if (html->Stash->str[i + 1] == '\n')
               dStr_erase(html->Stash, i, 1);
            else
               html->Stash->str[i] = '\n';
         }
      }
   
      /* The HTML3.2 spec says it can have "text and character entities". */
      str = Html_parse_entities(html, html->Stash->str, html->Stash->len);
      form = html->forms->getRef (html->forms->size() - 1);
      form->inputs->get(form->inputs->size() - 1).init_str = str;
      widget = (Widget*)(form->inputs->get(form->inputs->size() - 1).widget);
      ((MultiLineTextResource *)((Embed *)widget)->getResource ())
         ->setText(str);

      html->InFlags &= ~IN_TEXTAREA;
   }
   Html_pop_tag(html, TagIdx);
}

/*
 * The textarea tag
 * (todo: It doesn't support wrapping).
 */
static void Html_tag_open_textarea(DilloHtml *html,
                                   const char *tag, int tagsize)
{
   DilloHtmlForm *form;
   char *name;
   const char *attrbuf;
   int cols, rows;
  
   /* We can't push a new <FORM> because the 'action' URL is unknown */
   if (!(html->InFlags & IN_FORM)) {
      MSG_HTML("<textarea> outside <form>\n");
      html->ReqTagClose = TRUE;
      return;
   }
   if (html->InFlags & IN_TEXTAREA) {
      MSG_HTML("nested <textarea>\n");
      html->ReqTagClose = TRUE;
      return;
   }
  
   html->InFlags |= IN_TEXTAREA;
   form = html->forms->getRef (html->forms->size() - 1);
   Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
  
   cols = 20;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cols")))
      cols = strtol(attrbuf, NULL, 10);
   rows = 10;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "rows")))
      rows = strtol(attrbuf, NULL, 10);
   name = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "name")))
      name = dStrdup(attrbuf);

   MultiLineTextResource *textres =
      HT2LT(html)->getResourceFactory()->createMultiLineTextResource (cols,
                                                                      rows);

   Widget *widget;
   Embed *embed;
   widget = embed = new Embed(textres);
   /* Readonly or not? */
   if (Html_get_attr(html, tag, tagsize, "readonly"))
      textres->setEditable(false);

   Html_add_input(form, DILLO_HTML_INPUT_TEXTAREA, widget, embed, name,
                  NULL, NULL, false);

   DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);

// widget = gtk_text_new(NULL, NULL);
// /* compare <input type=text> */
// gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
//                          GTK_SIGNAL_FUNC(gtk_true),
//                          NULL);
//
// /* Calculate the width and height based on the cols and rows
//  * todo: Get it right... Get the metrics from the font that will be used.
//  */
// gtk_widget_set_usize(widget, 6 * cols, 16 * rows);
//
// /* If the attribute readonly isn't specified we make the textarea
//  * editable. If readonly is set we don't have to do anything.
//  */
// if (!Html_get_attr(html, tag, tagsize, "readonly"))
//    gtk_text_set_editable(GTK_TEXT(widget), TRUE);
//
// scroll = gtk_scrolled_window_new(NULL, NULL);
// gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
//                                GTK_POLICY_AUTOMATIC,
//                                GTK_POLICY_AUTOMATIC);
// gtk_container_add(GTK_CONTAINER(scroll), widget);
// gtk_widget_show(widget);
// gtk_widget_show(scroll);
//
// Html_add_input(form, DILLO_HTML_INPUT_TEXTAREA,
//                widget, name, NULL, NULL, FALSE);
// dFree(name);
//
// embed_gtk = a_Dw_embed_gtk_new ();
// a_Dw_embed_gtk_add_gtk (DW_EMBED_GTK (embed_gtk), scroll);
// DW2TB(html->dw)->addWidget (embed_gtk,
//                             S_TOP(html)->style);
}

/*
 * <SELECT>
 */
/* The select tag is quite tricky, because of gorpy html syntax. */
static void Html_tag_open_select(DilloHtml *html, const char *tag, int tagsize)
{
// const char *attrbuf;
// int size, type, multi;

   if (!(html->InFlags & IN_FORM)) {
      MSG_HTML("<select> outside <form>\n");
      return;
   }
   if (html->InFlags & IN_SELECT) {
      MSG_HTML("nested <select>\n");
      return;
   }
   html->InFlags |= IN_SELECT;
   html->InFlags &= ~IN_OPTION;

   DilloHtmlForm *form = html->forms->getRef (html->forms->size() - 1);
   char *name = Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   OptionMenuResource *res =
      HT2LT(html)->getResourceFactory()->createOptionMenuResource ();
   Widget *widget;
   Embed *embed;
   widget = embed = new Embed(res);
   DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);

// size = 0;
// if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
//    size = strtol(attrbuf, NULL, 10);
//
// multi = (Html_get_attr(html, tag, tagsize, "multiple")) ? 1 : 0;
// if (size < 1)
//    size = multi ? 10 : 1;
//
// if (size == 1) {
//    menu = gtk_menu_new();
//    widget = gtk_option_menu_new();
//    type = DILLO_HTML_INPUT_SELECT;
// } else {
//    menu = gtk_list_new();
//    widget = menu;
//    if (multi)
//       gtk_list_set_selection_mode(GTK_LIST(menu), GTK_SELECTION_MULTIPLE);
//    type = DILLO_HTML_INPUT_SEL_LIST;
// }

   DilloHtmlSelect *select = new DilloHtmlSelect;
   select->options = new misc::SimpleVector<DilloHtmlOption *> (4);
   Html_add_input(form, DILLO_HTML_INPUT_SELECT, widget, embed, name,
                  NULL, select, false);
   Html_stash_init(html);
   dFree(name);
}

/*
 * ?
 */
static void Html_option_finish(DilloHtml *html)
{
   DilloHtmlForm *form =
      html->forms->getRef (html->forms->size() - 1);
   DilloHtmlInput *input =
      form->inputs->getRef (form->inputs->size() - 1);
   if (input->type == DILLO_HTML_INPUT_SELECT ||
       input->type == DILLO_HTML_INPUT_SEL_LIST) {
      DilloHtmlSelect *select =
         input->select;
      DilloHtmlOption *option =
         select->options->get (select->options->size() - 1);
      option->content =
         Html_parse_entities(html, html->Stash->str, html->Stash->len);
   }
}

/*
 * <OPTION>
 */
static void Html_tag_open_option(DilloHtml *html, const char *tag, int tagsize)
{
   if (!(html->InFlags & IN_FORM &&
         html->InFlags & IN_SELECT ))
      return;
   if (html->InFlags & IN_OPTION)
      Html_option_finish(html);
   html->InFlags |= IN_OPTION;

   DilloHtmlForm *form =
      html->forms->getRef (html->forms->size() - 1);
   DilloHtmlInput *input =
      form->inputs->getRef (form->inputs->size() - 1);

   if (input->type == DILLO_HTML_INPUT_SELECT ||
       input->type == DILLO_HTML_INPUT_SEL_LIST) {

      DilloHtmlOption *option = new DilloHtmlOption;
      option->value =
         Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
      option->content = NULL;
      option->selected =
         (Html_get_attr(html, tag, tagsize, "selected") != NULL);
      option->enabled =
         (Html_get_attr(html, tag, tagsize, "disabled") == NULL);

      int size = input->select->options->size ();
      input->select->options->increase ();
      input->select->options->set (size, option);
   }

   Html_stash_init(html);
}

/*
 * ?
 */
static void Html_tag_close_select(DilloHtml *html, int TagIdx)
{
   if (html->InFlags & IN_FORM &&
       html->InFlags & IN_SELECT) {
      if (html->InFlags & IN_OPTION)
         Html_option_finish(html);
      html->InFlags &= ~IN_SELECT;
      html->InFlags &= ~IN_OPTION;

      DilloHtmlForm *form =
         html->forms->getRef (html->forms->size() - 1);
      DilloHtmlInput *input =
         form->inputs->getRef (form->inputs->size() - 1);
      SelectionResource *res =
         (SelectionResource*)((Embed*)input->widget)->getResource();

      int size = input->select->options->size ();
      if (size > 0) {
         // is anything selected? 
         bool some_selected = false;
         for (int i = 0; i < size; i++) {
            DilloHtmlOption *option =
               input->select->options->get (i);
            if (option->selected) {
               some_selected = true;
               break;
            }
         }

         // select the first if nothing else is selected 
         // BUG(?): should not do this for MULTI selections 
         if (! some_selected)
            input->select->options->get (0)->selected = true;

         // add the items to the resource 
         for (int i = 0; i < size; i++) {
            DilloHtmlOption *option =
               input->select->options->get (i);
            bool enabled = option->enabled;
            bool selected = option->selected;
            res->addItem(option->content,enabled,selected);
         }
      }
   }

   Html_pop_tag(html, TagIdx);
}

/*
 * Set the Document Base URI
 */
static void Html_tag_open_base(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   DilloUrl *BaseUrl;

   if (html->InFlags & IN_HEAD) {
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
         BaseUrl = Html_url_new(html, attrbuf, "", 0, 0, 0, 1);
         if (URL_SCHEME_(BaseUrl)) {
            /* Pass the URL_SpamSafe flag to the new base url */
            a_Url_set_flags(
               BaseUrl, URL_FLAGS(html->base_url) & URL_SpamSafe);
            a_Url_free(html->base_url);
            html->base_url = BaseUrl;
         } else {
            MSG_HTML("base URI is relative (it MUST be absolute)\n");
            a_Url_free(BaseUrl);
         }
      }
   } else {
      MSG_HTML("the BASE element must appear in the HEAD section\n");
   }
}

/*
 * <CODE>
 */
static void Html_tag_open_code(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, prefs.fw_fontname, 0, 0, 0);
}

/*
 * <DFN>
 */
static void Html_tag_open_dfn(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 2, 3);
}

/*
 * <KBD>
 */
static void Html_tag_open_kbd(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, prefs.fw_fontname, 0, 0, 0);
}

/*
 * <SAMP>
 */
static void Html_tag_open_samp(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, prefs.fw_fontname, 0, 0, 0);
}

/*
 * <VAR>
 */
static void Html_tag_open_var(DilloHtml *html, const char *tag, int tagsize)
{
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <SUB>
 */
static void Html_tag_open_sub(DilloHtml *html, const char *tag, int tagsize)
{
   HTML_SET_TOP_ATTR (html, valign, VALIGN_SUB);
}

/*
 * <SUP>
 */
static void Html_tag_open_sup(DilloHtml *html, const char *tag, int tagsize)
{
   HTML_SET_TOP_ATTR (html, valign, VALIGN_SUPER);
}

/*
 * <DIV> (todo: make a complete implementation)
 */
static void Html_tag_open_div(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);
   Html_tag_set_align_attr (html, tag, tagsize);
}

/*
 * </DIV>, also used for </TABLE> and </CENTER>
 */
static void Html_tag_close_div(DilloHtml *html, int TagIdx)
{
   DW2TB(html->dw)->addParbreak (0, S_TOP(html)->style);
   Html_pop_tag(html, TagIdx);
}

/*
 * Default close for most tags - just pop the stack.
 */
static void Html_tag_close_default(DilloHtml *html, int TagIdx)
{
   Html_pop_tag(html, TagIdx);
}

/*
 * Default close for paragraph tags - pop the stack and break.
 */
static void Html_tag_close_par(DilloHtml *html, int TagIdx)
{
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
   Html_pop_tag(html, TagIdx);
}


/*
 * Function index for the open and close functions for each tag
 * (Alphabetically sorted for a binary search)
 *
 * Explanation for the 'Flags' field:
 *
 *   {"address", B8(010110), ...}
 *                  |||||`- inline element
 *                  ||||`-- block element
 *                  |||`--- inline container
 *                  ||`---- block container
 *                  |`----- body element
 *                  `------ head element
 *
 *   Notes:
 *     - The upper two bits are not used yet.
 *     - Empty elements have both inline and block container clear.
 *       (flow have both set)
 */
struct _TagInfo{
   const char *name;
   unsigned char Flags;
   char EndTag;
   uchar_t TagLevel;
   TagOpenFunct open;
   TagCloseFunct close;
};


const TagInfo Tags[] = {
 {"a", B8(010101),'R',2, Html_tag_open_a, Html_tag_close_a},
 {"abbr", B8(010101),'R',2, Html_tag_open_abbr, Html_tag_close_default},
 /* acronym 010101 */
 {"address", B8(010110),'R',2, Html_tag_open_address, Html_tag_close_par},
 {"area", B8(010001),'F',0, Html_tag_open_area, Html_tag_close_default},
 {"b", B8(010101),'R',2, Html_tag_open_b, Html_tag_close_default},
 {"base", B8(100001),'F',0, Html_tag_open_base, Html_tag_close_default},
 /* basefont 010001 */
 /* bdo 010101 */
 {"big", B8(010101),'R',2, Html_tag_open_big_small, Html_tag_close_default},
 {"blockquote", B8(011110),'R',2,Html_tag_open_blockquote,Html_tag_close_par},
 {"body", B8(011110),'O',1, Html_tag_open_body, Html_tag_close_body},
 {"br", B8(010001),'F',0, Html_tag_open_br, Html_tag_close_default},
 {"button", B8(011101),'R',2, Html_tag_open_button, Html_tag_close_default},
 /* caption */
 {"center", B8(011110),'R',2, Html_tag_open_center, Html_tag_close_div},
 {"cite", B8(010101),'R',2, Html_tag_open_cite, Html_tag_close_default},
 {"code", B8(010101),'R',2, Html_tag_open_code, Html_tag_close_default},
 /* col 010010 'F' */
 /* colgroup */
 {"dd", B8(011110),'O',1, Html_tag_open_dd, Html_tag_close_par},
 {"del", B8(011101),'R',2, Html_tag_open_strike, Html_tag_close_default},
 {"dfn", B8(010101),'R',2, Html_tag_open_dfn, Html_tag_close_default},
 /* dir 011010 */
 /* todo: complete <div> support! */
 {"div", B8(011110),'R',2, Html_tag_open_div, Html_tag_close_div},
 {"dl", B8(011010),'R',2, Html_tag_open_dl, Html_tag_close_par},
 {"dt", B8(010110),'O',1, Html_tag_open_dt, Html_tag_close_par},
 {"em", B8(010101),'R',2, Html_tag_open_em, Html_tag_close_default},
 /* fieldset */
 {"font", B8(010101),'R',2, Html_tag_open_font, Html_tag_close_default},
 {"form", B8(011110),'R',2, Html_tag_open_form, Html_tag_close_form},
 {"frame", B8(010010),'F',0, Html_tag_open_frame, Html_tag_close_default},
 {"frameset", B8(011110),'R',2,Html_tag_open_frameset, Html_tag_close_default},
 {"h1", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"h2", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"h3", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"h4", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"h5", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"h6", B8(010110),'R',2, Html_tag_open_h, Html_tag_close_h},
 {"head", B8(101101),'O',1, Html_tag_open_head, Html_tag_close_head},
 {"hr", B8(010010),'F',0, Html_tag_open_hr, Html_tag_close_default},
 {"html", B8(001110),'O',1, Html_tag_open_html, Html_tag_close_html},
 {"i", B8(010101),'R',2, Html_tag_open_i, Html_tag_close_default},
 {"iframe", B8(011110),'R',2, Html_tag_open_frame, Html_tag_close_default},
 {"img", B8(010001),'F',0, Html_tag_open_img, Html_tag_close_default},
 {"input", B8(010001),'F',0, Html_tag_open_input, Html_tag_close_default},
 /* ins */
 {"isindex", B8(110001),'F',0, Html_tag_open_isindex, Html_tag_close_default},
 {"kbd", B8(010101),'R',2, Html_tag_open_kbd, Html_tag_close_default},
 /* label 010101 */
 /* legend 01?? */
 {"li", B8(011110),'O',1, Html_tag_open_li, Html_tag_close_li},
 /* link 100000 'F' */
 {"map", B8(011001),'R',2, Html_tag_open_map, Html_tag_close_map},
 /* menu 1010 -- todo: not exactly 1010, it can contain LI and inline */
 {"menu", B8(011010),'R',2, Html_tag_open_menu, Html_tag_close_par},
 {"meta", B8(100001),'F',0, Html_tag_open_meta, Html_tag_close_default},
 /* noframes 1011 */
 /* noscript 1011 */
 /* object 11xxxx */
 {"ol", B8(011010),'R',2, Html_tag_open_ol, Html_tag_close_par},
 /* optgroup */
 {"option", B8(010001),'O',1, Html_tag_open_option, Html_tag_close_default},
 {"p", B8(010110),'O',1, Html_tag_open_p, Html_tag_close_par},
 /* param 010001 'F' */
 {"pre", B8(010110),'R',2, Html_tag_open_pre, Html_tag_close_pre},
 /* q 010101 */
 {"s", B8(010101),'R',2, Html_tag_open_strike, Html_tag_close_default},
 {"samp", B8(010101),'R',2, Html_tag_open_samp, Html_tag_close_default},
 {"script", B8(111001),'R',2, Html_tag_open_script, Html_tag_close_script},
 {"select", B8(011001),'R',2, Html_tag_open_select, Html_tag_close_select},
 {"small", B8(010101),'R',2, Html_tag_open_big_small, Html_tag_close_default},
 /* span 0101 */
 {"strike", B8(010101),'R',2, Html_tag_open_strike, Html_tag_close_default},
 {"strong", B8(010101),'R',2, Html_tag_open_strong, Html_tag_close_default},
 {"style", B8(100101),'R',2, Html_tag_open_style, Html_tag_close_style},
 {"sub", B8(010101),'R',2, Html_tag_open_sub, Html_tag_close_default},
 {"sup", B8(010101),'R',2, Html_tag_open_sup, Html_tag_close_default},
 {"table", B8(011010),'R',5, Html_tag_open_table, Html_tag_close_div},
 /* tbody */
 {"td", B8(011110),'O',3, Html_tag_open_td, Html_tag_close_default},
 {"textarea", B8(010101),'R',2,Html_tag_open_textarea,Html_tag_close_textarea},
 /* tfoot */
 {"th", B8(011110),'O',1, Html_tag_open_th, Html_tag_close_default},
 /* thead */
 {"title", B8(100101),'R',2, Html_tag_open_title, Html_tag_close_title},
 {"tr", B8(011010),'O',4, Html_tag_open_tr, Html_tag_close_default},
 {"tt", B8(010101),'R',2, Html_tag_open_tt, Html_tag_close_default},
 {"u", B8(010101),'R',2, Html_tag_open_u, Html_tag_close_default},
 {"ul", B8(011010),'R',2, Html_tag_open_ul, Html_tag_close_par},
 {"var", B8(010101),'R',2, Html_tag_open_var, Html_tag_close_default}

};
#define NTAGS (sizeof(Tags)/sizeof(Tags[0]))


/*
 * Compares tag from buffer ('/' or '>' or space-ended string) [p1]
 * with tag from taglist (lowercase, zero ended string) [p2]
 * Return value: as strcmp()
 */
static int Html_tag_compare(const char *p1, const char *p2)
{
   while ( *p2 ) {
      if (tolower(*p1) != *p2)
         return(tolower(*p1) - *p2);
      ++p1;
      ++p2;
   }
   return !strchr(" >/\n\r\t", *p1);
}

/*
 * Get 'tag' index
 * return -1 if tag is not handled yet
 */
static int Html_tag_index(const char *tag)
{
   int low, high, mid, cond;

   /* Binary search */
   low = 0;
   high = NTAGS - 1;          /* Last tag index */
   while (low <= high) {
      mid = (low + high) / 2;
      if ((cond = Html_tag_compare(tag, Tags[mid].name)) < 0 )
         high = mid - 1;
      else if (cond > 0)
         low = mid + 1;
      else
         return mid;
   }
   return -1;
}

/*
 * For elements with optional close, check whether is time to close.
 * Return value: (1: Close, 0: Don't close)
 * --tuned for speed.
 */
static int Html_needs_optional_close(int old_idx, int cur_idx)
{
   static int i_P = -1, i_LI, i_TD, i_TR, i_TH, i_DD, i_DT, i_OPTION;
               // i_THEAD, i_TFOOT, i_COLGROUP;

   if (i_P == -1) {
    /* initialize the indexes of elements with optional close */
    i_P  = Html_tag_index("p"),
    i_LI = Html_tag_index("li"),
    i_TD = Html_tag_index("td"),
    i_TR = Html_tag_index("tr"),
    i_TH = Html_tag_index("th"),
    i_DD = Html_tag_index("dd"),
    i_DT = Html_tag_index("dt"),
    i_OPTION = Html_tag_index("option");
    // i_THEAD = Html_tag_index("thead");
    // i_TFOOT = Html_tag_index("tfoot");
    // i_COLGROUP = Html_tag_index("colgroup");
   }

   if (old_idx == i_P || old_idx == i_DT) {
      /* P and DT are closed by block elements */
      return (Tags[cur_idx].Flags & 2);
   } else if (old_idx == i_LI) {
      /* LI closes LI */
      return (cur_idx == i_LI);
   } else if (old_idx == i_TD || old_idx == i_TH) {
      /* TD and TH are closed by TD, TH and TR */
      return (cur_idx == i_TD || cur_idx == i_TH || cur_idx == i_TR);
   } else if (old_idx == i_TR) {
      /* TR closes TR */
      return (cur_idx == i_TR);
   } else if (old_idx ==  i_DD) {
      /* DD is closed by DD and DT */
      return (cur_idx == i_DD || cur_idx == i_DT);
   } else if (old_idx ==  i_OPTION) {
      return 1;  // OPTION always needs close
   }

   /* HTML, HEAD, BODY are handled by Html_test_section(), not here. */
   /* todo: TBODY is pending */
   return 0;
}


/*
 * Conditional cleanup of the stack (at open time).
 * - This helps catching block elements inside inline containers (a BUG).
 * - It also closes elements with "optional" close tag.
 *
 * This function is called when opening a block element or <OPTION>.
 *
 * It searches the stack closing open inline containers, and closing
 * elements with optional close tag when necessary.
 *
 * Note: OPTION is the only non-block element with an optional close.
 */
static void Html_stack_cleanup_at_open(DilloHtml *html, int new_idx)
{
   /* We know that the element we're about to push is a block element.
    * (except for OPTION, which is an empty inline, so is closed anyway)
    * Notes:
    *   Its 'tag' is not yet pushed into the stack,
    *   'new_idx' is its index inside Tags[].
    */

   if (!html->TagSoup)
      return;

   while (html->stack->size() > 1) {
      int oldtag_idx = S_TOP(html)->tag_idx;

      if (Tags[oldtag_idx].EndTag == 'O') {    // Element with optional close
         if (!Html_needs_optional_close(oldtag_idx, new_idx))
            break;
      } else if (Tags[oldtag_idx].Flags & 8) { // Block container
         break;
      }

      /* we have an inline (or empty) container... */
      if (Tags[oldtag_idx].EndTag == 'R') {
         MSG_HTML("<%s> is not allowed to contain <%s>. -- closing <%s>\n",
                  Tags[oldtag_idx].name, Tags[new_idx].name,
                  Tags[oldtag_idx].name);
      }

      /* Workaround for Apache and its bad HTML directory listings... */
      if ((html->InFlags & IN_PRE) &&
          strcmp(Tags[new_idx].name, "hr") == 0)
         break;

      /* This call closes the top tag only. */
      Html_tag_cleanup_at_close(html, oldtag_idx);
   }
}

/*
 * HTML, HEAD and BODY elements have optional open and close tags.
 * Handle this "magic" here.
 */
static void Html_test_section(DilloHtml *html, int new_idx, int IsCloseTag)
{
   const char *tag;
   int tag_idx;

   if (!(html->InFlags & IN_HTML) && html->DocType == DT_NONE)
      MSG_HTML("the required DOCTYPE declaration is missing (or invalid)\n");

   if (!(html->InFlags & IN_HTML)) {
      tag = "<html>";
      tag_idx = Html_tag_index(tag + 1);
      if (tag_idx != new_idx || IsCloseTag) {
         /* implicit open */
         Html_force_push_tag(html, tag_idx);
         Tags[tag_idx].open (html, tag, strlen(tag));
      }
   }

   if (Tags[new_idx].Flags & 32) {
      /* head element */
      if (!(html->InFlags & IN_HEAD)) {
         tag = "<head>";
         tag_idx = Html_tag_index(tag + 1);
         if (tag_idx != new_idx || IsCloseTag) {
            /* implicit open of the head element */
            Html_force_push_tag(html, tag_idx);
            Tags[tag_idx].open (html, tag, strlen(tag));
         }
      }

   } else if (Tags[new_idx].Flags & 16) {
      /* body element */
      if (html->InFlags & IN_HEAD) {
         tag = "</head>";
         tag_idx = Html_tag_index(tag + 2);
         Tags[tag_idx].close (html, tag_idx);
      }
      tag = "<body>";
      tag_idx = Html_tag_index(tag + 1);
      if (tag_idx != new_idx || IsCloseTag) {
         /* implicit open */
         Html_force_push_tag(html, tag_idx);
         Tags[tag_idx].open (html, tag, strlen(tag));
      }
   }
}

/*
 * Process a tag, given as 'tag' and 'tagsize'. -- tagsize is [1 based]
 * ('tag' must include the enclosing angle brackets)
 * This function calls the right open or close function for the tag.
 */
static void Html_process_tag(DilloHtml *html, char *tag, int tagsize)
{
   int ci, ni;           /* current and new tag indexes */
   const char *attrbuf;
   char *start = tag + 1; /* discard the '<' */
   int IsCloseTag = (*start == '/');

   ni = Html_tag_index(start + IsCloseTag);

   /* todo: doctype parsing is a bit fuzzy, but enough for the time being */
   if (ni == -1 && !(html->InFlags & IN_HTML)) {
      if (tagsize > 9 && !dStrncasecmp(tag, "<!doctype", 9))
         Html_parse_doctype(html, tag, tagsize);
   }

   if (!(html->InFlags & IN_HTML)) {
      _MSG("\nDoctype: %f\n\n", html->DocTypeVersion);
   }

   /* Handle HTML, HEAD and BODY. Elements with optional open and close */
   if (ni != -1 && !(html->InFlags & IN_BODY) /* && parsing HTML */)
      Html_test_section(html, ni, IsCloseTag);

   /* White space handling */
   if (html->SPCPending) {
      if (SGML_SPCDEL) {
         /* SGML_SPCDEL requires space pending and open tag */
         if (!IsCloseTag)
            DW2TB(html->dw)->addSpace(S_TOP(html)->style);
         html->SPCPending = FALSE;
      } else {
         /* custom space handling: preserve pending space past close tags */
         if (!IsCloseTag) {
            DW2TB(html->dw)->addSpace(S_TOP(html)->style);
            html->SPCPending = FALSE;
         }
      }
   }

   /* Tag processing */
   ci = S_TOP(html)->tag_idx;
   if (ni != -1) {

      if (!IsCloseTag) {
         /* Open function */

         /* Cleanup when opening a block element, or
          * when openning over an element with optional close */
         if (Tags[ni].Flags & 2 || (ci != -1 && Tags[ci].EndTag == 'O'))
            Html_stack_cleanup_at_open(html, ni);

         /* todo: this is only raising a warning, take some defined action.
          * Note: apache uses IMG inside PRE (we could use its "alt"). */
         if ((html->InFlags & IN_PRE) && Html_tag_pre_excludes(ni))
            MSG_HTML("<pre> is not allowed to contain <%s>\n", Tags[ni].name);

         /* Push the tag into the stack */
         Html_push_tag(html, ni);

         /* Call the open function for this tag */
         Tags[ni].open (html, tag, tagsize);

         /* Now parse attributes that can appear on any tag */
         if (tagsize >= 8 &&        /* length of "<t id=i>" */
             (attrbuf = Html_get_attr2(html, tag, tagsize, "id",
                                       HTML_LeftTrim | HTML_RightTrim))) {
            /* According to the SGML declaration of HTML 4, all NAME values
             * occuring outside entities must be converted to uppercase
             * (this is what "NAMECASE GENERAL YES" says). But the HTML 4
             * spec states in Sec. 7.5.2 that anchor ids are case-sensitive.
             * So we don't do it and hope for better specs in the future ...
             */
            Html_check_name_val(html, attrbuf, "id");
            /* We compare the "id" value with the url-decoded "name" value */
            if (!html->NameVal || strcmp(html->NameVal, attrbuf)) {
               if (html->NameVal)
                  MSG_HTML("'id' and 'name' attribute of <a> tag differ\n");
               Html_add_anchor(html, attrbuf);
            }
         }

         /* Reset NameVal */
         if (html->NameVal) {
            dFree(html->NameVal);
            html->NameVal = NULL;
         }

         /* let the parser know this was an open tag */
         html->PrevWasOpenTag = TRUE;

         /* Request inmediate close for elements with forbidden close tag. */
         /* todo: XHTML always requires close tags. A simple implementation
          * of the commented clause below will make it work. */
         if  (/* parsing HTML && */ Tags[ni].EndTag == 'F')
            html->ReqTagClose = TRUE;
      }

      /* Close function: test for </x>, ReqTagClose, <x /> and <x/> */
      if (*start == '/' ||                                      /* </x>    */
          html->ReqTagClose ||                                  /* request */
          (tag[tagsize - 2] == '/' &&                           /* XML:    */
           (isspace(tag[tagsize - 3]) ||                        /*  <x />  */
            (size_t)tagsize == strlen(Tags[ni].name) + 3))) {   /*  <x/>   */

         Tags[ni].close (html, ni);
         /* This was a close tag */
         html->PrevWasOpenTag = FALSE;
         html->ReqTagClose = FALSE;
      }

   } else {
      /* tag not working - just ignore it */
   }
}

/*
 * Get attribute value for 'attrname' and return it.
 *  Tags start with '<' and end with a '>' (Ex: "<P align=center>")
 *  tagsize = strlen(tag) from '<' to '>', inclusive.
 *
 * Returns one of the following:
 *    * The value of the attribute.
 *    * An empty string if the attribute exists but has no value.
 *    * NULL if the attribute doesn't exist.
 */
static const char *Html_get_attr2(DilloHtml *html,
                                  const char *tag,
                                  int tagsize,
                                  const char *attrname,
                                  int tag_parsing_flags)
{
   int i, isocode, entsize, Found = 0, delimiter = 0, attr_pos = 0;
   Dstr *Buf = html->attr_data;
   DilloHtmlTagParsingState state = SEEK_ATTR_START;

   dReturn_val_if_fail(*attrname, NULL);

   dStr_truncate(Buf, 0);

   for (i = 1; i < tagsize; ++i) {
      switch (state) {
      case SEEK_ATTR_START:
         if (isspace(tag[i]))
            state = SEEK_TOKEN_START;
         else if (tag[i] == '=')
            state = SEEK_VALUE_START;
         break;

      case MATCH_ATTR_NAME:
         if ((Found = (!(attrname[attr_pos]) &&
                       (tag[i] == '=' || isspace(tag[i]) || tag[i] == '>')))) {
            state = SEEK_TOKEN_START;
            --i;
         } else if (tolower(tag[i]) != tolower(attrname[attr_pos++]))
            state = SEEK_ATTR_START;
         break;

      case SEEK_TOKEN_START:
         if (tag[i] == '=') {
            state = SEEK_VALUE_START;
         } else if (!isspace(tag[i])) {
            attr_pos = 0;
            state = (Found) ? FINISHED : MATCH_ATTR_NAME;
            --i;
         }
         break;
      case SEEK_VALUE_START:
         if (!isspace(tag[i])) {
            delimiter = (tag[i] == '"' || tag[i] == '\'') ? tag[i] : ' ';
            i -= (delimiter == ' ');
            state = (Found) ? GET_VALUE : SKIP_VALUE;
         }
         break;

      case SKIP_VALUE:
         if ((delimiter == ' ' && isspace(tag[i])) || tag[i] == delimiter)
            state = SEEK_TOKEN_START;
         break;
      case GET_VALUE:
         if ((delimiter == ' ' && (isspace(tag[i]) || tag[i] == '>')) ||
             tag[i] == delimiter) {
            state = FINISHED;
         } else if (tag[i] == '&' &&
                    (tag_parsing_flags & HTML_ParseEntities)) {
            if ((isocode = Html_parse_entity(html, tag+i,
                                             tagsize-i, &entsize)) >= 0) {
               if (isocode >= 128) {
                  char buf[4];
                  int k, n = utf8encode(isocode, buf);
                  for (k = 0; k < n; ++k)
                     dStr_append_c(Buf, buf[k]);
               } else {
                  dStr_append_c(Buf, (char) isocode);
               }
               i += entsize-1;
            } else {
               dStr_append_c(Buf, tag[i]);
            }
         } else if (tag[i] == '\r' || tag[i] == '\t') {
            dStr_append_c(Buf, ' ');
         } else if (tag[i] == '\n') {
            /* ignore */
         } else {
            dStr_append_c(Buf, tag[i]);
         }
         break;

      case FINISHED:
         i = tagsize;
         break;
      }
   }

   if (tag_parsing_flags & HTML_LeftTrim)
      while (isspace(Buf->str[0]))
         dStr_erase(Buf, 0, 1);
   if (tag_parsing_flags & HTML_RightTrim)
      while (Buf->len && isspace(Buf->str[Buf->len - 1]))
         dStr_truncate(Buf, Buf->len - 1);

   return (Found) ? Buf->str : NULL;
}

/*
 * Call Html_get_attr2 telling it to parse entities and strip the result
 */
static const char *Html_get_attr(DilloHtml *html,
                                 const char *tag,
                                 int tagsize,
                                 const char *attrname)
{
   return Html_get_attr2(html, tag, tagsize, attrname,
                         HTML_LeftTrim | HTML_RightTrim | HTML_ParseEntities);
}

/*
 * "Html_get_attr with default"
 * Call Html_get_attr() and strdup() the returned string.
 * If the attribute isn't found a copy of 'def' is returned.
 */
static char *Html_get_attr_wdef(DilloHtml *html,
                               const char *tag,
                               int tagsize,
                               const char *attrname,
                               const char *def)
{
   const char *attrbuf = Html_get_attr(html, tag, tagsize, attrname);

   return attrbuf ? dStrdup(attrbuf) : dStrdup(def);
}

/*
 * Add a widget to the page.
 */
static void Html_add_widget(DilloHtml *html,
                            Widget *widget,
                            char *width_str,
                            char *height_str,
                            StyleAttrs *style_attrs)
{
   StyleAttrs new_style_attrs;
   Style *style;

   new_style_attrs = *style_attrs;
   new_style_attrs.width = width_str ?
      Html_parse_length (html, width_str) : LENGTH_AUTO;
   new_style_attrs.height = height_str ?
      Html_parse_length (html, height_str) : LENGTH_AUTO;
   style = Style::create (HT2LT(html), &new_style_attrs);
   DW2TB(html->dw)->addWidget (widget, style);
   style->unref ();
}


/*
 * Dispatch the apropriate function for 'Op'
 * This function is a Cache client and gets called whenever new data arrives
 *  Op      : operation to perform.
 *  CbData  : a pointer to a DilloHtml structure
 *  Buf     : a pointer to new data
 *  BufSize : new data size (in bytes)
 */
static void Html_callback(int Op, CacheClient_t *Client)
{
   DilloHtml *html = (DilloHtml*)Client->CbData;

   if (Op) { /* EOF */
      html->write((char*)Client->Buf, Client->BufSize, 1);
      html->closeParser(Client->Key);
   } else {
      html->write((char*)Client->Buf, Client->BufSize, 0);
   }
}

/*
 * Here's where we parse the html and put it into the Textblock structure.
 * Return value: number of bytes parsed
 */
static int Html_write_raw(DilloHtml *html, char *buf, int bufsize, int Eof)
{
   char ch = 0, *p, *text;
   Textblock *textblock;
   int token_start, buf_index;

   dReturn_val_if_fail ((textblock = DW2TB(html->dw)) != NULL, 0);

   /* Now, 'buf' and 'bufsize' define a buffer aligned to start at a token
    * boundary. Iterate through tokens until end of buffer is reached. */
   buf_index = 0;
   token_start = buf_index;
   while (buf_index < bufsize) {
      /* invariant: buf_index == bufsize || token_start == buf_index */

      if (S_TOP(html)->parse_mode ==
          DILLO_HTML_PARSE_MODE_VERBATIM) {
         /* Non HTML code here, let's skip until closing tag */
         do {
            char *tag = S_TOP(html)->tag_name;
            buf_index += strcspn(buf + buf_index, "<");
            if (buf_index + (int)strlen(tag) + 3 > bufsize) {
               buf_index = bufsize;
            } else if (strncmp(buf + buf_index, "</", 2) == 0 &&
                       Html_match_tag(tag, buf+buf_index+2, strlen(tag)+1)) {
               /* copy VERBATIM text into the stash buffer */
               text = dStrndup(buf + token_start, buf_index - token_start);
               dStr_append(html->Stash, text);
               dFree(text);
               token_start = buf_index;
               break;
            } else
               ++buf_index;
         } while (buf_index < bufsize);
      }

      if (isspace(buf[buf_index])) {
         /* whitespace: group all available whitespace */
         while (++buf_index < bufsize && isspace(buf[buf_index]));
         Html_process_space(html, buf + token_start, buf_index - token_start);
         token_start = buf_index;

      } else if (buf[buf_index] == '<' && (ch = buf[buf_index + 1]) &&
                 (isalpha(ch) || strchr("/!?", ch)) ) {
         /* Tag */
         if (buf_index + 3 < bufsize && !strncmp(buf + buf_index, "<!--", 4)) {
            /* Comment: search for close of comment, skipping over
             * everything except a matching "-->" tag. */
            while ( (p = (char*) memchr(buf + buf_index, '>',
                                        bufsize - buf_index)) ){
               buf_index = p - buf + 1;
               if (p[-1] == '-' && p[-2] == '-') break;
            }
            if (p) {
               /* Got the whole comment. Let's throw it away! :) */
               token_start = buf_index;
            } else
               buf_index = bufsize;
         } else {
            /* Tag: search end of tag (skipping over quoted strings) */
            html->CurrTagOfs = html->Start_Ofs + token_start;

            while ( buf_index < bufsize ) {
               buf_index++;
               buf_index += strcspn(buf + buf_index, ">\"'<");
               if ((ch = buf[buf_index]) == '>') {
                  break;
               } else if (ch == '"' || ch == '\'') {
                  /* Skip over quoted string */
                  buf_index++;
                  buf_index += strcspn(buf + buf_index,
                                       (ch == '"') ? "\">" : "'>");
                  if (buf[buf_index] == '>') {
                     /* Unterminated string value? Let's look ahead and test:
                      * (<: unterminated, closing-quote: terminated) */
                     int offset = buf_index + 1;
                     offset += strcspn(buf + offset,
                                       (ch == '"') ? "\"<" : "'<");
                     if (buf[offset] == ch || !buf[offset]) {
                        buf_index = offset;
                     } else {
                        MSG_HTML("attribute lacks closing quote\n");
                        break;
                     }
                  }
               } else if (ch == '<') {
                  /* unterminated tag detected */
                  p = dStrndup(buf+token_start+1,
                               strcspn(buf+token_start+1, " <"));
                  MSG_HTML("<%s> element lacks its closing '>'\n", p);
                  dFree(p);
                  --buf_index;
                  break;
               }
            }
            if (buf_index < bufsize) {
               buf_index++;
               Html_process_tag(html, buf + token_start,
                                buf_index - token_start);
               token_start = buf_index;
            }
         }
      } else {
         /* A Word: search for whitespace or tag open */
         while (++buf_index < bufsize) {
            buf_index += strcspn(buf + buf_index, " <\n\r\t\f\v");
            if (buf[buf_index] == '<' && (ch = buf[buf_index + 1]) &&
                !isalpha(ch) && !strchr("/!?", ch))
               continue;
            break;
         }
         if (buf_index < bufsize || Eof) {
            /* successfully found end of token */
            Html_process_word(html, buf + token_start,
                              buf_index - token_start);
            token_start = buf_index;
         }
      }
   }/*while*/

   textblock->flush ();

   return token_start;
}


