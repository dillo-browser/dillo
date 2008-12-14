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

#include "bw.h"         /* for BrowserWindow */
#include "msg.h"
#include "binaryconst.h"
#include "colors.h"

#include "misc.h"
#include "uicmd.hh"
#include "history.h"
#include "nav.h"
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
typedef struct _DilloHtmlClass   DilloHtmlClass;

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
static const char *Html_get_attr2(DilloHtml *html,
                                  const char *tag,
                                  int tagsize,
                                  const char *attrname,
                                  int tag_parsing_flags);
static int Html_write_raw(DilloHtml *html, char *buf, int bufsize, int Eof);
static void Html_load_image(BrowserWindow *bw, DilloUrl *url,
                            DilloImage *image);
static void Html_callback(int Op, CacheClient_t *Client);
static void Html_tag_cleanup_at_close(DilloHtml *html, int TagIdx);

/*-----------------------------------------------------------------------------
 * Local Data
 *---------------------------------------------------------------------------*/
/* The following array of font sizes has to be _strictly_ increasing */
static const int FontSizes[] = {10, 12, 14, 18, 22, 28};
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
 * Collect HTML error strings.
 */
void DilloHtml::bugMessage(const char *format, ... )
{
   va_list argp;

   dStr_sprintfa(bw->page_bugs,
                 "HTML warning: line %d, ",
                 getCurTagLineNumber());
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
         BUG_MSG("URL has %d illegal character%s (%d space%s)\n",
                 n_ic, suffix, n_ic_spc, suffix);
      } else if (n_ic_spc == 0) {
         BUG_MSG("URL has %d illegal character%s (%d in {00-1F, 7F} range)\n",
                 n_ic, suffix, n_ic);
      } else {
         BUG_MSG("URL has %d illegal character%s: "
                 "%d space%s, and %d in {00-1F, 7F} range\n",
                 n_ic, suffix,
                 n_ic_spc, n_ic_spc > 1 ? "s" : "", n_ic-n_ic_spc);
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
 * Add a new image.
 */
static int Html_add_new_linkimage(DilloHtml *html,
                                  DilloUrl **url, DilloImage *image)
{
   DilloLinkImage *li = dNew(DilloLinkImage, 1);
   li->url = *url;
   li->image = image;

   int ni = html->images->size();
   html->images->increase();
   html->images->set(ni, li);
   return ni;
}

/*
 * Set the font at the top of the stack. BImask specifies which
 * attributes in BI should be changed.
 */
void a_Html_set_top_font(DilloHtml *html, const char *name, int size,
                         int BI, int BImask)
{
#if 0
   FontAttrs font_attrs;

   font_attrs = *html->styleEngine->style ()->font;
   if (name)
      font_attrs.name = name;
   if (size)
      font_attrs.size = size;
   if (BImask & 1)
      font_attrs.weight = (BI & 1) ? 700 : 400;
   if (BImask & 2)
      font_attrs.style = (BI & 2) ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL;

   HTML_SET_TOP_ATTR (html, font,
                      Font::create (HT2LT(html), &font_attrs));
#endif
}

/*
 * Evaluates the ALIGN attribute (left|center|right|justify) and
 * sets the style at the top of the stack.
 */
void a_Html_tag_set_align_attr(DilloHtml *html,
                               CssPropertyList *props,        
                               const char *tag, int tagsize)
{
   const char *align;

   if ((align = a_Html_get_attr(html, tag, tagsize, "align"))) {
      TextAlignType textAlignType = TEXT_ALIGN_LEFT;

      if (dStrcasecmp (align, "left") == 0)
         textAlignType = TEXT_ALIGN_LEFT;
      else if (dStrcasecmp (align, "right") == 0)
         textAlignType = TEXT_ALIGN_RIGHT;
      else if (dStrcasecmp (align, "center") == 0)
         textAlignType = TEXT_ALIGN_CENTER;
      else if (dStrcasecmp (align, "justify") == 0)
         textAlignType = TEXT_ALIGN_JUSTIFY;
#if 0
      else if (dStrcasecmp (align, "char") == 0) {
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
      props->set (CssProperty::CSS_PROPERTY_TEXT_ALIGN, textAlignType);
   }
}

/*
 * Evaluates the VALIGN attribute (top|bottom|middle|baseline) and
 * sets the style in style_attrs. Returns true when set.
 */
bool a_Html_tag_set_valign_attr(DilloHtml *html, const char *tag,
                                int tagsize, CssPropertyList *props)
{
   const char *attr;
   VAlignType valign;

   if ((attr = a_Html_get_attr(html, tag, tagsize, "valign"))) {
      if (dStrcasecmp (attr, "top") == 0)
         valign = VALIGN_TOP;
      else if (dStrcasecmp (attr, "bottom") == 0)
         valign = VALIGN_BOTTOM;
      else if (dStrcasecmp (attr, "baseline") == 0)
         valign = VALIGN_BASELINE;
      else
         valign = VALIGN_MIDDLE;

      props->set (CssProperty::CSS_PROPERTY_VERTICAL_ALIGN, valign);
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

   DW2TB(html->dw)->addParbreak (space, html->styleEngine->wordStyle ());
   DW2TB(html->dw)->addWidget (textblock, html->styleEngine->style ());
   DW2TB(html->dw)->addParbreak (space, html->styleEngine->wordStyle ());
   S_TOP(html)->textblock = html->dw = textblock;
   S_TOP(html)->hand_over_break = true;

   /* Handle it when the user clicks on a link */
   html->connectSignals(textblock);
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
DilloHtml::DilloHtml(BrowserWindow *p_bw, const DilloUrl *url,
                     const char *content_type)
{
   /* Init event receiver */
   linkReceiver.html = this;

   /* Init main variables */
   bw = p_bw;
   page_url = a_Url_dup(url);
   base_url = a_Url_dup(url);
   dw = NULL;

   a_Bw_add_doc(p_bw, this);

   /* Init for-parsing variables */
   Buf_Consumed = 0;
   Start_Buf = NULL;
   Start_Ofs = 0;

   _MSG("DilloHtml(): content type: %s\n", content_type);
   this->content_type = dStrdup(content_type);

   /* get charset */
   a_Misc_parse_content_type(content_type, NULL, NULL, &charset);

   stop_parser = false;

   CurrTagOfs = 0;
   OldTagOfs = 0;
   OldTagLine = 1;

   DocType = DT_NONE;    /* assume Tag Soup 0.0!   :-) */
   DocTypeVersion = 0.0f;

   stack = new misc::SimpleVector <DilloHtmlState> (16);
   stack->increase();
   stack->getRef(0)->table_cell_props = NULL;
   stack->getRef(0)->parse_mode = DILLO_HTML_PARSE_MODE_INIT;
   stack->getRef(0)->table_mode = DILLO_HTML_TABLE_MODE_NONE;
   stack->getRef(0)->cell_text_align_set = false;
   stack->getRef(0)->list_type = HTML_LIST_NONE;
   stack->getRef(0)->list_number = 0;
   stack->getRef(0)->tag_idx = -1;               /* MUST not be used */
   stack->getRef(0)->textblock = NULL;
   stack->getRef(0)->table = NULL;
   stack->getRef(0)->ref_list_item = NULL;
   stack->getRef(0)->current_bg_color = prefs.bg_color;
   stack->getRef(0)->hand_over_break = false;

   styleEngine = new StyleEngine (HT2LT (this));

   InFlags = IN_NONE;

   Stash = dStr_new("");
   StashSpace = false;

   pre_column = 0;
   PreFirstChar = false;
   PrevWasCR = false;
   PrevWasSPC = false;
   InVisitedLink = false;
   ReqTagClose = false;
   CloseOneTag = false;
   TagSoup = true;
   NameVal = NULL;

   Num_HTML = Num_HEAD = Num_BODY = Num_TITLE = 0;

   attr_data = dStr_sized_new(1024);

   parse_finished = false;

   /* Init page-handling variables */
   forms = new misc::SimpleVector <DilloHtmlForm*> (1);
   inputs_outside_form = new misc::SimpleVector <DilloHtmlInput*> (1);
   links = new misc::SimpleVector <DilloUrl*> (64);
   images = new misc::SimpleVector <DilloLinkImage*> (16);
   //a_Dw_image_map_list_init(&maps);

   link_color = -1;
   visited_color = -1;

   /* Initialize the main widget */
   initDw();
   /* Hook destructor to the dw delete call */
   dw->setDeleteCallback(Html_free, this);
}

/*
 * Miscelaneous initializations for Dw
 */
void DilloHtml::initDw()
{
   dReturn_if_fail (dw == NULL);

   /* Create the main widget */
   dw = stack->getRef(0)->textblock = new Textblock (prefs.limit_text_width);

   stack->getRef(0)->table_cell_props = NULL;

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

   if (!parse_finished)
      freeParseData();

   a_Bw_remove_doc(bw, this);

   a_Url_free(page_url);
   a_Url_free(base_url);

   for (int i = 0; i < forms->size(); i++)
      a_Html_form_delete (forms->get(i));
   delete(forms);

   for (int i = 0; i < inputs_outside_form->size(); i++)
      a_Html_input_delete(inputs_outside_form->get(i));
   delete(inputs_outside_form);

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
}

/*
 * Return the line number of the tag being processed by the parser.
 * Also update the offsets.
 */
int DilloHtml::getCurTagLineNumber()
{
   int i, ofs, line;
   const char *p = Start_Buf;

   dReturn_val_if_fail(p != NULL, -1);

   ofs = CurrTagOfs;
   line = OldTagLine;
   for (i = OldTagOfs; i < ofs; ++i)
      if (p[i] == '\n')
         ++line;
   OldTagOfs = CurrTagOfs;
   OldTagLine = line;
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

   /* force the close of elements left open (TODO: not for XHTML) */
   while ((si = stack->size() - 1)) {
      if (stack->getRef(si)->tag_idx != -1) {
         Html_tag_cleanup_at_close(this, stack->getRef(si)->tag_idx);
      }
   }
   /* Remove this client from our active list */
   a_Bw_close_client(bw, ClientKey);

   freeParseData();
   parse_finished = true;
}

/*
 * Allocate and insert form information.
 */
int DilloHtml::formNew(DilloHtmlMethod method, const DilloUrl *action,
                       DilloHtmlEnc enc, const char *charset)
{
   DilloHtmlForm *form = a_Html_form_new (this,method,action,enc,charset);
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
   dReturn_if_fail (bw->nav_expecting == FALSE);

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
   if (link == -1) {
      _MSG(" Link  LEAVE  notify...\n");
      a_UIcmd_set_msg(bw, "");
      a_UIcmd_set_pointer_on_link(bw, FALSE);
   } else {
      _MSG(" Link  ENTER  notify...\n");
      Html_set_link_coordinates(html, link, x, y);
      a_UIcmd_set_msg(bw, "%s", URL_STR(html->links->get(link)));
      a_UIcmd_set_pointer_on_link(bw, TRUE);
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
         a_UIcmd_image_popup(
            bw, html->images->get(img)->url, loaded_img, linkurl);
         ret = true;
      } else {
         if (link == -1) {
            a_UIcmd_page_popup(bw, a_History_get_url(NAV_TOP_UIDX(bw)),
                               bw->num_page_bugs != 0,
                               html->unloadedImages());
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
         BUG_MSG("numeric character reference out of range\n");
         isocode = -2;
      }

      if (isocode != -1) {
         if (*s == ';')
            s++;
         else if (prefs.show_extra_warnings)
            BUG_MSG("numeric character reference without trailing ';'\n");
      }

   } else if (isalpha(*s)) {
      /* character entity reference */
      while (*++s && (isalnum(*s) || strchr(":_.-", *s))) ;
      c = *s;
      *s = 0;

      if ((i = Html_entity_search(tok)) == -1) {
         if ((html->DocType == DT_HTML && html->DocTypeVersion == 4.01f) ||
             html->DocType == DT_XHTML)
            BUG_MSG("undefined character entity '%s'\n", tok);
         isocode = -3;
      } else
         isocode = Entities[i].isocode;

      if (c == ';')
         s++;
      else if (prefs.show_extra_warnings)
         BUG_MSG("character entity reference without trailing ';'\n");
   }

   *entsize = s-tok+1;
   dFree(tok);

   if (isocode >= 145 && isocode <= 151) {
      /* TODO: remove this hack. */
      isocode = Html_ms_stupid_quotes_2ucs(isocode);
   } else if (isocode == -1 && prefs.show_extra_warnings)
      BUG_MSG("literal '&'\n");

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
   char *spc;
   int i, offset;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH) {
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
               DW2TB(html->dw)->addText (spc, html->styleEngine->wordStyle ());
               dFree(spc);
               spaceCnt = 0;
            }
            DW2TB(html->dw)->addLinebreak (html->styleEngine->wordStyle ());
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
               BUG_MSG("TAB character inside <PRE>\n");
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
         spc = dStrnfill(spaceCnt, ' ');
         DW2TB(html->dw)->addText (spc, html->styleEngine->wordStyle ());
         dFree(spc);
      }

   } else {
      if (SGML_SPCDEL) {
         /* SGML_SPCDEL ignores white space inmediately after an open tag */
      } else if (!html->PrevWasSPC) {
         DW2TB(html->dw)->addSpace(html->styleEngine->wordStyle ());
         html->PrevWasSPC = true;
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
   char *Pword, ch;
   DilloHtmlParseMode parse_mode = S_TOP(html)->parse_mode;

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

   if (parse_mode == DILLO_HTML_PARSE_MODE_STASH  ||
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
            ch = Pword[i];
            Pword[i] = 0;
            DW2TB(html->dw)->addText(Pword, html->styleEngine->wordStyle ());
            Pword[i] = ch;
            html->pre_column += i - start;
            html->PreFirstChar = false;
         }
      dFree(Pword);

   } else {
      if (!memchr(word,'&', size)) {
         /* No entities */
         DW2TB(html->dw)->addText(word, html->styleEngine->wordStyle ());
      } else {
         /* Collapse white-space entities inside the word (except &nbsp;) */
         Pword = a_Html_parse_entities(html, word, size);
         for (i = 0; Pword[i]; ++i)
            if (strchr("\t\f\n\r", Pword[i]))
               for (j = i; (Pword[j] = Pword[j+1]); ++j) ;
   
         DW2TB(html->dw)->addText(Pword, html->styleEngine->wordStyle ());
         dFree(Pword);
      }
   }

   html->PrevWasSPC = false;
   if (html->InFlags & IN_LI)
      html->WordAfterLI = true;
}

/*
 * Does the tag in tagstr (e.g. "p") match the tag in the tag, tagsize
 * structure, with the initial < skipped over (e.g. "P align=center>")?
 */
static bool Html_match_tag(const char *tagstr, char *tag, int tagsize)
{
   int i;

   for (i = 0; i < tagsize && tagstr[i] != '\0'; i++) {
      if (tolower(tagstr[i]) != tolower(tag[i]))
         return false;
   }
   /* The test for '/' is for xml compatibility: "empty/>" will be matched. */
   if (i < tagsize && (isspace(tag[i]) || tag[i] == '>' || tag[i] == '/'))
      return true;
   return false;
}

/*
 * This function is called after popping the stack, to
 * handle nested DwPage widgets.
 */
static void Html_eventually_pop_dw(DilloHtml *html, bool hand_over_break)
{
   if (html->dw != S_TOP(html)->textblock) {
      if (hand_over_break)
         DW2TB(html->dw)->handOverBreak (html->styleEngine->style ());
      DW2TB(html->dw)->flush ();
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
   if (S_TOP(html)->table_cell_props)
      S_TOP(html)->table_cell_props->ref ();
   html->dw = S_TOP(html)->textblock;
}

/*
 * Push the tag (used to force en element with optional open into the stack)
 * Note: now it's the same as Html_push_tag(), but things may change...
 */
static void Html_force_push_tag(DilloHtml *html, int tag_idx)
{
   html->styleEngine->startElement (tag_idx);
   Html_push_tag(html, tag_idx);
}

/*
 * Pop the top tag in the stack
 */
static void Html_real_pop_tag(DilloHtml *html)
{
   bool hand_over_break;

   html->styleEngine->endElement (S_TOP(html)->tag_idx);
   if (S_TOP(html)->table_cell_props)
      S_TOP(html)->table_cell_props->unref ();
   hand_over_break = S_TOP(html)->hand_over_break;
   html->stack->setSize (html->stack->size() - 1);
   Html_eventually_pop_dw(html, hand_over_break);
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
static void Html_tag_cleanup_at_close(DilloHtml *html, int TagIdx)
{
   int w3c_mode = !prefs.w3c_plus_heuristics;
   int stack_idx, cmp = 1;
   int new_idx = TagIdx;

   if (html->CloseOneTag) {
      Html_real_pop_tag(html);
      html->CloseOneTag = false;
      return;
   }

   /* Look for the candidate tag to close */
   stack_idx = html->stack->size() - 1;
   while (stack_idx &&
          (cmp = (new_idx != html->stack->getRef(stack_idx)->tag_idx)) &&
          ((w3c_mode &&
            Tags[html->stack->getRef(stack_idx)->tag_idx].EndTag == 'O') ||
           ((!w3c_mode &&
            (Tags[html->stack->getRef(stack_idx)->tag_idx].EndTag == 'O')) ||
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
            BUG_MSG("  - forcing close of open tag: <%s>\n",
                    Tags[toptag_idx].name);

         /* Close this and only this tag */
         html->CloseOneTag = true;
         _MSG("Close: %*s%s\n", html->stack->size()," ",Tags[toptag_idx].name);
         Tags[toptag_idx].close (html, toptag_idx);
      }

   } else {
      if (stack_idx == 0) {
         BUG_MSG("unexpected closing tag: </%s>.\n", Tags[new_idx].name);
      } else {
         BUG_MSG("unexpected closing tag: </%s>. -- expected </%s>\n",
                 Tags[new_idx].name,
                 Tags[html->stack->getRef(stack_idx)->tag_idx].name);
      }
   }
}

/*
 * Cleanup (conditional), and Pop the tag (if it matches)
 */
void a_Html_pop_tag(DilloHtml *html, int TagIdx)
{
   Html_tag_cleanup_at_close(html, TagIdx);
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
         BUG_MSG("Garbage after length: %s\n", attr);
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
int32_t a_Html_color_parse(DilloHtml *html,
                           const char *subtag, int32_t default_color)
{
   int err = 1;
   int32_t color = a_Color_parse(subtag, default_color, &err);

   if (err) {
      BUG_MSG("color is not in \"#RRGGBB\" format\n");
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
      BUG_MSG("'%s' value is not of the form "
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
         for (ntag[i++] = ' '; isspace(p[1]); ++p) ;
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
      BUG_MSG("HTML element was already open\n");
   }
}

/*
 * Handle close HTML element
 */
static void Html_tag_close_html(DilloHtml *html, int TagIdx)
{
   /* TODO: may add some checks here */
   if (html->Num_HTML == 1) {
      /* beware of pages with multiple HTML close tags... :-P */
      html->InFlags &= ~IN_HTML;
   }
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Handle open HEAD element
 */
static void Html_tag_open_head(DilloHtml *html, const char *tag, int tagsize)
{
   if (html->InFlags & IN_BODY || html->Num_BODY > 0) {
      BUG_MSG("HEAD element must go before the BODY section\n");
      html->ReqTagClose = true;
      return;
   }

   if (!(html->InFlags & IN_HEAD))
      html->InFlags |= IN_HEAD;
   ++html->Num_HEAD;

   if (html->Num_HEAD > 1) {
      BUG_MSG("HEAD element was already open\n");
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
         BUG_MSG("HEAD section lacks the TITLE element\n");
   
      html->InFlags &= ~IN_HEAD;
   }
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Handle open TITLE
 * calls stash init, where the title string will be stored
 */
static void Html_tag_open_title(DilloHtml *html, const char *tag, int tagsize)
{
   ++html->Num_TITLE;
   a_Html_stash_init(html);
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
      BUG_MSG("the TITLE element must be inside the HEAD section\n");
   }
   a_Html_pop_tag(html, TagIdx);
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
static void Html_tag_close_script(DilloHtml *html, int TagIdx)
{
   /* eventually the stash will be sent to an interpreter for parsing */
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Handle open STYLE
 * store the contents to the stash where (in the future) the style
 * sheet interpreter can get it.
 */
static void Html_tag_open_style(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
}

/*
 * Handle close STYLE
 */
static void Html_tag_close_style(DilloHtml *html, int TagIdx)
{
   html->styleEngine->parse(html->Stash->str, html->Stash->len,
                            0, CSS_ORIGIN_AUTHOR);
   a_Html_pop_tag(html, TagIdx);
}

/*
 * <BODY>
 */
static void Html_tag_open_body(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   Textblock *textblock;
   CssPropertyList props;
   int32_t color;

   if (!(html->InFlags & IN_BODY))
      html->InFlags |= IN_BODY;
   ++html->Num_BODY;

   if (html->Num_BODY > 1) {
      BUG_MSG("BODY element was already open\n");
      return;
   }
   if (html->InFlags & IN_HEAD) {
      /* if we're here, it's bad XHTML, no need to recover */
      BUG_MSG("unclosed HEAD element\n");
   }

   textblock = DW2TB(html->dw);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      color = a_Html_color_parse(html, attrbuf, prefs.bg_color);
      if (color == 0xffffff && !prefs.allow_white_bg)
         color = prefs.bg_color;
      S_TOP(html)->current_bg_color = color;
      props.set (CssProperty::CSS_PROPERTY_BACKGROUND_COLOR, color);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "text"))) {
      color = a_Html_color_parse(html, attrbuf, prefs.text_color);
      props.set (CssProperty::CSS_PROPERTY_COLOR, color);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "link")))
      html->link_color = a_Html_color_parse(html, attrbuf, -1);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "vlink")))
      html->visited_color = a_Html_color_parse(html, attrbuf, -1);

   if (prefs.contrast_visited_color) {
      /* get a color that has a "safe distance" from text, link and bg */
      html->visited_color =
            a_Color_vc(html->visited_color,
                       html->styleEngine->style ()->color->getColor(),
                       html->link_color,
                       S_TOP(html)->current_bg_color);
   }

   html->styleEngine->setNonCssHints (&props);
   html->dw->setStyle (html->styleEngine->style ());

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
   a_Html_pop_tag(html, TagIdx);
}

/*
 * <P>
 * TODO: what's the point between adding the parbreak before and
 *       after the push?
 */
static void Html_tag_open_p(DilloHtml *html, const char *tag, int tagsize)
{
   CssPropertyList props;

   if ((html->InFlags & IN_LI) && !html->WordAfterLI) {
      /* ignore first parbreak after an empty <LI> */
      html->WordAfterLI = true;
   } else {
      DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   }
   a_Html_tag_set_align_attr (html, &props, tag, tagsize);
   html->styleEngine->setNonCssHints (&props);
}

/*
 * <FRAME>, <IFRAME>
 * TODO: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frame (DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   char *src;
   DilloUrl *url;
   Textblock *textblock;
   StyleAttrs style_attrs;
   Style *link_style;
   Widget *bullet;

   textblock = DW2TB(html->dw);

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src")))
      return;

   if (!(url = a_Html_url_new(html, attrbuf, NULL, 0)))
      return;

   src = dStrdup(attrbuf);

   style_attrs = *(html->styleEngine->style ());

   if (a_Capi_get_flags(url) & CAPI_IsCached) { /* visited frame */
      style_attrs.color =
         Color::create (HT2LT(html), html->visited_color);
   } else {                                    /* unvisited frame */
      style_attrs.color = Color::create (HT2LT(html), html->link_color);
   }
   style_attrs.textDecoration |= TEXT_DECORATION_UNDERLINE;
   style_attrs.x_link = Html_set_new_link(html, &url);
   style_attrs.cursor = CURSOR_POINTER;
   link_style = Style::create (HT2LT(html), &style_attrs);

   textblock->addParbreak (5, html->styleEngine->wordStyle ());

   /* The bullet will be assigned the current list style, which should
    * be "disc" by default, but may in very weird pages be different.
    * Anyway, there should be no harm. */
   bullet = new Bullet();
   textblock->addWidget(bullet, html->styleEngine->style ());
   textblock->addSpace(html->styleEngine->wordStyle ());

   if (tolower(tag[1]) == 'i') {
      /* IFRAME usually comes with very long advertising/spying URLS,
       * to not break rendering we will force name="IFRAME" */
      textblock->addText ("IFRAME", link_style);

   } else {
      /* FRAME:
       * If 'name' tag is present use it, if not use 'src' value */
      if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
         textblock->addText (src, link_style);
      } else {
         textblock->addText (attrbuf, link_style);
      }
   }

   textblock->addParbreak (5, html->styleEngine->wordStyle ());

   link_style->unref ();
   dFree(src);
}

