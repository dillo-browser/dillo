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

#include <fltk/events.h> // for the unfortunate (temporary?) event key testing
#include <fltk/Window.h>
#include <fltk/ask.h>
#include <fltk/file_chooser.h>
#include <fltk/TextBuffer.h>
#include <fltk/Input.h>
#include <fltk/CheckButton.h>
#include <fltk/ReturnButton.h>
#include <fltk/TextDisplay.h>

#include "dialog.hh"
#include "misc.h"
#include "uicmd.hh"
#include "prefs.h"

using namespace fltk;

/*
 * Close dialog window.
 */
static void window_close_cb(Widget *, void *vwin)
{
   Window *window = (Window*)vwin;

   window->destroy();
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
 * Show the save file dialog.
 *
 * Return: pointer to chosen filename, or NULL on Cancel.
 */
const char *a_Dialog_save_file(const char *msg,
                               const char *pattern, const char *fname)
{
   return file_chooser(msg, pattern, fname);
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
 * Show a new window with the provided text
 */
void a_Dialog_text_window(const char *txt, const char *title)
{
   int wh = 500, ww = 480, bh = 30;
   int lines, line_num_width;
   TextBuffer *text_buf = new TextBuffer();
   text_buf->text(txt);
 
   Window *window = new Window(ww, wh, title ? title : "Untitled");
   window->begin();
 
    TextDisplay *td = new TextDisplay(0,0,ww, wh-bh);
    td->buffer(text_buf);
    /* enable wrapping lines; text uses entire width of window */
    td->wrap_mode(true, 0);

    /* 11.0 instead of 12.0 because the dialog's font is a bit bigger */
    td->textsize((int) rint(11.0 * prefs.font_factor));
    fltk::setfont(td->textfont(), td->textsize());

    lines = td->total_lines();
    line_num_width = 2;
    while (lines /= 10)
       ++line_num_width;
    line_num_width = (int)(line_num_width * fltk::getwidth("0"));
    td->linenumber_width(line_num_width);

    ReturnButton *b = new ReturnButton (0, wh-bh, ww, bh, "Close");
    b->callback(window_close_cb, window);

   window->resizable(td);
   window->end();
   window->show();
}

/*
 * Dialog to find text in page.
 */
class TextFinder : public Window {
public:
   TextFinder(int ww, int wh, BrowserWindow *bw);
   BrowserWindow *bw;
   Input *i;
   CheckButton *cb;
   ReturnButton *findb;
   Button *clrb;
   Button *clsb;
};

/*
 * Find next occurrence of input key
 */
static void findtext_search_cb(Widget *, void *vtf)
{
   TextFinder *tf = (TextFinder *)vtf;
   const char *key = tf->i->value();
   bool case_sens = tf->cb->value();

   /*
    * Somehow fltk even regards the first loss of focus for the
    * window as a WHEN_ENTER_KEY_ALWAYS event.
    */ 
   int e = event_key();
   if ((e != ReturnKey) && (e != LeftButton))
      return;

   if (key[0] != '\0')
      a_UIcmd_findtext_search(tf->bw, key, case_sens);
      
}

/*
 * Reset search state
 */
static void findtext_clear_cb(Widget *, void *vtf)
{
   TextFinder *tf = (TextFinder *)vtf;
   tf->i->value("");
   a_UIcmd_findtext_reset(tf->bw);
}

/*
 * Construct text search window
 */
TextFinder::TextFinder(int ww, int wh, BrowserWindow *bw) :
                      Window(ww, wh, "unwanted title")
{
   int button_width = 70, ih = 35, bh = 30, gap = 10;

   this->bw = bw;

   begin();
    i = new Input(0, 0, ww, ih);
    i->when(WHEN_ENTER_KEY_ALWAYS);
    i->callback(findtext_search_cb, this);

    cb = new CheckButton(0, ih, ww, wh-ih-bh, "Case-sensitive");

    findb = new ReturnButton(gap, wh-bh, button_width, bh, "Find");
    findb->callback(findtext_search_cb, this);

    clrb = new Button(button_width+2*gap, wh-bh, button_width, bh, "Clear");
    clrb->callback(findtext_clear_cb, this);

    clsb = new Button(2*button_width+3*gap, wh-bh, button_width, bh, "Close");
    clsb->callback(window_close_cb, this);
   end();

   hotspot(i);         // place input widget beneath the cursor
}

void a_Dialog_findtext(BrowserWindow *bw)
{
   TextFinder *tf = new TextFinder(250, 90, bw);
   tf->show();
}
