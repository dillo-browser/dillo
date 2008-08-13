/*
 * File: form.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "form.hh"
#include "html_common.hh"

#include <errno.h>
#include <iconv.h>

#include "lout/misc.hh"
#include "dw/core.hh"
#include "dw/textblock.hh"

#define DEBUG_LEVEL 10
#include "debug.h"

#include "misc.h"
#include "msg.h"
#include "prefs.h"
#include "nav.h"
#include "uicmd.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;

/*
 * Forward declarations 
 */

class DilloHtmlReceiver;
class DilloHtmlSelect;
class DilloHtmlOption;

static dw::core::ui::Embed *Html_input_image(DilloHtml *html,
                                             const char *tag, int tagsize);

static void Html_option_finish(DilloHtml *html);

/*
 * Typedefs 
 */

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

/*
 * Class declarations  
 */

class DilloHtmlForm {
   friend class DilloHtmlReceiver;
   friend class DilloHtmlInput;

   DilloHtml *html;
   void eventHandler(dw::core::ui::Resource *resource,
                     int click_x, int click_y);
   void submit(DilloHtmlInput *input, int click_x, int click_y);
   DilloUrl *buildQueryUrl(DilloHtmlInput *input, int click_x, int click_y);
   Dstr *buildQueryData(DilloHtmlInput *active_submit, int x, int y);
   char *makeMultipartBoundary(iconv_t encoder, DilloHtmlInput *active_submit);
   Dstr *encodeText(iconv_t encoder, Dstr **input);
   void urlencodeAppend(Dstr *str, const char *val);
   void appendInputUrlencode(Dstr *data,
                             const char *name, const char *value);
   void appendInputMultipartFiles(Dstr* data,
                                  const char *boundary,
                                  const char *name, Dstr *file,
                                  const char *filename);
   void appendInputMultipart(Dstr *data,
                             const char *boundary,
                             const char *name,
                             const char *value);
   void appendClickposUrlencode(Dstr *data, Dstr *name, int x,int y);
   void appendClickposMultipart(Dstr *data,
                                const char *boundary,
                                Dstr *name, int x, int y);

public:  //BUG: for now everything is public
   DilloHtmlMethod method;
   DilloUrl *action;
   DilloHtmlEnc enc;
   char *submit_charset;

   lout::misc::SimpleVector<DilloHtmlInput*> *inputs;

   int num_entry_fields;
   int num_submit_buttons;

   DilloHtmlReceiver *form_receiver;

public:
   DilloHtmlForm (DilloHtml *html, 
                  DilloHtmlMethod method, const DilloUrl *action,
                  DilloHtmlEnc enc, const char *charset);
   ~DilloHtmlForm ();
   DilloHtmlInput *getInput (dw::core::ui::Resource *resource);
   DilloHtmlInput *getRadioInput (const char *name);
   void reset ();
   void addInput(DilloHtmlInput *input, DilloHtmlInputType type);
};

class DilloHtmlReceiver:
   public dw::core::ui::Resource::ActivateReceiver,
   public dw::core::ui::ButtonResource::ClickedReceiver
{
   friend class DilloHtmlForm;
   DilloHtmlForm* form;
   DilloHtmlReceiver (DilloHtmlForm* form2) { form = form2; }
   ~DilloHtmlReceiver () { }
   void activate (dw::core::ui::Resource *resource);
   void clicked (dw::core::ui::ButtonResource *resource,
                 int buttonNo, int x, int y);
};

class DilloHtmlInput {

   // DilloHtmlForm::addInput() calls connectTo() 
   friend class DilloHtmlForm;

public:  //BUG: for now everything is public
   DilloHtmlInputType type;
   dw::core::ui::Embed *embed; /* May be NULL (think: hidden input) */
   char *name;
   char *init_str;    /* note: some overloading - for buttons, init_str
                         is simply the value of the button; for text
                         entries, it is the initial value */
   DilloHtmlSelect *select;
   bool init_val;     /* only meaningful for buttons */
   Dstr *file_data;   /* only meaningful for file inputs.
                         todo: may become a list... */

private:
   void connectTo(DilloHtmlReceiver *form_receiver);
   void activate(DilloHtmlForm *form, bool force_submit,
                 int click_x, int click_y);
   void readFile(BrowserWindow *bw);

public:
   DilloHtmlInput (DilloHtmlInputType type,
                   dw::core::ui::Embed *embed,
                   const char *name,
                   const char *init_str,
                   bool init_val);
   ~DilloHtmlInput ();
   void appendValuesTo(Dlist *values, bool is_active_submit);
   void reset();
};

class DilloHtmlSelect {
   friend class DilloHtmlInput;
private:
   lout::misc::SimpleVector<DilloHtmlOption *> *options;
   DilloHtmlSelect ();
   ~DilloHtmlSelect ();
public:
   DilloHtmlOption *getCurrentOption ();
   void addOption (char *value, bool selected, bool enabled);
   void ensureSelection ();
   void addOptionsTo (dw::core::ui::SelectionResource *res);
   void appendValuesTo (Dlist *values,
                        dw::core::ui::SelectionResource *res);
};

class DilloHtmlOption {
   friend class DilloHtmlSelect;
public:
   char *value, *content;
   bool selected, enabled;
private:
   DilloHtmlOption (char *value, bool selected, bool enabled);
   ~DilloHtmlOption (); 
};

/*
 * Form API 
 */

DilloHtmlForm *a_Html_form_new (DilloHtml *html,
                                DilloHtmlMethod method,
                                const DilloUrl *action,
                                DilloHtmlEnc enc,
                                const char *charset)
{
   return new DilloHtmlForm (html,method,action,enc,charset);
}

void a_Html_form_delete (DilloHtmlForm *form)
{
   delete form;
}

void a_Html_input_delete (DilloHtmlInput *input)
{
   delete input;
}

/*
 * Form parsing functions 
 */

/*
 * Add an HTML control
 */
static void Html_add_input(DilloHtml *html, DilloHtmlInputType type,
                           dw::core::ui::Embed *embed, const char *name,
                           const char *init_str, bool init_val)
{
   _MSG("name=[%s] init_str=[%s] init_val=[%d]\n", name, init_str, init_val);
   DilloHtmlInput *input = new DilloHtmlInput(type, embed, name, init_str,
                                              init_val);
   if (html->InFlags & IN_FORM) {
      html->getCurrentForm()->addInput(input, type);
   } else {
      int ni = html->inputs_outside_form->size();
      html->inputs_outside_form->increase();
      html->inputs_outside_form->set(ni, input);
   }
}

/*
 * Find radio input by name
 */
static DilloHtmlInput *Html_get_radio_input(DilloHtml *html, const char *name)
{
   lout::misc::SimpleVector<DilloHtmlInput*>* inputs;

   if (html->InFlags & IN_FORM)
      inputs = html->getCurrentForm()->inputs;
   else
      inputs = html->inputs_outside_form;

   for (int idx = 0; idx < inputs->size(); idx++) {
      DilloHtmlInput *input = inputs->get(idx);
      if (input->type == DILLO_HTML_INPUT_RADIO &&
          input->name && !dStrcasecmp(input->name, name))
         return input;
   }
   return NULL;
}