/*
 * <FRAMESET>
 * TODO: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frameset (DilloHtml *html,
                                    const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   DW2TB(html->dw)->addText("--FRAME--", html->styleEngine->wordStyle ());
   Html_add_textblock(html, 5);
}

/*
 * <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_open_h(DilloHtml *html, const char *tag, int tagsize)
{
   CssPropertyList props;

   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());

   a_Html_tag_set_align_attr (html, &props, tag, tagsize);
   html->styleEngine->setNonCssHints (&props);

   /* First finalize unclosed H tags (we test if already named anyway) */
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   a_Menu_pagemarks_add(html->bw, DW2TB(html->dw),
                        html->styleEngine->style (), (tag[2] - '0'));
   a_Html_stash_init(html);
   S_TOP(html)->parse_mode =
      DILLO_HTML_PARSE_MODE_STASH_AND_BODY;
}

/*
 * Handle close: <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_close_h(DilloHtml *html, int TagIdx)
{
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   a_Html_pop_tag(html, TagIdx);
}

/*
 * <BIG> | <SMALL>
 */
static void Html_tag_open_big_small(DilloHtml *html,
                                    const char *tag, int tagsize)
{
}

static void Html_tag_open_span(DilloHtml *html,
                               const char *tag, int tagsize)
{
   html->styleEngine->inheritBackgroundColor();
}


