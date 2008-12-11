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



#include <ctype.h>
#include <fltk/Window.h>
#include <fltk/run.h>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/textblock.hh"

using namespace lout::container::typed;
using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

static FltkPlatform *platform;
static Layout *layout;
static ::fltk::Window *window;
static FltkViewport *viewport;
static Style *topWidgetStyle, *widgetStyle, *wordStyle, *headingStyle;
static Textblock *topTextblock = NULL;
static int textblockNo = 0;

static const char *numbers[10] = {
   "one", "two", "three", "four", "five",
   "six", "seven", "eight", "nine", "ten"
};

static void anchorCallback (::fltk::Widget *widget, void *data)
{
   layout->setAnchor (numbers[(long)data]);
}

static void textTimeout (void *data)
{
   Textblock *oldTop = topTextblock;
   topTextblock = new Textblock (false);
   
   if (oldTop) {
      oldTop->addLinebreak (wordStyle);
      oldTop->addWidget (topTextblock, widgetStyle);
   } else {
      topTextblock->setStyle (topWidgetStyle);
      layout->setWidget (topTextblock);
   }

   topTextblock->addAnchor (numbers[textblockNo], headingStyle);

   char buf[16];
   strcpy (buf, numbers[textblockNo]);
   buf[0] = toupper (buf[0]);
   topTextblock->addText (buf, headingStyle);
   topTextblock->addParbreak (5, headingStyle);
  
   for (int i = 0; i < 30; i++) {
      strcpy (buf, numbers[textblockNo]);
      if (i == 0)
         buf[0] = toupper (buf[0]);
      strcat (buf, i == 29 ? "." : ",");

      topTextblock->addText (buf, wordStyle);
      topTextblock->addSpace (wordStyle);
   }

   topTextblock->flush ();
   
   textblockNo++;
   if (textblockNo < 10)
      ::fltk::repeat_timeout (1, textTimeout, NULL);

}

int main(int argc, char **argv)
{
   char *buttonLabel[10];

   platform = new FltkPlatform ();
   layout = new Layout (platform);

   window = new ::fltk::Window(250, 200, "Dw Find Test");
   window->begin();

   viewport = new FltkViewport (50, 0, 200, 200);
   layout->attachView (viewport);

   for (int i = 0; i < 10; i++) {
      char buf[16];
      strcpy (buf, numbers[i]);
      buf[0] = toupper (buf[0]);
      buttonLabel[i] = strdup(buf);
      ::fltk::Button *button =
           new ::fltk::Button(0, 20 * i, 50, 20, buttonLabel[i]);
      button->callback (anchorCallback, (void*)i);
      button->when (::fltk::WHEN_RELEASE);
   }

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.font = Font::create (layout, &fontAttrs);
   styleAttrs.margin.setVal (5);
   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);
   topWidgetStyle = Style::create (layout, &styleAttrs);

   styleAttrs.margin.left = 20;
   styleAttrs.margin.right = 0;
   styleAttrs.backgroundColor = NULL;
   widgetStyle = Style::create (layout, &styleAttrs);

   styleAttrs.margin.left = 0;
   wordStyle = Style::create (layout, &styleAttrs);

   fontAttrs.size = 28;
   fontAttrs.weight = 700;
   styleAttrs.font = Font::create (layout, &fontAttrs);
   headingStyle = Style::create (layout, &styleAttrs);

   ::fltk::add_timeout (0, textTimeout, NULL);

   window->resizable(viewport);
   window->show();

   int errorCode = ::fltk::run();

   topWidgetStyle->unref ();
   widgetStyle->unref ();
   wordStyle->unref ();
   headingStyle->unref ();
   for (int i = 0; i < 10; i++)
      delete buttonLabel[i];
   delete layout;

   return errorCode;
}
