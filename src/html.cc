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
#include <ctype.h>      /* for isspace */
#include <string.h>     /* for memcpy and memmove */
#include <stdlib.h>
#include <stdio.h>      /* for sprintf */
#include <errno.h>

#include "bw.h"         /* for BrowserWindow */
#include "msg.h"
#include "binaryconst.h"
#include "colors.h"
#include "utf8.hh"

#include "misc.h"
#include "uicmd.hh"
#include "history.h"
#include "menu.hh"
#include "prefs.h"
#include "capi.h"
#include "html.hh"
#include "html_common.hh"
#include "form.hh"
#include "table.hh"

#include "dw/textblock.hh"
#include "dw/bullet.hh"
#include "dw/listitem.hh"
#include "dw/image.hh"
#include "dw/ruler.hh"

/*-----------------------------------------------------------------------------
 * Defines
 *---------------------------------------------------------------------------*/

/* Define to 1 to ignore white space immediately after an open tag,
 * and immediately before a close tag. */
#define SGML_SPCDEL 0

#define TAB_SIZE 8

/*-----------------------------------------------------------------------------
 * Name spaces
 *---------------------------------------------------------------------------*/
using namespace lout;
using namespace dw;
using namespace dw::core;
using namespace dw::core::ui;
using namespace dw::core::style;

/*-----------------------------------------------------------------------------
 * Typedefs
 *---------------------------------------------------------------------------*/
class DilloHtml;
typedef void (*TagOpenFunct) (DilloHtml *html, const char *tag, int tagsize);
typedef void (*TagCloseFunct) (DilloHtml *html);

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


/*
 * Exported function with C linkage.
 */
extern "C" {
void *a_Html_text(const char *type, void *P, CA_Callback_t *Call,void **Data);
}

/*-----------------------------------------------------------------------------
 * Forward declarations
 *---------------------------------------------------------------------------*/
static int Html_write_raw(DilloHtml *html, char *buf, int bufsize, int Eof);
static bool Html_load_image(BrowserWindow *bw, DilloUrl *url,
                            const DilloUrl *requester, DilloImage *image);
static void Html_callback(int Op, CacheClient_t *Client);
static void Html_tag_cleanup_at_close(DilloHtml *html, int TagIdx);

/*-----------------------------------------------------------------------------
 * Local Data
 *---------------------------------------------------------------------------*/
/* Parsing table structure */
typedef struct {
   const char *name;      /* element name */
   unsigned char Flags;   /* flags (explained near the table data) */
   char EndTag;           /* Is it Required, Optional or Forbidden */
   uchar_t TagLevel;      /* Used to heuristically parse bad HTML  */
   TagOpenFunct open;     /* Open function */
   TagOpenFunct content;  /* Content function */
   TagCloseFunct close;   /* Close function */
} TagInfo;
extern const TagInfo Tags[];

/*-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 * Main Code
 *-----------------------------------------------------------------------------
 *---------------------------------------------------------------------------*/

/*
 * Collect HTML error strings.
 */
void DilloHtml::bugMessage(const char *format, ... )
{
   va_list argp;

   if (bw->num_page_bugs)
      dStr_append_c(bw->page_bugs, '\n');
   dStr_sprintfa(bw->page_bugs,
                 "HTML warning: line %d, ",
                 getCurrLineNumber());
   va_start(argp, format);
   dStr_vsprintfa(bw->page_bugs, format, argp);
   va_end(argp);
   a_UIcmd_set_bug_prog(bw, ++bw->num_page_bugs);
}

/*
 * Wrapper for a_Url_new that adds an error detection message.
 * If use_base_url is TRUE, it uses base_url. Otherwise it uses html->base_url.
 */
DilloUrl *a_Html_url_new(DilloHtml *html,
                         const char *url_str, const char *base_url,
                         int use_base_url)
{
   DilloUrl *url;
   int n_ic, n_ic_spc;

   url = a_Url_new(url_str,
                   (use_base_url) ? base_url : URL_STR_(html->base_url));
   if ((n_ic = URL_ILLEGAL_CHARS(url)) != 0) {
      const char *suffix = (n_ic) > 1 ? "s" : "";
      n_ic_spc = URL_ILLEGAL_CHARS_SPC(url);
      if (n_ic == n_ic_spc) {
         BUG_MSG("URL has %d illegal space%s ('%s').", n_ic, suffix, url_str);
      } else if (n_ic_spc == 0) {
         BUG_MSG("URL has %d illegal byte%s in {00-1F, 7F-FF} range ('%s').",
                 n_ic, suffix, url_str);
      } else {
         BUG_MSG("URL has %d illegal byte%s: "
                 "%d space%s and %d in {00-1F, 7F-FF} range ('%s').",
                 n_ic, suffix,
                 n_ic_spc, n_ic_spc > 1 ? "s" : "", n_ic-n_ic_spc, url_str);
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
   DilloHtml *html = new DilloHtml(web->bw, web->url, Type);

   *Data = (void*)html;
   *Call = (CA_Callback_t)Html_callback;

   return (void*)html->dw;
}

static void Html_free(void *data)
{
   delete ((DilloHtml*)data);
}

/*
 * Used by the "Load images" page menuitem.
 */
void a_Html_load_images(void *v_html, DilloUrl *pattern)
{
   DilloHtml *html = (DilloHtml*)v_html;

   html->loadImages(pattern);
}

/*
 * Search for form
 */
static bool Html_contains_form(DilloHtml *html, void *v_form)
{
   for (int i = 0; i < html->forms->size(); i++) {
      if (html->forms->get(i) == v_form) {
         return true;
      }
   }
   return false;
}

/*
 * Used by the "Submit form" form menuitem.
 */
void a_Html_form_submit(void *v_html, void *v_form)
{
   DilloHtml *html = (DilloHtml*)v_html;

   if (Html_contains_form(html, v_form)) {
      /* it's still valid */
     a_Html_form_submit2(v_form);
   }
}

/*
 * Used by the "Reset form" form menuitem.
 */
void a_Html_form_reset(void *v_html, void *v_form)
{
   DilloHtml *html = (DilloHtml*)v_html;

   if (Html_contains_form(html, v_form)) {
      /* it's still valid */
     a_Html_form_reset2(v_form);
   }
}

/*
 * Used by the "Show/Hide hiddens" form menuitem.
 */
void a_Html_form_display_hiddens(void *v_html, void *v_form, bool_t display)
{
   DilloHtml *html = (DilloHtml*)v_html;

   if (Html_contains_form(html, v_form)) {
      /* it's still valid */
      a_Html_form_display_hiddens2(v_form, (display != 0));
   }
}

/*
 * Set the URL data for image maps.
 */
static void Html_set_link_coordinates(DilloHtml *html, int link, int x, int y)
{
   char data[64];

   if (x != -1) {
      snprintf(data, 64, "?%d,%d", x, y);
      a_Url_set_ismap_coords(html->links->get(link), data);
   }
}

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
 * Evaluates the ALIGN attribute (left|center|right|justify) and
 * sets the style at the top of the stack.
 */
void a_Html_tag_set_align_attr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *align;

   if ((align = a_Html_get_attr(html, tag, tagsize, "align"))) {
      TextAlignType textAlignType = TEXT_ALIGN_LEFT;

      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("The align attribute is obsolete in HTML5.");

      if (dStrAsciiCasecmp (align, "left") == 0)
         textAlignType = TEXT_ALIGN_LEFT;
      else if (dStrAsciiCasecmp (align, "right") == 0)
         textAlignType = TEXT_ALIGN_RIGHT;
      else if (dStrAsciiCasecmp (align, "center") == 0)
         textAlignType = TEXT_ALIGN_CENTER;
      else if (dStrAsciiCasecmp (align, "justify") == 0)
         textAlignType = TEXT_ALIGN_JUSTIFY;
#if 0
      else if (dStrAsciiCasecmp (align, "char") == 0) {
         /* TODO: Actually not supported for <p> etc. */
         v.textAlign = TEXT_ALIGN_STRING;
         if ((charattr = a_Html_get_attr(html, tag, tagsize, "char"))) {
            if (charattr[0] == 0)
               /* TODO: ALIGN=" ", and even ALIGN="&32;" will reult in
                * an empty string (don't know whether the latter is
                * correct, has to be clarified with the specs), so
                * that for empty strings, " " is assumed. */
               style_attrs.textAlignChar = ' ';
            else
               style_attrs.textAlignChar = charattr[0];
         } else
            /* TODO: Examine LANG attr of <html>. */
            style_attrs.textAlignChar = '.';
      }
#endif
      html->styleEngine->setNonCssHint(CSS_PROPERTY_TEXT_ALIGN, CSS_TYPE_ENUM,
                                       textAlignType);
   }
}

/*
 * Evaluates the VALIGN attribute (top|bottom|middle|baseline) and
 * sets the style in style_attrs. Returns true when set.
 */
bool a_Html_tag_set_valign_attr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attr;
   VAlignType valign;

   if ((attr = a_Html_get_attr(html, tag, tagsize, "valign"))) {
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("The valign attribute is obsolete in HTML5.");

      if (dStrAsciiCasecmp (attr, "top") == 0)
         valign = VALIGN_TOP;
      else if (dStrAsciiCasecmp (attr, "bottom") == 0)
         valign = VALIGN_BOTTOM;
      else if (dStrAsciiCasecmp (attr, "baseline") == 0)
         valign = VALIGN_BASELINE;
      else
         valign = VALIGN_MIDDLE;

      html->styleEngine->setNonCssHint (CSS_PROPERTY_VERTICAL_ALIGN,
                                        CSS_TYPE_ENUM, valign);
      return true;
   } else
      return false;
}


/*
 * Create and add a new Textblock to the current Textblock
 */
static void Html_add_textblock(DilloHtml *html, int space)
{
   Textblock *textblock = new Textblock (prefs.limit_text_width);

   HT2TB(html)->addParbreak (space, html->wordStyle ());
   HT2TB(html)->addWidget (textblock, html->style ());
   HT2TB(html)->addParbreak (space, html->wordStyle ());
   S_TOP(html)->textblock = html->dw = textblock;
   S_TOP(html)->hand_over_break = true;
}

/*
 * Create and initialize a new DilloHtml class
 */
DilloHtml::DilloHtml(BrowserWindow *p_bw, const DilloUrl *url,
                     const char *content_type)
{
   /* Init main variables */
   bw = p_bw;
   page_url = a_Url_dup(url);
   base_url = a_Url_dup(url);
   dw = NULL;

   /* Init event receiver */
   linkReceiver.html = this;
   HT2LT(this)->connectLink (&linkReceiver);

   a_Bw_add_doc(p_bw, this);

   /* Init for-parsing variables */
   Start_Buf = NULL;
   Start_Ofs = 0;

   _MSG("DilloHtml(): content type: %s\n", content_type);
   this->content_type = dStrdup(content_type);

   /* get charset */
   a_Misc_parse_content_type(content_type, NULL, NULL, &charset);

   stop_parser = false;

   CurrOfs = OldOfs = 0;
   OldLine = 1;

   DocType = DT_NONE;    /* assume Tag Soup 0.0!   :-) */
   DocTypeVersion = 0.0f;

   styleEngine = new StyleEngine (HT2LT (this), page_url, base_url);

   cssUrls = new misc::SimpleVector <DilloUrl*> (1);

   stack = new misc::SimpleVector <DilloHtmlState> (16);
   stack->increase();
   stack->getRef(0)->parse_mode = DILLO_HTML_PARSE_MODE_INIT;
   stack->getRef(0)->table_mode = DILLO_HTML_TABLE_MODE_NONE;
   stack->getRef(0)->table_border_mode = DILLO_HTML_TABLE_BORDER_SEPARATE;
   stack->getRef(0)->cell_text_align_set = false;
   stack->getRef(0)->display_none = false;
   stack->getRef(0)->list_type = HTML_LIST_NONE;
   stack->getRef(0)->list_number = 0;
   stack->getRef(0)->tag_idx = -1;               /* MUST not be used */
   stack->getRef(0)->textblock = NULL;
   stack->getRef(0)->table = NULL;
   stack->getRef(0)->ref_list_item = NULL;
   stack->getRef(0)->hand_over_break = false;

   InFlags = IN_NONE;

   Stash = dStr_new("");
   StashSpace = false;

   pre_column = 0;
   PreFirstChar = false;
   PrevWasCR = false;
   InVisitedLink = false;
   ReqTagClose = false;
   TagSoup = true;
   loadCssFromStash = false;

   Num_HTML = Num_HEAD = Num_BODY = Num_TITLE = 0;

   attr_data = dStr_sized_new(1024);

   non_css_link_color = -1;
   non_css_visited_color = -1;
   visited_color = -1;

   /* Init page-handling variables */
   forms = new misc::SimpleVector <DilloHtmlForm*> (1);
   inputs_outside_form = new misc::SimpleVector <DilloHtmlInput*> (1);
   links = new misc::SimpleVector <DilloUrl*> (64);
   images = new misc::SimpleVector <DilloHtmlImage*> (16);

   /* Initialize the main widget */
   initDw();
   /* Hook destructor to the dw delete call */
   dw->setDeleteCallback(Html_free, this);
}

/*
 * Miscellaneous initializations for Dw
 */
void DilloHtml::initDw()
{
   dReturn_if_fail (dw == NULL);

   /* Create the main widget */
   dw = stack->getRef(0)->textblock =  new Textblock (prefs.limit_text_width);

   bw->num_page_bugs = 0;
   dStr_truncate(bw->page_bugs, 0);
}

/*
 * Free memory used by the DilloHtml class.
 */
DilloHtml::~DilloHtml()
{
   _MSG("::~DilloHtml(this=%p)\n", this);

   freeParseData();

   a_Bw_remove_doc(bw, this);

   a_Url_free(page_url);
   a_Url_free(base_url);

   for (int i = 0; i < cssUrls->size(); i++)
      a_Url_free(cssUrls->get(i));
   delete (cssUrls);

   for (int i = 0; i < forms->size(); i++)
      a_Html_form_delete (forms->get(i));
   delete(forms);

   for (int i = 0; i < inputs_outside_form->size(); i++)
      a_Html_input_delete(inputs_outside_form->get(i));
   delete(inputs_outside_form);

   for (int i = 0; i < links->size(); i++)
      a_Url_free(links->get(i));
   delete (links);

   for (int i = 0; i < images->size(); i++) {
      DilloHtmlImage *img = images->get(i);
      a_Url_free(img->url);
      a_Image_unref(img->image);
      dFree(img);
   }
   delete (images);

   delete styleEngine;
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

   _MSG("DilloHtml::write BufSize=%d Start_Ofs=%d\n", BufSize, Start_Ofs);
#if 0
   char *aux = dStrndup(Buf, BufSize);
   MSG(" {%s}\n", aux);
   dFree(aux);
#endif

   /* Update Start_Buf. It may be used after the parser is stopped */
   Start_Buf = Buf;

   dReturn_if (dw == NULL);
   dReturn_if (stop_parser == true);

   token_start = Html_write_raw(this, buf, bufsize, Eof);
   Start_Ofs += token_start;
}

