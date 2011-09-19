/*
 * File: dialog.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// UI dialogs

#include <math.h> // for rint()

#include <FL/Fl_Window.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Item.H>

#include "msg.h"
#include "dialog.hh"
#include "misc.h"
#include "prefs.h"

/*
 * Local Data
 */
static int input_answer;
static char *input_str = NULL;
static int choice5_answer;


/*
 * Local sub classes
 */

//----------------------------------------------------------------------------
/*
 * Used to enable CTRL+{a,e,d,k} in search dialog (for start,end,del,cut)
 * TODO: bind down arrow to a search engine selection list.
 */
class CustInput3 : public Fl_Input {
public:
   CustInput3 (int x, int y, int w, int h, const char* l=0) :
      Fl_Input(x,y,w,h,l) {};
   int handle(int e);
};

int CustInput3::handle(int e)
{
   int k = Fl::event_key();

   _MSG("CustInput3::handle event=%d\n", e);

   // We're only interested in some flags
   unsigned modifier = Fl::event_state() & (FL_SHIFT | FL_CTRL | FL_ALT);

   if (e == FL_KEYBOARD && modifier == FL_CTRL) {
      if (k == 'a' || k == 'e') {
         position(k == 'a' ? 0 : size());
         return 1;
      } else if (k == 'k') {
         cut(position(), size());
         return 1;
      } else if (k == 'd') {
         cut(position(), position()+1);
         return 1;
      }
   }
   return Fl_Input::handle(e);
}

/*
 * Used to make the ENTER key activate the CustChoice
 */
class CustChoice : public Fl_Choice {
public:
   CustChoice (int x, int y, int w, int h, const char* l=0) :
      Fl_Choice(x,y,w,h,l) {};
   int handle(int e) {
      if (e == FL_KEYBOARD &&
          (Fl::event_key() == FL_Enter || Fl::event_key() == FL_Down) &&
          (Fl::event_state() & (FL_SHIFT|FL_CTRL|FL_ALT|FL_META)) == 0) {
         return Fl_Choice::handle(FL_PUSH);
      }
      return Fl_Choice::handle(e);
   };
};

//----------------------------------------------------------------------------


/*
 * Display a message in a popup window.
 */
void a_Dialog_msg(const char *msg)
{
   fl_message("%s", msg);
}


/*
 * Callback for a_Dialog_input()
 */
static void input_cb(Fl_Widget *button, void *number)
{
  input_answer = VOIDP2INT(number);
  button->window()->hide();
}

/*
 * Dialog for one line of Input with a message.
 * avoids the sound bell in fl_input(), and allows customization
 *
 * Return value: string on success, NULL upon Cancel or Close window
 */
const char *a_Dialog_input(const char *msg)
{
   static Fl_Menu_Item *pm = 0;
   int ww = 450, wh = 130, gap = 10, ih = 60, bw = 80, bh = 30;

   input_answer = 0;

   Fl_Window *window = new Fl_Window(ww,wh,"Ask");
   window->set_modal();
   window->begin();
    Fl_Group* ib = new Fl_Group(0,0,window->w(),window->h());
    ib->begin();
    window->resizable(ib);

    /* '?' Icon */
    Fl_Box* o = new Fl_Box(gap, gap, ih, ih);
    o->box(FL_THIN_UP_BOX);
    o->labelfont(FL_TIMES_BOLD);
    o->labelsize(34);
    o->color(FL_WHITE);
    o->labelcolor(FL_BLUE);
    o->label("?");
    o->show();

    Fl_Box *box = new Fl_Box(ih+2*gap,gap,ww-(ih+3*gap),ih/2, msg);
    box->labelfont(FL_HELVETICA);
    box->labelsize(14);
    box->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CLIP|FL_ALIGN_WRAP);

    CustInput3 *c_inp = new CustInput3(ih+2*gap,gap+ih/2+gap,ww-(ih+3*gap),24);
    c_inp->labelsize(14);
    c_inp->textsize(14);

    CustChoice *ch = new CustChoice(1*gap,ih+3*gap,180,24);
    if (!pm) {
       int n_it = dList_length(prefs.search_urls);
       pm = new Fl_Menu_Item[n_it+1];
       memset(pm, '\0', sizeof(Fl_Menu_Item[n_it+1]));
       for (int i = 0, j = 0; i < n_it; i++) {
          char *label, *url, *source;
          source = (char *)dList_nth_data(prefs.search_urls, i);
          if (!source || a_Misc_parse_search_url(source, &label, &url) < 0)
             continue;
          pm[j++].label(FL_NORMAL_LABEL, strdup(label));
       }
    }
    ch->tooltip("Select search engine");
    ch->menu(pm);
    ch->value(prefs.search_url_idx);
    ch->textcolor(FL_DARK_BLUE);

    int xpos = ww-2*(gap+bw), ypos = ih+3*gap;
    Fl_Return_Button *rb = new Fl_Return_Button(xpos, ypos, bw, bh, "OK");
    rb->align(FL_ALIGN_INSIDE|FL_ALIGN_CLIP);
    rb->box(FL_UP_BOX);
    rb->callback(input_cb, INT2VOIDP(1));

    xpos = ww-(gap+bw);
    Fl_Button *b = new Fl_Button(xpos, ypos, bw, bh, "Cancel");
    b->align(FL_ALIGN_INSIDE|FL_ALIGN_CLIP);
    b->box(FL_UP_BOX);
    b->callback(input_cb, INT2VOIDP(2));

   window->end();

   window->show();
   while (window->shown())
      Fl::wait();
   if (input_answer == 1) {
      /* we have a string, save it */
      dFree(input_str);
      input_str = dStrdup(c_inp->value());
      prefs.search_url_idx = ch->value();
   }
   delete window;

   return (input_answer == 1) ? input_str : NULL;
}

