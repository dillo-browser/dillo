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

#include "misc.h"
#include "msg.h"
#include "prefs.h"
#include "uicmd.hh"

using namespace lout;
using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::core::ui;

/*
 * Forward declarations
 */

class DilloHtmlReceiver;
class DilloHtmlSelect;

static Embed *Html_input_image(DilloHtml *html, const char *tag, int tagsize);

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
   bool showing_hiddens;
   bool enabled;
   void eventHandler(Resource *resource, EventButton *event);
   DilloUrl *buildQueryUrl(DilloHtmlInput *active_input);
   Dstr *buildQueryData(DilloHtmlInput *active_submit);
   char *makeMultipartBoundary(iconv_t char_encoder,
                               DilloHtmlInput *active_submit);
   Dstr *encodeText(iconv_t char_encoder, Dstr **input);
   void strUrlencodeAppend(Dstr *dstr, const char *str);
   void inputUrlencodeAppend(Dstr *data, const char *name, const char *value);
   void inputMultipartAppend(Dstr *data, const char *boundary,
                             const char *name, const char *value);
   void filesInputMultipartAppend(Dstr* data, const char *boundary,
                                  const char *name, Dstr *file,
                                  const char *filename);
   void imageInputUrlencodeAppend(Dstr *data, Dstr *name, Dstr *x, Dstr *y);
   void imageInputMultipartAppend(Dstr *data, const char *boundary, Dstr *name,
                                  Dstr *x, Dstr *y);

public:  //BUG: for now everything is public
   DilloHtmlMethod method;
   DilloUrl *action;
   DilloHtmlEnc content_type;
   char *submit_charset;

   lout::misc::SimpleVector<DilloHtmlInput*> *inputs;

   int num_entry_fields;

   DilloHtmlReceiver *form_receiver;

public:
   DilloHtmlForm (DilloHtml *html,
                  DilloHtmlMethod method, const DilloUrl *action,
                  DilloHtmlEnc content_type, const char *charset,
                  bool enabled);
   ~DilloHtmlForm ();
   DilloHtmlInput *getInput (Resource *resource);
   DilloHtmlInput *getRadioInput (const char *name);
   void submit(DilloHtmlInput *active_input, EventButton *event);
   void reset ();
   void display_hiddens(bool display);
   void addInput(DilloHtmlInput *input, DilloHtmlInputType type);
   void setEnabled(bool enabled);
};

class DilloHtmlReceiver:
   public Resource::ActivateReceiver,
   public Resource::ClickedReceiver
{
   friend class DilloHtmlForm;
   DilloHtmlForm* form;
   DilloHtmlReceiver (DilloHtmlForm* form2) { form = form2; }
   ~DilloHtmlReceiver () { }
   void activate (Resource *resource);
   void enter (Resource *resource);
   void leave (Resource *resource);
   void clicked (Resource *resource, EventButton *event);
};

class DilloHtmlInput {

   // DilloHtmlForm::addInput() calls connectTo()
   friend class DilloHtmlForm;

public:  //BUG: for now everything is public
   DilloHtmlInputType type;
   Embed *embed; /* May be NULL (think: hidden input) */
   char *name;
   char *init_str;    /* note: some overloading - for buttons, init_str
                         is simply the value of the button; for text
                         entries, it is the initial value */
   DilloHtmlSelect *select;
   bool init_val;     /* only meaningful for buttons */
   Dstr *file_data;   /* only meaningful for file inputs.
                         TODO: may become a list... */

private:
   void connectTo(DilloHtmlReceiver *form_receiver);
   void activate(DilloHtmlForm *form, int num_entry_fields,EventButton *event);
   void readFile(BrowserWindow *bw);

public:
   DilloHtmlInput (DilloHtmlInputType type, Embed *embed,
                   const char *name, const char *init_str, bool init_val);
   ~DilloHtmlInput ();
   void appendValuesTo(Dlist *values, bool is_active_submit);
   void reset();
   void setEnabled(bool enabled) {if (embed) embed->setEnabled(enabled); };
};

class DilloHtmlOptbase
{
public:
   virtual ~DilloHtmlOptbase () {};
   virtual bool isSelected() {return false;}
   virtual bool select() {return false;}
   virtual const char *getValue() {return NULL;}
   virtual void setContent(const char *str, int len)
      {MSG_ERR("Form: Optbase setContent()\n");}
   virtual void addSelf(SelectionResource *res) = 0;
};

class DilloHtmlOptgroup : public DilloHtmlOptbase {
private:
   char *label;
   bool enabled;
public:
   DilloHtmlOptgroup (char *label, bool enabled);
   virtual ~DilloHtmlOptgroup ();
   void addSelf (SelectionResource *res)
      {res->pushGroup(label, enabled);}
};

class DilloHtmlOptgroupClose : public DilloHtmlOptbase {
public:
   virtual ~DilloHtmlOptgroupClose () {};
   void addSelf (SelectionResource *res)
      {res->popGroup();}
};

class DilloHtmlOption : public DilloHtmlOptbase {
   friend class DilloHtmlSelect;
public:
   char *value, *label, *content;
   bool selected, enabled;
   DilloHtmlOption (char *value, char *label, bool selected, bool enabled);
   virtual ~DilloHtmlOption ();
   bool isSelected() {return selected;}
   bool select() {return (selected = true);}
   const char *getValue() {return value ? value : content;}
   void setContent(const char *str, int len) {content = dStrndup(str, len);}
   void addSelf (SelectionResource *res)
      {res->addItem(label ? label : content, enabled, selected);}
};

class DilloHtmlSelect {
   friend class DilloHtmlInput;
private:
   lout::misc::SimpleVector<DilloHtmlOptbase *> *opts;
   DilloHtmlSelect ();
   ~DilloHtmlSelect ();
public:
   DilloHtmlOptbase *getCurrentOpt ();
   void addOpt (DilloHtmlOptbase *opt);
   void ensureSelection ();
   void addOptsTo (SelectionResource *res);
   void reset (SelectionResource *res);
   void appendValuesTo (Dlist *values, SelectionResource *res);
};

/*
 * Form API
 */

DilloHtmlForm *a_Html_form_new (DilloHtml *html, DilloHtmlMethod method,
                                const DilloUrl *action,
                                DilloHtmlEnc content_type, const char *charset,
                                bool enabled)
{
   return new DilloHtmlForm (html, method, action, content_type, charset,
                             enabled);
}

void a_Html_form_delete (DilloHtmlForm *form)
{
   delete form;
}

void a_Html_input_delete (DilloHtmlInput *input)
{
   delete input;
}

void a_Html_form_submit2(void *vform)
{
   ((DilloHtmlForm *)vform)->submit(NULL, NULL);
}

void a_Html_form_reset2(void *vform)
{
   ((DilloHtmlForm *)vform)->reset();
}

void a_Html_form_display_hiddens2(void *vform, bool display)
{
   ((DilloHtmlForm *)vform)->display_hiddens(display);
}