/*
 * Return the line number of the tag/word being processed by the parser.
 * Also update the offsets.
 */
int DilloHtml::getCurrLineNumber()
{
   int i, ofs, line;
   const char *p = Start_Buf;

   dReturn_val_if_fail(p != NULL, -1);
   /* Disable line counting for META hack. Buffers differ. */
   dReturn_val_if((InFlags & IN_META_HACK), -1);

   ofs = CurrOfs;
   line = OldLine;
   for (i = OldOfs; i < ofs; ++i)
      if (p[i] == '\n' || (p[i] == '\r' && p[i+1] != '\n'))
         ++line;
   OldOfs = CurrOfs;
   OldLine = line;
   return line;
}

/*
 * Free parsing data.
 */
void DilloHtml::freeParseData()
{
   delete(stack);

   dStr_free(Stash, TRUE);
   dStr_free(attr_data, TRUE);
   dFree(content_type);
   dFree(charset);
}

/*
 * Finish parsing a HTML page. Close the parser and close the client.
 * The class is not deleted here, it remains until the widget is destroyed.
 */
void DilloHtml::finishParsing(int ClientKey)
{
   int si;

   dReturn_if (stop_parser == true);

   /* flag we've already parsed up to the last byte */
   InFlags |= IN_EOF;

   /* force the close of elements left open (TODO: not for XHTML) */
   while ((si = stack->size() - 1)) {
      if (stack->getRef(si)->tag_idx != -1) {
         Html_tag_cleanup_at_close(this, stack->getRef(si)->tag_idx);
      }
   }

   /* Nothing left to do with the parser. Clear all flags, except EOF. */
   InFlags = IN_EOF;

   /* Remove this client from our active list */
   a_Bw_close_client(bw, ClientKey);
}

/*
 * Allocate and insert form information.
 */
int DilloHtml::formNew(DilloHtmlMethod method, const DilloUrl *action,
                       DilloHtmlEnc enc, const char *charset)
{
   // avoid data loss on repush after CSS stylesheets have been loaded
   bool enabled = bw->NumPendingStyleSheets == 0;
   DilloHtmlForm *form = a_Html_form_new (this, method, action,
                                          enc, charset, enabled);
   int nf = forms->size ();
   forms->increase ();
   forms->set (nf, form);
   _MSG("Html formNew: action=%s nform=%d\n", action, nf);
   return forms->size();
}

/*
 * Get the current form.
 */
DilloHtmlForm *DilloHtml::getCurrentForm ()
{
   return forms->get (forms->size() - 1);
}

bool_t DilloHtml::unloadedImages()
{
   for (int i = 0; i < images->size(); i++) {
      if (images->get(i)->image != NULL) {
         return TRUE;
      }
   }
   return FALSE;
}

/*
 * Load images if they were disabled.
 */
void DilloHtml::loadImages (const DilloUrl *pattern)
{
   dReturn_if (a_Bw_expecting(bw));

   /* If the user asked for a specific image, the user (NULL) is the requester,
    * and the domain mechanism will always permit the request. But if the user
    * just asked for all images (clicking "Load images"), use the page URL as
    * the requester so that the domain mechanism can act as a filter.
    * If the possible patterns become more complex, it might be good to have
    * the caller supply the requester instead.
    */
   const DilloUrl *requester = pattern ? NULL : this->page_url;

   for (int i = 0; i < images->size(); i++) {
      DilloHtmlImage *hi = images->get(i);

      if (hi->image) {
         assert(hi->url);
         if ((!pattern) || (!a_Url_cmp(hi->url, pattern))) {
            if (Html_load_image(bw, hi->url, requester, hi->image)) {
               a_Image_unref (hi->image);
               hi->image = NULL;  // web owns it now
            }
         }
      }
   }
}

/*
 * Save URL in a vector (may be loaded later).
 */
void DilloHtml::addCssUrl(const DilloUrl *url)
{
   int nu = cssUrls->size();
   cssUrls->increase();
   cssUrls->set(nu, a_Url_dup(url));
}

