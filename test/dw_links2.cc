/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include <fltk/Window.h>
#include <fltk/run.h>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/textblock.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

class LinkTestReceiver: public Widget::LinkReceiver
{
   bool enter (Widget *widget, int link, int x, int y);
   bool press (Widget *widget, int link, int x, int y, EventButton *event);
   bool release (Widget *widget, int link, int x, int y, EventButton *event);
   bool click (Widget *widget, int link, int x, int y, EventButton *event);
};

bool LinkTestReceiver::enter (Widget *widget, int link, int x, int y)
{
   printf ("enter: %d\n", link);
   return true;
}

bool LinkTestReceiver::press (Widget *widget, int link, int x, int y,
                              EventButton *event)
{
   printf ("press: %d\n", link);
   return true;
}

bool LinkTestReceiver::release (Widget *widget, int link, int x, int y,
                                EventButton *event)
{
   printf ("release: %d\n", link);
   return true;
}

bool LinkTestReceiver::click (Widget *widget, int link, int x, int y,
                              EventButton *event)
{
   printf ("click: %d\n", link);
   return true;
}

int main(int argc, char **argv)
{
   int MainIdx;
   int ww = 200, wh = 300, lh = 24;

   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   ::fltk::Window *window = new ::fltk::Window(200, 300, "Dw Links");
   window->begin();
    ::fltk::Widget *Panel = new ::fltk::Widget(0, 0, ww, lh, "CONTROL PANEL");

    Panel->color(::fltk::GRAY15);
    Panel->labelcolor(::fltk::WHITE);
    ::fltk::Widget *Main =
       new ::fltk::Widget(0, lh, ww, wh - 2*lh, "MAIN RENDERING AREA");
    Main->color(::fltk::GRAY20);
    Main->labelcolor(::fltk::WHITE);
    MainIdx = window->find(Main);
    /* status bar */
    ::fltk::Widget *Bar =
       new ::fltk::Widget(0, wh - lh, 200, lh, "STATUS BAR...");
    Bar->color(::fltk::GRAY15);
    Bar->labelcolor(::fltk::WHITE);

    window->resizable(Main);
   window->end();

   //
   // Create the main Dw and add some text there.
   //
   window->remove(MainIdx);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, lh, ww, wh - 2*lh);
   layout->attachView (viewport);

   window->end();


   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   styleAttrs.font = Font::create (layout, &fontAttrs);

   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   Style *widgetStyle = Style::create (layout, &styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   textblock->connectLink (new LinkTestReceiver ());

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.cursor = CURSOR_TEXT;

   Style *wordStyle = Style::create (layout, &styleAttrs);

   styleAttrs.color = Color::create (layout, 0x0000ff);
   styleAttrs.textDecoration = TEXT_DECORATION_UNDERLINE;
   styleAttrs.cursor = CURSOR_POINTER;
   
   for(int i = 1; i <= 30; i++) {
      char buf[4];
      sprintf(buf, "%d.", i);

      const char *words1[] = {
         "This", "is", "the", buf, "paragraph.",
         "Here", "comes", "some", "more", "text",
         "to", "demonstrate", "word", "wrapping.",
         NULL };
      const char *words2[] = {
         "Click", "here", "for", "more..", NULL };

      for(int j = 0; words1[j]; j++) {
         textblock->addText (words1[j], wordStyle);
         textblock->addSpace(wordStyle);
      }
      
      styleAttrs.x_link = i;
      Style *linkStyle = Style::create (layout, &styleAttrs);
      
      for(int j = 0; words2[j]; j++) {
         textblock->addText (words2[j], linkStyle);
         textblock->addSpace(wordStyle);
      }
      
      linkStyle->unref ();

      textblock->addParbreak(10, wordStyle);
   }

   wordStyle->unref();

   textblock->flush ();

   window->resizable(viewport);
   window->show();
   int errorCode = ::fltk::run();

   delete layout;

   return errorCode;
}