/*
 * Form parsing functions
 */

/*
 * Add an HTML control
 */
static void Html_add_input(DilloHtml *html, DilloHtmlInputType type,
                           Embed *embed, const char *name,
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

      if (html->bw->NumPendingStyleSheets > 0) {
         input->setEnabled(false);
      }
   }
}

/*
 * Find radio input by name
 */
static DilloHtmlInput *Html_get_radio_input(DilloHtml *html, const char *name)
{
   if (name) {
      lout::misc::SimpleVector<DilloHtmlInput*>* inputs;

      if (html->InFlags & IN_FORM)
         inputs = html->getCurrentForm()->inputs;
      else
         inputs = html->inputs_outside_form;

      for (int idx = 0; idx < inputs->size(); idx++) {
         DilloHtmlInput *input = inputs->get(idx);
         if (input->type == DILLO_HTML_INPUT_RADIO &&
             input->name && !dStrAsciiCasecmp(input->name, name))
            return input;
      }
   }
   return NULL;
}

/*
 * Get the current input if available.
 */
static DilloHtmlInput *Html_get_current_input(DilloHtml *html)
{
   lout::misc::SimpleVector<DilloHtmlInput*>* inputs;

   if (html->InFlags & IN_FORM)
      inputs = html->getCurrentForm()->inputs;
   else
      inputs = html->inputs_outside_form;

   return (inputs && inputs->size() > 0) ?
            inputs->get (inputs->size() - 1) : NULL;
}

/*
 * Handle <FORM> tag
 */
void Html_tag_open_form(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *action;
   DilloHtmlMethod method;
   DilloHtmlEnc content_type;
   char *charset, *first;
   const char *attrbuf;

   HT2TB(html)->addParbreak (9, html->wordStyle ());

   if (html->InFlags & IN_FORM) {
      BUG_MSG("Nested <form>.");
      return;
   }
   html->InFlags |= IN_FORM;
   html->InFlags &= ~IN_SELECT;
   html->InFlags &= ~IN_OPTION;
   html->InFlags &= ~IN_TEXTAREA;

   method = DILLO_HTML_METHOD_GET;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "method"))) {
      if (!dStrAsciiCasecmp(attrbuf, "post")) {
         method = DILLO_HTML_METHOD_POST;
      } else if (dStrAsciiCasecmp(attrbuf, "get")) {
         BUG_MSG("<form> submission method unknown: '%s'.", attrbuf);
      }
   }
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "action")))
      action = a_Html_url_new(html, attrbuf, NULL, 0);
   else {
      if (html->DocType != DT_HTML || html->DocTypeVersion <= 4.01f)
         BUG_MSG("<form> requires action attribute.");
      action = a_Url_dup(html->base_url);
   }
   content_type = DILLO_HTML_ENC_URLENCODED;
   if ((method == DILLO_HTML_METHOD_POST) &&
       ((attrbuf = a_Html_get_attr(html, tag, tagsize, "enctype")))) {
      if (!dStrAsciiCasecmp(attrbuf, "multipart/form-data"))
         content_type = DILLO_HTML_ENC_MULTIPART;
   }
   charset = NULL;
   first = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "accept-charset"))) {
      /* a list of acceptable charsets, separated by commas or spaces */
      char *ptr = first = dStrdup(attrbuf);
      while (ptr && !charset) {
         char *curr = dStrsep(&ptr, " ,");
         if (!dStrAsciiCasecmp(curr, "utf-8")) {
            charset = curr;
         } else if (!dStrAsciiCasecmp(curr, "UNKNOWN")) {
            /* defined to be whatever encoding the document is in */
            charset = html->charset;
         }
      }
      if (!charset)
         charset = first;
   }
   if (!charset)
      charset = html->charset;
   html->formNew(method, action, content_type, charset);
   dFree(first);
   a_Url_free(action);
}

void Html_tag_close_form(DilloHtml *html)
{
   html->InFlags &= ~IN_FORM;
   html->InFlags &= ~IN_SELECT;
   html->InFlags &= ~IN_OPTION;
   html->InFlags &= ~IN_TEXTAREA;
}

/*
 * get size, restrict it to reasonable value
 */
static int Html_input_get_size(DilloHtml *html, const char *attrbuf)
{
   const int MAX_SIZE = 1024;
   int size = 20;

   if (attrbuf) {
      size = strtol(attrbuf, NULL, 10);
      if (size < 1 || size > MAX_SIZE) {
         int badSize = size;
         size = (size < 1 ? 20 : MAX_SIZE);
         BUG_MSG("<input> size=%d, using size=%d instead.", badSize, size);
      }
   }
   return size;
}

/*
 * Add a new input to current form
 */