/*
 * <BR>
 */
static void Html_tag_open_br(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addLinebreak (html->styleEngine->wordStyle ());
}

/*
 * <FONT>
 */
static void Html_tag_open_font(DilloHtml *html, const char *tag, int tagsize)
{
   /*Font font;*/
   const char *attrbuf;
   int32_t color;
   CssPropertyList props;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "color"))) {
      if (prefs.contrast_visited_color && html->InVisitedLink) {
         color = html->visited_color;
      } else { 
         /* use the tag-specified color */
         color = a_Html_color_parse(html, attrbuf, -1);
      }
      if (color != -1)
         props.set (CssProperty::CSS_PROPERTY_COLOR, color);
   }

// \todo reenable font face handling when font selection is implemented
//   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "face")))
//      props.set (CssProperty::CSS_PROPERTY_FONT_FAMILY, attrbuf);

   html->styleEngine->setNonCssHints (&props);
}

/*
 * <ABBR>
 */
static void Html_tag_open_abbr(DilloHtml *html, const char *tag, int tagsize)
{
// DwTooltip *tooltip;
// const char *attrbuf;
//
// if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {
//    tooltip = a_Dw_tooltip_new_no_ref(attrbuf);
//    HTML_SET_TOP_ATTR(html, x_tooltip, tooltip);
// }
}

