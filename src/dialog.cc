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

#include <fltk/Window.h>
#include <fltk/ask.h>
#include <fltk/file_chooser.h>
#include <fltk/TextBuffer.h>
#include <fltk/ReturnButton.h>
#include <fltk/TextDisplay.h>

#include "dialog.hh"
#include "misc.h"

using namespace fltk;

/*
 * Callback for the text window dialog.
 */
void text_window_cb(Widget *, void *vwin)
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
   TextBuffer *text_buf = new TextBuffer();
   text_buf->text(txt);
 
   Window *window = new Window(ww, wh, title ? title : "Untitled");
   window->begin();
 
    TextDisplay *td = new TextDisplay(0,0,ww, wh-bh);
    td->buffer(text_buf);

    ReturnButton *b = new ReturnButton (0, wh-bh, ww, bh, "Close");
    b->callback(text_window_cb, window);

   window->resizable(window);
   window->end();
   window->show();
}