/*
 * Get the current input.
 * Note that this _assumes_ that there _is_ a current input.
 */
static DilloHtmlInput *Html_get_current_input(DilloHtml *html)
{
   lout::misc::SimpleVector<DilloHtmlInput*>* inputs;

   if (html->InFlags & IN_FORM)
      inputs = html->getCurrentForm()->inputs;
   else
      inputs = html->inputs_outside_form;

   return inputs->get (inputs->size() - 1);
}

/*
 * Handle <FORM> tag
 */
void Html_tag_open_form(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *action;
   DilloHtmlMethod method;
   DilloHtmlEnc enc;
   char *charset, *first;
   const char *attrbuf;

   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);

   if (html->InFlags & IN_FORM) {
      BUG_MSG("nested forms\n");
      return;
   }
   html->InFlags |= IN_FORM;
   html->InFlags &= ~IN_SELECT;
   html->InFlags &= ~IN_OPTION;
   html->InFlags &= ~IN_TEXTAREA;

   method = DILLO_HTML_METHOD_GET;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "method"))) {
      if (!dStrcasecmp(attrbuf, "post"))
         method = DILLO_HTML_METHOD_POST;
      /* todo: maybe deal with unknown methods? */
   }
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "action")))
      action = a_Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
   else
      action = a_Url_dup(html->base_url);
   enc = DILLO_HTML_ENC_URLENCODING;
   if ((method == DILLO_HTML_METHOD_POST) &&
       ((attrbuf = a_Html_get_attr(html, tag, tagsize, "enctype")))) {
      if (!dStrcasecmp(attrbuf, "multipart/form-data"))
         enc = DILLO_HTML_ENC_MULTIPART;
   }
   charset = NULL;
   first = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "accept-charset"))) {
      /* a list of acceptable charsets, separated by commas or spaces */
      char *ptr = first = dStrdup(attrbuf);
      while (ptr && !charset) {
         char *curr = dStrsep(&ptr, " ,");
         if (!dStrcasecmp(curr, "utf-8")) {
            charset = curr;
         } else if (!dStrcasecmp(curr, "UNKNOWN")) {
            /* defined to be whatever encoding the document is in */
            charset = html->charset;
         }
      }
      if (!charset)
         charset = first;
   }
   if (!charset)
      charset = html->charset;
   html->formNew(method, action, enc, charset);
   dFree(first);
   a_Url_free(action);
}

void Html_tag_close_form(DilloHtml *html, int TagIdx)
{
   static const char *SubmitTag =
      "<input type='submit' value='?Submit?' alt='dillo-generated-button'>";
   DilloHtmlForm *form;
// int i;
  
   if (html->InFlags & IN_FORM) {
      form = html->getCurrentForm ();
      /* If we don't have a submit button and the user desires one,
         let's add a custom one */
      if (form->num_submit_buttons == 0) {
         if (prefs.show_extra_warnings || form->num_entry_fields != 1)
            BUG_MSG("FORM lacks a Submit button\n");
         if (prefs.generate_submit) {
            BUG_MSG(" (added a submit button internally)\n");
            Html_tag_open_input(html, SubmitTag, strlen(SubmitTag));
            form->num_submit_buttons = 0;
         }
      }
  
//    /* Make buttons sensitive again */
//    for (i = 0; i < form->inputs->size(); i++) {
//       input_i = form->inputs->get(i);
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

   a_Html_pop_tag(html, TagIdx);
}

/*
 * Add a new input to current form
 */
void Html_tag_open_input(DilloHtml *html, const char *tag, int tagsize)
{
   DilloHtmlInputType inp_type;
   dw::core::ui::Embed *embed = NULL;
   char *value, *name, *type, *init_str;
   const char *attrbuf, *label;
   bool init_val = false;
  
   if (html->InFlags & IN_SELECT) {
      BUG_MSG("<input> element inside <select>\n");
      return;
   }
   if (html->InFlags & IN_BUTTON) {
      BUG_MSG("<input> element inside <button>\n");
      return;
   }
  
   /* Get 'value', 'name' and 'type' */
   value = a_Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
   name = a_Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   type = a_Html_get_attr_wdef(html, tag, tagsize, "type", "");
  
   init_str = NULL;
   inp_type = DILLO_HTML_INPUT_UNKNOWN;
   if (!dStrcasecmp(type, "password")) {
      inp_type = DILLO_HTML_INPUT_PASSWORD;
      dw::core::ui::EntryResource *entryResource =
         HT2LT(html)->getResourceFactory()->createEntryResource (10, true);
      embed = new dw::core::ui::Embed (entryResource);
      init_str = (value) ? value : NULL;
   } else if (!dStrcasecmp(type, "checkbox")) {
      inp_type = DILLO_HTML_INPUT_CHECKBOX;
      dw::core::ui::CheckButtonResource *check_b_r =
         HT2LT(html)->getResourceFactory()->createCheckButtonResource(false);
      embed = new dw::core::ui::Embed (check_b_r);
      init_val = (a_Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : dStrdup("on");
   } else if (!dStrcasecmp(type, "radio")) {
      inp_type = DILLO_HTML_INPUT_RADIO;
      dw::core::ui::RadioButtonResource *rb_r = NULL;
      DilloHtmlInput *input = Html_get_radio_input(html, name);
      if (input)
         rb_r =
            (dw::core::ui::RadioButtonResource*)
            input->embed->getResource();
      rb_r = HT2LT(html)->getResourceFactory()
                ->createRadioButtonResource(rb_r, false);
      embed = new dw::core::ui::Embed (rb_r);
      init_val = (a_Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : NULL;
   } else if (!dStrcasecmp(type, "hidden")) {
      inp_type = DILLO_HTML_INPUT_HIDDEN;
      if (value)
         init_str = dStrdup(a_Html_get_attr(html, tag, tagsize, "value"));
   } else if (!dStrcasecmp(type, "submit")) {
      inp_type = DILLO_HTML_INPUT_SUBMIT;
      init_str = (value) ? value : dStrdup("submit");
      dw::core::ui::LabelButtonResource *label_b_r =
         HT2LT(html)->getResourceFactory()
         ->createLabelButtonResource(init_str);
      embed = new dw::core::ui::Embed (label_b_r);
//    gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
   } else if (!dStrcasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_RESET;
      init_str = (value) ? value : dStrdup("Reset");
      dw::core::ui::LabelButtonResource *label_b_r =
         HT2LT(html)->getResourceFactory()
         ->createLabelButtonResource(init_str);
      embed = new dw::core::ui::Embed (label_b_r);
//    gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
   } else if (!dStrcasecmp(type, "image")) {
      if (URL_FLAGS(html->base_url) & URL_SpamSafe) {
         /* Don't request the image; make a text submit button instead */
         inp_type = DILLO_HTML_INPUT_SUBMIT;
         attrbuf = a_Html_get_attr(html, tag, tagsize, "alt");
         label = attrbuf ? attrbuf : value ? value : name ? name : "Submit";
         init_str = dStrdup(label);
         dw::core::ui::LabelButtonResource *label_b_r =
            HT2LT(html)->getResourceFactory()
            ->createLabelButtonResource(init_str);
         embed = new dw::core::ui::Embed (label_b_r);
//       gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
      } else {
         inp_type = DILLO_HTML_INPUT_IMAGE;
         /* use a dw_image widget */
         embed = Html_input_image(html, tag, tagsize);
         init_str = value;
      }
   } else if (!dStrcasecmp(type, "file")) {
      bool valid = true;
      if (html->InFlags & IN_FORM) {
         DilloHtmlForm *form = html->getCurrentForm();
         if (form->method != DILLO_HTML_METHOD_POST) {
            valid = false;
            BUG_MSG("Forms with file input MUST use HTTP POST method\n");
            MSG("File input ignored in form not using HTTP POST method\n");
         } else if (form->enc != DILLO_HTML_ENC_MULTIPART) {
            valid = false;
            BUG_MSG("Forms with file input MUST use multipart/form-data"
                    " encoding\n");
            MSG("File input ignored in form not using multipart/form-data"
                " encoding\n");
         }
      }
      if (valid) {
         inp_type = DILLO_HTML_INPUT_FILE;
         init_str = dStrdup("File selector");
         dw::core::ui::LabelButtonResource *lbr =
            HT2LT(html)->getResourceFactory()->
               createLabelButtonResource(init_str);
         embed = new dw::core::ui::Embed (lbr);
      }
   } else if (!dStrcasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
      if (value) {
         init_str = value;
         dw::core::ui::LabelButtonResource *label_b_r =
            HT2LT(html)->getResourceFactory()
            ->createLabelButtonResource(init_str);
         embed = new dw::core::ui::Embed (label_b_r);
      }
   } else if (!dStrcasecmp(type, "text") || !*type) {
      /* Text input, which also is the default */
      inp_type = DILLO_HTML_INPUT_TEXT;
      dw::core::ui::EntryResource *entryResource =
         HT2LT(html)->getResourceFactory()->createEntryResource (10, false);
      embed = new dw::core::ui::Embed (entryResource);
      init_str = (value) ? value : NULL;
   } else {
      /* Unknown input type */
      BUG_MSG("Unknown input type: \"%s\"\n", type);
   }

   if (inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      Html_add_input(html, inp_type, embed, name,
                     (init_str) ? init_str : "", init_val);
   }
  
   if (embed != NULL && inp_type != DILLO_HTML_INPUT_IMAGE && 
       inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      if (inp_type == DILLO_HTML_INPUT_TEXT ||
          inp_type == DILLO_HTML_INPUT_PASSWORD) {
         dw::core::ui::EntryResource *entryres =
            (dw::core::ui::EntryResource*)embed->getResource();
         /* Readonly or not? */
         if (a_Html_get_attr(html, tag, tagsize, "readonly"))
            entryres->setEditable(false);

//       /* Set width of the entry */
//       if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "size")))
//          gtk_widget_set_usize(widget, (strtol(attrbuf, NULL, 10) + 1) *
//                               gdk_char_width(widget->style->font, '0'), 0);
//
//       /* Maximum length of the text in the entry */
//       if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "maxlength")))
//          gtk_entry_set_max_length(GTK_ENTRY(widget),
//                                   strtol(attrbuf, NULL, 10));
      }

      if (prefs.standard_widget_colors) {
         HTML_SET_TOP_ATTR(html, color, NULL);
         HTML_SET_TOP_ATTR(html, backgroundColor, NULL);
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
void Html_tag_open_isindex(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *action;
   dw::core::ui::Embed *embed;
   const char *attrbuf;

   if (html->InFlags & IN_FORM) {
      MSG("<isindex> inside <form> not handled.\n");
      return;
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "action")))
      action = a_Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0);
   else
      action = a_Url_dup(html->base_url);
  
   html->formNew(DILLO_HTML_METHOD_GET, action, DILLO_HTML_ENC_URLENCODING,
                 html->charset);
   html->InFlags |= IN_FORM;
  
   DW2TB(html->dw)->addParbreak (9, S_TOP(html)->style);
  
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "prompt")))
      DW2TB(html->dw)->addText(attrbuf, S_TOP(html)->style);
 
   dw::core::ui::EntryResource *entryResource =
      HT2LT(html)->getResourceFactory()->createEntryResource (10, false);
   embed = new dw::core::ui::Embed (entryResource);
   Html_add_input(html, DILLO_HTML_INPUT_INDEX, embed, NULL, NULL, FALSE);

   if (prefs.standard_widget_colors) {
      HTML_SET_TOP_ATTR(html, color, NULL);
      HTML_SET_TOP_ATTR(html, backgroundColor, NULL);
   }
   DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);
  
   a_Url_free(action);
   html->InFlags &= ~IN_FORM;
}