void Html_tag_open_input(DilloHtml *html, const char *tag, int tagsize)
{
   DilloHtmlInputType inp_type;
   Resource *resource = NULL;
   Embed *embed = NULL;
   char *value, *name, *type, *init_str, *placeholder = NULL;
   const char *attrbuf, *label;
   bool init_val = false;
   ResourceFactory *factory;

   if (html->InFlags & IN_SELECT) {
      BUG_MSG("<input> inside <select>.");
      return;
   }
   if (html->InFlags & IN_BUTTON) {
      BUG_MSG("<input> inside <button>.");
      return;
   }

   factory = HT2LT(html)->getResourceFactory();

   /* Get 'value', 'name' and 'type' */
   value = a_Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
   name = a_Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   type = a_Html_get_attr_wdef(html, tag, tagsize, "type", "");

   init_str = NULL;
   inp_type = DILLO_HTML_INPUT_UNKNOWN;
   if (!dStrAsciiCasecmp(type, "password")) {
      inp_type = DILLO_HTML_INPUT_PASSWORD;
      placeholder = a_Html_get_attr_wdef(html, tag,tagsize,"placeholder",NULL);
      attrbuf = a_Html_get_attr(html, tag, tagsize, "size");
      int size = Html_input_get_size(html, attrbuf);
      resource = factory->createEntryResource (size, true, NULL, placeholder);
      init_str = value;
   } else if (!dStrAsciiCasecmp(type, "checkbox")) {
      inp_type = DILLO_HTML_INPUT_CHECKBOX;
      resource = factory->createCheckButtonResource(false);
      init_val = (a_Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : dStrdup("on");
   } else if (!dStrAsciiCasecmp(type, "radio")) {
      inp_type = DILLO_HTML_INPUT_RADIO;
      RadioButtonResource *rb_r = NULL;
      DilloHtmlInput *input = Html_get_radio_input(html, name);
      if (input)
         rb_r = (RadioButtonResource*) input->embed->getResource();
      resource = factory->createRadioButtonResource(rb_r, false);
      init_val = (a_Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = value;
   } else if (!dStrAsciiCasecmp(type, "hidden")) {
      inp_type = DILLO_HTML_INPUT_HIDDEN;
      init_str = value;
      int size = Html_input_get_size(html, NULL);
      resource = factory->createEntryResource(size, false, name, NULL);
   } else if (!dStrAsciiCasecmp(type, "submit")) {
      inp_type = DILLO_HTML_INPUT_SUBMIT;
      init_str = (value) ? value : dStrdup("submit");
      resource = factory->createLabelButtonResource(init_str);
   } else if (!dStrAsciiCasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_RESET;
      init_str = (value) ? value : dStrdup("Reset");
      resource = factory->createLabelButtonResource(init_str);
   } else if (!dStrAsciiCasecmp(type, "image")) {
      if (URL_FLAGS(html->base_url) & URL_SpamSafe) {
         /* Don't request the image; make a text submit button instead */
         inp_type = DILLO_HTML_INPUT_SUBMIT;
         attrbuf = a_Html_get_attr(html, tag, tagsize, "alt");
         label = attrbuf ? attrbuf : value ? value : name ? name : "Submit";
         init_str = dStrdup(label);
         resource = factory->createLabelButtonResource(init_str);
      } else {
         inp_type = DILLO_HTML_INPUT_IMAGE;
         /* use a dw_image widget */
         embed = Html_input_image(html, tag, tagsize);
         init_str = value;
      }
   } else if (!dStrAsciiCasecmp(type, "file")) {
      bool valid = true;
      if (html->InFlags & IN_FORM) {
         DilloHtmlForm *form = html->getCurrentForm();
         if (form->method != DILLO_HTML_METHOD_POST) {
            valid = false;
            BUG_MSG("<form> with file input MUST use HTTP POST method.");
            MSG("File input ignored in form not using HTTP POST method\n");
         } else if (form->content_type != DILLO_HTML_ENC_MULTIPART) {
            valid = false;
            BUG_MSG("<form> with file input MUST use multipart/form-data"
                    " encoding.");
            MSG("File input ignored in form not using multipart/form-data"
                " encoding\n");
         }
      }
      if (valid) {
         inp_type = DILLO_HTML_INPUT_FILE;
         init_str = dStrdup("File selector");
         resource = factory->createLabelButtonResource(init_str);
      }
   } else if (!dStrAsciiCasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
      if (value) {
         init_str = value;
         resource = factory->createLabelButtonResource(init_str);
      }
   } else {
      /* Text input, which also is the default */
      inp_type = DILLO_HTML_INPUT_TEXT;
      placeholder = a_Html_get_attr_wdef(html, tag,tagsize,"placeholder",NULL);
      attrbuf = a_Html_get_attr(html, tag, tagsize, "size");
      int size = Html_input_get_size(html, attrbuf);
      resource = factory->createEntryResource(size, false, NULL, placeholder);
      init_str = value;
   }
   if (resource)
      embed = new Embed (resource);

   if (inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      Html_add_input(html, inp_type, embed, name,
                     (init_str) ? init_str : "", init_val);
   }

   if (embed != NULL && inp_type != DILLO_HTML_INPUT_IMAGE &&
       inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      if (inp_type == DILLO_HTML_INPUT_HIDDEN) {
         /* TODO Perhaps do this with access to current form setting */
         embed->setDisplayed(false);
      }
      if (inp_type == DILLO_HTML_INPUT_TEXT ||
          inp_type == DILLO_HTML_INPUT_PASSWORD) {
         if (a_Html_get_attr(html, tag, tagsize, "readonly"))
            ((EntryResource *) resource)->setEditable(false);

         /* Maximum length of the text in the entry */
         if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "maxlength"))) {
            int maxlen = strtol(attrbuf, NULL, 10);
            ((EntryResource *) resource)->setMaxLength(maxlen);
         }
      }
      if (prefs.show_tooltip &&
          (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

         html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                           attrbuf);
      }
      HT2TB(html)->addWidget (embed, html->backgroundStyle());
   }
   dFree(type);
   dFree(name);
   if (init_str != value)
      dFree(init_str);
   dFree(placeholder);
   dFree(value);
}

/*
 * The ISINDEX tag is just a deprecated form of <INPUT type=text> with
 * implied FORM, afaics.
 */
void Html_tag_open_isindex(DilloHtml *html, const char *tag, int tagsize)
{
   DilloUrl *action;
   Embed *embed;
   const char *attrbuf;

   if (html->InFlags & IN_FORM) {
      MSG("<isindex> inside <form> not handled.\n");
      return;
   }

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "action")))
      action = a_Html_url_new(html, attrbuf, NULL, 0);
   else
      action = a_Url_dup(html->base_url);

   html->formNew(DILLO_HTML_METHOD_GET, action, DILLO_HTML_ENC_URLENCODED,
                 html->charset);
   html->InFlags |= IN_FORM;

   HT2TB(html)->addParbreak (9, html->wordStyle ());

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "prompt")))
      HT2TB(html)->addText(attrbuf, html->wordStyle ());

   ResourceFactory *factory = HT2LT(html)->getResourceFactory();
   EntryResource *entryResource = factory->createEntryResource (20, false,
                                                                NULL, NULL);
   embed = new Embed (entryResource);
   Html_add_input(html, DILLO_HTML_INPUT_INDEX, embed, NULL, NULL, FALSE);

   HT2TB(html)->addWidget (embed, html->backgroundStyle ());

   a_Url_free(action);
   html->InFlags &= ~IN_FORM;
}

void Html_tag_open_textarea(DilloHtml *html, const char *tag, int tagsize)
{
   assert((html->InFlags & (IN_BUTTON | IN_SELECT | IN_TEXTAREA)) == 0);

   html->InFlags |= IN_TEXTAREA;
}

/*
 * The textarea tag
 */
void Html_tag_content_textarea(DilloHtml *html, const char *tag, int tagsize)
{
   const int MAX_COLS=1024, MAX_ROWS=10000;

   char *name;
   const char *attrbuf;
   int cols, rows;

   a_Html_stash_init(html);
   S_TOP(html)->parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "cols"))) {
      cols = strtol(attrbuf, NULL, 10);
   } else {
      if (html->DocType != DT_HTML || html->DocTypeVersion <= 4.01f)
         BUG_MSG("<textarea> requires cols attribute.");
      cols = 20;
   }
   if (cols < 1 || cols > MAX_COLS) {
      int badCols = cols;
      cols = (cols < 1 ? 20 : MAX_COLS);
      BUG_MSG("<textarea> cols=%d, using cols=%d instead.", badCols, cols);
   }
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "rows"))) {
      rows = strtol(attrbuf, NULL, 10);
   } else {
      if (html->DocType != DT_HTML || html->DocTypeVersion <= 4.01f)
         BUG_MSG("<textarea> requires rows attribute.");
      rows = 10;
   }
   if (rows < 1 || rows > MAX_ROWS) {
      int badRows = rows;
      rows = (rows < 1 ? 2 : MAX_ROWS);
      BUG_MSG("<textarea> rows=%d, using rows=%d instead.", badRows, rows);
   }
   name = NULL;
   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "name")))
      name = dStrdup(attrbuf);

   attrbuf = a_Html_get_attr(html, tag, tagsize, "placeholder");

   ResourceFactory *factory = HT2LT(html)->getResourceFactory();
   MultiLineTextResource *textres =
      factory->createMultiLineTextResource (cols, rows, attrbuf);

   Embed *embed = new Embed(textres);
   /* Readonly or not? */
   if (a_Html_get_attr(html, tag, tagsize, "readonly"))
      textres->setEditable(false);
   Html_add_input(html, DILLO_HTML_INPUT_TEXTAREA, embed, name, NULL, false);

   HT2TB(html)->addWidget (embed, html->backgroundStyle ());
   dFree(name);
}