/*
 * Dialog for password
 */
const char *a_Dialog_passwd(const char *msg)
{
   return fl_password("%s", "", msg);
}

/*
 * Show the save file dialog.
 *
 * Return: pointer to chosen filename, or NULL on Cancel.
 */
const char *a_Dialog_save_file(const char *msg,
                               const char *pattern, const char *fname)
{
   return fl_file_chooser(msg, pattern, fname);
}

/*
 * Show the select file dialog.
 *
 * Return: pointer to chosen filename, or NULL on Cancel.
 */
const char *a_Dialog_select_file(const char *msg,
                                 const char *pattern, const char *fname)
{
   /*
    * FileChooser::type(MULTI) appears to allow multiple files to be selected,
    * but just follow save_file's path for now.
    */
   return a_Dialog_save_file(msg, pattern, fname);
}

/*
 * Show the open file dialog.
 *
 * Return: pointer to chosen filename, or NULL on Cancel.
 */
char *a_Dialog_open_file(const char *msg,
                         const char *pattern, const char *fname)
{
   const char *fc_name;

   fc_name = fl_file_chooser(msg, pattern, fname);
   return (fc_name) ? a_Misc_escape_chars(fc_name, "% ") : NULL;
}

/*
 * Close text window.
 */
static void text_window_close_cb(Fl_Widget *, void *vtd)
{
   Fl_Text_Display *td = (Fl_Text_Display *)vtd;
   Fl_Text_Buffer *buf = td->buffer();

   delete (Fl_Window*)td->window();
   delete buf;
}

/*
 * Show a new window with the provided text
 */
void a_Dialog_text_window(const char *txt, const char *title)
{
   int wh = prefs.height, ww = prefs.width, bh = 30;

   Fl_Window *window = new Fl_Window(ww, wh, title ? title : "Dillo text");
   Fl_Group::current(0);


    Fl_Text_Buffer *buf = new Fl_Text_Buffer();
    buf->text(txt);
    Fl_Text_Display *td = new Fl_Text_Display(0,0,ww, wh-bh);
    td->buffer(buf);
    td->textsize((int) rint(14.0 * prefs.font_factor));

    /* enable wrapping lines; text uses entire width of window */
    td->wrap_mode(true, false);
   window->add(td);

    Fl_Return_Button *b = new Fl_Return_Button (0, wh-bh, ww, bh, "Close");
    b->callback(text_window_close_cb, td);
   window->add(b);

   window->callback(text_window_close_cb, td);
   window->resizable(td);
   window->show();
}

/*--------------------------------------------------------------------------*/

static void choice5_cb(Fl_Widget *button, void *number)
{
  choice5_answer = VOIDP2INT(number);
  _MSG("choice5_cb: %d\n", choice5_answer);
  button->window()->hide();
}

/*
 * Make a question-dialog with a question and up to five alternatives.
 * (if less alternatives, non used parameters must be NULL).
 *
 * Return value: 0 = dialog was cancelled, 1-5 = selected alternative.
 */