/*
 * The textarea tag
 * (todo: It doesn't support wrapping).
 */
void Html_tag_open_textarea(DilloHtml *html, const char *tag, int tagsize)
{
   char *name;
   const char *attrbuf;
   int cols, rows;
  
   if (html->InFlags & IN_TEXTAREA) {
      BUG_MSG("nested <textarea>\n");
      html->ReqTagClose = TRUE;
      return;
   }
  
   html->InFlags |= IN_TEXTAREA;
   a_Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;
  
   cols = 20;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cols")))
      cols = strtol(attrbuf, NULL, 10);
   rows = 10;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "rows")))
      rows = strtol(attrbuf, NULL, 10);
   name = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name")))
      name = dStrdup(attrbuf);

   dw::core::ui::MultiLineTextResource *textres =
      HT2LT(html)->getResourceFactory()->createMultiLineTextResource (cols,
                                                                      rows);

   dw::core::ui::Embed *embed = new dw::core::ui::Embed(textres);
   /* Readonly or not? */
   if (a_Html_get_attr(html, tag, tagsize, "readonly"))
      textres->setEditable(false);
   Html_add_input(html, DILLO_HTML_INPUT_TEXTAREA, embed, name, NULL, false);
   DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);
   dFree(name);
}

/*
 * Close  textarea
 * (TEXTAREA is parsed in VERBATIM mode, and entities are handled here)
 */
void Html_tag_close_textarea(DilloHtml *html, int TagIdx)
{
   char *str;
   DilloHtmlInput *input;
   int i;

   if (html->InFlags & IN_TEXTAREA) {
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
      str = a_Html_parse_entities(html, html->Stash->str, html->Stash->len);
      input = Html_get_current_input(html);
      input->init_str = str;
      ((dw::core::ui::MultiLineTextResource *)input->embed->getResource ())
         ->setText(str);

      html->InFlags &= ~IN_TEXTAREA;
   }
   a_Html_pop_tag(html, TagIdx);
}

/*
 * <SELECT>
 */