/*
 * <B>
 */
static void Html_tag_open_b(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <STRONG>
 */
static void Html_tag_open_strong(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <I>
 */
static void Html_tag_open_i(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <EM>
 */
static void Html_tag_open_em(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <CITE>
 */
static void Html_tag_open_cite(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <CENTER>
 */
static void Html_tag_open_center(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (0, html->styleEngine->wordStyle ());
}

/*
 * <ADDRESS>
 */
static void Html_tag_open_address(DilloHtml *html,
                                  const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
}

/*
 * <TT>
 */
static void Html_tag_open_tt(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * Read image-associated tag attributes,
 * create new image and add it to the html page (if add is TRUE).
 */
DilloImage *a_Html_add_new_image(DilloHtml *html, const char *tag,
                                 int tagsize, DilloUrl *url,
                                 bool add)
{
   const int MAX_W = 6000, MAX_H = 6000;

   DilloImage *Image;
   char *width_ptr, *height_ptr, *alt_ptr;
   const char *attrbuf;
   Length l_w, l_h;
   int space, border, w = 0, h = 0;
   bool load_now;
   CssPropertyList props;

// if (prefs.show_tooltip &&
//     (attrbuf = a_Html_get_attr(html, tag, tagsize, "title")))
//    style_attrs->x_tooltip = a_Dw_tooltip_new_no_ref(attrbuf);

   alt_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "alt", NULL);
   if ((!alt_ptr || !*alt_ptr) && !a_UIcmd_get_images_enabled(html->bw)) {
      dFree(alt_ptr);
      alt_ptr = dStrdup("[IMG]"); // Place holder for img_off mode
   }
   width_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "width", NULL);
   height_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "height", NULL);
   // Check for malicious values
   // TODO: the same for percentage and relative lengths.
   if (width_ptr) {
      l_w = a_Html_parse_length (html, width_ptr);
      w = (int) CSS_LENGTH_TYPE(l_w) == CSS_LENGTH_TYPE_PX ? CSS_LENGTH_VALUE(l_w) : 0;
   }
   if (height_ptr) {
      l_h = a_Html_parse_length (html, height_ptr);
      h = (int) CSS_LENGTH_TYPE(l_h) == CSS_LENGTH_TYPE_PX ? CSS_LENGTH_VALUE(l_h) : 0;
   }
   if (w < 0 || h < 0 || abs(w*h) > MAX_W * MAX_H) {
      dFree(width_ptr);
      dFree(height_ptr);
      width_ptr = height_ptr = NULL;
      MSG("a_Html_add_new_image: suspicious image size request %dx%d\n", w, h);
   } else {
      if (width_ptr)
         props.set (CssProperty::CSS_PROPERTY_WIDTH, l_w);
      if (height_ptr)
         props.set (CssProperty::CSS_PROPERTY_HEIGHT, l_h);
   }

   /* TODO: we should scale the image respecting its ratio.
    *       As the image size is not known at this time, maybe a flag
    *       can be set to scale it later.
   if ((width_ptr && !height_ptr) || (height_ptr && !width_ptr))
      [...]
   */

   /* Spacing to the left and right */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "hspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0) {
         space = CSS_CREATE_LENGTH(space, CSS_LENGTH_TYPE_PX);
         props.set (CssProperty::CSS_PROPERTY_MARGIN_LEFT, space);
         props.set (CssProperty::CSS_PROPERTY_MARGIN_RIGHT, space);
      }
   }

   /* Spacing at the top and bottom */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "vspace"))) {
      space = strtol(attrbuf, NULL, 10);
      if (space > 0) {
         space = CSS_CREATE_LENGTH(space, CSS_LENGTH_TYPE_PX);
         props.set (CssProperty::CSS_PROPERTY_MARGIN_TOP, space);
         props.set (CssProperty::CSS_PROPERTY_MARGIN_BOTTOM, space);
      }
   }

   /* Border */
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "border"))) {
      border = strtol(attrbuf, NULL, 10);
      if (border >= 0) {
         border = CSS_CREATE_LENGTH(border, CSS_LENGTH_TYPE_PX);
         props.set (CssProperty::CSS_PROPERTY_BORDER_TOP_WIDTH, border);
         props.set (CssProperty::CSS_PROPERTY_BORDER_BOTTOM_WIDTH, border);
         props.set (CssProperty::CSS_PROPERTY_BORDER_LEFT_WIDTH, border);
         props.set (CssProperty::CSS_PROPERTY_BORDER_RIGHT_WIDTH, border);
      }
   }

   /* x_img is an index to a list of {url,image} pairs.
    * We know Html_add_new_linkimage() will use size() as its next index */
   props.set (CssProperty::PROPERTY_X_IMG, html->images->size());

   html->styleEngine->setNonCssHints(&props);

   /* Add a new image widget to this page */
   Image = a_Image_new(0, 0, alt_ptr, S_TOP(html)->current_bg_color);
   if (add)
      DW2TB(html->dw)->addWidget((Widget*)Image->dw, html->styleEngine->style());

   load_now = a_UIcmd_get_images_enabled(html->bw) ||
              (a_Capi_get_flags(url) & CAPI_IsCached);
   Html_add_new_linkimage(html, &url, load_now ? NULL : Image);
   if (load_now)
      Html_load_image(html->bw, url, Image);

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
   const char *attrbuf;

   /* This avoids loading images. Useful for viewing suspicious HTML email. */
   if (URL_FLAGS(html->base_url) & URL_SpamSafe)
      return;

   if (!(attrbuf = a_Html_get_attr(html, tag, tagsize, "src")) ||
       !(url = a_Html_url_new(html, attrbuf, NULL, 0)))
      return;

   textblock = DW2TB(html->dw);

   usemap_url = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "usemap")))
      /* TODO: usemap URLs outside of the document are not used. */
      usemap_url = a_Html_url_new(html, attrbuf, NULL, 0);

   Image = a_Html_add_new_image(html, tag, tagsize, url, true);

   /* Image maps */
   if (a_Html_get_attr(html, tag, tagsize, "ismap")) {
      ((::dw::Image*)Image->dw)->setIsMap();
      _MSG("  Html_tag_open_img: server-side map (ISMAP)\n");
   } else if (html->styleEngine->style ()->x_link != -1 &&
              usemap_url == NULL) {
      /* For simple links, we have to suppress the "image_pressed" signal.
       * This is overridden for USEMAP images. */
//    a_Dw_widget_set_button_sensitive (IM2DW(Image->dw), FALSE);
   }

   if (usemap_url) {
      ((::dw::Image*)Image->dw)->setUseMap(&html->maps,
                            new ::object::String(usemap_url->url_string->str));
      a_Url_free (usemap_url);
   }
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
      BUG_MSG("nested <map>\n");
   } else {
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
         hash_name = dStrconcat("#", attrbuf, NULL);
         url = a_Html_url_new(html, hash_name, NULL, 0);
         html->maps.startNewMap(new ::object::String(url->url_string->str));
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
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Read coords in a string, returning a vector of ints.
 */
static
misc::SimpleVector<int> *Html_read_coords(DilloHtml *html, const char *str)
{
   int i, coord;
   const char *tail = str;
   char *newtail = NULL;
   misc::SimpleVector<int> *coords = new misc::SimpleVector<int> (4);

   i = 0;
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
         BUG_MSG("usemap coords MUST be separated by commas.\n");
      }
      tail = newtail + 1;
   }

   return coords;
}