bool DilloHtml::HtmlLinkReceiver::enter (Widget *widget, int link, int img,
                                         int x, int y)
{
   BrowserWindow *bw = html->bw;

   _MSG(" ** ");
   if (link == -1) {
      _MSG(" Link  LEAVE  notify...\n");
      a_UIcmd_set_msg(bw, "");
   } else {
      _MSG(" Link  ENTER  notify...\n");
      Html_set_link_coordinates(html, link, x, y);
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
         const bool_t loaded_img = (html->images->get(img)->image == NULL);
         a_UIcmd_image_popup(bw, html->images->get(img)->url, loaded_img,
                             html->page_url, linkurl);
         ret = true;
      } else {
         if (link == -1) {
            a_UIcmd_page_popup(bw, bw->num_page_bugs != 0, html->cssUrls);
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
      if (event->button == 1){
         // load all instances of this image
         DilloUrl *pattern = html->images->get(img)->url;
         html->loadImages(pattern);
         return true;
      }
   }

   if (link != -1) {
      DilloUrl *url = html->links->get(link);
      _MSG("clicked on URL %d: %s\n", link, a_Url_str (url));

      Html_set_link_coordinates(html, link, x, y);

      if (event->button == 1) {
         a_UIcmd_open_url(bw, url);
      } else if (event->button == 2) {
         if (prefs.middle_click_opens_new_tab) {
            int focus = prefs.focus_new_tab ? 1 : 0;
            if (event->state == SHIFT_MASK) focus = !focus;
            a_UIcmd_open_url_nt(bw, url, focus);
         } else
            a_UIcmd_open_url_nw(bw, url);
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
void a_Html_stash_init(DilloHtml *html)
{
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_STASH;
   html->StashSpace = false;
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
   {"nbsp",0240},  {"ndash",020023},{"ne",021140},   {"ni",021013},
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
         BUG_MSG("Numeric character reference \"%s\" out of range.", tok);
         isocode = -2;
      }

      if (isocode != -1) {
         if (*s == ';')
            s++;
         else if (prefs.show_extra_warnings)
            BUG_MSG("Numeric character reference without trailing ';'.");
      }

   } else if (isalpha(*s)) {
      /* character entity reference */
      while (*++s && (isalnum(*s) || strchr(":_.-", *s))) ;
      c = *s;
      *s = 0;

      if ((i = Html_entity_search(tok)) >= 0) {
         isocode = Entities[i].isocode;
      } else {
         if (html->DocType == DT_XHTML && !strcmp(tok, "apos")) {
            isocode = 0x27;
         } else {
            if ((html->DocType == DT_HTML && html->DocTypeVersion == 4.01f) ||
                html->DocType == DT_XHTML)
               BUG_MSG("Undefined character entity '%s'.", tok);
            isocode = -3;
         }
      }
      if (c == ';')
         s++;
      else if (prefs.show_extra_warnings)
         BUG_MSG("Character entity reference without trailing ';'.");
   }

   *entsize = s-tok+1;
   dFree(tok);

   if (isocode >= 145 && isocode <= 151) {
      /* TODO: remove this hack. */
      isocode = Html_ms_stupid_quotes_2ucs(isocode);
   } else if (isocode == -1 && prefs.show_extra_warnings)
      BUG_MSG("Literal '&'.");

   return isocode;
}

/*
 * Convert all the entities in a token to utf8 encoding. Takes
 * a token and its length, and returns a newly allocated string.
 */
char *a_Html_parse_entities(DilloHtml *html, const char *token, int toksize)
{
   const char *esc_set = "&";
   char *new_str, buf[4];
   int i, j, k, n, s, isocode, entsize;

   new_str = dStrndup(token, toksize);
   s = strcspn(new_str, esc_set);
   if (new_str[s] == 0)
      return new_str;

   for (i = j = s; i < toksize; i++) {
      if (token[i] == '&' &&
          (isocode = Html_parse_entity(html, token+i,
                                       toksize-i, &entsize)) >= 0) {
         if (isocode >= 128) {
            /* multibyte encoding */
            n = a_Utf8_encode(isocode, buf);
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
 * For white-space: pre-line, we must break the line if encountering a newline.
 * Otherwise, collapse whitespace as usual.
 */
static void Html_process_space_pre_line(DilloHtml *html, const char *space,
                                        int spacesize)
{
   int i, breakCnt = 0;

   for (i = 0; i < spacesize; i++) {
      /* Support for "\r", "\n" and "\r\n" line breaks */
      if (space[i] == '\r' || (space[i] == '\n' && !html->PrevWasCR)) {
         breakCnt++;
         html->PrevWasCR = (space[i] == '\r');

         HT2TB(html)->addLinebreak (html->wordStyle ());
      }
   }
   if (breakCnt == 0) {
      HT2TB(html)->addSpace(html->wordStyle ());
   }
}

/*
 * Parse spaces
 */
static void Html_process_space(DilloHtml *html, const char *space,
                               int spacesize)
{
   char *spc;
   int i, offset;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

   if (S_TOP(html)->display_none) {
      /* do nothing */
   } else if (parse_mode == DILLO_HTML_PARSE_MODE_STASH) {
      html->StashSpace = (html->Stash->len > 0);

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      dStr_append_l(html->Stash, space, spacesize);

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_PRE) {
      int spaceCnt = 0;

      /* re-scan the string for characters that cause line breaks */
      for (i = 0; i < spacesize; i++) {
         /* Support for "\r", "\n" and "\r\n" line breaks (skips the first) */
         if (!html->PreFirstChar &&
             (space[i] == '\r' || (space[i] == '\n' && !html->PrevWasCR))) {

            if (spaceCnt) {
               spc = dStrnfill(spaceCnt, ' ');
               HT2TB(html)->addText (spc, spaceCnt, html->wordStyle ());
               dFree(spc);
               spaceCnt = 0;
            }
            HT2TB(html)->addLinebreak (html->wordStyle ());
            html->pre_column = 0;
         }
         html->PreFirstChar = false;

         /* cr and lf should not be rendered -- they appear as a break */
         switch (space[i]) {
         case '\r':
         case '\n':
            break;
         case '\t':
            if (prefs.show_extra_warnings)
               BUG_MSG("TAB character inside <pre>.");
            offset = TAB_SIZE - html->pre_column % TAB_SIZE;
            spaceCnt += offset;
            html->pre_column += offset;
            break;
         default:
            spaceCnt++;
            html->pre_column++;
            break;
         }

         html->PrevWasCR = (space[i] == '\r');
      }

      if (spaceCnt) {
         // add break possibility for the white-space:pre-wrap case
         HT2TB(html)->addBreakOption (html->wordStyle (), false);
         spc = dStrnfill(spaceCnt, ' ');
         HT2TB(html)->addText (spc, spaceCnt, html->wordStyle ());
         dFree(spc);
      }

   } else {
      if (SGML_SPCDEL) {
         /* SGML_SPCDEL ignores white space immediately after an open tag */
      } else if (html->wordStyle ()->whiteSpace == WHITE_SPACE_PRE_LINE) {
         Html_process_space_pre_line(html, space, spacesize);
      } else {
         HT2TB(html)->addSpace(html->wordStyle ());
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
 * 'word' is a '\0'-terminated string.
 */
static void Html_process_word(DilloHtml *html, const char *word, int size)
{
   int i, j, start;
   char *Pword;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

   if (S_TOP(html)->display_none)
      return;

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH ||
       parse_mode == DILLO_HTML_PARSE_MODE_STASH_AND_BODY) {
      if (html->StashSpace) {
         dStr_append_c(html->Stash, ' ');
         html->StashSpace = false;
      }
      Pword = a_Html_parse_entities(html, word, size);
      dStr_append(html->Stash, Pword);
      dFree(Pword);

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      /* word goes in untouched, it is not processed here. */
      dStr_append_l(html->Stash, word, size);
   }

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH ||
       parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
      /* skip until the closing instructions */

   } else if (parse_mode == DILLO_HTML_PARSE_MODE_PRE) {
      /* all this overhead is to catch white-space entities */
      Pword = a_Html_parse_entities(html, word, size);
      for (start = i = 0; Pword[i]; start = i)
         if (isspace(Pword[i])) {
            while (Pword[++i] && isspace(Pword[i])) ;
            Html_process_space(html, Pword + start, i - start);
         } else {
            while (Pword[++i] && !isspace(Pword[i])) ;
            HT2TB(html)->addText(Pword + start, i - start, html->wordStyle ());
            html->pre_column += i - start;
            html->PreFirstChar = false;
         }
      dFree(Pword);

   } else {
      const char *word2, *beyond_word2;

      Pword = NULL;
      if (!memchr(word,'&', size)) {
         /* No entities */
         word2 = word;
         beyond_word2 = word + size;
      } else {
         /* Collapse white-space entities inside the word (except &nbsp;) */
         Pword = a_Html_parse_entities(html, word, size);
         /* Collapse adjacent " \t\f\n\r" characters into a single space */
         for (i = j = 0; (Pword[i] = Pword[j]); ++i, ++j) {
            if (strchr(" \t\f\n\r", Pword[i])) {
               if (i == 0 || (i > 0 && Pword[i-1] != ' '))
                  Pword[i] = ' ';
               else
                  for (--i; Pword[j+1] && strchr(" \t\f\n\r", Pword[j+1]); ++j)
                     ;
            }
         }
         word2 = Pword;
         beyond_word2 = word2 + strlen(word2);
      }
      for (start = i = 0; word2[i]; start = i) {
         int len;

         if (isspace(word2[i])) {
            while (word2[++i] && isspace(word2[i])) ;
            Html_process_space(html, word2 + start, i - start);
         } else if (!strncmp(word2+i, utf8_zero_width_space, 3)) {
            i += 3;
            HT2TB(html)->addBreakOption(html->wordStyle (), false);
         } else if (a_Utf8_ideographic(word2+i, beyond_word2, &len)) {
            i += len;
            HT2TB(html)->addText(word2 + start, i - start, html->wordStyle ());
            HT2TB(html)->addBreakOption(html->wordStyle (), false);
         } else {
            do {
               i += len;
            } while (word2[i] && !isspace(word2[i]) &&
                     strncmp(word2+i, utf8_zero_width_space, 3) &&
                     (!a_Utf8_ideographic(word2+i, beyond_word2, &len)));
            HT2TB(html)->addText(word2 + start, i - start, html->wordStyle ());
         }
      }
      if (Pword == word2)
         dFree(Pword);
   }
}

/*
 * Does the tag in tagstr (e.g. "p") match the tag in the tag, tagsize
 * structure, with the initial < skipped over (e.g. "P align=center>")?
 */
static bool Html_match_tag(const char *tagstr, char *tag, int tagsize)
{
   int i;

   for (i = 0; i < tagsize && tagstr[i] != '\0'; i++) {
      if (D_ASCII_TOLOWER(tagstr[i]) != D_ASCII_TOLOWER(tag[i]))
         return false;
   }
   /* The test for '/' is for xml compatibility: "empty/>" will be matched. */
   if (i < tagsize && (isspace(tag[i]) || tag[i] == '>' || tag[i] == '/'))
      return true;
   return false;
}

/*
 * This function is called after popping the stack, to
 * handle nested Textblock widgets.
 */
static void Html_eventually_pop_dw(DilloHtml *html, bool hand_over_break)
{
   if (html->dw != S_TOP(html)->textblock) {
      if (hand_over_break)
         HT2TB(html)->handOverBreak (html->style ());
      HT2TB(html)->flush ();
      html->dw = S_TOP(html)->textblock;
   }
}

/*
 * Push the tag (copying attributes from the top of the stack)
 */
static void Html_push_tag(DilloHtml *html, int tag_idx)
{
   int n_items;

   n_items = html->stack->size ();
   html->stack->increase ();
   /* We'll copy the former stack item and just change the tag and its index
    * instead of copying all fields except for tag.  --Jcid */
   *html->stack->getRef(n_items) = *html->stack->getRef(n_items - 1);
   html->stack->getRef(n_items)->tag_idx = tag_idx;
   html->dw = S_TOP(html)->textblock;
}

/*
 * Push the tag (used to force en element with optional open into the stack)
 * Note: now it's the same as Html_push_tag(), but things may change...
 */
static void Html_force_push_tag(DilloHtml *html, int tag_idx)
{
   html->startElement (tag_idx);
   Html_push_tag(html, tag_idx);
}

/*
 * Pop the top tag in the stack
 */
static void Html_real_pop_tag(DilloHtml *html)
{
   bool hand_over_break;

   html->styleEngine->endElement (S_TOP(html)->tag_idx);
   hand_over_break = S_TOP(html)->hand_over_break;
   html->stack->setSize (html->stack->size() - 1);
   Html_eventually_pop_dw(html, hand_over_break);
}

/*
 * Cleanup the stack to a given index.
 */
static void Html_tag_cleanup_to_idx(DilloHtml *html, int idx)
{
   int s_sz;
   while ((s_sz = html->stack->size()) > idx) {
      int toptag_idx = S_TOP(html)->tag_idx;
      TagInfo toptag = Tags[toptag_idx];
      if (s_sz > idx + 1 && toptag.EndTag != 'O')
         BUG_MSG("  - forcing close of open tag: <%s>.", toptag.name);
      _MSG("Close: %*s%s\n", size," ", toptag.name);
      if (toptag.close)
         toptag.close(html);
      Html_real_pop_tag(html);
   }
}

/*
 * Default close function for tags.
 * (conditional cleanup of the stack)
 * There are several ways of doing it. Considering the HTML 4.01 spec
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
static void Html_tag_cleanup_at_close(DilloHtml *html, int new_idx)
{
   static int i_BUTTON = a_Html_tag_index("button"),
              i_SELECT = a_Html_tag_index("select"),
              i_TEXTAREA = a_Html_tag_index("textarea");
   int w3c_mode = !prefs.w3c_plus_heuristics;
   int stack_idx, tag_idx, matched = 0, expected = 0;
   TagInfo new_tag = Tags[new_idx];

   /* Look for the candidate tag to close */
   stack_idx = html->stack->size();
   while (--stack_idx) {
      tag_idx = html->stack->getRef(stack_idx)->tag_idx;
      if (tag_idx == new_idx) {
         /* matching tag found */
         matched = 1;
         break;
      } else if (Tags[tag_idx].EndTag == 'O') {
         /* skip an optional tag */
         continue;
      } else if ((new_idx == i_BUTTON && html->InFlags & IN_BUTTON) ||
                 (new_idx == i_SELECT && html->InFlags & IN_SELECT) ||
                 (new_idx == i_TEXTAREA && html->InFlags & IN_TEXTAREA)) {
         /* let these elements close tags inside them */
         continue;
      } else if (w3c_mode || Tags[tag_idx].TagLevel >= new_tag.TagLevel) {
         /* this is the tag that should have been closed */
         expected = 1;
         break;
      }
   }

   if (matched) {
      Html_tag_cleanup_to_idx(html, stack_idx);
   } else if (expected) {
      BUG_MSG("Unexpected closing tag: </%s> -- expected </%s>.",
              new_tag.name, Tags[tag_idx].name);
   } else {
      BUG_MSG("Unexpected closing tag: </%s>.", new_tag.name);
   }
}

/*
 * Avoid nesting and inter-nesting of BUTTON, SELECT and TEXTAREA,
 * by closing them before opening another.
 * This is not an HTML SPEC restriction , but it avoids lots of trouble
 * inside dillo (concurrent inputs), and makes almost no sense to have.
 */
static void Html_tag_cleanup_nested_inputs(DilloHtml *html, int new_idx)
{
   static int i_BUTTON = a_Html_tag_index("button"),
              i_SELECT = a_Html_tag_index("select"),
              i_TEXTAREA = a_Html_tag_index("textarea");
   int stack_idx, u_idx, matched = 0;

   dReturn_if_fail(html->InFlags & (IN_BUTTON | IN_SELECT | IN_TEXTAREA));
   dReturn_if_fail(new_idx == i_BUTTON || new_idx == i_SELECT ||
                   new_idx == i_TEXTAREA);

   /* Get the unclosed tag index */
   u_idx = (html->InFlags & IN_BUTTON) ? i_BUTTON :
                 (html->InFlags & IN_SELECT) ? i_SELECT : i_TEXTAREA;

   /* Look for it inside the stack */
   stack_idx = html->stack->size();
   while (--stack_idx) {
      if (html->stack->getRef(stack_idx)->tag_idx == u_idx) {
         /* matching tag found */
         matched = 1;
         break;
      }
   }

   if (matched) {
      BUG_MSG("Attempt to nest <%s> element inside <%s> -- closing <%s>.",
              Tags[new_idx].name, Tags[u_idx].name, Tags[u_idx].name);
      Html_tag_cleanup_to_idx(html, stack_idx);
   } else {
      MSG_WARN("Inconsistent parser state, flag is SET but no '%s' element"
               "was found in the stack\n", Tags[u_idx].name);
   }

   html->InFlags &= ~(IN_BUTTON | IN_SELECT | IN_TEXTAREA);
}


/*
 * Some parsing routines.
 */

/*
 * Used by a_Html_parse_length
 */
static CssLength Html_parse_length_or_multi_length (const char *attr,
                                                    char **endptr)
{
   CssLength l;
   double v;
   char *end;

   v = strtod (attr, &end);
   switch (*end) {
   case '%':
      end++;
      l = CSS_CREATE_LENGTH (v / 100, CSS_LENGTH_TYPE_PERCENTAGE);
      break;

   case '*':
      end++;
      l = CSS_CREATE_LENGTH (v, CSS_LENGTH_TYPE_RELATIVE);
      break;
/*
   The "px" suffix seems not allowed by HTML4.01 SPEC.
   case 'p':
      if (end[1] == 'x')
         end += 2;
*/
   default:
      l = CSS_CREATE_LENGTH (v, CSS_LENGTH_TYPE_PX);
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
CssLength a_Html_parse_length (DilloHtml *html, const char *attr)
{
   CssLength l;
   char *end;

   l = Html_parse_length_or_multi_length (attr, &end);
   if (CSS_LENGTH_TYPE (l) == CSS_LENGTH_TYPE_RELATIVE)
      /* not allowed as &Length; */
      l = CSS_CREATE_LENGTH(0.0, CSS_LENGTH_TYPE_AUTO);
   else {
      /* allow only whitespaces */
      if (*end && !isspace (*end)) {
         BUG_MSG("Garbage after length: '%s'.", attr);
         l = CSS_CREATE_LENGTH(0.0, CSS_LENGTH_TYPE_AUTO);
      }
   }

   _MSG("a_Html_parse_length: \"%s\" %d\n", attr, CSS_LENGTH_VALUE(l));
   return l;
}

/*
 * Parse a color attribute.
 * Return value: parsed color, or default_color (+ error msg) on error.
 */
int32_t a_Html_color_parse(DilloHtml *html, const char *str,
                           int32_t default_color)
{
   int err = 1;
   int32_t color = a_Color_parse(str, default_color, &err);

   if (err) {
      BUG_MSG("Color '%s' is not in \"#RRGGBB\" format.", str);
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
   if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f) {
      bool valid = *val && !strchr(val, ' ');

      if (!valid) {
         BUG_MSG("'%s' value \"%s\" must not be empty and must not contain "
                 "spaces.", attrname, val);
      }
      return valid ? 1 : 0;
   } else {
      int i;

      for (i = 0; val[i]; ++i)
         if (!isascii(val[i]) || !(isalnum(val[i]) || strchr(":_.-", val[i])))
            break;

      if (val[i] || !(isascii(val[0]) && isalpha(val[0])))
         BUG_MSG("%s attribute value \"%s\" is not of the form "
                 "'[A-Za-z][A-Za-z0-9:_.-]*'.", attrname, val);

      return !(val[i]);
   }
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
   static const char HTML_SGML_sig [] = "<!DOCTYPE HTML PUBLIC ";
   static const char HTML20     [] = "-//IETF//DTD HTML";
   static const char HTML32     [] = "-//W3C//DTD HTML 3.2";
   static const char HTML40     [] = "-//W3C//DTD HTML 4.0";
   static const char HTML401    [] = "-//W3C//DTD HTML 4.01";
   static const char HTML401_url[] = "http://www.w3.org/TR/html4/";
   static const char XHTML1     [] = "-//W3C//DTD XHTML 1.0";
   static const char XHTML1_url [] = "http://www.w3.org/TR/xhtml1/DTD/";
   static const char XHTML11    [] = "-//W3C//DTD XHTML 1.1";
   static const char XHTML11_url[] = "http://www.w3.org/TR/xhtml11/DTD/";

   size_t i;
   int quote;
   char *p, *ntag = dStrndup(tag, tagsize);

   /* Tag sanitization: Collapse whitespace between tokens
    * and replace '\n' and '\r' with ' ' inside quoted strings. */
   for (i = 0, p = ntag; *p; ++p) {
      if (isspace(*p)) {
         for (ntag[i++] = ' '; isspace(p[1]); ++p) ;
      } else if ((quote = *p) == '"' || *p == '\'') {
         for (ntag[i++] = *p++; (ntag[i] = *p) && ntag[i++] != quote; ++p) {
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

   if (html->DocType != DT_NONE)
      BUG_MSG("Multiple DOCTYPE declarations.");

   /* The default DT_NONE type is TagSoup */
   if (i > strlen(HTML_SGML_sig) && // avoid out of bounds reads!
       !dStrnAsciiCasecmp(ntag, HTML_SGML_sig, strlen(HTML_SGML_sig))) {
      p = ntag + strlen(HTML_SGML_sig) + 1;
      if (!strncmp(p, HTML401, strlen(HTML401)) &&
          dStriAsciiStr(p + strlen(HTML401), HTML401_url)) {
         html->DocType = DT_HTML;
         html->DocTypeVersion = 4.01f;
      } else if (!strncmp(p, XHTML1, strlen(XHTML1)) &&
                 dStriAsciiStr(p + strlen(XHTML1), XHTML1_url)) {
         html->DocType = DT_XHTML;
         html->DocTypeVersion = 1.0f;
      } else if (!strncmp(p, XHTML11, strlen(XHTML11)) &&
                 dStriAsciiStr(p + strlen(XHTML11), XHTML11_url)) {
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
   } else if (!dStrAsciiCasecmp(ntag, "<!DOCTYPE html>") ||
              !dStrAsciiCasecmp(ntag, "<!DOCTYPE html >") ||
              !dStrAsciiCasecmp(ntag,
                           "<!DOCTYPE html SYSTEM \"about:legacy-compat\">") ||
              !dStrAsciiCasecmp(ntag,
                             "<!DOCTYPE html SYSTEM 'about:legacy-compat'>")) {
      html->DocType = DT_HTML;
      html->DocTypeVersion = 5.0f;
   }
   if (html->DocType == DT_NONE) {
      html->DocType = DT_UNRECOGNIZED;
      BUG_MSG("DOCTYPE not recognized: ('%s').", ntag);
   }
   dFree(ntag);
}

/*
 * Handle open HTML element
 */
static void Html_tag_open_html(DilloHtml *html, const char *tag, int tagsize)
{
   /* The IN_HTML flag will be kept set until at IN_EOF condition.
    * This allows to handle pages with multiple or uneven HTML tags */

   if (!(html->InFlags & IN_HTML))
      html->InFlags |= IN_HTML;
   if (html->Num_HTML < UCHAR_MAX)
      ++html->Num_HTML;

   if (html->Num_HTML > 1) {
      BUG_MSG("<html> was already open.");
      html->ReqTagClose = true;
   }
}

/*
 * Handle close HTML element
 */
static void Html_tag_close_html(DilloHtml *html)
{
   _MSG("Html_tag_close_html: Num_HTML=%d\n", html->Num_HTML);
}

/*
 * Handle open HEAD element
 */
static void Html_tag_open_head(DilloHtml *html, const char *tag, int tagsize)
{
   if (html->InFlags & IN_BODY) {
      BUG_MSG("<head> must go before the BODY section.");
      html->ReqTagClose = true;
      return;
   }

   if (html->Num_HEAD < UCHAR_MAX)
      ++html->Num_HEAD;
   if (html->InFlags & IN_HEAD) {
      BUG_MSG("<head> was already open.");
      html->ReqTagClose = true;
   } else if (html->Num_HEAD > 1) {
      BUG_MSG("<head> already finished -- ignoring.");
      html->ReqTagClose = true;
   } else {
      html->InFlags |= IN_HEAD;
   }
}

/*
 * Handle close HEAD element
 * Note: HEAD is parsed once completely got.
 */
static void Html_tag_close_head(DilloHtml *html)
{
   if (html->InFlags & IN_HEAD) {
      if (html->Num_HEAD == 1) {
         /* match for the well formed start of HEAD section */
         if (html->Num_TITLE == 0)
            BUG_MSG("<head> lacks <title>.");

         html->InFlags &= ~IN_HEAD;

         /* charset is already set, load remote stylesheets now */
         for (int i = 0; i < html->cssUrls->size(); i++) {
            a_Html_load_stylesheet(html, html->cssUrls->get(i));
         }
      } else if (html->Num_HEAD > 1) {
         --html->Num_HEAD;
      }
   } else {
      /* not reached, see Html_tag_cleanup_at_close() */
   }
}

/*
 * Handle open TITLE
 * calls stash init, where the title string will be stored
 */
static void Html_tag_open_title(DilloHtml *html, const char *tag, int tagsize)
{
   /* fill the stash buffer so TITLE content can be ignored
    * when not valid, redundant or outside HEAD section */
   a_Html_stash_init(html);

   if (html->InFlags & IN_HEAD) {
      if (html->Num_TITLE < UCHAR_MAX)
         ++html->Num_TITLE;
      if (html->Num_TITLE > 1)
         BUG_MSG("Redundant <title>.");
   } else {
      BUG_MSG("<title> must be inside <head> -- ignoring.");
   }
}

/*
 * Handle close TITLE
 * set page-title in the browser window and in the history.
 */
static void Html_tag_close_title(DilloHtml *html)
{
   if (html->InFlags & IN_HEAD && html->Num_TITLE == 1) {
      /* title is only valid inside HEAD */
      a_UIcmd_set_page_title(html->bw, html->Stash->str);
      a_History_set_title_by_url(html->page_url, html->Stash->str);
   }
}

/*
 * Handle open SCRIPT
 * initializes stash, where the embedded code will be stored.
 * MODE_VERBATIM is used because MODE_STASH catches entities.
 */
static void Html_tag_open_script(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
}

/*
 * Handle close SCRIPT
 */
static void Html_tag_close_script(DilloHtml *html)
{
   /* eventually the stash will be sent to an interpreter for parsing */
}

/*
 * Handle open STYLE
 * Store contents in the stash where the style sheet interpreter can get it.
 */
static void Html_tag_open_style(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;

   html->loadCssFromStash = true;

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "type"))) {
      if (html->DocType != DT_HTML || html->DocTypeVersion <= 4.01f)
         BUG_MSG("<style> requires type attribute.");
   } else if (dStrAsciiCasecmp(attrbuf, "text/css")) {
      html->loadCssFromStash = false;
   }
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "media")) &&
       dStrAsciiCasecmp(attrbuf, "all") && !dStriAsciiStr(attrbuf, "screen")) {
      /* HTML 4.01 sec. 6.13 says that media descriptors are case-sensitive,
       * but sec. 14.2.3 says that the attribute is case-insensitive.
       * TODO can be a comma-separated list.
       * TODO handheld.
       */
      html->loadCssFromStash = false;
   }

   a_Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
}

/*
 * Handle close STYLE
 */
static void Html_tag_close_style(DilloHtml *html)
{
   if (prefs.parse_embedded_css && html->loadCssFromStash)
      html->styleEngine->parse(html, html->base_url, html->Stash->str,
                               html->Stash->len, CSS_ORIGIN_AUTHOR);
}

/*
 * <BODY>
 */
static void Html_tag_open_body(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   int32_t color;
   int tag_index_a = a_Html_tag_index ("a");
   style::Color *bgColor;
   style::StyleImage *bgImage;
   style::BackgroundRepeat bgRepeat;
   style::BackgroundAttachment bgAttachment;
   style::Length bgPositionX, bgPositionY;

   _MSG("Html_tag_open_body Num_BODY=%d\n", html->Num_BODY);
   if (!(html->InFlags & IN_BODY))
      html->InFlags |= IN_BODY;
   if (html->Num_BODY < UCHAR_MAX)
      ++html->Num_BODY;

   if (html->Num_BODY > 1) {
      BUG_MSG("<body> was already open.");
      html->ReqTagClose = true;
      return;
   }

   if (html->InFlags & IN_HEAD) {
      /* if we're here, it's bad XHTML, no need to recover */
      BUG_MSG("Unclosed <head>.");
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      color = a_Html_color_parse(html, attrbuf, -1);

      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<body> bgcolor attribute is obsolete.");

      if (color != -1)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BACKGROUND_COLOR,
                                           CSS_TYPE_COLOR, color);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "text"))) {
      color = a_Html_color_parse(html, attrbuf, -1);

      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<body> text attribute is obsolete.");

      if (color != -1)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_COLOR,
                                           CSS_TYPE_COLOR, color);
   }

   html->restyle ();

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "link"))) {
      html->non_css_link_color = a_Html_color_parse(html, attrbuf, -1);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<body> link attribute is obsolete.");
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "vlink"))) {
      html->non_css_visited_color = a_Html_color_parse(html, attrbuf, -1);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<body> vlink attribute is obsolete.");
   }

   html->dw->setStyle (html->style ());

   bgColor = html->styleEngine->backgroundColor ();
   if (bgColor)
      HT2LT(html)->setBgColor(bgColor);

   bgImage = html->styleEngine->backgroundImage (&bgRepeat, &bgAttachment,
                                                 &bgPositionX, &bgPositionY);
   if (bgImage)
      HT2LT(html)->setBgImage(bgImage, bgRepeat, bgAttachment, bgPositionX,
                              bgPositionY);

   /* Determine a color for visited links.
    * This color is computed once per page and used for immediate feedback
    * when clicking a link.
    * On reload style including color for visited links is computed properly
    * according to CSS.
    */
   html->startElement (tag_index_a);
   html->styleEngine->setPseudoVisited ();
   if (html->non_css_visited_color != -1) {
      html->styleEngine->setNonCssHint (CSS_PROPERTY_COLOR, CSS_TYPE_COLOR,
                                        html->non_css_visited_color);
   }
   html->visited_color = html->style ()->color->getColor ();
   html->styleEngine->endElement (tag_index_a);

   if (prefs.contrast_visited_color) {
      /* get a color that has a "safe distance" from text, link and bg */
      html->visited_color =
         a_Color_vc(html->visited_color,
            html->style ()->color->getColor(),
            html->non_css_link_color,
            html->backgroundStyle()->backgroundColor->getColor());
   }


   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_BODY;
}