/* The select tag is quite tricky, because of gorpy html syntax. */
void Html_tag_open_select(DilloHtml *html, const char *tag, int tagsize)
{
// const char *attrbuf;
// int size, type, multi;

   if (html->InFlags & IN_SELECT) {
      BUG_MSG("nested <select>\n");
      return;
   }
   html->InFlags |= IN_SELECT;
   html->InFlags &= ~IN_OPTION;

   char *name = a_Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   dw::core::ui::ResourceFactory *factory =
      HT2LT(html)->getResourceFactory ();
   DilloHtmlInputType type;
   dw::core::ui::SelectionResource *res;
   if (a_Html_get_attr(html, tag, tagsize, "multiple")) {
      type = DILLO_HTML_INPUT_SEL_LIST;
      res = factory->createListResource (
         dw::core::ui::ListResource::SELECTION_MULTIPLE);
   } else {
      type = DILLO_HTML_INPUT_SELECT;
      res = factory->createOptionMenuResource ();
   }
   dw::core::ui::Embed *embed = new dw::core::ui::Embed(res);
   if (prefs.standard_widget_colors) {
      /* Colors cannot be NULL because SELECT can contain other elements */
      HTML_SET_TOP_ATTR(html, color, Color::createSimple (HT2LT(html), 0));
      HTML_SET_TOP_ATTR(html, backgroundColor,
                        Color::createShaded (HT2LT(html), 0xffffff));
   }
   DW2TB(html->dw)->addWidget (embed, S_TOP(html)->style);

// size = 0;
// if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "size")))
//    size = strtol(attrbuf, NULL, 10);
//
// multi = (a_Html_get_attr(html, tag, tagsize, "multiple")) ? 1 : 0;
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

   Html_add_input(html, type, embed, name, NULL, false);
   a_Html_stash_init(html);
   dFree(name);
}

/*
 * ?
 */
void Html_tag_close_select(DilloHtml *html, int TagIdx)
{
   if (html->InFlags & IN_SELECT) {
      if (html->InFlags & IN_OPTION)
         Html_option_finish(html);
      html->InFlags &= ~IN_SELECT;
      html->InFlags &= ~IN_OPTION;

      DilloHtmlInput *input = Html_get_current_input(html);
      DilloHtmlSelect *select = input->select;

      // BUG(?): should not do this for MULTI selections 
      select->ensureSelection ();

      dw::core::ui::SelectionResource *res =
         (dw::core::ui::SelectionResource*)input->embed->getResource();
      select->addOptionsTo (res);
   }

   a_Html_pop_tag(html, TagIdx);
}

/*
 * <OPTION>
 */
void Html_tag_open_option(DilloHtml *html, const char *tag, int tagsize)
{
   if (!(html->InFlags & IN_SELECT)) {
      BUG_MSG("<option> element outside <select>\n");
      return;
   }
   if (html->InFlags & IN_OPTION)
      Html_option_finish(html);
   html->InFlags |= IN_OPTION;

   DilloHtmlInput *input = Html_get_current_input(html);

   if (input->type == DILLO_HTML_INPUT_SELECT ||
       input->type == DILLO_HTML_INPUT_SEL_LIST) {
      char *value =
         a_Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
      bool selected =
         (a_Html_get_attr(html, tag, tagsize, "selected") != NULL);
      bool enabled =
         (a_Html_get_attr(html, tag, tagsize, "disabled") == NULL);
      input->select->addOption(value,selected,enabled);
   }

   a_Html_stash_init(html);
}

/*
 * <BUTTON>
 */
void Html_tag_open_button(DilloHtml *html, const char *tag, int tagsize)
{
   /*
    * Buttons are rendered on one line, this is (at several levels) a
    * bit simpler. May be changed in the future.
    */
   DilloHtmlInputType inp_type;
   char *type;

   if (html->InFlags & IN_BUTTON) {
      BUG_MSG("nested <button>\n");
      return;
   }
   html->InFlags |= IN_BUTTON;

   type = a_Html_get_attr_wdef(html, tag, tagsize, "type", "");

   if (!dStrcasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
   } else if (!dStrcasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_BUTTON_RESET;
   } else if (!dStrcasecmp(type, "submit") || !*type) {
      /* submit button is the default */
      inp_type = DILLO_HTML_INPUT_BUTTON_SUBMIT;
   } else {
      inp_type = DILLO_HTML_INPUT_UNKNOWN;
      BUG_MSG("Unknown button type: \"%s\"\n", type);
   }

   if (inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      /* Render the button */
      dw::core::style::StyleAttrs style_attrs;
      dw::core::style::Style *style;
      dw::core::Widget *page;
      dw::core::ui::Embed *embed;
      char *name, *value;

      style_attrs = *S_TOP(html)->style;
      style_attrs.margin.setVal(0);
      style_attrs.borderWidth.setVal(0);
      style_attrs.padding.setVal(0);
      style = Style::create (HT2LT(html), &style_attrs);

      page = new Textblock (prefs.limit_text_width);
      page->setStyle (style);

      dw::core::ui::ComplexButtonResource *complex_b_r =
         HT2LT(html)->getResourceFactory()
         ->createComplexButtonResource(page, true);
      embed = new dw::core::ui::Embed(complex_b_r);
// a_Dw_button_set_sensitive (DW_BUTTON (button), FALSE);

      DW2TB(html->dw)->addParbreak (5, style);
      DW2TB(html->dw)->addWidget (embed, style);
      DW2TB(html->dw)->addParbreak (5, style);
      style->unref ();

      S_TOP(html)->textblock = html->dw = page;
      /* right button press for menus for button contents */
      html->connectSignals(page);

      value = a_Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
      name = a_Html_get_attr_wdef(html, tag, tagsize, "name", NULL);

      Html_add_input(html, inp_type, embed, name, value, FALSE);
      dFree(name);
      dFree(value);
   }
   dFree(type);
}

/*
 * Handle close <BUTTON>
 */
void Html_tag_close_button(DilloHtml *html, int TagIdx)
{
   html->InFlags &= ~IN_BUTTON;
   a_Html_pop_tag(html, TagIdx);
}

/*
 * Class implementations 
 */

/*
 * DilloHtmlForm 
 */

/*
 * Constructor 
 */
DilloHtmlForm::DilloHtmlForm (DilloHtml *html2,
                              DilloHtmlMethod method2,
                              const DilloUrl *action2,
                              DilloHtmlEnc enc2,
                              const char *charset)
{
   html = html2;
   method = method2;
   action = a_Url_dup(action2);
   enc = enc2;
   submit_charset = dStrdup(charset);
   inputs = new misc::SimpleVector <DilloHtmlInput*> (4);
   num_entry_fields = 0;
   num_submit_buttons = 0;
   form_receiver = new DilloHtmlReceiver (this);
}

/*
 * Destructor 
 */
DilloHtmlForm::~DilloHtmlForm ()
{
   a_Url_free(action);
   dFree(submit_charset);
   for (int j = 0; j < inputs->size(); j++)
      delete inputs->get(j);
   delete(inputs);
   if (form_receiver)
      delete(form_receiver);
}

void DilloHtmlForm::eventHandler(dw::core::ui::Resource *resource,
                                 int click_x, int click_y)
{
   MSG("DilloHtmlForm::eventHandler\n");
   DilloHtmlInput *input = getInput(resource);
   if (input) {
      bool force_submit =
         prefs.enterpress_forces_submit ||
         num_entry_fields == 1;
      input->activate (this, force_submit, click_x, click_y);
   } else {
      MSG("DilloHtmlForm::eventHandler: ERROR, input not found!\n");
   }
}

/*
 * Submit.
 * (Called by eventHandler())
 * click_x and click_y are used only by input images.
 */
void DilloHtmlForm::submit(DilloHtmlInput *input,
                           int click_x, int click_y)
{
   DilloUrl *url = buildQueryUrl(input, click_x, click_y);
   if (url) {
      a_Nav_push(html->bw, url);
      a_Url_free(url);
   }
   // /* now, make the rendered area have its focus back */
   // gtk_widget_grab_focus(GTK_BIN(bw->render_main_scroll)->child);
}

