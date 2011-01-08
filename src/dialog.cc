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
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>

#include "msg.h"
#include "dialog.hh"
#include "misc.h"
#include "prefs.h"

/*
 * Close dialog window.
 */
static void window_close_cb(Fl_Widget *, void *vwin)
{
   delete (Fl_Window*)vwin;
}

/*
 * Display a message in a popup window.
 */
void a_Dialog_msg(const char *msg)
{
   fl_message("%s", msg);
}

/*
 * Offer a three choice dialog.
 * The option string that begins with "*" is the default.
 *
 * Return: 0, 1 or 2 (esc = 2, window close = 2)
 */
int a_Dialog_choice3(const char *msg,
                     const char *b0, const char *b1, const char *b2)
{
   return fl_choice(msg, b0, b1, b2);
}

/*
 * Dialog for one line of Input with a message.
 */
const char *a_Dialog_input(const char *msg)
{
   return fl_input("%s", "", msg);
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

//#include <fltk/FileIcon.h>
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
 * Show a new window with the provided text
 */
void a_Dialog_text_window(const char *txt, const char *title)
{
   //int wh = 600, ww = 650, bh = 30;
   int wh = prefs.height, ww = prefs.width, bh = 30;
   Font *textfont = font(prefs.font_monospace, 0);

   Fl_Window *window = new Fl_Window(ww, wh, title ? title : "Untitled");
   window->callback(window_close_cb, window);
   window->begin();

    Fl_Text_Display *td = new Fl_Text_Display(0,0,ww, wh-bh);
    td->buffer()->text(txt);

    if (textfont)
       td->textfont(textfont);
    td->textsize((int) rint(13.0 * prefs.font_factor));
    fltk::setfont(td->textfont(), td->textsize());

    /* enable wrapping lines; text uses entire width of window */
    td->wrap_mode(true, false);
    /* WORKAROUND: FLTK may not display all the lines without this */
    td->size(ww+1,wh-bh);

    Fl_Return_Button *b = new Fl_Return_Button (0, wh-bh, ww, bh, "Close");
    b->callback(window_close_cb, window);

   window->resizable(td);
   window->end();
   window->show();
}

/*--------------------------------------------------------------------------*/
static int choice5_answer;

static void choice5_cb(Fl_Widget *button, void *number)
{
  choice5_answer = VOIDP2INT(number);
  _MSG("choice5_cb: %d\n", choice5_answer);
  button->window()->make_exec_return(true);
}

/*
 * Make a question-dialog with a question and some alternatives.
 * Return value: 0 = dialog was cancelled, 1-5 = selected alternative.
 */
int a_Dialog_choice5(const char *QuestionTxt,
                     const char *alt1, const char *alt2, const char *alt3,
                     const char *alt4, const char *alt5)
{
   choice5_answer = 0;

   int ww = 440, wh = 150, bw = 50, bh = 45, nb = 0;
   const char *txt[7];

   txt[0] = txt[6] = NULL;
   txt[1] = alt1; txt[2] = alt2; txt[3] = alt3;
   txt[4] = alt4; txt[5] = alt5;
   for (int i=1; txt[i]; ++i, ++nb) ;

   Fl_Window *window = new Fl_Window(ww,wh,"Choice5");
   window->begin();
    Fl_Group* ib = new Fl_Group(0,0,window->w(),window->h());
    ib->begin();
    window->resizable(ib);

    Fl_Box *box = new Fl_Box(0,0,ww,wh-bh, QuestionTxt);
    box->box(FL_DOWN_BOX);
    box->labelfont(FL_HELVETICA_BOLD_ITALIC);
    box->labelsize(14);

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
    }
   window->end();

   //window->hotspot(box);
   window->exec();
   delete window;
   _MSG("Choice5 answer = %d\n", choice5_answer);

   return choice5_answer;
}


/*--------------------------------------------------------------------------*/
/*
 * ret: 0 = Cancel, 1 = OK
 */
static void Dialog_user_password_cb(Fl_Widget *button, void *vIntPtr)
{
   int ret = VOIDP2INT(vIntPtr);
  _MSG("Dialog_user_password_cb: %d\n", ret);
  button->window()->make_exec_return(ret);
}

/*
 * Make a user/password dialog.
 * Call the callback with the result (OK or not) and the given user and
 *   password if OK.
 */
int a_Dialog_user_password(const char *message, UserPasswordCB cb, void *vp)
{
   int ok,
      window_w = 300, window_h = 280,
      input_x = 80, input_w = 200, input_h = 30,
      button_y = 230, button_h = 30;

   Fl_Window *window =
      new Fl_Window(window_w,window_h,"User/Password");
   window->begin();

   /* message */
   Fl_Output *message_output =
      new Fl_Output(20,20,window_w-40,100);
   /* BUG type() not tested */
   message_output->type(FL_NORMAL_OUTPUT | FL_INPUT_WRAP);
   message_output->box(FL_DOWN_BOX);
   message_output->value(message);
   message_output->textfont(FL_HELVETICA_BOLD_ITALIC);
   message_output->textsize(14);

   /* inputs */
   Fl_Input *user_input =
      new Fl_Input(input_x,140,input_w,input_h,"User");
   user_input->labelsize(14);
   user_input->textsize(14);
   Fl_Secret_Input *password_input =
      new Fl_Secret_Input(input_x,180,input_w,input_h,"Password");
   password_input->labelsize(14);
   password_input->textsize(14);

   /* "OK" button */
   Fl_Button *ok_button =
      new Fl_Button(200,button_y,50,button_h,"OK");
   ok_button->labelsize(14);
   ok_button->callback(Dialog_user_password_cb);
   ok_button->user_data(INT2VOIDP(1));

   /* "Cancel" button */
   Fl_Button *cancel_button =
      new Fl_Button(50,button_y,100,button_h,"Cancel");
   cancel_button->labelsize(14);
   cancel_button->callback(Dialog_user_password_cb);
   cancel_button->user_data(INT2VOIDP(0));

   window->end();
   window->size_range(window_w,window_h,window_w,window_h);
   window->resizable(window);

   if ((ok = window->exec())) {
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