/*
 * Close  textarea
 * (TEXTAREA is parsed in VERBATIM mode, and entities are handled here)
 */
void Html_tag_close_textarea(DilloHtml *html)
{
   char *str;
   DilloHtmlInput *input;
   int i;

   if (html->InFlags & IN_TEXTAREA && !S_TOP(html)->display_none) {
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
      if (input) {
         input->init_str = str;
         ((MultiLineTextResource *)input->embed->getResource ())->setText(str);
      }

   }
   html->InFlags &= ~IN_TEXTAREA;
}

/*
 * <SELECT>
 */
/* The select tag is quite tricky, because of gorpy html syntax. */
void Html_tag_open_select(DilloHtml *html, const char *tag, int tagsize)
{
   const char *attrbuf;
   int rows = 0;

   assert((html->InFlags & (IN_BUTTON | IN_SELECT | IN_TEXTAREA)) == 0);

   html->InFlags |= IN_SELECT;
   html->InFlags &= ~IN_OPTION;

   char *name = a_Html_get_attr_wdef(html, tag, tagsize, "name", NULL);
   ResourceFactory *factory = HT2LT(html)->getResourceFactory ();
   DilloHtmlInputType type;
   SelectionResource *res;
   bool multi = a_Html_get_attr(html, tag, tagsize, "multiple") != NULL;

   if ((attrbuf = a_Html_get_attr(html, tag, tagsize, "size"))) {
      rows = strtol(attrbuf, NULL, 10);
      if (rows > 100)
         rows = 100;
   }
   if (rows < 1)
      rows = multi ? 10 : 1;

   if (rows == 1 && multi == false) {
      type = DILLO_HTML_INPUT_SELECT;
      res = factory->createOptionMenuResource ();
   } else {
      ListResource::SelectionMode mode;

      type = DILLO_HTML_INPUT_SEL_LIST;
      mode = multi ? ListResource::SELECTION_MULTIPLE
                   : ListResource::SELECTION_AT_MOST_ONE;
      res = factory->createListResource (mode, rows);
   }
   Embed *embed = new Embed(res);

   if (prefs.show_tooltip &&
       (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

      html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                        attrbuf);
   }
   HT2TB(html)->addWidget (embed, html->backgroundStyle ());

   Html_add_input(html, type, embed, name, NULL, false);
   a_Html_stash_init(html);
   dFree(name);
}

/*
 * ?
 */
void Html_tag_close_select(DilloHtml *html)
{
   if (html->InFlags & IN_SELECT) {
      if (html->InFlags & IN_OPTION)
         Html_option_finish(html);
      html->InFlags &= ~IN_SELECT;
      html->InFlags &= ~IN_OPTION;

      DilloHtmlInput *input = Html_get_current_input(html);
      if (input) {
         DilloHtmlSelect *select = input->select;
         if (input->type == DILLO_HTML_INPUT_SELECT) {
            // option menu interface requires that something be selected */
            select->ensureSelection ();
         }
         select->addOptsTo ((SelectionResource*)input->embed->getResource());
      }
   }
}

void Html_tag_open_optgroup(DilloHtml *html, const char *tag, int tagsize)
{
   if (!(html->InFlags & IN_SELECT)) {
      BUG_MSG("<optgroup> outside <select>.");
      return;
   }
   if (html->InFlags & IN_OPTGROUP) {
      BUG_MSG("Nested <optgroup>.");
      return;
   }
   if (html->InFlags & IN_OPTION) {
      Html_option_finish(html);
      html->InFlags &= ~IN_OPTION;
   }

   html->InFlags |= IN_OPTGROUP;

   DilloHtmlInput *input = Html_get_current_input(html);
   if (input &&
       (input->type == DILLO_HTML_INPUT_SELECT ||
        input->type == DILLO_HTML_INPUT_SEL_LIST)) {
      char *label = a_Html_get_attr_wdef(html, tag, tagsize, "label", NULL);
      bool enabled = (a_Html_get_attr(html, tag, tagsize, "disabled") == NULL);

      if (!label) {
         BUG_MSG("<optgroup> requires label attribute.");
         label = strdup("");
      }

      DilloHtmlOptgroup *opt =
         new DilloHtmlOptgroup (label, enabled);

      input->select->addOpt(opt);
   }
}

void Html_tag_close_optgroup(DilloHtml *html)
{
   if (html->InFlags & IN_OPTGROUP) {
      html->InFlags &= ~IN_OPTGROUP;

      if (html->InFlags & IN_OPTION) {
         Html_option_finish(html);
         html->InFlags &= ~IN_OPTION;
      }

      DilloHtmlInput *input = Html_get_current_input(html);
      if (input &&
          (input->type == DILLO_HTML_INPUT_SELECT ||
           input->type == DILLO_HTML_INPUT_SEL_LIST)) {
         DilloHtmlOptgroupClose *opt = new DilloHtmlOptgroupClose ();

         input->select->addOpt(opt);
      }
   }
}

/*
 * <OPTION>
 */
void Html_tag_open_option(DilloHtml *html, const char *tag, int tagsize)
{
   if (!(html->InFlags & IN_SELECT)) {
      BUG_MSG("<option> outside <select>.");
      return;
   }
   if (html->InFlags & IN_OPTION)
      Html_option_finish(html);
   html->InFlags |= IN_OPTION;

   DilloHtmlInput *input = Html_get_current_input(html);
   if (input &&
       (input->type == DILLO_HTML_INPUT_SELECT ||
        input->type == DILLO_HTML_INPUT_SEL_LIST)) {
      char *value = a_Html_get_attr_wdef(html, tag, tagsize, "value", NULL);
      char *label = a_Html_get_attr_wdef(html, tag, tagsize, "label", NULL);
      bool selected = (a_Html_get_attr(html, tag, tagsize,"selected") != NULL);
      bool enabled = (a_Html_get_attr(html, tag, tagsize, "disabled") == NULL);

      DilloHtmlOption *option =
         new DilloHtmlOption (value, label, selected, enabled);

      input->select->addOpt(option);
   }

   a_Html_stash_init(html);
}