/*
 * BODY
 */
static void Html_tag_close_body(DilloHtml *html)
{
   /* Some tag soup pages use multiple BODY tags...
    * Defer clearing the IN_BODY flag until IN_EOF */
}

/*
 * <P>
 * TODO: what's the point between adding the parbreak before and
 *       after the push?
 */
static void Html_tag_open_p(DilloHtml *html, const char *tag, int tagsize)
{
   CssPropertyList props;

   a_Html_tag_set_align_attr (html, tag, tagsize);
}

/*
 * <FRAME>, <IFRAME>
 * TODO: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frame (DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   DilloUrl *url;
   CssPropertyList props;

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src")))
      return;

   if (!(url = a_Html_url_new(html, attrbuf, NULL, 0)))
      return;

   if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
      /* visited frame */
      html->styleEngine->setPseudoVisited ();
   } else {
      /* unvisited frame */
      html->styleEngine->setPseudoLink ();
   }

   html->styleEngine->setNonCssHint (PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                     Html_set_new_link(html,&url));
}

static void
 Html_tag_content_frame (DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   char *src;
   Textblock *textblock;
   Widget *bullet;

   textblock = HT2TB(html);

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src")))
      return;

   src = dStrdup(attrbuf);

   textblock->addParbreak (5, html->wordStyle ());

   bullet = new Bullet();
   textblock->addWidget(bullet, html->wordStyle ());
   textblock->addSpace(html->wordStyle ());

   if (D_ASCII_TOLOWER(tag[1]) == 'i') {
      /* IFRAME usually comes with very long advertising/spying URLS,
       * to not break rendering we will force name="IFRAME" */
      textblock->addText ("IFRAME", html->wordStyle ());

   } else {
      /* FRAME:
       * If 'name' tag is present use it, if not use 'src' value */
      if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
         textblock->addText (src, html->wordStyle ());
      } else {
         textblock->addText (attrbuf, html->wordStyle ());
      }
   }

   textblock->addParbreak (5, html->wordStyle ());

   dFree(src);
}

/*
 * <FRAMESET>
 * TODO: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_content_frameset (DilloHtml *html,
                                    const char *tag, int tagsize)
{
   HT2TB(html)->addParbreak (9, html->wordStyle ());
   HT2TB(html)->addText("--FRAME--", html->wordStyle ());
   Html_add_textblock(html, 5);
}

/*
 * <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_open_h(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_tag_set_align_attr (html, tag, tagsize);

   a_Html_stash_init(html);
   S_TOP(html)->parse_mode =
      DILLO_HTML_PARSE_MODE_STASH_AND_BODY;
}

/*
 * <BR>
 */
static void Html_tag_content_br(DilloHtml *html, const char *tag, int tagsize)
{
   HT2TB(html)->addLinebreak (html->wordStyle ());
}

/*
 * <FONT>
 */
static void Html_tag_open_font(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   char *fontFamily = NULL;
   int32_t color;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "color"))) {
      if (prefs.contrast_visited_color && html->InVisitedLink) {
         color = html->visited_color;
      } else {
         /* use the tag-specified color */
         color = a_Html_color_parse(html, attrbuf, -1);
      }
      if (color != -1)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_COLOR,
                                           CSS_TYPE_COLOR, color);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "face"))) {
      fontFamily = dStrdup(attrbuf);
       html->styleEngine->setNonCssHint (CSS_PROPERTY_FONT_FAMILY,
                                         CSS_TYPE_SYMBOL, fontFamily);
   }

   dFree(fontFamily);
}

/*
 * <ABBR>
 */
static void Html_tag_open_abbr(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;

   html->styleEngine->inheritBackgroundColor ();

   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

      html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                        attrbuf);
   }
}

/*
 * Read image-associated tag attributes and create new image.
 */
void a_Html_common_image_attrs(DilloHtml *html, const char *tag, int tagsize)
{
   char *width_ptr, *height_ptr;
   const char *attrbuf;
   CssLength l_w  = CSS_CREATE_LENGTH(0.0, CSS_LENGTH_TYPE_AUTO);
   CssLength l_h  = CSS_CREATE_LENGTH(0.0, CSS_LENGTH_TYPE_AUTO);
   int w = 0, h = 0;

   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {
      html->styleEngine->setNonCssHint(PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                       attrbuf);
   }
   width_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "width", NULL);
   height_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "height", NULL);
   // Check for malicious values
   // TODO: the same for percentage and relative lengths.
   if (width_ptr) {
      l_w = a_Html_parse_length (html, width_ptr);
      w = (int) (CSS_LENGTH_TYPE(l_w) == CSS_LENGTH_TYPE_PX ?
                 CSS_LENGTH_VALUE(l_w) : 0);
   }
   if (height_ptr) {
      l_h = a_Html_parse_length (html, height_ptr);
      h = (int) (CSS_LENGTH_TYPE(l_h) == CSS_LENGTH_TYPE_PX ?
                 CSS_LENGTH_VALUE(l_h) : 0);
   }
   /* Check for suspicious image size request that would cause
    * an excessive amount of memory to be allocated for the
    * image buffer.
    * Be careful to avoid integer overflows during the checks.
    * There is an additional check in dw/image.cc to catch cases
    * where only one dimension is given and the image is scaled
    * preserving its original aspect ratio.
    * Size requests passed via CSS are also checked there.
    */
   if (w < 0 || h < 0 ||
       w > IMAGE_MAX_AREA || h > IMAGE_MAX_AREA ||
       (h > 0 &&  w > IMAGE_MAX_AREA / h)) {
      dFree(width_ptr);
      dFree(height_ptr);
      width_ptr = height_ptr = NULL;
      MSG("a_Html_common_image_attrs: suspicious image size request %d x %d\n",
          w, h);
   } else {
      if (CSS_LENGTH_TYPE(l_w) != CSS_LENGTH_TYPE_AUTO)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE, l_w);
      if (CSS_LENGTH_TYPE(l_h) != CSS_LENGTH_TYPE_AUTO)
         html->styleEngine->setNonCssHint (CSS_PROPERTY_HEIGHT,
                                           CSS_TYPE_LENGTH_PERCENTAGE, l_h);
   }

   /* TODO: we should scale the image respecting its ratio.
    *       As the image size is not known at this time, maybe a flag
    *       can be set to scale it later.
   if ((width_ptr && !height_ptr) || (height_ptr && !width_ptr))
      [...]
   */

   /* x_img is an index to a list of {url,image} pairs.
    * We know a_Html_image_new() will use size() as its next index */
   html->styleEngine->setNonCssHint (PROPERTY_X_IMG, CSS_TYPE_INTEGER,
                                     html->images->size());


   dFree(width_ptr);
   dFree(height_ptr);
}

DilloImage *a_Html_image_new(DilloHtml *html, const char *tag, int tagsize)
{
   bool load_now;
   char *alt_ptr;
   const char *attrbuf;
   DilloUrl *url;
   DilloImage *image;

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src")) ||
       !(url = a_Html_url_new(html, attrbuf, NULL, 0)))
      return NULL;

   alt_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "alt", NULL);
   if ((!alt_ptr || !*alt_ptr) && !prefs.load_images) {
      dFree(alt_ptr);
      alt_ptr = dStrdup("[IMG]"); // Place holder for img_off mode
   }

   dw::Image *dw = new dw::Image(alt_ptr);
   image =
      a_Image_new(html->dw->getLayout(), (void*)(dw::core::ImgRenderer*)dw, 0);

   if (HT2TB(html)->getBgColor())
      image->bg_color = HT2TB(html)->getBgColor()->getColor();

   DilloHtmlImage *hi = dNew(DilloHtmlImage, 1);
   hi->url = url;
   html->images->increase();
   html->images->set(html->images->size() - 1, hi);

   load_now = prefs.load_images ||
              !dStrAsciiCasecmp(URL_SCHEME(url), "data") ||
              (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached);

   if (load_now && Html_load_image(html->bw, url, html->page_url, image)) {
      // hi->image is NULL if dillo tries to load the image immediately
      hi->image = NULL;
   } else {
      // otherwise a reference is kept in html->images
      hi->image = image;
      a_Image_ref(image);
   }

   dFree(alt_ptr);
   return image;
}

/*
 * Tell cache to retrieve image
 */
static bool Html_load_image(BrowserWindow *bw, DilloUrl *url,
                            const DilloUrl *requester, DilloImage *Image)
{
   DilloWeb *Web;
   int ClientKey;
   /* Fill a Web structure for the cache query */
   Web = a_Web_new(bw, url, requester);
   Web->Image = Image;
   a_Image_ref(Image);
   Web->flags |= WEB_Image;
   /* Request image data from the cache */
   if ((ClientKey = a_Capi_open_url(Web, NULL, NULL)) != 0) {
      a_Bw_add_client(bw, ClientKey, 0);
      a_Bw_add_url(bw, url);
   }
   return ClientKey != 0;
}

