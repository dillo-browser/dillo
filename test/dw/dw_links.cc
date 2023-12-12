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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/textblock.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

class LinkTestReceiver: public Layout::LinkReceiver
{
   bool enter (Widget *widget, int link, int img, int x, int y);
   bool press (Widget *widget, int link, int img, int x, int y,
               EventButton *event);
   bool release (Widget *widget, int link, int img, int x, int y,
                 EventButton *event);
   bool click (Widget *widget, int link, int img,
               int x, int y, EventButton *event);
};

bool LinkTestReceiver::enter (Widget *widget, int link, int img, int x, int y)
{
   printf ("enter: %d\n", link);
   return true;
}

bool LinkTestReceiver::press (Widget *widget, int link, int img, int x, int y,
                              EventButton *event)
{
   printf ("press: %d\n", link);
   return true;
}

bool LinkTestReceiver::release (Widget *widget, int link, int img, int x,int y,
                                EventButton *event)
{
   printf ("release: %d\n", link);
   return true;
}

bool LinkTestReceiver::click (Widget *widget, int link, int img, int x, int y,
                              EventButton *event)
{
   printf ("click: %d\n", link);
   return true;
}

int main(int argc, char **argv)
{
   LinkTestReceiver linkTestReceiver;
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Fl_Window *window = new Fl_Window(200, 300, "Dw Links");
   window->box(FL_NO_BOX);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, 0, 200, 300);
   layout->attachView (viewport);

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = FONT_VARIANT_NORMAL;
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);

   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   Style *widgetStyle = Style::create (&styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   layout->connectLink (&linkTestReceiver);

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.cursor = CURSOR_TEXT;

   Style *wordStyle = Style::create (&styleAttrs);

   styleAttrs.color = Color::create (layout, 0x0000ff);
   styleAttrs.textDecoration = TEXT_DECORATION_UNDERLINE;
   styleAttrs.cursor = CURSOR_POINTER;

   for(int i = 1; i <= 10; i++) {
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
         textblock->addText(words1[j], wordStyle);
         textblock->addSpace(wordStyle);
      }

      styleAttrs.x_link = i;
      Style *linkStyle = Style::create (&styleAttrs);

      for(int j = 0; words2[j]; j++) {
         textblock->addText(words2[j], linkStyle);
         textblock->addSpace(wordStyle);
      }

      linkStyle->unref ();

      textblock->addParbreak(10, wordStyle);
   }

   wordStyle->unref();

   textblock->flush ();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
