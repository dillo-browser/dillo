/*
 * Dillo Widget
 *
 * Copyright 2013 Sebastian Geerken <sgeerken@dillo.org>
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
#include "../dw/image.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

static StyleImage *image;
static Layout *layout;
static int imgRow = 0;

static void imageInitTimeout (void *data)
{
   Imgbuf *imgbuf = layout->createImgbuf (Imgbuf::RGB, 400, 200, 1);
   image->getMainImgRenderer()->setBuffer (imgbuf, false);
}

static void imageDrawTimeout (void *data)
{
   Imgbuf *imgbuf = image->getImgbuf ();

   if (imgbuf) {
      for (int i = 0; i < 1; i++) {
         byte buf[3 * 400];
         for(int x = 0; x < 400; x++) {
            buf[3 * x + 0] = x * 255 / 399;
            buf[3 * x + 1] = (399 - x) * 255 / 399;
            buf[3 * x + 2] = imgRow * 255 / 199;
         }

         imgbuf->copyRow (imgRow, buf);
         image->getMainImgRenderer()->drawRow (imgRow);
         imgRow++;
      }
   }

   if(imgRow < 200)
      Fl::repeat_timeout (0.5, imageDrawTimeout, NULL);
}

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   layout = new Layout (platform);

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

   styleAttrs.color =  Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   image = styleAttrs.backgroundImage = StyleImage::create ();

   Style *widgetStyle = Style::create (&styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.backgroundImage = NULL;

   Style *wordStyle = Style::create (&styleAttrs);

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

   Fl::add_timeout (2.0, imageInitTimeout, NULL);
   Fl::add_timeout (0.1, imageDrawTimeout, NULL);

   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