static void Html_tag_open_img(DilloHtml *html, const char *tag, int tagsize)
{
   int space, border;
   const char *attrbuf;

   a_Html_common_image_attrs(html, tag, tagsize);

   /* Spacing to the left and right */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "hspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0) {
         space = CSS_CREATE_LENGTH(space, CSS_LENGTH_TYPE_PX);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_MARGIN_LEFT,
                                           CSS_TYPE_LENGTH_PERCENTAGE, space);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_MARGIN_RIGHT,
                                           CSS_TYPE_LENGTH_PERCENTAGE, space);
      }
   }

   /* Spacing at the top and bottom */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "vspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0) {
         space = CSS_CREATE_LENGTH(space, CSS_LENGTH_TYPE_PX);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_MARGIN_TOP,
                                           CSS_TYPE_LENGTH_PERCENTAGE, space);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_MARGIN_BOTTOM,
                                           CSS_TYPE_LENGTH_PERCENTAGE, space);
      }
   }

   /* Border */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "border"))) {
      border = strtol(attrbuf, NULL, 10);
      if (border >= 0) {
         border = CSS_CREATE_LENGTH(border, CSS_LENGTH_TYPE_PX);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE, border);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE, border);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE, border);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                           CSS_TYPE_LENGTH_PERCENTAGE, border);

         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_STYLE,
                                           CSS_TYPE_ENUM, BORDER_SOLID);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                           CSS_TYPE_ENUM, BORDER_SOLID);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_STYLE,
                                           CSS_TYPE_ENUM, BORDER_SOLID);
         html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                           CSS_TYPE_ENUM, BORDER_SOLID);
      }
   }

}

/*
 * Create a new Image struct and request the image-url to the cache
 * (If it either hits or misses, is not relevant here; that's up to the
 *  cache functions)
 */
static void Html_tag_content_img(DilloHtml *html, const char *tag, int tagsize)
{
   DilloImage *Image;
   DilloUrl *usemap_url;
   const char *attrbuf;

   /* This avoids loading images. Useful for viewing suspicious HTML email. */
   if (URL_FLAGS(html->base_url) & URL_SpamSafe)
      return;

   Image = a_Html_image_new(html, tag, tagsize);
   if (!Image)
      return;

   usemap_url = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "usemap")))
      /* TODO: usemap URLs outside of the document are not used. */
      usemap_url = a_Html_url_new(html, attrbuf, NULL, 0);

   // At this point, we know that Image->ir represents an image
   // widget. Notice that the order of the casts matters, because of
   // multiple inheritance.
   dw::Image *dwi = (dw::Image*)(dw::core::ImgRenderer*)Image->img_rndr;
   HT2TB(html)->addWidget(dwi, html->style());

   /* Image maps */
   if (a_Html_get_attr(html, tag, tagsize, "ismap")) {
      dwi->setIsMap();
      _MSG("  Html_tag_open_img: server-side map (ISMAP)\n");
   } else if (html->style ()->x_link != -1 &&
              usemap_url == NULL) {
      /* For simple links, we have to suppress the "image_pressed" signal.
       * This is overridden for USEMAP images. */
//    a_Dw_widget_set_button_sensitive (IM2DW(Image->dw), FALSE);
   }

   if (usemap_url) {
      dwi->setUseMap(&html->maps, new ::object::String(URL_STR(usemap_url)));
      a_Url_free (usemap_url);
   }
}

/*
 * <map>
 */
static void Html_tag_content_map(DilloHtml *html, const char *tag, int tagsize)
{
   char *hash_name;
   const char *attrbuf;
   DilloUrl *url;

   if (html->InFlags & IN_MAP) {
      BUG_MSG("Nested <map>.");
   } else {
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
         html->InFlags |= IN_MAP;
         hash_name = dStrconcat("#", attrbuf, NULL);
         url = a_Html_url_new(html, hash_name, NULL, 0);
         html->maps.startNewMap(new ::object::String(URL_STR(url)));
         a_Url_free (url);
         dFree(hash_name);
      } else {
         BUG_MSG("<map> requires name attribute.");
      }
   }
}

/*
 * Handle close <MAP>
 */
static void Html_tag_close_map(DilloHtml *html)
{
   /* This is a hack for the perhaps frivolous feature of drawing image map
    * shapes when there is no image to display. If this map is defined after
    * an image that has not been loaded (img != NULL), tell the image to
    * redraw. (It will only do so if it uses a map.)
    */
   for (int i = 0; i < html->images->size(); i++) {
      DilloImage *img = html->images->get(i)->image;

      if (img) {
         // At this point, we know that img->ir represents an image
         // widget. (Really? Is this assumtion safe?) Notice that the
         // order of the casts matters, because of multiple
         // inheritance.
         dw::Image *dwi = (dw::Image*)(dw::core::ImgRenderer*)img->img_rndr;
         dwi->forceMapRedraw();
      }
   }
   html->InFlags &= ~IN_MAP;
}

/*
 * Read coords in a string, returning a vector of ints.
 */
static
misc::SimpleVector<int> *Html_read_coords(DilloHtml *html, const char *str)
{
   int coord;
   const char *tail = str;
   char *newtail = NULL;
   misc::SimpleVector<int> *coords = new misc::SimpleVector<int> (4);

   while (1) {
      coord = strtol(tail, &newtail, 10);
      if (coord == 0 && newtail == tail)
         break;
      coords->increase();
      coords->set(coords->size() - 1, coord);
      while (isspace(*newtail))
         newtail++;
      if (!*newtail)
         break;
      if (*newtail != ',') {
         BUG_MSG("<area> coords must be integers separated by commas.");
      }
      tail = newtail + 1;
   }

   return coords;
}

/*
 * <AREA>
 */
static void
 Html_tag_content_area(DilloHtml *html, const char *tag, int tagsize)
{
   enum types {UNKNOWN, RECTANGLE, CIRCLE, POLYGON, BACKGROUND};
   types type;
   misc::SimpleVector<int> *coords = NULL;
   DilloUrl* url;
   const char *attrbuf;
   int link = -1;
   Shape *shape = NULL;

   if (!(html->InFlags & IN_MAP)) {
      BUG_MSG("<area> not inside <map>.");
      return;
   }
   attrbuf = a_Html_get_attr(html, tag, tagsize, "shape");

   if (!attrbuf || !*attrbuf || !dStrAsciiCasecmp(attrbuf, "rect")) {
      /* the default shape is a rectangle */
      type = RECTANGLE;
   } else if (dStrAsciiCasecmp(attrbuf, "default") == 0) {
      /* "default" is the background */
      type = BACKGROUND;
   } else if (dStrAsciiCasecmp(attrbuf, "circle") == 0) {
      type = CIRCLE;
   } else if (dStrnAsciiCasecmp(attrbuf, "poly", 4) == 0) {
      type = POLYGON;
   } else {
      BUG_MSG("<area> unknown shape: '%s'.", attrbuf);
      type = UNKNOWN;
   }
   if (type == RECTANGLE || type == CIRCLE || type == POLYGON) {
      /* TODO: add support for coords in % */
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "coords"))) {
         coords = Html_read_coords(html, attrbuf);

         if (type == RECTANGLE) {
            if (coords->size() != 4)
               BUG_MSG("<area> rectangle must have four coordinate values.");
            if (coords->size() >= 4)
               shape = new Rectangle(coords->get(0),
                                     coords->get(1),
                                     coords->get(2) - coords->get(0),
                                     coords->get(3) - coords->get(1));
         } else if (type == CIRCLE) {
            if (coords->size() != 3)
               BUG_MSG("<area> circle must have three coordinate values.");
            if (coords->size() >= 3)
               shape = new Circle(coords->get(0), coords->get(1),
                                  coords->get(2));
         } else if (type == POLYGON) {
            Polygon *poly;
            int i;
            if (coords->size() % 2)
               BUG_MSG("<area> polygon with odd number of coordinates.");
            shape = poly = new Polygon();
            for (i = 0; i < (coords->size() / 2); i++)
               poly->addPoint(coords->get(2*i), coords->get(2*i + 1));
         }
         delete(coords);
      }
   }
   if (shape != NULL || type == BACKGROUND) {
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "href"))) {
         url = a_Html_url_new(html, attrbuf, NULL, 0);
         dReturn_if_fail ( url != NULL );
         if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "alt")))
            a_Url_set_alt(url, attrbuf);

         link = Html_set_new_link(html, &url);
      }
      if (type == BACKGROUND)
         html->maps.setCurrentMapDefaultLink(link);
      else
         html->maps.addShapeToCurrentMap(shape, link);
   }
}

/*
 * <OBJECT>
 * Simply provide a link if the object is something downloadable.
 */
static void Html_tag_open_object(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *url, *base_url = NULL;
   const char *attrbuf;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "codebase"))) {
      base_url = a_Html_url_new(html, attrbuf, NULL, 0);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "data"))) {
      url = a_Html_url_new(html, attrbuf,
                           URL_STR(base_url), (base_url != NULL));
      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->styleEngine->setPseudoVisited ();
      } else {
         html->styleEngine->setPseudoLink ();
      }

      html->styleEngine->setNonCssHint(PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                       Html_set_new_link(html, &url));
   }
   a_Url_free(base_url);
}

static void Html_tag_content_object(DilloHtml *html, const char *tag,
                                    int tagsize)
{
   if (a_Html_get_attr(html, tag, tagsize, "data"))
      HT2TB(html)->addText("[OBJECT]", html->wordStyle ());
}

/*
 * <VIDEO>
 * Provide a link to the video.
 */
static void Html_tag_open_video(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *url;
   const char *attrbuf;

   if (html->InFlags & IN_MEDIA) {
      MSG("<video> not handled when already inside a media element.\n");
      return;
   }
   /* TODO: poster attr */

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "src"))) {
      url = a_Html_url_new(html, attrbuf, NULL, 0);
      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->styleEngine->setPseudoVisited ();
      } else {
         html->styleEngine->setPseudoLink ();
      }

      html->styleEngine->setNonCssHint(PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                       Html_set_new_link(html, &url));

      HT2TB(html)->addText("[VIDEO]", html->wordStyle ());
   }
   html->InFlags |= IN_MEDIA;
}

/*
 * <AUDIO>
 * Provide a link to the audio.
 */
static void Html_tag_open_audio(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *url;
   const char *attrbuf;

   if (html->InFlags & IN_MEDIA) {
      MSG("<audio> not handled when already inside a media element.\n");
      return;
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "src"))) {
      url = a_Html_url_new(html, attrbuf, NULL, 0);
      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->styleEngine->setPseudoVisited ();
      } else {
         html->styleEngine->setPseudoLink ();
      }

      html->styleEngine->setNonCssHint(PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                       Html_set_new_link(html, &url));

      HT2TB(html)->addText("[AUDIO]", html->wordStyle ());
   }
   html->InFlags |= IN_MEDIA;
}

/*
 * <SOURCE>
 * Media resource; provide a link to its address.
 */
static void Html_tag_open_source(DilloHtml *html, const char *tag,
                                    int tagsize)
{
   const char *attrbuf;

   if (!(html->InFlags & IN_MEDIA)) {
      BUG_MSG("<source> not inside a media element.");
      return;
   }
   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src"))) {
      BUG_MSG("<source> requires src attribute.");
      return;
   } else {
      DilloUrl *url = a_Html_url_new(html, attrbuf, NULL, 0);

      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->styleEngine->setPseudoVisited ();
      } else {
         html->styleEngine->setPseudoLink ();
      }
      html->styleEngine->setNonCssHint(PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                       Html_set_new_link(html, &url));
   }
}

static void Html_tag_content_source(DilloHtml *html, const char *tag,
                                    int tagsize)
{
   if ((html->InFlags & IN_MEDIA) && a_Html_get_attr(html, tag, tagsize,"src"))
      HT2TB(html)->addText("[MEDIA SOURCE]", html->wordStyle ());
}

/*
 * Media (AUDIO/VIDEO) close function
 */
static void Html_tag_close_media(DilloHtml *html)
{
   html->InFlags &= ~IN_MEDIA;
}

/*
 * <EMBED>
 * Provide a link to embedded content.
 */
static void Html_tag_open_embed(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "src"))) {
      DilloUrl *url = a_Html_url_new(html, attrbuf, NULL, 0);

      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->styleEngine->setPseudoVisited ();
      } else {
         html->styleEngine->setPseudoLink ();
      }

      html->styleEngine->setNonCssHint(PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                       Html_set_new_link(html, &url));
   }
}

static void Html_tag_content_embed(DilloHtml *html,const char *tag,int tagsize)
{
   if (a_Html_get_attr(html, tag, tagsize, "src"))
      HT2TB(html)->addText("[EMBED]", html->wordStyle ());
}

/*
 * Test and extract the link from a javascript instruction.
 */
static const char* Html_get_javascript_link(DilloHtml *html)
{
   size_t i;
   char ch, *p1, *p2;
   Dstr *Buf = html->attr_data;

   if (dStrnAsciiCasecmp("javascript", Buf->str, 10) == 0) {
      i = strcspn(Buf->str, "'\"");
      ch = Buf->str[i];
      if ((ch == '"' || ch == '\'') &&
          (p2 = strchr(Buf->str + i + 1 , ch))) {
         p1 = Buf->str + i;
         BUG_MSG("Link depends on javascript().");
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
   if (!HT2TB(html)->addAnchor (name, html->style ()))
      BUG_MSG("Anchor names must be unique within the document (\"%s\").",
              name);
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
   DilloUrl *url;
   const char *attrbuf;

   /* TODO: add support for MAP with A HREF */
   if (html->InFlags & IN_MAP)
      Html_tag_content_area(html, tag, tagsize);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "href"))) {
      /* if it's a javascript link, extract the reference. */
      if (D_ASCII_TOLOWER(attrbuf[0]) == 'j')
         attrbuf = Html_get_javascript_link(html);

      url = a_Html_url_new(html, attrbuf, NULL, 0);
      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags_with_redirection(url) & CAPI_IsCached) {
         html->InVisitedLink = true;
         html->styleEngine->setPseudoVisited ();
         if (html->non_css_visited_color != -1)
            html->styleEngine->setNonCssHint(CSS_PROPERTY_COLOR,
                                             CSS_TYPE_COLOR,
                                             html->non_css_visited_color);
      } else {
         html->styleEngine->setPseudoLink ();
         if (html->non_css_link_color != -1)
            html->styleEngine->setNonCssHint(CSS_PROPERTY_COLOR,
                                             CSS_TYPE_COLOR,
                                             html->non_css_link_color);
      }

      html->styleEngine->setNonCssHint (PROPERTY_X_LINK, CSS_TYPE_INTEGER,
                                        Html_set_new_link(html, &url));
   }
   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {
      html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                        attrbuf);
   }

   html->styleEngine->inheritBackgroundColor ();

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
      char *nameVal;
      const char *id = html->styleEngine->getId ();

      if (prefs.show_extra_warnings)
         Html_check_name_val(html, attrbuf, "name");

      nameVal = a_Url_decode_hex_str(attrbuf);

      if (nameVal) {
         /* We compare the "id" value with the url-decoded "name" value */
         if (!id || strcmp(nameVal, id)) {
            if (id)
               BUG_MSG("In <a>, id ('%s') and name ('%s') attributes differ.",
                        id, nameVal);
            Html_add_anchor(html, nameVal);
         }

         dFree(nameVal);
      }
   }
}

