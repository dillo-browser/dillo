/*
 * Dillo Widget
 *
 * Copyright 2014 Sebastian Geerken <sgeerken@dillo.org>
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
#include "dw_simple_container.hh"
#include "../dw/textblock.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Fl_Window *window = new Fl_Window(200, 300, "Dw Example");
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
   styleAttrs.font = style::Font::create (layout, &fontAttrs);

   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   Style *textblockStyle1 = Style::create (&styleAttrs);

   styleAttrs.backgroundColor = NULL;
   styleAttrs.margin.setVal (0);

   Style *textblockStyle2 = Style::create (&styleAttrs);
   Style *wordStyle = Style::create (&styleAttrs);

   styleAttrs.borderWidth.setVal (5);
   styleAttrs.setBorderColor (Color::create (layout, 0x800080));
   styleAttrs.setBorderStyle (BORDER_DASHED);
   styleAttrs.padding.setVal (5);

   Style *containerStyle = Style::create (&styleAttrs);

   Textblock *textblock1 = new Textblock (false);
   textblock1->setStyle (textblockStyle1);
   layout->setWidget (textblock1);

   SimpleContainer *simpleContainer = new SimpleContainer ();
   simpleContainer->setStyle (containerStyle);
   textblock1->addWidget (simpleContainer, containerStyle);

   Textblock *textblock2 = new Textblock (false);
   textblock2->setStyle (textblockStyle2);
   simpleContainer->setChild (textblock2);

   const char *words[] = { "This", "is", "only", "a", "short", "paragraph.",
                           NULL };

   for(int j = 0; words[j]; j++) {
      textblock2->addText(words[j], wordStyle);
      textblock2->addSpace(wordStyle);
   }

   textblockStyle1->unref();
   textblockStyle2->unref();
   containerStyle->unref();
   wordStyle->unref();

   textblock1->flush ();
   textblock2->flush ();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