/*
 * <AREA>
 */
static void Html_tag_open_area(DilloHtml *html, const char *tag, int tagsize)
{
   enum types {UNKNOWN, RECTANGLE, CIRCLE, POLYGON, BACKGROUND};
   types type;
   misc::SimpleVector<int> *coords = NULL;
   DilloUrl* url;
   const char *attrbuf;
   int link = -1;
   Shape *shape = NULL;
  
   if (!(html->InFlags & IN_MAP)) {
      BUG_MSG("<area> element not inside <map>\n");
      return;
   }
   attrbuf = a_Html_get_attr(html, tag, tagsize, "shape");

   if (!attrbuf || !*attrbuf || !dStrcasecmp(attrbuf, "rect")) {
      /* the default shape is a rectangle */
      type = RECTANGLE;
   } else if (dStrcasecmp(attrbuf, "default") == 0) {
      /* "default" is the background */
      type = BACKGROUND;
   } else if (dStrcasecmp(attrbuf, "circle") == 0) {
      type = CIRCLE;
   } else if (dStrncasecmp(attrbuf, "poly", 4) == 0) {
      type = POLYGON;
   } else {
      BUG_MSG("<area> unknown shape: \"%s\"\n", attrbuf);
      type = UNKNOWN;
   }
   if (type == RECTANGLE || type == CIRCLE || type == POLYGON) {
      /* TODO: add support for coords in % */
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "coords"))) {
         coords = Html_read_coords(html, attrbuf);

         if (type == RECTANGLE) {
            if (coords->size() != 4)
               BUG_MSG("<area> rectangle must have four coordinate values\n");
            if (coords->size() >= 4)
               shape = new Rectangle(coords->get(0),
                                     coords->get(1),
                                     coords->get(2) - coords->get(0),
                                     coords->get(3) - coords->get(1));
         } else if (type == CIRCLE) {
            if (coords->size() != 3)
               BUG_MSG("<area> circle must have three coordinate values\n");
            if (coords->size() >= 3)
               shape = new Circle(coords->get(0), coords->get(1),
                                  coords->get(2));
         } else if (type == POLYGON) {
            Polygon *poly;
            int i;
            if (coords->size() % 2)
               BUG_MSG("<area> polygon with odd number of coordinates\n");
            shape = poly = new Polygon();
            for (i = 0; i < (coords->size() / 2); i++)
               poly->addPoint(coords->get(2*i), coords->get(2*i + 1));
            if (i) {
               /* be sure to close it */
               poly->addPoint(coords->get(0), coords->get(1));
            }
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
   StyleAttrs style_attrs;
   Style *style;
   DilloUrl *url, *base_url = NULL;
   const char *attrbuf;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "codebase"))) {
      base_url = a_Html_url_new(html, attrbuf, NULL, 0);
   }
   
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "data"))) {
      url = a_Html_url_new(html, attrbuf,
                           URL_STR(base_url), (base_url != NULL));
      dReturn_if_fail ( url != NULL );

      style_attrs = *html->styleEngine->style ();

      if (a_Capi_get_flags(url) & CAPI_IsCached) {
         style_attrs.color = Color::create (
            HT2LT(html),
            html->visited_color
/*
            a_Color_vc(html->visited_color,
                       html->styleEngine->style()->color->getColor(),
                       html->link_color,
                       html->styleEngine->style()->backgroundColor->getColor()
                      );
*/
            );
      } else {
         style_attrs.color = Color::create (HT2LT(html), html->link_color);
      }

      style_attrs.textDecoration |= TEXT_DECORATION_UNDERLINE;
      style_attrs.x_link = Html_set_new_link(html, &url);
      style_attrs.cursor = CURSOR_POINTER;

      style = Style::create (HT2LT(html), &style_attrs);
      DW2TB(html->dw)->addText("[OBJECT]", style);
      style->unref ();
   }
   a_Url_free(base_url);
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
         BUG_MSG("link depends on javascript()\n");
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
   if (!DW2TB(html->dw)->addAnchor (name, html->styleEngine->style ()))
      BUG_MSG("Anchor names must be unique within the document\n");
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
   CssPropertyList props;
   const char *attrbuf;

   /* TODO: add support for MAP with A HREF */
   if (html->InFlags & IN_MAP)
      Html_tag_open_area(html, tag, tagsize);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "href"))) {
      /* if it's a javascript link, extract the reference. */
      if (tolower(attrbuf[0]) == 'j')
         attrbuf = Html_get_javascript_link(html);

      url = a_Html_url_new(html, attrbuf, NULL, 0);
      dReturn_if_fail ( url != NULL );

      if (a_Capi_get_flags(url) & CAPI_IsCached) {
         html->InVisitedLink = true;
         html->styleEngine->setPseudoVisited ();
         if (html->visited_color != -1)
            props.set (CssProperty::CSS_PROPERTY_COLOR, html->visited_color);
      } else {
         html->styleEngine->setPseudoLink ();
         if (html->link_color != -1)
            props.set (CssProperty::CSS_PROPERTY_COLOR, html->link_color);
      }

      props.set (CssProperty::PROPERTY_X_LINK, Html_set_new_link(html, &url));

      html->styleEngine->setNonCssHints (&props);
   }

   html->styleEngine->inheritBackgroundColor ();

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name"))) {
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
   html->InVisitedLink = false;
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Insert underlined text in the page.
 */