/*
 * Build a new query URL.
 * (Called by submit())
 */
DilloUrl *DilloHtmlForm::buildQueryUrl(DilloHtmlInput *input,
                                       int click_x, int click_y)
{
   DilloUrl *new_url = NULL;

   if ((method == DILLO_HTML_METHOD_GET) ||
       (method == DILLO_HTML_METHOD_POST)) {
      Dstr *DataStr;
      DilloHtmlInput *active_submit = NULL;

      _MSG("DilloHtmlForm::buildQueryUrl: action=%s\n",URL_STR_(action));

      if (num_submit_buttons > 0) {
         if ((input->type == DILLO_HTML_INPUT_SUBMIT) ||
             (input->type == DILLO_HTML_INPUT_IMAGE) ||
             (input->type == DILLO_HTML_INPUT_BUTTON_SUBMIT)) {
            active_submit = input;
         }
      }

      DataStr = buildQueryData(active_submit, click_x, click_y);
      if (DataStr) {
         /* action was previously resolved against base URL */
         char *action_str = dStrdup(URL_STR(action));

         if (method == DILLO_HTML_METHOD_POST) {
            new_url = a_Url_new(action_str, NULL, 0, 0, 0);
            /* new_url keeps the dStr and sets DataStr to NULL */
            a_Url_set_data(new_url, &DataStr);
            a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Post);
            if (enc == DILLO_HTML_ENC_MULTIPART)
               a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_MultipartEnc);
         } else {
            /* remove <fragment> and <query> sections if present */
            char *url_str, *p;
            if ((p = strchr(action_str, '#')))
               *p = 0;
            if ((p = strchr(action_str, '?')))
               *p = 0;

            url_str = dStrconcat(action_str, "?", DataStr->str, NULL);
            new_url = a_Url_new(url_str, NULL, 0, 0, 0);
            a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Get);
            dFree(url_str);
         }
         dStr_free(DataStr, 1);
         dFree(action_str);
      }
   } else {
      MSG("DilloHtmlForm::buildQueryUrl: Method unknown\n");
   }

   return new_url;
}

/*
 * Construct the data for a query URL
 */
Dstr *DilloHtmlForm::buildQueryData(DilloHtmlInput *active_submit,
                                    int x, int y)
{
   Dstr *DataStr = NULL;
   char *boundary = NULL;
   iconv_t encoder = (iconv_t) -1;

   if (submit_charset && dStrcasecmp(submit_charset, "UTF-8")) {
      encoder = iconv_open(submit_charset, "UTF-8");
      if (encoder == (iconv_t) -1) {
         MSG_WARN("Cannot convert to character encoding '%s'\n",
                  submit_charset);
      } else {
         MSG("Form character encoding: '%s'\n", submit_charset);
      }
   }

   if (enc == DILLO_HTML_ENC_MULTIPART) {
      if (!(boundary = makeMultipartBoundary(encoder, active_submit)))
         MSG_ERR("Cannot generate multipart/form-data boundary.\n");
   }

   if ((enc == DILLO_HTML_ENC_URLENCODING) || (boundary != NULL)) {
      Dlist *values = dList_new(5);

      DataStr = dStr_sized_new(4096);
      for (int input_idx = 0; input_idx < inputs->size(); input_idx++) {
         DilloHtmlInput *input = inputs->get (input_idx);
         Dstr *name = dStr_new(input->name);
         bool is_active_submit = (input == active_submit);

         name = encodeText(encoder, &name);

         if (input->type == DILLO_HTML_INPUT_IMAGE) {
            if (is_active_submit){
               if (enc == DILLO_HTML_ENC_URLENCODING)
                  appendClickposUrlencode(DataStr, name, x, y);
               else if (enc == DILLO_HTML_ENC_MULTIPART)
                  appendClickposMultipart(DataStr, boundary, name, x,y);
            }
         } else {
            input->appendValuesTo(values, is_active_submit);

            if (input->type == DILLO_HTML_INPUT_FILE &&
                dList_length(values) > 0) {
               if (dList_length(values) > 1)
                  MSG_WARN("multiple files per form control not supported\n");
               Dstr *file = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, file);

               /* Get filename and encode it. Do not encode file contents. */
               dw::core::ui::LabelButtonResource *lbr =
                  (dw::core::ui::LabelButtonResource*)
                  input->embed->getResource();
               const char *filename = lbr->getLabel();
               if (filename[0] && strcmp(filename, input->init_str)) {
                  char *p = strrchr(filename, '/');
                  if (p)
                     filename = p + 1;     /* don't reveal path */
                  Dstr *dfilename = dStr_new(filename);
                  dfilename = encodeText(encoder, &dfilename);
                  appendInputMultipartFiles(DataStr, boundary, name->str,
                                            file, dfilename->str);
                  dStr_free(dfilename, 1);
               }
               dStr_free(file, 1);
            } else if (input->type == DILLO_HTML_INPUT_INDEX) {
               Dstr *val = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, val);
               val = encodeText(encoder, &val);
               urlencodeAppend(DataStr, val->str);
               dStr_free(val, 1);
            } else {
               int length = dList_length(values), i;
               for (i = 0; i < length; i++) {
                  Dstr *val = (Dstr *) dList_nth_data(values, 0);
                  dList_remove(values, val);
                  val = encodeText(encoder, &val);
                  if (enc == DILLO_HTML_ENC_URLENCODING)
                     appendInputUrlencode(DataStr, name->str, val->str);
                  else if (enc == DILLO_HTML_ENC_MULTIPART)
                     appendInputMultipart(DataStr, boundary,
                                          name->str, val->str);
                  dStr_free(val, 1);
               }
            }
         }
         dStr_free(name, 1);
      }
      if (DataStr->len > 0) {
         if (enc == DILLO_HTML_ENC_URLENCODING) {
            if (DataStr->str[DataStr->len - 1] == '&')
               dStr_truncate(DataStr, DataStr->len - 1);
         } else if (enc == DILLO_HTML_ENC_MULTIPART) {
            dStr_append(DataStr, "--");
         }
      }
      dList_free(values);
   }
   dFree(boundary);
   if (encoder != (iconv_t) -1)
      (void)iconv_close(encoder);
   return DataStr;
}

/*
 * Generate a boundary string for use in separating the parts of a
 * multipart/form-data submission.
 */