int a_Dialog_choice5(const char *QuestionTxt,
                     const char *alt1, const char *alt2, const char *alt3,
                     const char *alt4, const char *alt5)
{
   choice5_answer = 0;

   int ww = 440, wh = 120, bw = 50, bh = 45, ih = 50, nb = 0;
   const char *txt[7];

   txt[0] = txt[6] = NULL;
   txt[1] = alt1; txt[2] = alt2; txt[3] = alt3;
   txt[4] = alt4; txt[5] = alt5;
   for (int i=1; txt[i]; ++i, ++nb)
      ;

   if (!nb) {
      MSG_ERR("a_Dialog_choice5: No choices.\n");
      return choice5_answer;
   }
   ww = 140 + nb*(bw+10);

   Fl_Window *window = new Fl_Window(ww,wh,"Choice5");
   window->set_modal();
   window->begin();
    Fl_Group* ib = new Fl_Group(0,0,window->w(),window->h());
    ib->begin();
    window->resizable(ib);

    /* '?' Icon */
    Fl_Box* o = new Fl_Box(10, (wh-bh-ih)/2, ih, ih);
    o->box(FL_THIN_UP_BOX);
    o->labelfont(FL_TIMES_BOLD);
    o->labelsize(34);
    o->color(FL_WHITE);
    o->labelcolor(FL_BLUE);
    o->label("?");
    o->show();

    Fl_Box *box = new Fl_Box(60,0,ww-60,wh-bh, QuestionTxt);
    box->labelfont(FL_HELVETICA);
    box->labelsize(14);
    box->align(FL_ALIGN_WRAP);

    Fl_Button *b;
    int xpos = 0, gap = 8;
    bw = (ww - gap)/nb - gap;
    xpos += gap;
    for (int i=1; i <= nb; ++i) {
       b = new Fl_Button(xpos, wh-bh, bw, bh, txt[i]);
       b->align(FL_ALIGN_WRAP|FL_ALIGN_CLIP);
       b->box(FL_UP_BOX);
       b->callback(choice5_cb, INT2VOIDP(i));
       xpos += bw + gap;
       /* TODO: set focus to the *-prefixed alternative */
    }
   window->end();

   window->show();
   while (window->shown())
      Fl::wait();
   _MSG("a_Dialog_choice5 answer = %d\n", choice5_answer);
   delete window;

   return choice5_answer;
}


/*--------------------------------------------------------------------------*/
static void Dialog_user_password_cb(Fl_Widget *button, void *)
{
   button->window()->user_data(button);
   button->window()->hide();
}

/*
 * Make a user/password dialog.
 * Call the callback with the result (OK or not) and the given user and
 *   password if OK.
 */
int a_Dialog_user_password(const char *message, UserPasswordCB cb, void *vp)
{
   int ok = 0, window_h = 280, y, msg_w, msg_h;
   const int window_w = 300, input_x = 80, input_w = 200, input_h = 30,
      button_h = 30;

   /* window is resized below */
   Fl_Window *window = new Fl_Window(window_w,window_h,"Dillo User/Password");
   Fl_Group::current(0);
   window->user_data(NULL);

   /* message */
   y = 20;
   msg_w = window_w - 40;
   Fl_Box *msg = new Fl_Box(20, y, msg_w, 100); /* resized below */
   msg->label(message);
   msg->labelfont(FL_HELVETICA);
   msg->labelsize(14);
   msg->align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);

   fl_font(msg->labelfont(), msg->labelsize());
   msg_w -= 6; /* The label doesn't fill the entire box. */
   fl_measure(msg->label(), msg_w, msg_h, 0); /* fl_measure wraps at msg_w */
   msg->size(msg->w(), msg_h);
   window->add(msg);

   /* inputs */
   y += msg_h + 20;
   Fl_Input *user_input = new Fl_Input(input_x, y, input_w, input_h, "User");
   user_input->labelsize(14);
   user_input->textsize(14);
   window->add(user_input);
   y += input_h + 10;
   Fl_Secret_Input *password_input =
      new Fl_Secret_Input(input_x, y, input_w, input_h, "Password");
   password_input->labelsize(14);
   password_input->textsize(14);
   window->add(password_input);

   /* "OK" button */
   y += input_h + 20;
   Fl_Button *ok_button = new Fl_Button(200, y, 50, button_h, "OK");
   ok_button->labelsize(14);
   ok_button->callback(Dialog_user_password_cb);
   window->add(ok_button);

   /* "Cancel" button */
   Fl_Button *cancel_button =
      new Fl_Button(50, y, 100, button_h, "Cancel");
   cancel_button->labelsize(14);
   cancel_button->callback(Dialog_user_password_cb);
   window->add(cancel_button);

   y += button_h + 20;
   window_h = y;
   window->size(window_w, window_h);
   window->size_range(window_w, window_h, window_w, window_h);
   window->resizable(window);

   window->show();
   while (window->shown())
      Fl::wait();

   ok = ((Fl_Widget *)window->user_data()) == ok_button ? 1 : 0;

   if (ok) {
      /* call the callback */
      const char *user, *password;
      user = user_input->value();
      password = password_input->value();
      _MSG("a_Dialog_user_passwd: ok = %d\n", ok);
      (*cb)(user, password, vp);
   }
   delete window;

   return ok;
}

