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

#include <fltk/Window.h>
#include <fltk/ask.h>
#include <fltk/file_chooser.h>
#include <fltk/TextBuffer.h>
#include <fltk/ReturnButton.h>
#include <fltk/TextDisplay.h>
#include <fltk/HighlightButton.h>
#include <fltk/WordwrapOutput.h>
#include <fltk/Input.h>
#include <fltk/SecretInput.h>

#include "msg.h"
#include "dialog.hh"
#include "misc.h"
#include "prefs.h"

using namespace fltk;

/*
 * Close dialog window.
 */
static void window_close_cb(Widget *, void *vwin)
{
   delete (Window*)vwin;
}

/*
 * Display a message in a popup window.
 */
void a_Dialog_msg(const char *msg)
{
   message("%s", msg);
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
   return choice(msg, b0, b1, b2);
}

/*
 * Dialog for one line of Input with a message.
 */
const char *a_Dialog_input(const char *msg)
{
   return input("%s", "", msg);
}

/*
 * Dialog for password
 */
const char *a_Dialog_passwd(const char *msg)
{
   return password("%s", "", msg);
}

/*
 * Show the save file dialog.
 *
 * Return: pointer to chosen filename, or NULL on Cancel.
 */
const char *a_Dialog_save_file(const char *msg,
                               const char *pattern, const char *fname)
{
   return file_chooser(msg, pattern, fname);
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
/*
   static int icons_loaded = 0;
   if (!icons_loaded)
      FileIcon::load_system_icons();
*/
   fc_name = file_chooser(msg, pattern, fname);
   return (fc_name) ? a_Misc_escape_chars(fc_name, "% ") : NULL;
}

/*
 * Make a new window with the provided text
 */
void *a_Dialog_make_text_window(const char *txt, const char *title)
{
   //int wh = 600, ww = 650, bh = 30;
   int wh = prefs.height, ww = prefs.width, bh = 30;
   int lines, line_num_width;
   Font *textfont = font(prefs.font_monospace, 0);

   Window *window = new Window(ww, wh, title ? title : "Untitled");
   window->callback(window_close_cb, window);
   window->begin();

    TextDisplay *td = new TextDisplay(0,0,ww, wh-bh);
    td->buffer()->text(txt);

    if (textfont)
       td->textfont(textfont);
    td->textsize((int) rint(13.0 * prefs.font_factor));
    fltk::setfont(td->textfont(), td->textsize());

    lines = td->total_lines();
    line_num_width = 2;
    while (lines /= 10)
       ++line_num_width;
    line_num_width = (int)(line_num_width * fltk::getwidth("0"));
    td->linenumber_width(line_num_width);

    /* enable wrapping lines; text uses entire width of window */
    td->wrap_mode(true, false);
    /* WORKAROUND: FLTK may not display all the lines without this */
    td->resize(ww+1,wh-bh);

    ReturnButton *b = new ReturnButton (0, wh-bh, ww, bh, "Close");
    b->callback(window_close_cb, window);

   window->resizable(td);
   window->end();
   return window;
}

/*
 * Show a window.
 */
void a_Dialog_show_text_window(void *vWindow)
{
   ((Window *)vWindow)->show();
}

/*--------------------------------------------------------------------------*/
static int choice5_answer;

static void choice5_cb(Widget *button, void *number)
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

   Window *window = new Window(ww,wh,"Choice5");
   window->begin();
    Group* ib = new Group(0,0,window->w(),window->h());
    ib->begin();
    window->resizable(ib);

    Widget *box = new Widget(0,0,ww,wh-bh, QuestionTxt);
    box->box(DOWN_BOX);
    box->labelfont(HELVETICA_BOLD_ITALIC);
    box->labelsize(14);

    HighlightButton *b;
    int xpos = 0, gap = 8;
    bw = (ww - gap)/nb - gap;
    xpos += gap;
    for (int i=1; i <= nb; ++i) {
       b = new HighlightButton(xpos, wh-bh, bw, bh, txt[i]);
       b->align(ALIGN_WRAP|ALIGN_CLIP);
       b->box(UP_BOX);
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
static void Dialog_user_password_cb(Widget *button, void *vIntPtr)
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

   Window *window =
      new Window(window_w,window_h,"User/Password");
   window->begin();

   /* message */
   WordwrapOutput *message_output =
      new WordwrapOutput(20,20,window_w-40,100);
   message_output->box(DOWN_BOX);
   message_output->text(message);
   message_output->textfont(HELVETICA_BOLD_ITALIC);
   message_output->textsize(14);

   /* inputs */
   Input *user_input =
      new Input(input_x,140,input_w,input_h,"User");
   user_input->labelsize(14);
   user_input->textsize(14);
   SecretInput *password_input =
      new SecretInput(input_x,180,input_w,input_h,"Password");
   password_input->labelsize(14);
   password_input->textsize(14);

   /* "OK" button */
   Button *ok_button =
      new Button(200,button_y,50,button_h,"OK");
   ok_button->labelsize(14);
   ok_button->callback(Dialog_user_password_cb);
   ok_button->user_data(INT2VOIDP(1));

   /* "Cancel" button */
   Button *cancel_button =
      new Button(50,button_y,100,button_h,"Cancel");
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