char *DilloHtmlForm::makeMultipartBoundary(iconv_t encoder,
                                           DilloHtmlInput *active_submit)
{
   const int max_tries = 10;
   Dlist *values = dList_new(5);
   Dstr *DataStr = dStr_new("");
   Dstr *boundary = dStr_new("");
   char *ret = NULL;

   /* fill DataStr with names, filenames, and values */
   for (int input_idx = 0; input_idx < inputs->size(); input_idx++) {
      Dstr *dstr;
      DilloHtmlInput *input = inputs->get (input_idx);
      bool is_active_submit = (input == active_submit);
      input->appendValuesTo(values, is_active_submit);

      if (input->name) {
         dstr = dStr_new(input->name);
         dstr = encodeText(encoder, &dstr);
         dStr_append_l(DataStr, dstr->str, dstr->len);
         dStr_free(dstr, 1);
      }
      if (input->type == DILLO_HTML_INPUT_FILE) {
         dw::core::ui::LabelButtonResource *lbr =
            (dw::core::ui::LabelButtonResource*)input->embed->getResource();
         const char *filename = lbr->getLabel();
         if (filename[0] && strcmp(filename, input->init_str)) {
            dstr = dStr_new(filename);
            dstr = encodeText(encoder, &dstr);
            dStr_append_l(DataStr, dstr->str, dstr->len);
            dStr_free(dstr, 1);
         }
      }
      int length = dList_length(values);
      for (int i = 0; i < length; i++) {
         dstr = (Dstr *) dList_nth_data(values, 0);
         dList_remove(values, dstr);
         if (input->type != DILLO_HTML_INPUT_FILE)
            dstr = encodeText(encoder, &dstr);
         dStr_append_l(DataStr, dstr->str, dstr->len);
         dStr_free(dstr, 1);
      }
   }

   /* generate a boundary that is not contained within the data */
   for (int i = 0; i < max_tries && !ret; i++) {
      // Firefox-style boundary
      dStr_sprintf(boundary, "---------------------------%d%d%d",
                   rand(), rand(), rand());
      dStr_truncate(boundary, 70);
      if (dStr_memmem(DataStr, boundary) == NULL)
         ret = boundary->str;
   }
   dList_free(values);
   dStr_free(DataStr, 1);
   dStr_free(boundary, (ret == NULL));
   return ret;
}

/*
 * Pass input text through character set encoder.
 * Return value: same input Dstr if no encoding is needed.
                 new Dstr when encoding (input Dstr is freed).
 */
Dstr *DilloHtmlForm::encodeText(iconv_t encoder, Dstr **input)
{
   int rc = 0;
   Dstr *output;
   const int bufsize = 128;
   inbuf_t *inPtr;
   char *buffer, *outPtr;
   size_t inLeft, outRoom;
   bool bad_chars = false;

   if ((encoder == (iconv_t) -1) || *input == NULL || (*input)->len == 0)
      return *input;

   output = dStr_new("");
   inPtr  = (*input)->str;
   inLeft = (*input)->len;
   buffer = dNew(char, bufsize);

   while ((rc != EINVAL) && (inLeft > 0)) {

      outPtr = buffer;
      outRoom = bufsize;

      rc = iconv(encoder, &inPtr, &inLeft, &outPtr, &outRoom);

      // iconv() on success, number of bytes converted
      //         -1, errno == EILSEQ illegal byte sequence found
      //                      EINVAL partial character ends source buffer
      //                      E2BIG  destination buffer is full
      //
      // GNU iconv has the undocumented(!) behavior that EILSEQ is also
      // returned when a character cannot be converted.

      dStr_append_l(output, buffer, bufsize - outRoom);

      if (rc == -1) {
         rc = errno;
      }
      if (rc == EILSEQ){
         /* count chars? (would be utf-8-specific) */
         bad_chars = true;
         inPtr++;
         inLeft--;
         dStr_append_c(output, '?');
      } else if (rc == EINVAL) {
         MSG_ERR("Html_decode_text: bad source string\n");
      }
   }

   if (bad_chars) {
      /*
       * It might be friendly to inform the caller, who would know whether
       * it is safe to display the beginning of the string in a message
       * (isn't, e.g., a password).
       */
      MSG_WARN("String cannot be converted cleanly.\n");
   }

   dFree(buffer);
   dStr_free(*input, 1);

   return output;
}
  
/*
 * Urlencode 'val' and append it to 'str'
 */
void DilloHtmlForm::urlencodeAppend(Dstr *str, const char *val)
{
   char *enc_val = a_Url_encode_hex_str(val);
   dStr_append(str, enc_val);
   dFree(enc_val);
}

/*
 * Append a name-value pair to url data using url encoding.
 */
void DilloHtmlForm::appendInputUrlencode(Dstr *data,
                                         const char *name,
                                         const char *value)
{
   if (name && name[0]) {
      urlencodeAppend(data, name);
      dStr_append_c(data, '=');
      urlencodeAppend(data, value);
      dStr_append_c(data, '&');
   }
}

/*
 * Append files to URL data using multipart encoding.
 * Currently only accepts one file.
 */
void DilloHtmlForm::appendInputMultipartFiles(Dstr* data,
                                              const char *boundary,
                                              const char *name,
                                              Dstr *file,
                                              const char *filename)
{
   const char *ctype, *ext;

   if (name && name[0]) {
      (void)a_Misc_get_content_type_from_data(file->str, file->len, &ctype);
      /* Heuristic: text/plain with ".htm[l]" extension -> text/html */
      if ((ext = strrchr(filename, '.')) &&
          !dStrcasecmp(ctype, "text/plain") &&
          (!dStrcasecmp(ext, ".html") || !dStrcasecmp(ext, ".htm"))) {
         ctype = "text/html";
      }

      if (data->len == 0) {
         dStr_append(data, "--");
         dStr_append(data, boundary);
      }
      // todo: encode name, filename
      dStr_sprintfa(data,
                    "\r\n"
                    "Content-Disposition: form-data; name=\"%s\"; "
                       "filename=\"%s\"\r\n"
                    "Content-Type: %s\r\n"
                    "\r\n", name, filename, ctype);

      dStr_append_l(data, file->str, file->len);

      dStr_sprintfa(data,
                    "\r\n"
                    "--%s", boundary);
   }
}

/*
 * Append a name-value pair to url data using multipart encoding.
 */
void DilloHtmlForm::appendInputMultipart(Dstr *data,
                                         const char *boundary,
                                         const char *name,
                                         const char *value)
{
   if (name && name[0]) {
      if (data->len == 0) {
         dStr_append(data, "--");
         dStr_append(data, boundary);
      }
      // todo: encode name (RFC 2231) [coming soon]
      dStr_sprintfa(data,
                    "\r\n"
                    "Content-Disposition: form-data; name=\"%s\"\r\n"
                    "\r\n"
                    "%s\r\n"
                    "--%s",
                    name, value, boundary);
   }
}

/*
 * Append an image button click position to url data using url encoding.
 */
void DilloHtmlForm::appendClickposUrlencode(Dstr *data,
                                            Dstr *name, int x,int y)
{
   if (name->len) {
      urlencodeAppend(data, name->str);
      dStr_sprintfa(data, ".x=%d&", x);
      urlencodeAppend(data, name->str);
      dStr_sprintfa(data, ".y=%d&", y);
   } else
      dStr_sprintfa(data, "x=%d&y=%d&", x, y);
}

/*
 * Append an image button click position to url data using multipart encoding.
 */
void DilloHtmlForm::appendClickposMultipart(Dstr *data,
                                            const char *boundary,
                                            Dstr *name, int x, int y)
{
   char posstr[16];
   int orig_len = name->len;

   if (orig_len)
      dStr_append_c(name, '.');
   dStr_append_c(name, 'x');

   snprintf(posstr, 16, "%d", x);
   appendInputMultipart(data, boundary, name->str, posstr);
   dStr_truncate(name, name->len - 1);
   dStr_append_c(name, 'y');
   snprintf(posstr, 16, "%d", y);
   appendInputMultipart(data, boundary, name->str, posstr);
   dStr_truncate(name, orig_len);
}

