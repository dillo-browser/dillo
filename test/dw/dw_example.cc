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



#include <FL/Fl_Window.H>
#include <FL/Fl.H>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/textblock.hh"


int main(int argc, char **argv)
{
   dw::fltk::FltkPlatform *platform = new dw::fltk::FltkPlatform ();
   dw::core::Layout *layout = new dw::core::Layout (platform);

   Fl_Window *window = new Fl_Window(200, 300, "Dw Example");
   window->box(FL_NO_BOX);
   window->begin();

   dw::fltk::FltkViewport *viewport =
      new dw::fltk::FltkViewport (0, 0, 200, 300);
   layout->attachView (viewport);

   dw::core::style::StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);

   dw::core::style::FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = dw::core::style::FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = dw::core::style::FONT_VARIANT_NORMAL;
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);

   styleAttrs.color =
      dw::core::style::Color::create (layout, 0x000000);
   styleAttrs.backgroundColor =
      dw::core::style::Color::create (layout, 0xffffff);

   dw::core::style::Style *widgetStyle =
      dw::core::style::Style::create (&styleAttrs);

   dw::Textblock *textblock = new dw::Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;

   dw::core::style::Style *wordStyle =
      dw::core::style::Style::create (&styleAttrs);

   for(int i = 1; i <= 10; i++) {
      char buf[4];
      sprintf(buf, "%d.", i);

      const char *words[] = { "This", "is", "the", buf, "paragraph.",
                              "Here", "comes", "some", "more", "text",
                              "to", "demonstrate", "word", "wrapping.",
                              NULL };

      for(int j = 0; words[j]; j++) {
         textblock->addText(words[j], wordStyle);
         textblock->addSpace(wordStyle);
      }

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