/*
 * <A> close function
 */
static void Html_tag_close_a(DilloHtml *html)
{
   html->InVisitedLink = false;
}

/*
 * <BLOCKQUOTE>
 */
static void Html_tag_open_blockquote(DilloHtml *html,
                                     const char *tag, int tagsize)
{
   Html_add_textblock(html, 9);
}

/*
 * <Q>
 */
static void Html_tag_open_q(DilloHtml *html, const char *tag, int tagsize)
{
   /*
    * Left Double Quotation Mark, which is wrong in many cases, but
    * should at least be widely recognized.
    */
   const char *U201C = "\xe2\x80\x9c";

   html->styleEngine->inheritBackgroundColor ();
   HT2TB(html)->addText (U201C, html->wordStyle ());
}

/*
 * </Q>
 */
static void Html_tag_close_q(DilloHtml *html)
{
   /* Right Double Quotation Mark */
   const char *U201D = "\xe2\x80\x9d";

   HT2TB(html)->addText (U201D, html->wordStyle ());
}

/*
 * Handle the <UL> tag.
 */
static void Html_tag_open_ul(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   ListStyleType list_style_type;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "type"))) {

      /* list_style_type explicitly defined */
      if (dStrAsciiCasecmp(attrbuf, "disc") == 0)
         list_style_type = LIST_STYLE_TYPE_DISC;
      else if (dStrAsciiCasecmp(attrbuf, "circle") == 0)
         list_style_type = LIST_STYLE_TYPE_CIRCLE;
      else if (dStrAsciiCasecmp(attrbuf, "square") == 0)
         list_style_type = LIST_STYLE_TYPE_SQUARE;
      else
         /* invalid value */
         list_style_type = LIST_STYLE_TYPE_DISC;

      html->styleEngine->setNonCssHint (CSS_PROPERTY_LIST_STYLE_TYPE,
                                        CSS_TYPE_ENUM, list_style_type);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<ul> type attribute is obsolete.");
   }

   S_TOP(html)->list_type = HTML_LIST_UNORDERED;
   S_TOP(html)->list_number = 0;
   S_TOP(html)->ref_list_item = NULL;
}

/*
 * Handle the <DIR> or <MENU> tag.
 * (Deprecated and almost the same as <UL>)
 */
static void Html_tag_open_dir(DilloHtml *html, const char *tag, int tagsize)
{
   html->styleEngine->inheritBackgroundColor ();
   HT2TB(html)->addParbreak (9, html->wordStyle ());

   S_TOP(html)->list_type = HTML_LIST_UNORDERED;
   S_TOP(html)->list_number = 0;
   S_TOP(html)->ref_list_item = NULL;

   if (prefs.show_extra_warnings)
      BUG_MSG("Obsolete list type; use <ul> instead.");
}

/*
 * Handle the <MENU> tag.
 */
static void Html_tag_open_menu(DilloHtml *html, const char *tag, int tagsize)
{
   /* In another bit of ridiculous mess from the HTML5 world, the menu
    * element, which was deprecated in HTML4:
    * - does not appear at all in W3C's HTML5 spec
    * - appears in WHATWG's HTML5 doc and the W3C's 5.1 draft, where it
    *   means something totally different than it did in the old days
    *   (now it's for popup menus and toolbar menus rather than being a
    *   sort of list).
    */
   if (!(html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f))
      Html_tag_open_dir(html, tag, tagsize);
}

/*
 * Handle the <OL> tag.
 */
static void Html_tag_open_ol(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   int n = 1;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "type"))) {
      ListStyleType listStyleType = LIST_STYLE_TYPE_DECIMAL;

      if (*attrbuf == '1')
         listStyleType = LIST_STYLE_TYPE_DECIMAL;
      else if (*attrbuf == 'a')
         listStyleType = LIST_STYLE_TYPE_LOWER_ALPHA;
      else if (*attrbuf == 'A')
         listStyleType = LIST_STYLE_TYPE_UPPER_ALPHA;
      else if (*attrbuf == 'i')
         listStyleType = LIST_STYLE_TYPE_LOWER_ROMAN;
      else if (*attrbuf == 'I')
         listStyleType = LIST_STYLE_TYPE_UPPER_ROMAN;

      html->styleEngine->setNonCssHint (CSS_PROPERTY_LIST_STYLE_TYPE,
                                        CSS_TYPE_ENUM, listStyleType);
   }

   S_TOP(html)->list_type = HTML_LIST_ORDERED;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "start")) &&
       (n = (int) strtol(attrbuf, NULL, 10)) < 0) {
      BUG_MSG("Illegal '-' character in START attribute; Starting from 0.");
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
   Style *style = html->style ();
   int *list_number;
   const char *attrbuf;

   if (S_TOP(html)->list_type == HTML_LIST_NONE)
      BUG_MSG("<li> outside <ul> or <ol>.");

   html->InFlags |= IN_LI;

   /* Get our parent tag's variables (used as state storage) */
   list_number = &html->stack->getRef(html->stack->size()-2)->list_number;

   if (style->listStyleType >= LIST_STYLE_TYPE_DECIMAL) {
      // ordered
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "value")) &&
          (*list_number = strtol(attrbuf, NULL, 10)) < 0) {
         BUG_MSG("Illegal negative list value attribute; Starting from 0.");
         *list_number = 0;
      }
   }
}

/*
 * Close <LI>
 */
static void Html_tag_close_li(DilloHtml *html)
{
   html->InFlags &= ~IN_LI;
   ((ListItem *)html->dw)->flush ();
}

/*
 * <HR>
 */
static void Html_tag_open_hr(DilloHtml *html, const char *tag, int tagsize)
{
   char *width_ptr;
   const char *attrbuf;
   int32_t size = 0;

   width_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "width", NULL);
   if (width_ptr) {
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<hr> width attribute is obsolete.");
      html->styleEngine->setNonCssHint (CSS_PROPERTY_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE,
                                        a_Html_parse_length (html, width_ptr));
      dFree(width_ptr);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "size"))) {
      size = strtol(attrbuf, NULL, 10);
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<hr> size attribute is obsolete.");
   }

   a_Html_tag_set_align_attr(html, tag, tagsize);

   /* TODO: evaluate attribute */
   if (a_Html_get_attr(html, tag, tagsize, "noshade")) {
      if (html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
         BUG_MSG("<hr> noshade attribute is obsolete.");
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_STYLE,
                                        CSS_TYPE_ENUM, BORDER_SOLID);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_STYLE,
                                        CSS_TYPE_ENUM, BORDER_SOLID);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_SOLID);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_STYLE,
                                        CSS_TYPE_ENUM, BORDER_SOLID);

      if (size <= 0)
         size = 1;
   }

   if (size > 0) {
      CssLength size_top = CSS_CREATE_LENGTH ((size+1)/2, CSS_LENGTH_TYPE_PX);
      CssLength size_bottom = CSS_CREATE_LENGTH (size / 2, CSS_LENGTH_TYPE_PX);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_TOP_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, size_top);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_LEFT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE, size_top);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE,
                                        size_bottom);
      html->styleEngine->setNonCssHint (CSS_PROPERTY_BORDER_RIGHT_WIDTH,
                                        CSS_TYPE_LENGTH_PERCENTAGE,
                                        size_bottom);
   }

}

static void Html_tag_content_hr(DilloHtml *html, const char *tag, int tagsize)
{
   Widget *hruler;
   HT2TB(html)->addParbreak (5, html->wordStyle ());

   hruler = new Ruler();
   hruler->setStyle (html->style ());
   HT2TB(html)->addWidget (hruler, html->style ());
   HT2TB(html)->addParbreak (5, html->wordStyle ());
}

/*
 * <DL>
 */
static void Html_tag_open_dl(DilloHtml *html, const char *tag, int tagsize)
{
   /* may want to actually do some stuff here. */
   html->styleEngine->inheritBackgroundColor ();
   HT2TB(html)->addParbreak (9, html->wordStyle ());
}

/*
 * <DT>
 */
static void Html_tag_open_dt(DilloHtml *html, const char *tag, int tagsize)
{
   html->styleEngine->inheritBackgroundColor ();
   HT2TB(html)->addParbreak (9, html->wordStyle ());
}

/*
 * <DD>
 */
static void Html_tag_open_dd(DilloHtml *html, const char *tag, int tagsize)
{
   Html_add_textblock(html, 9);
}

/*
 * <PRE>
 */
static void Html_tag_open_pre(DilloHtml *html, const char *tag, int tagsize)
{
   html->styleEngine->inheritBackgroundColor ();
   HT2TB(html)->addParbreak (9, html->wordStyle ());

   html->InFlags |= IN_PRE;
}

/*
 * Custom close for <PRE>
 */
static void Html_tag_close_pre(DilloHtml *html)
{
   html->InFlags &= ~IN_PRE;
}

/*
 * Check whether a tag is in the "excluding" element set for PRE
 * Excl. Set = {IMG, OBJECT, APPLET, BIG, SMALL, SUB, SUP, FONT, BASEFONT}
 */
static int Html_tag_pre_excludes(DilloHtml *html, int tag_idx)
{
   if (!(html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)) {
      /* HTML5 doesn't say anything about excluding elements */
      const char *es_set[] = {"img", "object", "applet", "big", "small", "sub",
                              "sup", "font", "basefont", NULL};
      static int ei_set[10], i;

      /* initialize array */
      if (!ei_set[0])
         for (i = 0; es_set[i]; ++i)
            ei_set[i] = a_Html_tag_index(es_set[i]);

      for (i = 0; ei_set[i]; ++i)
         if (tag_idx == ei_set[i])
            return 1;
   }
   return 0;
}

/*
 * Update the document's content type information based on meta tag data.
 */
static void Html_update_content_type(DilloHtml *html, const char *content)
{
   const char *new_content = a_Capi_set_content_type(html->page_url, content,
                                                     "meta");
   /* Cannot ask cache whether the content type was changed, as
    * this code in another bw might have already changed it for us.
    */
   if (a_Misc_content_type_cmp(html->content_type, new_content)) {
      html->stop_parser = true; /* The cache buffer is no longer valid */
      a_UIcmd_repush(html->bw);
   }
}

/*
 * Handle <META>
 * We do not support http-equiv=refresh with delay>0 because it's
 * non standard, (the HTML 4.01 SPEC recommends explicitly to avoid it).
 * More info at:
 *   http://lists.w3.org/Archives/Public/www-html/2000Feb/thread.html#msg232
 * Instant client-side redirects (delay=0) are supported:
 *   http://www.w3.org/TR/2008/NOTE-WCAG20-TECHS-20081211/H76.html
 *
 * TODO: Note that we're sending custom HTML while still IN_HEAD. This
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

   const char *p, *equiv, *charset, *content;
   char delay_str[64], *mr_url;
   DilloUrl *new_url;
   int delay;

   /* only valid inside HEAD */
   if (!(html->InFlags & IN_HEAD)) {
      if (!((html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f) &&
            a_Html_get_attr(html, tag, tagsize, "itemprop"))) {
         /* With the HTML 5.1 draft spec, meta with itemprop may appear
          * in the body.
          */
         BUG_MSG("This <meta> element must be inside the HEAD section.");
      }
      return;
   }

   if ((equiv = a_Html_get_attr(html, tag, tagsize, "http-equiv"))) {
      if (!dStrAsciiCasecmp(equiv, "refresh") &&
          (content = a_Html_get_attr(html, tag, tagsize, "content"))) {

         /* Get delay, if present, and make a message with it */
         if ((delay = strtol(content, NULL, 0))) {
            snprintf(delay_str, 64, " after %d second%s.",
                     delay, (delay > 1) ? "s" : "");
         } else {
            sprintf(delay_str, ".");
         }
         /* Skip to anything after "URL=" or ";" if "URL=" is not found */
         if ((p = dStriAsciiStr(content, "url=")))
            content = p + strlen("url=");
         else if ((p = strstr(content, ";")))
            content = p + strlen(";");
         /* Handle the case of a quoted URL */
         if (*content == '"' || *content == '\'') {
            if ((p = strchr(content + 1, *content)))
               mr_url = dStrndup(content + 1, p - content - 1);
            else
               mr_url = dStrdup(content + 1);
         } else {
            mr_url = dStrdup(content);
         }
         new_url = a_Html_url_new(html, mr_url, NULL, 0);

         if (a_Url_cmp(html->base_url, new_url) == 0) {
            /* redirection loop, or empty url string: ignore */
            BUG_MSG("<meta> refresh: %s.",
                    *mr_url ? "redirection loop" : "no target URL");
         } else if (delay == 0) {
            /* zero-delay redirection */
            html->stop_parser = true;
            if (URL_FLAGS(html->base_url) & URL_SpamSafe) {
               a_UIcmd_set_msg(html->bw,
                  "WARNING: local URL with META refresh.  Aborting.");
            } else if (a_Capi_dpi_verify_request(html->bw, new_url)) {
               a_UIcmd_redirection0((void*)html->bw, new_url);
            }
         } else {
            /* Send a custom HTML message.
             * TODO: This is a hairy hack,
             *       It'd be much better to build a widget. */
            Dstr *ds_msg = dStr_sized_new(256);
            dStr_sprintf(ds_msg, meta_template, URL_STR(new_url), delay_str);
            {
               int o_InFlags = html->InFlags;
               int o_TagSoup = html->TagSoup;
               html->InFlags = IN_BODY + IN_META_HACK;
               html->TagSoup = false;
               Html_write_raw(html, ds_msg->str, ds_msg->len, 0);
               html->TagSoup = o_TagSoup;
               html->InFlags = o_InFlags;
            }
            dStr_free(ds_msg, 1);
         }
         a_Url_free(new_url);
         dFree(mr_url);

      } else if (!dStrAsciiCasecmp(equiv, "content-type") &&
                 (content = a_Html_get_attr(html, tag, tagsize, "content"))) {
         _MSG("Html_tag_open_meta: content={%s}\n", content);
         Html_update_content_type(html, content);
      }
   } else if (html->DocType == DT_HTML && html->DocTypeVersion == 5.0f &&
              (charset = a_Html_get_attr(html, tag, tagsize, "charset"))) {
      char *content = dStrconcat("text/html; charset=", charset, NULL);

      Html_update_content_type(html, content);
      dFree(content);
   }
}

