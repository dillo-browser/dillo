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
#include "../dw/listitem.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Fl_Window *window = new Fl_Window(200, 300, "Dw Border Test");
   window->box(FL_NO_BOX);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, 0, 200, 300);
   layout->attachView (viewport);

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.borderWidth.setVal (2);
   styleAttrs.setBorderColor (Color::create (layout, 0xffffff));
   styleAttrs.setBorderStyle (BORDER_INSET);
   styleAttrs.padding.setVal (5);

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

   Style *widgetStyle1 = Style::create (&styleAttrs);

   styleAttrs.backgroundColor = Color::create (layout, 0xffff80);
   styleAttrs.margin.setVal (0);
   styleAttrs.borderWidth.setVal (1);
   styleAttrs.setBorderColor (Color::create (layout, 0x4040ff));
   styleAttrs.setBorderStyle (BORDER_SOLID);
   styleAttrs.padding.setVal (1);

   Style *widgetStyle2 = Style::create (&styleAttrs);

   Textblock *textblock1 = new Textblock (false);
   textblock1->setStyle (widgetStyle1);
   layout->setWidget (textblock1);

   widgetStyle1->unref();

   styleAttrs.borderWidth.setVal (0);
   styleAttrs.padding.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.cursor = CURSOR_TEXT;

   Style *wordStyle = Style::create (&styleAttrs);

   const char *words1[] = { "Some", "random", "text.", NULL };
   const char *words2[] = { "A", "nested", "paragraph.", NULL };

   for(int i = 0; words1[i]; i++) {
      if(i != 0)
         textblock1->addSpace (wordStyle);
      textblock1->addText (words1[i], wordStyle);
   }

   for(int i = 0; i < 1; i++) {
      textblock1->addParbreak(0, wordStyle);

      Textblock *textblock2 = new Textblock (false);
      textblock1->addWidget (textblock2, widgetStyle2);

      for(int j = 0; words2[j]; j++) {
         if(j != 0)
            textblock2->addSpace (wordStyle);
         textblock2->addText (words2[j], wordStyle);
      }

      textblock2->flush ();
   }

   textblock1->flush ();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   widgetStyle2->unref();
   wordStyle->unref();
   delete layout;

   return errorCode;
}