void Html_tag_close_option(DilloHtml *html)
{
   if (html->InFlags & IN_OPTION) {
      Html_option_finish(html);
      html->InFlags &= ~IN_OPTION;
   }
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

   assert((html->InFlags & (IN_BUTTON | IN_SELECT | IN_TEXTAREA)) == 0);

   html->InFlags |= IN_BUTTON;
   type = a_Html_get_attr_wdef(html, tag, tagsize, "type", "");

   if (!dStrAsciiCasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
   } else if (!dStrAsciiCasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_BUTTON_RESET;
   } else if (!dStrAsciiCasecmp(type, "submit") || !*type) {
      /* submit button is the default */
      inp_type = DILLO_HTML_INPUT_BUTTON_SUBMIT;
   } else {
      inp_type = DILLO_HTML_INPUT_UNKNOWN;
      BUG_MSG("<button> type unknown: '%s'.", type);
   }

   if (inp_type != DILLO_HTML_INPUT_UNKNOWN) {
      /* Render the button */
      Widget *page;
      Embed *embed;
      const char *attrbuf;
      char *name, *value;

      if (prefs.show_tooltip &&
          (attrbuf = a_Html_get_attr(html, tag, tagsize, "title"))) {

         html->styleEngine->setNonCssHint (PROPERTY_X_TOOLTIP, CSS_TYPE_STRING,
                                           attrbuf);
      }
      /* We used to have Textblock (prefs.limit_text_width, ...) here,
       * but it caused 100% CPU usage.
       */
      page = new Textblock (false);
      page->setStyle (html->backgroundStyle ());

      ResourceFactory *factory = HT2LT(html)->getResourceFactory();
      Resource *resource = factory->createComplexButtonResource(page, true);
      embed = new Embed(resource);
// a_Dw_button_set_sensitive (DW_BUTTON (button), FALSE);

      HT2TB(html)->addParbreak (5, html->wordStyle ());
      HT2TB(html)->addWidget (embed, html->backgroundStyle ());
      HT2TB(html)->addParbreak (5, html->wordStyle ());

      S_TOP(html)->textblock = html->dw = page;

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
void Html_tag_close_button(DilloHtml *html)
{
   html->InFlags &= ~IN_BUTTON;
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
                              DilloHtmlEnc content_type2,
                              const char *charset, bool enabled)
{
   html = html2;
   method = method2;
   action = a_Url_dup(action2);
   content_type = content_type2;
   submit_charset = dStrdup(charset);
   inputs = new misc::SimpleVector <DilloHtmlInput*> (4);
   num_entry_fields = 0;
   showing_hiddens = false;
   this->enabled = enabled;
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

void DilloHtmlForm::eventHandler(Resource *resource, EventButton *event)
{
   _MSG("DilloHtmlForm::eventHandler\n");
   if (event && (event->button == 3)) {
      a_UIcmd_form_popup(html->bw, html->page_url, this, showing_hiddens);
   } else {
      DilloHtmlInput *input = getInput(resource);
      if (input) {
         input->activate (this, num_entry_fields, event);
      } else {
        MSG("DilloHtmlForm::eventHandler: ERROR, input not found!\n");
      }
   }
}

/*
 * Submit.
 * (Called by eventHandler())
 */
void DilloHtmlForm::submit(DilloHtmlInput *active_input, EventButton *event)
{
   DilloUrl *url = buildQueryUrl(active_input);
   if (url) {
      if (event && event->button == 2) {
         if (prefs.middle_click_opens_new_tab) {
            int focus = prefs.focus_new_tab ? 1 : 0;
            if (event->state == SHIFT_MASK) focus = !focus;
            a_UIcmd_open_url_nt(html->bw, url, focus);
         } else {
            a_UIcmd_open_url_nw(html->bw, url);
         }
      } else {
         a_UIcmd_open_url(html->bw, url);
      }
      a_Url_free(url);
   }
}

/*
 * Build a new query URL.
 * (Called by submit())
 */
DilloUrl *DilloHtmlForm::buildQueryUrl(DilloHtmlInput *active_input)
{
   DilloUrl *new_url = NULL;

   if ((method == DILLO_HTML_METHOD_GET) ||
       (method == DILLO_HTML_METHOD_POST)) {
      Dstr *DataStr;
      DilloHtmlInput *active_submit = NULL;

      _MSG("DilloHtmlForm::buildQueryUrl: action=%s\n",URL_STR_(action));

      if (active_input) {
         if ((active_input->type == DILLO_HTML_INPUT_SUBMIT) ||
             (active_input->type == DILLO_HTML_INPUT_IMAGE) ||
             (active_input->type == DILLO_HTML_INPUT_BUTTON_SUBMIT)) {
            active_submit = active_input;
         }
      }

      DataStr = buildQueryData(active_submit);
      if (DataStr) {
         /* action was previously resolved against base URL */
         char *action_str = dStrdup(URL_STR(action));

         if (method == DILLO_HTML_METHOD_POST) {
            new_url = a_Url_new(action_str, NULL);
            /* new_url keeps the dStr and sets DataStr to NULL */
            a_Url_set_data(new_url, &DataStr);
            a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Post);
            if (content_type == DILLO_HTML_ENC_MULTIPART)
               a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_MultipartEnc);
         } else {
            /* remove <fragment> and <query> sections if present */
            char *url_str, *p;
            if ((p = strchr(action_str, '#')))
               *p = 0;
            if ((p = strchr(action_str, '?')))
               *p = 0;

            url_str = dStrconcat(action_str, "?", DataStr->str, NULL);
            new_url = a_Url_new(url_str, NULL);
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
Dstr *DilloHtmlForm::buildQueryData(DilloHtmlInput *active_submit)
{
   Dstr *DataStr = NULL;
   char *boundary = NULL;
   iconv_t char_encoder = (iconv_t) -1;

   if (submit_charset && dStrAsciiCasecmp(submit_charset, "UTF-8")) {
      /* Some iconv implementations, given "//TRANSLIT", will do their best to
       * transliterate the string. Under the circumstances, doing so is likely
       * for the best.
       */
      char *translit = dStrconcat(submit_charset, "//TRANSLIT", NULL);

      char_encoder = iconv_open(translit, "UTF-8");
      dFree(translit);

      if (char_encoder == (iconv_t) -1)
         char_encoder = iconv_open(submit_charset, "UTF-8");

      if (char_encoder == (iconv_t) -1) {
         MSG_WARN("Cannot convert to character encoding '%s'\n",
                  submit_charset);
      } else {
         MSG("Form character encoding: '%s'\n", submit_charset);
      }
   }

   if (content_type == DILLO_HTML_ENC_MULTIPART) {
      if (!(boundary = makeMultipartBoundary(char_encoder, active_submit)))
         MSG_ERR("Cannot generate multipart/form-data boundary.\n");
   }

   if ((content_type == DILLO_HTML_ENC_URLENCODED) || (boundary != NULL)) {
      Dlist *values = dList_new(5);

      DataStr = dStr_sized_new(4096);
      for (int i = 0; i < inputs->size(); i++) {
         DilloHtmlInput *input = inputs->get (i);
         Dstr *name = dStr_new(input->name);
         bool is_active_submit = (input == active_submit);
         int valcount;

         name = encodeText(char_encoder, &name);

         input->appendValuesTo(values, is_active_submit);

         if ((valcount = dList_length(values)) > 0) {
            if (input->type == DILLO_HTML_INPUT_FILE) {
               if (valcount > 1)
                  MSG_WARN("multiple files per form control not supported\n");
               Dstr *file = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, file);

               /* Get filename and encode it. Do not encode file contents. */
               LabelButtonResource *lbr =
                            (LabelButtonResource*) input->embed->getResource();
               const char *filename = lbr->getLabel();
               if (filename[0] && strcmp(filename, input->init_str)) {
                  const char *p = strrchr(filename, '/');
                  if (p)
                     filename = p + 1;     /* don't reveal path */
                  Dstr *dfilename = dStr_new(filename);
                  dfilename = encodeText(char_encoder, &dfilename);
                  filesInputMultipartAppend(DataStr, boundary, name->str,
                                            file, dfilename->str);
                  dStr_free(dfilename, 1);
               }
               dStr_free(file, 1);
            } else if (input->type == DILLO_HTML_INPUT_INDEX) {
               /* no name */
               Dstr *val = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, val);
               val = encodeText(char_encoder, &val);
               strUrlencodeAppend(DataStr, val->str);
               dStr_free(val, 1);
            } else if (input->type == DILLO_HTML_INPUT_IMAGE) {
               Dstr *x, *y;
               x = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, x);
               y = (Dstr *) dList_nth_data(values, 0);
               dList_remove(values, y);
               if (content_type == DILLO_HTML_ENC_URLENCODED)
                  imageInputUrlencodeAppend(DataStr, name, x, y);
               else if (content_type == DILLO_HTML_ENC_MULTIPART)
                  imageInputMultipartAppend(DataStr, boundary, name, x, y);
               dStr_free(x, 1);
               dStr_free(y, 1);
            } else {
               for (int j = 0; j < valcount; j++) {
                  Dstr *val = (Dstr *) dList_nth_data(values, 0);
                  dList_remove(values, val);
                  val = encodeText(char_encoder, &val);
                  if (content_type == DILLO_HTML_ENC_URLENCODED)
                     inputUrlencodeAppend(DataStr, name->str, val->str);
                  else if (content_type == DILLO_HTML_ENC_MULTIPART)
                     inputMultipartAppend(DataStr, boundary, name->str,
                                          val->str);
                  dStr_free(val, 1);
               }
            }
         }
         dStr_free(name, 1);
      }
      if (DataStr->len > 0) {
         if (content_type == DILLO_HTML_ENC_URLENCODED) {
            if (DataStr->str[DataStr->len - 1] == '&')
               dStr_truncate(DataStr, DataStr->len - 1);
         } else if (content_type == DILLO_HTML_ENC_MULTIPART) {
            dStr_append(DataStr, "--");
         }
      }
      dList_free(values);
   }
   dFree(boundary);
   if (char_encoder != (iconv_t) -1)
      (void)iconv_close(char_encoder);
   return DataStr;
}

/*
 * Generate a boundary string for use in separating the parts of a
 * multipart/form-data submission.
 */
char *DilloHtmlForm::makeMultipartBoundary(iconv_t char_encoder,
                                           DilloHtmlInput *active_submit)
{
   const int max_tries = 10;
   Dlist *values = dList_new(5);
   Dstr *DataStr = dStr_new("");
   Dstr *boundary = dStr_new("");
   char *ret = NULL;

   /* fill DataStr with names, filenames, and values */
   for (int i = 0; i < inputs->size(); i++) {
      Dstr *dstr;
      DilloHtmlInput *input = inputs->get (i);
      bool is_active_submit = (input == active_submit);
      input->appendValuesTo(values, is_active_submit);

      if (input->name) {
         dstr = dStr_new(input->name);
         dstr = encodeText(char_encoder, &dstr);
         dStr_append_l(DataStr, dstr->str, dstr->len);
         dStr_free(dstr, 1);
      }
      if (input->type == DILLO_HTML_INPUT_FILE) {
         LabelButtonResource *lbr =
            (LabelButtonResource*)input->embed->getResource();
         const char *filename = lbr->getLabel();
         if (filename[0] && strcmp(filename, input->init_str)) {
            dstr = dStr_new(filename);
            dstr = encodeText(char_encoder, &dstr);
            dStr_append_l(DataStr, dstr->str, dstr->len);
            dStr_free(dstr, 1);
         }
      }
      int length = dList_length(values);
      for (int i = 0; i < length; i++) {
         dstr = (Dstr *) dList_nth_data(values, 0);
         dList_remove(values, dstr);
         if (input->type != DILLO_HTML_INPUT_FILE)
            dstr = encodeText(char_encoder, &dstr);
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
 *               new Dstr when encoding (input Dstr is freed).
 */
Dstr *DilloHtmlForm::encodeText(iconv_t char_encoder, Dstr **input)
{
   int rc = 0;
   Dstr *output;
   const int bufsize = 128;
   inbuf_t *inPtr;
   char *buffer, *outPtr;
   size_t inLeft, outRoom;
   bool bad_chars = false;

   if ((char_encoder == (iconv_t) -1) || *input == NULL || (*input)->len == 0)
      return *input;

   output = dStr_new("");
   inPtr  = (*input)->str;
   inLeft = (*input)->len;
   buffer = dNew(char, bufsize);

   while ((rc != EINVAL) && (inLeft > 0)) {

      outPtr = buffer;
      outRoom = bufsize;

      rc = iconv(char_encoder, &inPtr, &inLeft, &outPtr, &outRoom);

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
         MSG_ERR("Form encode text: bad source string.\n");
      }
   }

   if (bad_chars) {
      /*
       * It might be friendly to inform the caller, who would know whether
       * it is safe to display the beginning of the string in a message
       * (isn't, e.g., a password).
       */
      MSG_WARN("Form encode text: string cannot be converted cleanly.\n");
   }

   dFree(buffer);
   dStr_free(*input, 1);

   return output;
}

/*
 * Urlencode 'str' and append it to 'dstr'
 */
void DilloHtmlForm::strUrlencodeAppend(Dstr *dstr, const char *str)
{
   char *encoded = a_Url_encode_hex_str(str);
   dStr_append(dstr, encoded);
   dFree(encoded);
}

/*
 * Append a name-value pair to url data using url encoding.
 */
void DilloHtmlForm::inputUrlencodeAppend(Dstr *data, const char *name,
                                         const char *value)
{
   if (name && name[0]) {
      strUrlencodeAppend(data, name);
      dStr_append_c(data, '=');
      strUrlencodeAppend(data, value);
      dStr_append_c(data, '&');
   }
}

/*
 * Append files to URL data using multipart encoding.
 * Currently only accepts one file.
 */
void DilloHtmlForm::filesInputMultipartAppend(Dstr* data,
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
          !dStrAsciiCasecmp(ctype, "text/plain") &&
          (!dStrAsciiCasecmp(ext, ".html") || !dStrAsciiCasecmp(ext, ".htm"))){
         ctype = "text/html";
      }

      if (data->len == 0) {
         dStr_append(data, "--");
         dStr_append(data, boundary);
      }
      dStr_sprintfa(data,
                    "\r\n"
                    "Content-Disposition: form-data; name=\"%s\"; "
                       "filename=\"", name);
      /*
       * Replace the characters that are the most likely to damage things.
       * For a while, there was some momentum to standardize on an encoding,
       * but HTML5/Ian Hickson/his Google masters are, as of late 2012,
       * evidently standing in opposition to all of that for some reason.
       */
      for (int i = 0; char c = filename[i]; i++) {
         if (c == '\"' || c == '\r' || c == '\n')
            c = '_';
         dStr_append_c(data, c);
      }
      dStr_sprintfa(data,
                    "\"\r\n"
                    "Content-Type: %s\r\n"
                    "\r\n", ctype);

      dStr_append_l(data, file->str, file->len);

      dStr_sprintfa(data,
                    "\r\n"
                    "--%s", boundary);
   }
}

/*
 * Append a name-value pair to url data using multipart encoding.
 */
void DilloHtmlForm::inputMultipartAppend(Dstr *data,
                                         const char *boundary,
                                         const char *name,
                                         const char *value)
{
   if (name && name[0]) {
      if (data->len == 0) {
         dStr_append(data, "--");
         dStr_append(data, boundary);
      }
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
void DilloHtmlForm::imageInputUrlencodeAppend(Dstr *data, Dstr *name, Dstr *x,
                                              Dstr *y)
{
   if (name->len) {
      strUrlencodeAppend(data, name->str);
      dStr_sprintfa(data, ".x=%s&", x->str);
      strUrlencodeAppend(data, name->str);
      dStr_sprintfa(data, ".y=%s&", y->str);
   } else
      dStr_sprintfa(data, "x=%s&y=%s&", x->str, y->str);
}

/*
 * Append an image button click position to url data using multipart encoding.
 */
void DilloHtmlForm::imageInputMultipartAppend(Dstr *data, const char *boundary,
                                              Dstr *name, Dstr *x, Dstr *y)
{
   int orig_len = name->len;

   if (orig_len)
      dStr_append_c(name, '.');
   dStr_append_c(name, 'x');

   inputMultipartAppend(data, boundary, name->str, x->str);
   dStr_truncate(name, name->len - 1);
   dStr_append_c(name, 'y');
   inputMultipartAppend(data, boundary, name->str, y->str);
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
 * Show/hide "hidden" form controls
 */
void DilloHtmlForm::display_hiddens(bool display)
{
   int size = inputs->size();
   for (int i = 0; i < size; i++) {
      DilloHtmlInput *input = inputs->get(i);
      if (input->type == DILLO_HTML_INPUT_HIDDEN) {
         input->embed->setDisplayed(display);
      }
   }
  showing_hiddens = display;
}

void DilloHtmlForm::setEnabled(bool enabled)
{
   for (int i = 0; i < inputs->size(); i++)
      inputs->get(i)->setEnabled(enabled);
}

/*
 * Add a new input.
 */
void DilloHtmlForm::addInput(DilloHtmlInput *input, DilloHtmlInputType type)
{
   input->connectTo (form_receiver);
   input->setEnabled (enabled);
   int ni = inputs->size ();
   inputs->increase ();
   inputs->set (ni,input);

   /* some stats */
   if (type == DILLO_HTML_INPUT_PASSWORD ||
       type == DILLO_HTML_INPUT_TEXT) {
      num_entry_fields++;
   }
}

/*
 * Return the input with a given resource.
 */
DilloHtmlInput *DilloHtmlForm::getInput (Resource *resource)
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
          input->name && !dStrAsciiCasecmp(input->name, name))
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

void DilloHtmlReceiver::activate (Resource *resource)
{
   form->eventHandler(resource, NULL);
}

/*
 * Enter a form control, as in "onmouseover".
 * For _pressing_ enter in a text control, see activate().
 */
void DilloHtmlReceiver::enter (Resource *resource)
{
   DilloHtml *html = form->html;
   DilloHtmlInput *input = form->getInput(resource);
   const char *msg = "";

   if ((input->type == DILLO_HTML_INPUT_SUBMIT) ||
       (input->type == DILLO_HTML_INPUT_IMAGE) ||
       (input->type == DILLO_HTML_INPUT_BUTTON_SUBMIT) ||
       (input->type == DILLO_HTML_INPUT_INDEX) ||
       ((prefs.enterpress_forces_submit || form->num_entry_fields == 1) &&
        ((input->type == DILLO_HTML_INPUT_PASSWORD) ||
         (input->type == DILLO_HTML_INPUT_TEXT)))) {
      /* The control can submit form. Show action URL. */
      msg = URL_STR(form->action);
   }
   a_UIcmd_set_msg(html->bw, "%s", msg);
}

/*
 * Leave a form control, or "onmouseout".
 */
void DilloHtmlReceiver::leave (Resource *resource)
{
   DilloHtml *html = form->html;
   a_UIcmd_set_msg(html->bw, "");
}

void DilloHtmlReceiver::clicked (Resource *resource,
                                 EventButton *event)
{
   form->eventHandler(resource, event);
}

/*
 * DilloHtmlInput
 */

/*
 * Constructor
 */
DilloHtmlInput::DilloHtmlInput (DilloHtmlInputType type2, Embed *embed2,
                                const char *name2, const char *init_str2,
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
   Resource *resource;
   if (embed && (resource = embed->getResource())) {
      resource->connectClicked (form_receiver);
      if (type == DILLO_HTML_INPUT_SUBMIT ||
          type == DILLO_HTML_INPUT_RESET ||
          type == DILLO_HTML_INPUT_BUTTON_SUBMIT ||
          type == DILLO_HTML_INPUT_BUTTON_RESET ||
          type == DILLO_HTML_INPUT_IMAGE ||
          type == DILLO_HTML_INPUT_FILE ||
          type == DILLO_HTML_INPUT_TEXT ||
          type == DILLO_HTML_INPUT_PASSWORD ||
          type == DILLO_HTML_INPUT_INDEX) {
         resource->connectActivate (form_receiver);
      }
   }
}

/*
 * Activate a form
 */
void DilloHtmlInput::activate(DilloHtmlForm *form, int num_entry_fields,
                              EventButton *event)
{
   switch (type) {
   case DILLO_HTML_INPUT_FILE:
      readFile (form->html->bw);
      break;
   case DILLO_HTML_INPUT_RESET:
   case DILLO_HTML_INPUT_BUTTON_RESET:
      form->reset();
      break;
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
      if (!(prefs.enterpress_forces_submit || num_entry_fields == 1)) {
         break;
      } else {
         /* fall through */
      }
   case DILLO_HTML_INPUT_SUBMIT:
   case DILLO_HTML_INPUT_BUTTON_SUBMIT:
   case DILLO_HTML_INPUT_IMAGE:
   case DILLO_HTML_INPUT_INDEX:
      form->submit(this, event);
      break;
   default:
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
         LabelButtonResource *lbr = (LabelButtonResource*)embed->getResource();
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
   case DILLO_HTML_INPUT_HIDDEN:
      {
         EntryResource *entryres = (EntryResource*)embed->getResource();
         dList_append(values, dStr_new(entryres->getText()));
      }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      {
         MultiLineTextResource *textres =
            (MultiLineTextResource*)embed->getResource();
         dList_append(values, dStr_new(textres->getText()));
      }
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
      {
         ToggleButtonResource *cb_r =
            (ToggleButtonResource*)embed->getResource();
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
   case DILLO_HTML_INPUT_SELECT:
   case DILLO_HTML_INPUT_SEL_LIST:
      {
         SelectionResource *sel_res = (SelectionResource*)embed->getResource();
         select->appendValuesTo (values, sel_res);
      }
      break;
   case DILLO_HTML_INPUT_FILE:
      {
         LabelButtonResource *lbr = (LabelButtonResource*)embed->getResource();
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
   case DILLO_HTML_INPUT_IMAGE:
      if (is_active_submit) {
         ComplexButtonResource *cbr =
            (ComplexButtonResource*)embed->getResource();
         Dstr *strX = dStr_new("");
         Dstr *strY = dStr_new("");
         dStr_sprintf(strX, "%d", cbr->getClickX());
         dStr_sprintf(strY, "%d", cbr->getClickY());
         dList_append(values, strX);
         dList_append(values, strY);
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
   case DILLO_HTML_INPUT_INDEX:
   case DILLO_HTML_INPUT_HIDDEN:
      {
         EntryResource *entryres = (EntryResource*)embed->getResource();
         entryres->setText(init_str ? init_str : "");
      }
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
   case DILLO_HTML_INPUT_RADIO:
      {
         ToggleButtonResource *tb_r =
            (ToggleButtonResource*)embed->getResource();
         tb_r->setActivated(init_val);
      }
      break;
   case DILLO_HTML_INPUT_SELECT:
   case DILLO_HTML_INPUT_SEL_LIST:
      if (select != NULL) {
         SelectionResource *sr = (SelectionResource *) embed->getResource();
         select->reset(sr);
      }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      if (init_str != NULL) {
         MultiLineTextResource *textres =
            (MultiLineTextResource*)embed->getResource();
         textres->setText(init_str ? init_str : "");
      }
      break;
   case DILLO_HTML_INPUT_FILE:
      {
         LabelButtonResource *lbr = (LabelButtonResource*)embed->getResource();
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
   opts = new misc::SimpleVector<DilloHtmlOptbase *> (4);
}

/*
 * Destructor
 */
DilloHtmlSelect::~DilloHtmlSelect ()
{
   int size = opts->size ();
   for (int k = 0; k < size; k++)
      delete opts->get (k);
   delete opts;
}

DilloHtmlOptbase *DilloHtmlSelect::getCurrentOpt ()
{
   return opts->get (opts->size() - 1);
}

void DilloHtmlSelect::addOpt (DilloHtmlOptbase *opt)
{
   int size = opts->size ();
   opts->increase ();
   opts->set (size, opt);
}

/*
 * Select the first option if nothing else is selected.
 */
void DilloHtmlSelect::ensureSelection()
{
   int size = opts->size ();
   if (size > 0) {
      for (int i = 0; i < size; i++) {
            DilloHtmlOptbase *opt = opts->get (i);
            if (opt->isSelected())
               return;
      }
      for (int i = 0; i < size; i++) {
         DilloHtmlOptbase *opt = opts->get (i);
         if (opt->select())
            break;
      }
   }
}

void DilloHtmlSelect::addOptsTo (SelectionResource *res)
{
   int size = opts->size ();
   for (int i = 0; i < size; i++) {
      DilloHtmlOptbase *opt = opts->get (i);
      opt->addSelf(res);
   }
}

void DilloHtmlSelect::reset (SelectionResource *res)
{
   int size = opts->size ();
   for (int i = 0; i < size; i++) {
      DilloHtmlOptbase *opt = opts->get (i);
      res->setItem(i, opt->isSelected());
   }
}

void DilloHtmlSelect::appendValuesTo (Dlist *values, SelectionResource *res)
{
   int size = opts->size ();
   for (int i = 0; i < size; i++) {
      if (res->isSelected (i)) {
         DilloHtmlOptbase *opt = opts->get (i);
         const char *val = opt->getValue();

         if (val)
            dList_append(values, dStr_new(val));
      }
   }
}

DilloHtmlOptgroup::DilloHtmlOptgroup (char *label, bool enabled)
{
   this->label = label;
   this->enabled = enabled;
}

DilloHtmlOptgroup::~DilloHtmlOptgroup ()
{
   dFree(label);
}

/*
 * DilloHtmlOption
 */

/*
 * Constructor
 */
DilloHtmlOption::DilloHtmlOption (char *value2, char *label2, bool selected2,
                                  bool enabled2)
{
   value = value2;
   label = label2;
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
   dFree(label);
   dFree(content);
}

/*
 * Utilities
 */

/*
 * Create input image for the form
 */
static Embed *Html_input_image(DilloHtml *html, const char *tag, int tagsize)
{
   DilloImage *Image;
   Embed *button = NULL;

   html->styleEngine->setPseudoLink ();

   /* create new image and add it to the button */
   a_Html_common_image_attrs(html, tag, tagsize);
   if ((Image = a_Html_image_new(html, tag, tagsize))) {
      // At this point, we know that Image->ir represents an image
      // widget. Notice that the order of the casts matters, because
      // of multiple inheritance.
      dw::Image *dwi = (dw::Image*)(dw::core::ImgRenderer*)Image->img_rndr;
      dwi->setStyle (html->backgroundStyle ());
      ResourceFactory *factory = HT2LT(html)->getResourceFactory();
      ComplexButtonResource *complex_b_r =
         factory->createComplexButtonResource(dwi, false);
      button = new Embed(complex_b_r);
      HT2TB(html)->addWidget (button, html->style ());
   }
   if (!button)
      MSG("Html_input_image: unable to create image submit.\n");
   return button;
}

/*
 * ?
 */
static void Html_option_finish(DilloHtml *html)
{
   DilloHtmlInput *input = Html_get_current_input(html);
   if (input &&
       (input->type == DILLO_HTML_INPUT_SELECT ||
        input->type == DILLO_HTML_INPUT_SEL_LIST)) {
      DilloHtmlOptbase *opt = input->select->getCurrentOpt ();
      opt->setContent (html->Stash->str, html->Stash->len);
   }
}