static void Html_tag_open_u(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * Insert strike-through text. Used by <S>, <STRIKE> and <DEL>.
 */
static void Html_tag_open_strike(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <BLOCKQUOTE>
 */
static void Html_tag_open_blockquote(DilloHtml *html, 
                                     const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   Html_add_textblock(html, 9);
}

/*
 * Handle the <UL> tag.
 */
static void Html_tag_open_ul(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   ListStyleType list_style_type;

   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   Html_add_textblock(html, 9);

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "type"))) {
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
         switch (html->styleEngine->style ()->listStyleType) {
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
 * Handle the <DIR> or <MENU> tag.
 * (Deprecated and almost the same as <UL>)
 */
static void Html_tag_open_dir(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());

   S_TOP(html)->list_type = HTML_LIST_UNORDERED;
   S_TOP(html)->list_number = 0;
   S_TOP(html)->ref_list_item = NULL;

   if (prefs.show_extra_warnings)
      BUG_MSG("Obsolete list type; use <UL> instead\n");
}

/*
 * Handle the <MENU> tag.
 */
static void Html_tag_open_menu(DilloHtml *html, const char *tag, int tagsize)
{
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
      CssPropertyList props;
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

      props.set (CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE, listStyleType);
      html->styleEngine->setNonCssHints (&props);
   }

   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   Html_add_textblock(html, 9);

   S_TOP(html)->list_type = HTML_LIST_ORDERED;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "start")) &&
       (n = (int) strtol(attrbuf, NULL, 10)) < 0) {
      BUG_MSG( "illegal '-' character in START attribute; Starting from 0\n");
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

   html->InFlags |= IN_LI;
   html->WordAfterLI = false;

   /* Get our parent tag's variables (used as state storage) */
   list_number = &html->stack->getRef(html->stack->size()-2)->list_number;
   ref_list_item = &html->stack->getRef(html->stack->size()-2)->ref_list_item;

   /* set the item style */
   word_style = html->styleEngine->wordStyle ();
   style_attrs = *word_style;
 //style_attrs.backgroundColor = Color::createShaded (HT2LT(html), 0xffff40);
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
      if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "value")) &&
          (*list_number = strtol(attrbuf, NULL, 10)) < 0) {
         BUG_MSG("illegal negative LIST VALUE attribute; Starting from 0\n");
         *list_number = 0;
      }
      numtostr((*list_number)++, buf, 16,
               html->styleEngine->style ()->listStyleType);
      list_item->initWithText (dStrdup(buf), word_style);
      list_item->addSpace (word_style);
      html->PrevWasSPC = true;
      break;
   case HTML_LIST_NONE:
      BUG_MSG("<li> outside <ul> or <ol>\n");
   default:
      list_item->initWithWidget (new Bullet(), word_style);
      list_item->addSpace (word_style);
      break;
   }
}