/*
 * Reset all inputs containing reset to their initial values.  In
 * general, reset is the reset button for the form.
 */
void DilloHtmlForm::reset ()
{
   int size = inputs->size();
   for (int i = 0; i < size; i++)
      inputs->get(i)->reset();
}

/*
 * Add a new input.
 */
void DilloHtmlForm::addInput(DilloHtmlInput *input, DilloHtmlInputType type)
{
   input->connectTo (form_receiver);
   int ni = inputs->size ();
   inputs->increase ();
   inputs->set (ni,input);

   /* some stats */
   if (type == DILLO_HTML_INPUT_PASSWORD ||
       type == DILLO_HTML_INPUT_TEXT) {
      num_entry_fields++;
   } else if (type == DILLO_HTML_INPUT_SUBMIT ||
              type == DILLO_HTML_INPUT_BUTTON_SUBMIT ||
              type == DILLO_HTML_INPUT_IMAGE) {
      num_submit_buttons++;
   }
}

/*
 * Return the input with a given resource.
 */
DilloHtmlInput *DilloHtmlForm::getInput (dw::core::ui::Resource *resource)
{
   for (int idx = 0; idx < inputs->size(); idx++) {
      DilloHtmlInput *input = inputs->get(idx);
      if (input->embed &&
          resource == input->embed->getResource())
         return input;
   }
   return NULL;
}

/*
 * Return a Radio input for the given name.
 */
DilloHtmlInput *DilloHtmlForm::getRadioInput (const char *name)
{
   for (int idx = 0; idx < inputs->size(); idx++) {
      DilloHtmlInput *input = inputs->get(idx);
      if (input->type == DILLO_HTML_INPUT_RADIO &&
          input->name && !dStrcasecmp(input->name, name))
         return input;
   }
   return NULL;
}

/*
 * DilloHtmlReceiver 
 *
 * TODO: Currently there's "clicked" for buttons, we surely need "enter" for
 * textentries, and maybe the "mouseover, ...." set for Javascript.
 */

void DilloHtmlReceiver::activate (dw::core::ui::Resource *resource)
{
   form->eventHandler(resource, -1, -1);
}

void DilloHtmlReceiver::clicked (dw::core::ui::ButtonResource *resource,
                                 int buttonNo, int x, int y)
{
   form->eventHandler(resource, x, y);
}

/*
 * DilloHtmlInput 
 */

/*
 * Constructor 
 */
DilloHtmlInput::DilloHtmlInput (DilloHtmlInputType type2,
                                dw::core::ui::Embed *embed2,
                                const char *name2,
                                const char *init_str2,
                                bool init_val2)
{
   type = type2;
   embed = embed2;
   name = (name2) ? dStrdup(name2) : NULL;
   init_str = (init_str2) ? dStrdup(init_str2) : NULL;
   init_val = init_val2;
   select = NULL;
   switch (type) {
   case DILLO_HTML_INPUT_SELECT:
   case DILLO_HTML_INPUT_SEL_LIST:
      select = new DilloHtmlSelect;
      break;
   default:
      break;
   }
   file_data = NULL;
   reset ();
}

/*
 * Destructor 
 */
DilloHtmlInput::~DilloHtmlInput ()
{
   dFree(name);
   dFree(init_str);
   dStr_free(file_data, 1);
   if (select)
      delete select;
}

/*
 * Connect to a receiver.
 */
void DilloHtmlInput::connectTo(DilloHtmlReceiver *form_receiver)
{
   dw::core::ui::Resource *resource = NULL;
   if (embed)
      resource = embed->getResource ();
   switch (type) {
   case DILLO_HTML_INPUT_UNKNOWN:
   case DILLO_HTML_INPUT_HIDDEN:
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
   case DILLO_HTML_INPUT_BUTTON:
   case DILLO_HTML_INPUT_TEXTAREA:
   case DILLO_HTML_INPUT_SELECT:
   case DILLO_HTML_INPUT_SEL_LIST:
      // do nothing 
      break;
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
   case DILLO_HTML_INPUT_INDEX:
      if (resource)
         resource->connectActivate (form_receiver);
      break;
   case DILLO_HTML_INPUT_SUBMIT:
   case DILLO_HTML_INPUT_RESET:
   case DILLO_HTML_INPUT_BUTTON_SUBMIT:
   case DILLO_HTML_INPUT_BUTTON_RESET:
   case DILLO_HTML_INPUT_IMAGE:
   case DILLO_HTML_INPUT_FILE:
      if (resource)
         ((dw::core::ui::ButtonResource *)resource)
            ->connectClicked (form_receiver);
      break;
   }
}

/*
 * Activate a form 
 */
void DilloHtmlInput::activate(DilloHtmlForm *form, bool force_submit,
                              int click_x, int click_y)
{
   switch (type) {
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
      if (force_submit)
         form->submit (this, click_x, click_y);
      break;
   case DILLO_HTML_INPUT_FILE:
      readFile (form->html->bw);
      break;
   case DILLO_HTML_INPUT_RESET:
   case DILLO_HTML_INPUT_BUTTON_RESET:
      reset ();
      break;
   default:
      form->submit (this, click_x, click_y);
      break;
   }
}

/*
 * Read a file into cache 
 */
void DilloHtmlInput::readFile (BrowserWindow *bw)
{
   const char *filename = a_UIcmd_select_file();
   if (filename) {
      a_UIcmd_set_msg(bw, "Loading file...");
      dStr_free(file_data, 1);
      file_data = a_Misc_file2dstr(filename);
      if (file_data) {
         a_UIcmd_set_msg(bw, "File loaded.");
         dw::core::ui::LabelButtonResource *lbr =
            (dw::core::ui::LabelButtonResource*)embed->getResource();
         lbr->setLabel(filename);
      } else {
         a_UIcmd_set_msg(bw, "ERROR: can't load: %s", filename);
      }
   }
}

/*
 * Get the values for a "successful control".
 */
void DilloHtmlInput::appendValuesTo(Dlist *values, bool is_active_submit)
{
   switch (type) {
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
   case DILLO_HTML_INPUT_INDEX:
      {
         dw::core::ui::EntryResource *entryres =
            (dw::core::ui::EntryResource*)embed->getResource();
         dList_append(values, dStr_new(entryres->getText()));
      }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      {
         dw::core::ui::MultiLineTextResource *textres =
            (dw::core::ui::MultiLineTextResource*)embed->getResource();
         dList_append(values, dStr_new(textres->getText()));
      }
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
      {
         dw::core::ui::ToggleButtonResource *cb_r =
            (dw::core::ui::ToggleButtonResource*)embed->getResource();
         if (name && init_str && cb_r->isActivated()) {
            dList_append(values, dStr_new(init_str));
         }
      }
      break;
   case DILLO_HTML_INPUT_SUBMIT:
   case DILLO_HTML_INPUT_BUTTON_SUBMIT:
      if (is_active_submit)
         dList_append(values, dStr_new(init_str));
      break;
   case DILLO_HTML_INPUT_HIDDEN:
      dList_append(values, dStr_new(init_str));
      break;
   case DILLO_HTML_INPUT_SELECT:
   case DILLO_HTML_INPUT_SEL_LIST:
      {  // brackets for compiler happiness.
         dw::core::ui::SelectionResource *sel_res =
            (dw::core::ui::SelectionResource*)embed->getResource();
         select->appendValuesTo (values, sel_res);
      }
      break;
   case DILLO_HTML_INPUT_FILE:
      {
         dw::core::ui::LabelButtonResource *lbr =
            (dw::core::ui::LabelButtonResource*)embed->getResource();
         const char *filename = lbr->getLabel();
         if (filename[0] && strcmp(filename, init_str)) {
            if (file_data) {
               Dstr *file = dStr_sized_new(file_data->len);
               dStr_append_l(file, file_data->str, file_data->len);
               dList_append(values, file);
            } else {
               MSG("FORM file input \"%s\" not loaded.\n", filename);
            }
         }
      }
      break;
   default:
      break;
   }
}