/*
 * Called by the network engine when a stylesheet has new data.
 */
static void Html_css_load_callback(int Op, CacheClient_t *Client)
{
   _MSG("Html_css_load_callback: Op=%d\n", Op);
   if (Op) { /* EOF */
      BrowserWindow *bw = ((DilloWeb *)Client->Web)->bw;
      /* Repush when we've got them all */
      if (--bw->NumPendingStyleSheets == 0)
         a_UIcmd_repush(bw);
   }
}

/*
 * Tell cache to retrieve a stylesheet
 */
void a_Html_load_stylesheet(DilloHtml *html, DilloUrl *url)
{
   char *data;
   int len;

   dReturn_if (url == NULL || ! prefs.load_stylesheets);

   _MSG("Html_load_stylesheet: ");
   if (a_Capi_get_buf(url, &data, &len)) {
      _MSG("cached URL=%s len=%d", URL_STR(url), len);
      if (a_Capi_get_flags_with_redirection(url) & CAPI_Completed) {
         if (strncmp("@charset \"", data, 10) == 0) {
            char *endq = strchr(data+10, '"');

            if (endq && (endq - data <= 51)) {
               /* IANA limits charset names to 40 characters */
               char *content_type;

               *endq = '\0';
               content_type = dStrconcat("text/css; charset=", data+10, NULL);
               *endq = '"';
               a_Capi_unref_buf(url);
               a_Capi_set_content_type(url, content_type, "meta");
               dFree(content_type);
               a_Capi_get_buf(url, &data, &len);
            }
         }
         html->styleEngine->parse(html, url, data, len, CSS_ORIGIN_AUTHOR);
      }
      a_Capi_unref_buf(url);
   } else {
      /* Fill a Web structure for the cache query */
      int ClientKey;
      DilloWeb *Web = a_Web_new(html->bw, url, html->page_url);
      Web->flags |= WEB_Stylesheet;
      if ((ClientKey = a_Capi_open_url(Web, Html_css_load_callback, NULL))) {
         ++html->bw->NumPendingStyleSheets;
         a_Bw_add_client(html->bw, ClientKey, 0);
         a_Bw_add_url(html->bw, url);
         MSG("NumPendingStyleSheets=%d\n", html->bw->NumPendingStyleSheets);
      }
   }
   _MSG("\n");
}

/*
 * Parse the LINK element (Only CSS stylesheets by now).
 * (If it either hits or misses, is not relevant here; that's up to the
 *  cache functions)
 *
 * TODO: How will we know when to use "handheld"? Ask the html->bw->ui for
 * screen dimensions, or a dillorc preference.
 */
static void Html_tag_open_link(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *url;
   const char *attrbuf;

   //char *tag_str = dStrndup(tag, tagsize);
   //MSG("Html_tag_open_link(): %s\n", tag_str);
   //dFree(tag_str);

   /* When viewing suspicious HTML email, don't load LINK */
   dReturn_if (URL_FLAGS(html->base_url) & URL_SpamSafe);

   /* Ignore LINK outside HEAD */
   if (!(html->InFlags & IN_HEAD)) {
      if (!((html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f) &&
            a_Html_get_attr(html, tag, tagsize, "itemprop"))) {
         /* With the HTML 5.1 draft spec, link with itemprop may appear
          * in the body.
          */
         BUG_MSG("This <link> element must be inside the HEAD section.");
      }
      return;
   }
   /* Remote stylesheets enabled? */
   dReturn_if_fail (prefs.load_stylesheets);
   /* CSS stylesheet link */
   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "rel")) ||
       dStrAsciiCasecmp(attrbuf, "stylesheet"))
      return;

   /* IMPLIED attributes? */
   if (((attrbuf = a_Html_get_attr(html, tag, tagsize, "type")) &&
        dStrAsciiCasecmp(attrbuf, "text/css")) ||
       ((attrbuf = a_Html_get_attr(html, tag, tagsize, "media")) &&
        !dStriAsciiStr(attrbuf, "screen") && dStrAsciiCasecmp(attrbuf, "all")))
      return;

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "href")) ||
       !(url = a_Html_url_new(html, attrbuf, NULL, 0)))
      return;

   _MSG("  Html_tag_open_link(): addCssUrl %s\n", URL_STR(url));

   html->addCssUrl(url);
   a_Url_free(url);
}

/*
 * Set the Document Base URI
 */
static void Html_tag_open_base(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   DilloUrl *BaseUrl;

   if (html->InFlags & IN_HEAD) {
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "href"))) {
         BaseUrl = a_Html_url_new(html, attrbuf, "", 1);
         if (URL_SCHEME_(BaseUrl)) {
            /* Pass the URL_SpamSafe flag to the new base url */
            a_Url_set_flags(
               BaseUrl, URL_FLAGS(html->base_url) & URL_SpamSafe);
            a_Url_free(html->base_url);
            html->base_url = BaseUrl;
         } else {
            BUG_MSG("<base> URI is relative (it MUST be absolute).");
            a_Url_free(BaseUrl);
         }
      }
   } else {
      BUG_MSG("<base> not inside HEAD section.");
   }
}

static void Html_tag_open_default(DilloHtml *html,const char *tag,int tagsize)
{
   html->styleEngine->inheritBackgroundColor();
}

/*
 * <SPAN>
 */
static void Html_tag_open_span(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;

   html->styleEngine->inheritBackgroundColor();

   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

      html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                        attrbuf);
   }
}

/*
 * html5 sectioning stuff: article aside nav section header footer
 */
static void Html_tag_open_sectioning(DilloHtml *html, const char *tag,
                                     int tagsize)
{
   const char *attrbuf;

   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

      html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                        attrbuf);
   }
}

/*
 * <DIV> (TODO: make a complete implementation)
 */
static void Html_tag_open_div(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_tag_set_align_attr (html, tag, tagsize);
   Html_tag_open_sectioning(html, tag, tagsize);
}

/*
 * Default close for paragraph tags - pop the stack and break.
 */
static void Html_tag_close_par(DilloHtml *html)
{
   HT2TB(html)->addParbreak (9, html->wordStyle ());
}

/*
 * <WBR> "The wbr element represents a line break opportunity."
 */
static void Html_tag_content_wbr(DilloHtml *html, const char *tag, int tagsize)
{
   HT2TB(html)->addBreakOption(html->wordStyle (), true);
}