/*
 * Close <LI>
 */
static void Html_tag_close_li(DilloHtml *html, int TagIdx)
{
   html->InFlags &= ~IN_LI;
   html->WordAfterLI = false;
   ((ListItem *)html->dw)->flush ();
   a_Html_pop_tag(html, TagIdx);
}

/*
 * <HR>
 */
static void Html_tag_open_hr(DilloHtml *html, const char *tag, int tagsize)
{
   Widget *hruler;
   CssPropertyList props;
   char *width_ptr;
   const char *attrbuf;
   int32_t size = 0;
  
   width_ptr = a_Html_get_attr_wdef(html, tag, tagsize, "width", NULL);
   if (width_ptr) {
      props.set (CssProperty::CSS_PROPERTY_WIDTH, 
         a_Html_parse_length (html, width_ptr));
      dFree(width_ptr);
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "size")))
      size = strtol(attrbuf, NULL, 10);
 
   a_Html_tag_set_align_attr(html, &props, tag, tagsize); 
  
   /* TODO: evaluate attribute */
   if (a_Html_get_attr(html, tag, tagsize, "noshade")) {
      props.set (CssProperty::CSS_PROPERTY_BORDER_TOP_STYLE, BORDER_SOLID);
      props.set (CssProperty::CSS_PROPERTY_BORDER_BOTTOM_STYLE, BORDER_SOLID);
      props.set (CssProperty::CSS_PROPERTY_BORDER_LEFT_STYLE, BORDER_SOLID);
      props.set (CssProperty::CSS_PROPERTY_BORDER_RIGHT_STYLE, BORDER_SOLID);

      if (size <= 0)
         size = 1;
   }
 
   if (size > 0) { 
      CssLength size_top = CSS_CREATE_LENGTH ((size + 1) / 2, CSS_LENGTH_TYPE_PX);
      CssLength size_bottom = CSS_CREATE_LENGTH (size / 2, CSS_LENGTH_TYPE_PX);
      props.set (CssProperty::CSS_PROPERTY_BORDER_TOP_WIDTH, size_top);
      props.set (CssProperty::CSS_PROPERTY_BORDER_LEFT_WIDTH, size_top);
      props.set (CssProperty::CSS_PROPERTY_BORDER_BOTTOM_WIDTH, size_bottom);
      props.set (CssProperty::CSS_PROPERTY_BORDER_RIGHT_WIDTH, size_bottom);
   }

   DW2TB(html->dw)->addParbreak (5, html->styleEngine->wordStyle ());

   html->styleEngine->setNonCssHints (&props);
   hruler = new Ruler();
   hruler->setStyle (html->styleEngine->style ());
   DW2TB(html->dw)->addWidget (hruler, html->styleEngine->style ());
   DW2TB(html->dw)->addParbreak (5, html->styleEngine->wordStyle ());
}

/*
 * <DL>
 */
static void Html_tag_open_dl(DilloHtml *html, const char *tag, int tagsize)
{
   /* may want to actually do some stuff here. */
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
}

/*
 * <DT>
 */
static void Html_tag_open_dt(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   a_Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <DD>
 */
static void Html_tag_open_dd(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   Html_add_textblock(html, 9);
}

/*
 * <PRE>
 */
static void Html_tag_open_pre(DilloHtml *html, const char *tag, int tagsize)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());

   /* Is the placement of this statement right? */
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_PRE;
   html->pre_column = 0;
   html->PreFirstChar = true;
   html->InFlags |= IN_PRE;
}

/*
 * Custom close for <PRE>
 */
static void Html_tag_close_pre(DilloHtml *html, int TagIdx)
{
   html->InFlags &= ~IN_PRE;
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   a_Html_pop_tag(html, TagIdx);
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
         ei_set[i] = a_Html_tag_index(es_set[i]);

   for (i = 0; ei_set[i]; ++i)
      if (tag_idx == ei_set[i])
         return 1;
   return 0;
}

/*
 * Handle <META>
 * We do not support http-equiv=refresh because it's non standard,
 * (the HTML 4.01 SPEC recommends explicitly to avoid it), and it
 * can be easily abused!
 *
 * More info at:
 *   http://lists.w3.org/Archives/Public/www-html/2000Feb/thread.html#msg232
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

   const char *equiv, *content;
   char delay_str[64];
   Dstr *ds_msg;
   int delay;

   /* only valid inside HEAD */
   if (!(html->InFlags & IN_HEAD)) {
      BUG_MSG("META elements must be inside the HEAD section\n");
      return;
   }

   if ((equiv = a_Html_get_attr(html, tag, tagsize, "http-equiv"))) {
      if (!dStrcasecmp(equiv, "refresh") &&
       (content = a_Html_get_attr(html, tag, tagsize, "content"))) {

         /* Get delay, if present, and make a message with it */
         if ((delay = strtol(content, NULL, 0)))
            snprintf(delay_str, 64, " after %d second%s.",
                       delay, (delay > 1) ? "s" : "");
         else
            sprintf(delay_str, ".");

         /* Skip to anything after "URL=" */
         while (*content && *(content++) != '=') ;

         /* Send a custom HTML message.
          * TODO: This is a hairy hack,
          *       It'd be much better to build a widget. */
         ds_msg = dStr_sized_new(256);
         dStr_sprintf(ds_msg, meta_template, content, delay_str);
         {
            int SaveFlags = html->InFlags;
            html->InFlags = IN_BODY;
            html->TagSoup = false;
            Html_write_raw(html, ds_msg->str, ds_msg->len, 0);
            html->TagSoup = true;
            html->InFlags = SaveFlags;
         }
         dStr_free(ds_msg, 1);

      } else if (!dStrcasecmp(equiv, "content-type") &&
                 (content = a_Html_get_attr(html, tag, tagsize, "content"))) {
         if (a_Misc_content_type_cmp(html->content_type, content)) {
            const bool_t force = FALSE;
            const char *new_content =
               a_Capi_set_content_type(html->page_url, content, force);
            /* Cannot ask cache whether the content type was changed, as
             * this code in another bw might have already changed it for us.
             */
            if (a_Misc_content_type_cmp(html->content_type, new_content)) {
               a_Nav_repush(html->bw);
               html->stop_parser = true;
            }
         }
      }   
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
            BUG_MSG("base URI is relative (it MUST be absolute)\n");
            a_Url_free(BaseUrl);
         }
      }
   } else {
      BUG_MSG("the BASE element must appear in the HEAD section\n");
   }
}