/*
 * Reset to the initial value.
 */
void DilloHtmlInput::reset ()
{
   switch (type) {
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
      {
         dw::core::ui::EntryResource *entryres =
            (dw::core::ui::EntryResource*)embed->getResource();
         entryres->setText(init_str ? init_str : "");
      }
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
      {
         dw::core::ui::ToggleButtonResource *tb_r =
            (dw::core::ui::ToggleButtonResource*)embed->getResource();
         tb_r->setActivated(init_val);
      }
      break;
   case DILLO_HTML_INPUT_SELECT:
      if (select != NULL) {
         /* this is in reverse order so that, in case more than one was
          * selected, we get the last one, which is consistent with handling
          * of multiple selected options in the layout code. */
//       for (i = select->num_options - 1; i >= 0; i--) {
//          if (select->options[i].init_val) {
//             gtk_menu_item_activate(GTK_MENU_ITEM
//                                    (select->options[i].menuitem));
//             Html_select_set_history(input);
//             break;
//          }
//       }
      }
      break;
   case DILLO_HTML_INPUT_SEL_LIST:
      if (!select)
         break;
//    for (i = 0; i < select->num_options; i++) {
//       if (select->options[i].init_val) {
//          if (select->options[i].menuitem->state == GTK_STATE_NORMAL)
//             gtk_list_select_child(GTK_LIST(select->menu),
//                                   select->options[i].menuitem);
//       } else {
//          if (select->options[i].menuitem->state==GTK_STATE_SELECTED)
//             gtk_list_unselect_child(GTK_LIST(select->menu),
//                                     select->options[i].menuitem);
//       }
//    }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      if (init_str != NULL) {
         dw::core::ui::MultiLineTextResource *textres =
            (dw::core::ui::MultiLineTextResource*)embed->getResource();
         textres->setText(init_str ? init_str : "");
      }
      break;
   case DILLO_HTML_INPUT_FILE:
      {
         dw::core::ui::LabelButtonResource *lbr =
            (dw::core::ui::LabelButtonResource*)embed->getResource();
         lbr->setLabel(init_str);
      }
      break;
   default:
      break;
   }
}

/*
 * DilloHtmlSelect 
 */

/*
 * Constructor 
 */
DilloHtmlSelect::DilloHtmlSelect ()
{
   options = new misc::SimpleVector<DilloHtmlOption *> (4);
}

/*
 * Destructor 
 */
DilloHtmlSelect::~DilloHtmlSelect ()
{
   int size = options->size ();
   for (int k = 0; k < size; k++)
      delete options->get (k);
   delete options;
}

DilloHtmlOption *DilloHtmlSelect::getCurrentOption ()
{
   return options->get (options->size() - 1);
}

void DilloHtmlSelect::addOption (char *value, bool selected, bool enabled)
{
   DilloHtmlOption *option =
      new DilloHtmlOption (value, selected, enabled);
   int size = options->size ();
   options->increase ();
   options->set (size, option);
}

/*
 * Select the first option if nothing else is selected.
 */
void DilloHtmlSelect::ensureSelection()
{
   int size = options->size ();
   if (size > 0) {
      for (int i = 0; i < size; i++) {
            DilloHtmlOption *option = options->get (i);
            if (option->selected)
               return;
      }
      DilloHtmlOption *option = options->get (0);
      option->selected = true;
   }
}

void DilloHtmlSelect::addOptionsTo (dw::core::ui::SelectionResource *res)
{
   int size = options->size ();
   for (int i = 0; i < size; i++) {
      DilloHtmlOption *option = options->get (i);
      res->addItem(option->content, option->enabled, option->selected);
   }
}

void DilloHtmlSelect::appendValuesTo (Dlist *values,
                                      dw::core::ui::SelectionResource *res)
{
   int size = options->size ();
   for (int i = 0; i < size; i++) {
      if (res->isSelected (i)) {
         DilloHtmlOption *option = options->get (i);
         char *val = option->value ? option->value : option->content;
         dList_append(values, dStr_new(val));
      }
   }
}

/*
 * DilloHtmlOption 
 */

/*
 * Constructor 
 */
DilloHtmlOption::DilloHtmlOption (char *value2,
                                  bool selected2,
                                  bool enabled2)
{
   value = value2;
   content = NULL;
   selected = selected2;
   enabled = enabled2;
}

/*
 * Destructor 
 */
DilloHtmlOption::~DilloHtmlOption ()
{
   dFree(value);
   dFree(content);
}

/*
 * Utilities 
 */

/*
 * Create input image for the form
 */
static dw::core::ui::Embed *Html_input_image(DilloHtml *html,
                                             const char *tag, int tagsize)
{
   const char *attrbuf;
   StyleAttrs style_attrs;
   DilloImage *Image;
   dw::core::ui::Embed *button = NULL;
   DilloUrl *url = NULL;
  
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "src")) &&
       (url = a_Html_url_new(html, attrbuf, NULL, 0, 0, 0, 0))) {
      style_attrs = *S_TOP(html)->style;
      style_attrs.cursor = CURSOR_POINTER;

      /* create new image and add it to the button */
      if ((Image = a_Html_add_new_image(html, tag, tagsize, url, &style_attrs,
                                        false))) {
         Style *style = Style::create (HT2LT(html), &style_attrs);
         IM2DW(Image)->setStyle (style);
         dw::core::ui::ComplexButtonResource *complex_b_r =
            HT2LT(html)->getResourceFactory()->createComplexButtonResource(
                                                          IM2DW(Image), false);
         button = new dw::core::ui::Embed(complex_b_r);
         DW2TB(html->dw)->addWidget (button, style);
//       gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
         style->unref();

         /* a right button press brings up the image menu */
         html->connectSignals((Widget*)Image->dw);
      } else {
         a_Url_free(url);
      }
   }

   if (!button)
      DEBUG_MSG(10, "Html_input_image: unable to create image submit.\n");
   return button;
}

/*
 * ?
 */
static void Html_option_finish(DilloHtml *html)
{
   DilloHtmlInput *input = Html_get_current_input(html);
   if (input->type == DILLO_HTML_INPUT_SELECT ||
       input->type == DILLO_HTML_INPUT_SEL_LIST) {
      DilloHtmlOption *option =
         input->select->getCurrentOption ();
      option->content =
         a_Html_parse_entities(html, html->Stash->str, html->Stash->len);
   }
}