/*
 * Function index for the open, content, and close functions for each tag
 * (Alphabetically sorted for a binary search).
 * The open and close functions are always called. They are used for style
 * handling and HTML bug reporting.
 * Content creation (e.g. adding new widgets or text) is done in the content
 * function, which is not called in the display:none case.
 * Note: many tags don't need a content function (e.g. <div>, <span>, ...).
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

const TagInfo Tags[] = {
 {"a", B8(011101),'R',2, Html_tag_open_a, NULL, Html_tag_close_a},
 {"abbr", B8(010101),'R',2, Html_tag_open_abbr, NULL, NULL},
 /* acronym 010101 -- obsolete in HTML5 */
 {"address", B8(010110),'R',2,Html_tag_open_default, NULL, Html_tag_close_par},
 {"area", B8(010001),'F',0, Html_tag_open_default, Html_tag_content_area,
                            NULL},
 {"article", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 {"aside", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 {"audio", B8(011101),'R',2, Html_tag_open_audio, NULL, Html_tag_close_media},
 {"b", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"base", B8(100001),'F',0, Html_tag_open_base, NULL, NULL},
 /* basefont 010001 -- obsolete in HTML5 */
 /* bdo 010101 */
 {"big", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"blockquote", B8(011110),'R',2, Html_tag_open_blockquote, NULL,
                                  NULL},
 {"body", B8(011110),'O',1, Html_tag_open_body, NULL, Html_tag_close_body},
 {"br", B8(010001),'F',0, Html_tag_open_default, Html_tag_content_br,
                          NULL},
 {"button", B8(011101),'R',2, Html_tag_open_button,NULL,Html_tag_close_button},
 /* caption */
 {"center", B8(011110),'R',2, Html_tag_open_default, NULL, NULL},
 {"cite", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"code", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 /* col 010010 'F' */
 /* colgroup */
 {"dd", B8(011110),'O',1, Html_tag_open_dd, NULL, NULL},
 {"del", B8(011101),'R',2, Html_tag_open_default, NULL, NULL},
 {"dfn", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"dir", B8(011010),'R',2, Html_tag_open_dir, NULL, Html_tag_close_par},
 /* TODO: complete <div> support! */
 {"div", B8(011110),'R',2, Html_tag_open_div, NULL, NULL},
 {"dl", B8(011010),'R',2, Html_tag_open_dl, NULL, Html_tag_close_par},
 {"dt", B8(010110),'O',1, Html_tag_open_dt, NULL, Html_tag_close_par},
 {"em", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"embed", B8(010001),'F',0, Html_tag_open_embed, Html_tag_content_embed,NULL},
 /* fieldset */
 {"figcaption", B8(011110),'R',2, Html_tag_open_default, NULL, NULL},
 {"figure", B8(011110),'R',2, Html_tag_open_default, NULL, NULL},
 {"font", B8(010101),'R',2, Html_tag_open_font, NULL, NULL},
 {"footer", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 {"form", B8(011110),'R',2, Html_tag_open_form, NULL, Html_tag_close_form},
 {"frame", B8(010010),'F',0, Html_tag_open_frame, Html_tag_content_frame,
                             NULL},
 {"frameset", B8(011110),'R',2, Html_tag_open_default,
                                Html_tag_content_frameset, NULL},
 {"h1", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"h2", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"h3", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"h4", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"h5", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"h6", B8(010110),'R',2, Html_tag_open_h, NULL, NULL},
 {"head", B8(101101),'O',1, Html_tag_open_head, NULL, Html_tag_close_head},
 {"header", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 {"hr", B8(010010),'F',0, Html_tag_open_hr, Html_tag_content_hr,
                          NULL},
 {"html", B8(001110),'O',1, Html_tag_open_html, NULL, Html_tag_close_html},
 {"i", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"iframe", B8(011110),'R',2, Html_tag_open_frame, Html_tag_content_frame,
                              NULL},
 {"img", B8(010001),'F',0, Html_tag_open_img, Html_tag_content_img,
                           NULL},
 {"input", B8(010001),'F',0, Html_tag_open_input, NULL, NULL},
 {"ins", B8(011101),'R',2, Html_tag_open_default, NULL, NULL},
 {"isindex", B8(110001),'F',0, Html_tag_open_isindex, NULL, NULL},
 {"kbd", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 /* label 010101 */
 /* legend 01?? */
 {"li", B8(011110),'O',1, Html_tag_open_li, NULL, Html_tag_close_li},
 {"link", B8(100001),'F',0, Html_tag_open_link, NULL, NULL},
 {"map", B8(011001),'R',2, Html_tag_open_default, Html_tag_content_map,
                           Html_tag_close_map},
 {"mark", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 /* menu 1010 -- TODO: not exactly 1010, it can contain LI and inline */
 {"menu", B8(011010),'R',2, Html_tag_open_menu, NULL, Html_tag_close_par},
 {"meta", B8(110001),'F',0, Html_tag_open_meta, NULL, NULL},
 {"nav", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 /* noframes 1011 -- obsolete in HTML5 */
 /* noscript 1011 */
 {"object", B8(111101),'R',2, Html_tag_open_object, Html_tag_content_object,
                              NULL},
 {"ol", B8(011010),'R',2, Html_tag_open_ol, NULL, NULL},
 {"optgroup", B8(010101),'O',1, Html_tag_open_optgroup, NULL,
                                Html_tag_close_optgroup},
 {"option", B8(010001),'O',0, Html_tag_open_option,NULL,Html_tag_close_option},
 {"p", B8(010110),'O',1, Html_tag_open_p, NULL, NULL},
 /* param 010001 'F' */
 {"pre", B8(010110),'R',2, Html_tag_open_pre, NULL, Html_tag_close_pre},
 {"q", B8(010101),'R',2, Html_tag_open_q, NULL, Html_tag_close_q},
 {"s", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"samp", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"script", B8(111001),'R',2, Html_tag_open_script,NULL,Html_tag_close_script},
 {"section", B8(011110),'R',2, Html_tag_open_sectioning, NULL, NULL},
 {"select", B8(010101),'R',2, Html_tag_open_select,NULL,Html_tag_close_select},
 {"small", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"source", B8(010001),'F',0, Html_tag_open_source, Html_tag_content_source,
                              NULL},
 {"span", B8(010101),'R',2, Html_tag_open_span, NULL, NULL},
 {"strike", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"strong", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"style", B8(100101),'R',2, Html_tag_open_style, NULL, Html_tag_close_style},
 {"sub", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"sup", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"table", B8(011010),'R',5, Html_tag_open_table, Html_tag_content_table,
                             NULL},
 /* tbody */
 {"td", B8(011110),'O',3, Html_tag_open_td, Html_tag_content_td,
                          NULL},
 {"textarea", B8(010101),'R', 2, Html_tag_open_textarea,
                          Html_tag_content_textarea, Html_tag_close_textarea},
 /* tfoot */
 {"th", B8(011110),'O',1, Html_tag_open_th, Html_tag_content_th,
                          NULL},
 /* thead */
 {"title", B8(100101),'R',2, Html_tag_open_title, NULL, Html_tag_close_title},
 {"tr", B8(011010),'O',4, Html_tag_open_tr, Html_tag_content_tr,
                          NULL},
 {"tt", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"u", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"ul", B8(011010),'R',2, Html_tag_open_ul, NULL, NULL},
 {"var", B8(010101),'R',2, Html_tag_open_default, NULL, NULL},
 {"video", B8(011101),'R',2, Html_tag_open_video, NULL, Html_tag_close_media},
 {"wbr", B8(010101),'F',0, Html_tag_open_default, Html_tag_content_wbr, NULL}
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
      if (D_ASCII_TOLOWER(*p1) != *p2)
         return(D_ASCII_TOLOWER(*p1) - *p2);
      ++p1;
      ++p2;
   }
   return !strchr(" >/\n\r\t", *p1);
}

/*
 * Get 'tag' index
 * return -1 if tag is not handled yet
 */
int a_Html_tag_index(const char *tag)
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
    i_P  = a_Html_tag_index("p"),
    i_LI = a_Html_tag_index("li"),
    i_TD = a_Html_tag_index("td"),
    i_TR = a_Html_tag_index("tr"),
    i_TH = a_Html_tag_index("th"),
    i_DD = a_Html_tag_index("dd"),
    i_DT = a_Html_tag_index("dt"),
    i_OPTION = a_Html_tag_index("option");
    // i_THEAD = a_Html_tag_index("thead");
    // i_TFOOT = a_Html_tag_index("tfoot");
    // i_COLGROUP = a_Html_tag_index("colgroup");
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
   /* TODO: TBODY is pending */
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
         BUG_MSG("<%s> is not allowed to contain <%s>. -- closing <%s>.",
                 Tags[oldtag_idx].name, Tags[new_idx].name,
                 Tags[oldtag_idx].name);
      }

      /* Workaround for Apache and its bad HTML directory listings... */
      if ((html->InFlags & IN_PRE) &&
          strcmp(Tags[new_idx].name, "hr") == 0)
         break;
      /* Avoid OPTION closing SELECT */
      if ((html->InFlags & IN_SELECT) &&
          strcmp(Tags[new_idx].name,"option") == 0)
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
      BUG_MSG("The required DOCTYPE declaration is missing. "
              "Handling as HTML4.");

   if (!(html->InFlags & IN_HTML)) {
      tag = "<html>";
      tag_idx = a_Html_tag_index(tag + 1);
      if (tag_idx != new_idx || IsCloseTag) {
         /* implicit open */
         Html_force_push_tag(html, tag_idx);
         _MSG("Open : %*s%s\n", html->stack->size()," ",Tags[tag_idx].name);
         Tags[tag_idx].open (html, tag, strlen(tag));
      }
   }

   if (Tags[new_idx].Flags & 32) {
      /* head element */
      if (!(html->InFlags & IN_HEAD) && html->Num_HEAD == 0) {
         tag = "<head>";
         tag_idx = a_Html_tag_index(tag + 1);
         if (tag_idx != new_idx || IsCloseTag) {
            /* implicit open of the head element */
            Html_force_push_tag(html, tag_idx);
            _MSG("Open : %*s%s\n", html->stack->size()," ",Tags[tag_idx].name);
            Tags[tag_idx].open (html, tag, strlen(tag));
         }
      }

   } else if (Tags[new_idx].Flags & 16) {
      /* body element */
      if (html->InFlags & IN_HEAD) {
         tag = "</head>";
         tag_idx = a_Html_tag_index(tag + 2);
         Html_tag_cleanup_at_close(html, tag_idx);
      }
      tag = "<body>";
      tag_idx = a_Html_tag_index(tag + 1);
      if (tag_idx != new_idx || IsCloseTag) {
         /* implicit open */
         Html_force_push_tag(html, tag_idx);
         _MSG("Open : %*s%s\n", html->stack->size()," ",Tags[tag_idx].name);
         Tags[tag_idx].open (html, tag, strlen(tag));
      }
   }
}

/*
 * Parse attributes that can appear on any tag.
 */
static void Html_parse_common_attrs(DilloHtml *html, char *tag, int tagsize)
{
   const char *attrbuf;
   char lang[3];

   if (tagsize >= 8 &&        /* length of "<t id=i>" */
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "id"))) {
      /* According to the SGML declaration of HTML 4, all NAME values
       * occuring outside entities must be converted to uppercase
       * (this is what "NAMECASE GENERAL YES" says). But the HTML 4
       * spec states in Sec. 7.5.2 that anchor ids are case-sensitive.
       * So we don't do it and hope for better specs in the future ...
       */
      Html_check_name_val(html, attrbuf, "id");

      html->styleEngine->setId(attrbuf);
   }

   if (tagsize >= 11 && (prefs.parse_embedded_css || prefs.load_stylesheets)) {
      /* length of "<t class=i>" or "<t style=i>" */
      attrbuf = a_Html_get_attr(html, tag, tagsize, "class");
      if (attrbuf)
         html->styleEngine->setClass (attrbuf);

      attrbuf = a_Html_get_attr(html, tag, tagsize, "style");
      if (attrbuf)
         html->styleEngine->setStyle (attrbuf);
   }

   /* handle "xml:lang" and "lang" attributes
    * We use only the first two chars of the value to deal with
    * extended language tags (see http://www.rfc-editor.org/rfc/bcp/bcp47.txt)
    */
   memset(lang, 0, sizeof(lang));
   if (tagsize >= 14) {
      /* length of "<t xml:lang=i>" */
      attrbuf = a_Html_get_attr(html, tag, tagsize, "xml:lang");
      if (attrbuf)
         strncpy(lang, attrbuf, 2);
   }
   if (!lang[0] && tagsize >= 10) { /* 'xml:lang' prevails over 'lang' */
      /* length of "<t lang=i>" */
      attrbuf = a_Html_get_attr(html, tag, tagsize, "lang");
      if (attrbuf)
         strncpy(lang, attrbuf, 2);
   }
   if (lang[0])
      html->styleEngine->setNonCssHint(PROPERTY_X_LANG, CSS_TYPE_STRING, lang);
}

/*
 * Warn when encountering elements that are obsolete in HTML5. This list
 * was from the "W3C Candidate Recommendation 6 August 2013".
 */
static void Html_check_html5_obsolete(DilloHtml *html, int ni)
{
   static int indexes[9] = {-1};

   if (indexes[0] == -1) {
      indexes[0] = a_Html_tag_index("dir");
      indexes[1] = a_Html_tag_index("frame");
      indexes[2] = a_Html_tag_index("frameset");
      indexes[3] = a_Html_tag_index("isindex");
      indexes[4] = a_Html_tag_index("strike");
      indexes[5] = a_Html_tag_index("big");
      indexes[6] = a_Html_tag_index("center");
      indexes[7] = a_Html_tag_index("font");
      indexes[8] = a_Html_tag_index("tt");
   }
   for (int i = 0; i < 9; i++) {
      if (indexes[i] == ni) {
         BUG_MSG("<%s> is obsolete in HTML5.", Tags[ni].name);
         break;
      }
   }
}

static void Html_display_block(DilloHtml *html)
{
   //HT2TB(html)->addParbreak (5, html->styleEngine->wordStyle ());
   Html_add_textblock(html, 0);
}

static void Html_display_listitem(DilloHtml *html)
{
   Style *style = html->style ();
   Style *wordStyle = html->wordStyle ();
   Widget **ref_list_item;
   ListItem *list_item;
   int *list_number;
   char buf[16];

   /* Get our parent tag's variables (used as state storage) */
   list_number = &html->stack->getRef(html->stack->size()-2)->list_number;
   ref_list_item = &html->stack->getRef(html->stack->size()-2)->ref_list_item;

   HT2TB(html)->addParbreak (0, wordStyle);

   list_item = new ListItem ((ListItem*)*ref_list_item,prefs.limit_text_width);
   HT2TB(html)->addWidget (list_item, style);
   HT2TB(html)->addParbreak (0, wordStyle);
   *ref_list_item = list_item;
   S_TOP(html)->textblock = html->dw = list_item;

   if (style->listStyleType == LIST_STYLE_TYPE_NONE) {
      // none
   } else if (style->listStyleType >= LIST_STYLE_TYPE_DECIMAL) {
      // ordered
      numtostr((*list_number)++, buf, 16, style->listStyleType);
      list_item->initWithText (buf, wordStyle);
   } else {
      // unordered
      list_item->initWithWidget (new Bullet(), wordStyle);
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
   char *start = tag + 1; /* discard the '<' */
   int IsCloseTag = (*start == '/');

   dReturn_if (html->stop_parser == true);

   ni = a_Html_tag_index(start + IsCloseTag);
   if (ni == -1) {
      /* TODO: doctype parsing is a bit fuzzy, but enough for the time being */
      if (!(html->InFlags & IN_HTML)) {
         if (tagsize > 9 && !dStrnAsciiCasecmp(tag, "<!doctype", 9))
            Html_parse_doctype(html, tag, tagsize);
      }
      /* Ignore unknown tags */
      return;
   }

   if (!IsCloseTag && html->DocType == DT_HTML && html->DocTypeVersion >= 5.0f)
      Html_check_html5_obsolete(html, ni);

   /* Handle HTML, HEAD and BODY. Elements with optional open and close */
   if (!(html->InFlags & IN_BODY) /* && parsing HTML */)
      Html_test_section(html, ni, IsCloseTag);

   /* Tag processing */
   ci = S_TOP(html)->tag_idx;
   switch (IsCloseTag) {
   case 0:
      /* Open function */

      /* Cleanup when opening a block element, or
       * when openning over an element with optional close */
      if (Tags[ni].Flags & 2 || (ci != -1 && Tags[ci].EndTag == 'O'))
         Html_stack_cleanup_at_open(html, ni);

      /* TODO: this is only raising a warning, take some defined action.
       * Note: apache uses IMG inside PRE (we could use its "alt"). */
      if ((html->InFlags & IN_PRE) && Html_tag_pre_excludes(html, ni))
         BUG_MSG("<pre> is not allowed to contain <%s>.", Tags[ni].name);

      /* Make sure these elements don't nest each other */
      if (html->InFlags & (IN_BUTTON | IN_SELECT | IN_TEXTAREA))
         Html_tag_cleanup_nested_inputs(html, ni);

      /* Push the tag into the stack */
      Html_push_tag(html, ni);

      html->startElement (ni);
      _MSG("Open : %*s%s\n", html->stack->size(), " ", Tags[ni].name);

      /* Parse attributes that can appear on any tag */
      Html_parse_common_attrs(html, tag, tagsize);

      /* Call the open function for this tag */
      _MSG("Html_process_tag Open : %s\n", Tags[ni].name);
      Tags[ni].open (html, tag, tagsize);

      if (! S_TOP(html)->display_none) {
         switch (html->style ()->display) {
            case DISPLAY_BLOCK:
               Html_display_block(html);
               break;
            case DISPLAY_LIST_ITEM:
               Html_display_listitem(html);
               break;
            case DISPLAY_NONE:
               S_TOP(html)->display_none = true;
               break;
            case DISPLAY_INLINE:
            case DISPLAY_INLINE_BLOCK: // TODO: implement inline-block
            default:
               break;
         }

         if (Tags[ni].content && ! S_TOP(html)->display_none) {
            Tags[ni].content (html, tag, tagsize);
         }
      }

      if (html->stop_parser)
         break;

      if (S_TOP(html)->parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM) {
         /* don't change anything */
      } else if (S_TOP(html)->parse_mode != DILLO_HTML_PARSE_MODE_PRE &&
          (html->style ()->whiteSpace == WHITE_SPACE_PRE ||
           html->style ()->whiteSpace == WHITE_SPACE_PRE_WRAP)) {
         S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_PRE;
         html->pre_column = 0;
         html->PreFirstChar = true;
      }

      if (html->styleEngine->getId ())
         Html_add_anchor(html, html->styleEngine->getId ());

      /* Request immediate close for elements with forbidden close tag. */
      /* TODO: XHTML always requires close tags. A simple implementation
       * of the commented clause below will make it work. */
      if (/* parsing HTML && */ Tags[ni].EndTag == 'F')
         html->ReqTagClose = true;

      /* Don't break! Open tags may also close themselves */

   default:
      /* Close function */

      /* Test for </x>, ReqTagClose, <x /> and <x/> */
      if (*start == '/' ||                                      /* </x>    */
          html->ReqTagClose ||                                  /* request */
          (tag[tagsize-2] == '/' &&                             /* XML:    */
           (strchr(" \"'", tag[tagsize-3]) ||                   /* [ "']/> */
            (size_t)tagsize == strlen(Tags[ni].name) + 3))) {   /*  <x/>   */

         _MSG("Html_process_tag Close: %s\n", Tags[ni].name);
         Html_tag_cleanup_at_close(html, ni);
         /* This was a close tag */
         html->ReqTagClose = false;
      }
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
         if (!attrname[attr_pos] &&
             (tag[i] == '=' || isspace(tag[i]) || tag[i] == '>')) {
            Found = 1;
            state = SEEK_TOKEN_START;
            --i;
         } else if (!tag[i]) {
            state = SEEK_ATTR_START; // NULL byte is not allowed
         } else {
            if (D_ASCII_TOLOWER(tag[i]) != D_ASCII_TOLOWER(attrname[attr_pos]))
               state = SEEK_ATTR_START;
            attr_pos++;
         }
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
                  int k, n = a_Utf8_encode(isocode, buf);
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
const char *a_Html_get_attr(DilloHtml *html,
                            const char *tag,
                            int tagsize,
                            const char *attrname)
{
   return Html_get_attr2(html, tag, tagsize, attrname,
                         HTML_LeftTrim | HTML_RightTrim | HTML_ParseEntities);
}

/*
 * "a_Html_get_attr with default"
 * Call a_Html_get_attr() and dStrdup() the returned string.
 * If the attribute isn't found a copy of 'def' is returned.
 */
char *a_Html_get_attr_wdef(DilloHtml *html,
                           const char *tag,
                           int tagsize,
                           const char *attrname,
                           const char *def)
{
   const char *attrbuf = a_Html_get_attr(html, tag, tagsize, attrname);

   return attrbuf ? dStrdup(attrbuf) : dStrdup(def);
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
      html->finishParsing(Client->Key);
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
   int token_start, buf_index;

   /* Now, 'buf' and 'bufsize' define a buffer aligned to start at a token
    * boundary. Iterate through tokens until end of buffer is reached. */
   buf_index = 0;
   token_start = buf_index;
   while ((buf_index < bufsize) && !html->stop_parser) {
      /* invariant: buf_index == bufsize || token_start == buf_index */

      if (S_TOP(html)->parse_mode ==
          DILLO_HTML_PARSE_MODE_VERBATIM) {
         /* Non HTML code here, let's skip until closing tag */
         do {
            const char *tag = Tags[S_TOP(html)->tag_idx].name;
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

         if (buf_index == bufsize)
            break;
      }

      if (isspace(buf[buf_index])) {
         /* whitespace: group all available whitespace */
         while (++buf_index < bufsize && isspace(buf[buf_index])) ;
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
            html->CurrOfs = html->Start_Ofs + token_start;

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
                        BUG_MSG("Attribute lacks closing quote.");
                        break;
                     }
                  }
               } else if (ch == '<') {
                  /* unterminated tag detected */
                  p = dStrndup(buf+token_start+1,
                               strcspn(buf+token_start+1, " <\n\r\t"));
                  BUG_MSG("<%s> lacks its closing '>'.", p);
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
         html->CurrOfs = html->Start_Ofs + token_start;

         while (++buf_index < bufsize) {
            buf_index += strcspn(buf + buf_index, " <\n\r\t\f\v");
            if (buf[buf_index] == '<' && (ch = buf[buf_index + 1]) &&
                !isalpha(ch) && !strchr("/!?", ch))
               continue;
            break;
         }
         if (buf_index < bufsize || Eof) {
            /* successfully found end of token */
            ch = buf[buf_index];
            buf[buf_index] = 0;
            Html_process_word(html, buf + token_start,
                              buf_index - token_start);
            buf[buf_index] = ch;
            token_start = buf_index;
         }
      }
   }/*while*/

   HT2TB(html)->flush ();

   return token_start;
}