/*
 * <CODE>
 */
static void Html_tag_open_code(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <DFN>
 */
static void Html_tag_open_dfn(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_set_top_font(html, NULL, 0, 2, 3);
}

/*
 * <KBD>
 */
static void Html_tag_open_kbd(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <SAMP>
 */
static void Html_tag_open_samp(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <VAR>
 */
static void Html_tag_open_var(DilloHtml *html, const char *tag, int tagsize)
{
   a_Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <SUB>
 */
static void Html_tag_open_sub(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <SUP>
 */
static void Html_tag_open_sup(DilloHtml *html, const char *tag, int tagsize)
{
}

/*
 * <DIV> (TODO: make a complete implementation)
 */
static void Html_tag_open_div(DilloHtml *html, const char *tag, int tagsize)
{
   CssPropertyList props;

   a_Html_tag_set_align_attr (html, &props, tag, tagsize);
   html->styleEngine->setNonCssHints (&props);
   Html_add_textblock(html, 0);
}

/*
 * </DIV>, also used for </TABLE> and </CENTER>
 */
static void Html_tag_close_div(DilloHtml *html, int TagIdx)
{
   DW2TB(html->dw)->addParbreak (0, html->styleEngine->wordStyle ());
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Default close for most tags - just pop the stack.
 */
static void Html_tag_close_default(DilloHtml *html, int TagIdx)
{
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Default close for paragraph tags - pop the stack and break.
 */
static void Html_tag_close_par(DilloHtml *html, int TagIdx)
{
   DW2TB(html->dw)->addParbreak (9, html->styleEngine->wordStyle ());
   a_Html_pop_tag(html, TagIdx);
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
 {"button", B8(011101),'R',2, Html_tag_open_button, Html_tag_close_button},
 /* caption */
 {"center", B8(011110),'R',2, Html_tag_open_center, Html_tag_close_div},
 {"cite", B8(010101),'R',2, Html_tag_open_cite, Html_tag_close_default},
 {"code", B8(010101),'R',2, Html_tag_open_code, Html_tag_close_default},
 /* col 010010 'F' */
 /* colgroup */
 {"dd", B8(011110),'O',1, Html_tag_open_dd, Html_tag_close_par},
 {"del", B8(011101),'R',2, Html_tag_open_strike, Html_tag_close_default},
 {"dfn", B8(010101),'R',2, Html_tag_open_dfn, Html_tag_close_default},
 {"dir", B8(011010),'R',2, Html_tag_open_dir, Html_tag_close_par},
 /* TODO: complete <div> support! */
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
 /* menu 1010 -- TODO: not exactly 1010, it can contain LI and inline */
 {"menu", B8(011010),'R',2, Html_tag_open_menu, Html_tag_close_par},
 {"meta", B8(100001),'F',0, Html_tag_open_meta, Html_tag_close_default},
 /* noframes 1011 */
 /* noscript 1011 */
 {"object", B8(111101),'R',2, Html_tag_open_object, Html_tag_close_default},
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
 {"select", B8(010101),'R',2, Html_tag_open_select, Html_tag_close_select},
 {"small", B8(010101),'R',2, Html_tag_open_big_small, Html_tag_close_default},
 {"span", B8(010101),'R',2, Html_tag_open_span, Html_tag_close_default},
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
         BUG_MSG("<%s> is not allowed to contain <%s>. -- closing <%s>\n",
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
      BUG_MSG("the required DOCTYPE declaration is missing (or invalid)\n");

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
      if (!(html->InFlags & IN_HEAD)) {
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

   ni = a_Html_tag_index(start + IsCloseTag);
   if (ni == -1) {
      /* TODO: doctype parsing is a bit fuzzy, but enough for the time being */
      if (!(html->InFlags & IN_HTML)) {
         if (tagsize > 9 && !dStrncasecmp(tag, "<!doctype", 9))
            Html_parse_doctype(html, tag, tagsize);
      }
      /* Ignore unknown tags */
      return;
   }

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
      if ((html->InFlags & IN_PRE) && Html_tag_pre_excludes(ni))
         BUG_MSG("<pre> is not allowed to contain <%s>\n", Tags[ni].name);

      /* Push the tag into the stack */
      Html_push_tag(html, ni);

      html->styleEngine->startElement (ni);
      _MSG("Open : %*s%s\n", html->stack->size(), " ", Tags[ni].name);

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
         if (attrbuf)
            html->styleEngine->setId (attrbuf);

         Html_check_name_val(html, attrbuf, "id");
         /* We compare the "id" value with the url-decoded "name" value */
         if (!html->NameVal || strcmp(html->NameVal, attrbuf)) {
            if (html->NameVal)
               BUG_MSG("'id' and 'name' attribute of <a> tag differ\n");
            Html_add_anchor(html, attrbuf);
         }
      }

      /* Reset NameVal */
      if (html->NameVal) {
         dFree(html->NameVal);
         html->NameVal = NULL;
      }

      if (tagsize >= 10) {       /* length of "<t class=i>" */
          attrbuf = Html_get_attr2(html, tag, tagsize, "class",
                                   HTML_LeftTrim | HTML_RightTrim);
          if (attrbuf)
            html->styleEngine->setClass (attrbuf);
      }

      if (tagsize >= 11) {       /* length of "<t style=i>" */
          attrbuf = Html_get_attr2(html, tag, tagsize, "style",
                                   HTML_LeftTrim | HTML_RightTrim);
          if (attrbuf)
            html->styleEngine->setStyle (attrbuf);
      }

      /* Call the open function for this tag */
      Tags[ni].open (html, tag, tagsize);
      if (html->stop_parser)
         break;

      /* Request inmediate close for elements with forbidden close tag. */
      /* TODO: XHTML always requires close tags. A simple implementation
       * of the commented clause below will make it work. */
      if  (/* parsing HTML && */ Tags[ni].EndTag == 'F')
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
   Textblock *textblock;
   int token_start, buf_index;

   dReturn_val_if_fail ((textblock = DW2TB(html->dw)) != NULL, 0);

   /* Now, 'buf' and 'bufsize' define a buffer aligned to start at a token
    * boundary. Iterate through tokens until end of buffer is reached. */
   buf_index = 0;
   token_start = buf_index;
   while ((buf_index < bufsize) && (html->stop_parser == false)) {
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
                        BUG_MSG("attribute lacks closing quote\n");
                        break;
                     }
                  }
               } else if (ch == '<') {
                  /* unterminated tag detected */
                  p = dStrndup(buf+token_start+1,
                               strcspn(buf+token_start+1, " <"));
                  BUG_MSG("<%s> element lacks its closing '>'\n", p);
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
            ch = buf[buf_index];
            buf[buf_index] = 0;
            Html_process_word(html, buf + token_start,
                              buf_index - token_start);
            buf[buf_index] = ch;
            token_start = buf_index;
         }
      }
   }/*while*/

   textblock->flush ();

   return token_start;
}


