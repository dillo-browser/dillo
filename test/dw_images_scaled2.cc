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
#include "../dw/image.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

static Layout *layout;
static Image *image1, *image2;
static core::Imgbuf *imgbuf = NULL;
static int imgRow = 0;

static void imageInitTimeout (void *data)
{
   imgbuf = layout->createImgbuf (Imgbuf::RGB, 400, 200, 1);
   image1->setBuffer (imgbuf);
   image2->setBuffer (imgbuf);
}

static void imageDrawTimeout (void *data)
{
   if (imgbuf) {
      for (int i = 0; i < 1; i++) {
         byte buf[3 * 400];
         for(int x = 0; x < 400; x++) {
            buf[3 * x + 0] = x * 255 / 399;
            buf[3 * x + 1] = (399 - x) * 255 / 399;
            buf[3 * x + 2] = imgRow * 255 / 199;
         }

         imgbuf->copyRow (imgRow, buf);
         image1->drawRow (imgRow);
         image2->drawRow (imgRow);
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

   Fl_Window *window = new Fl_Window(410, 210, "Dw Scaled Image 2");
   window->box(FL_NO_BOX);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, 0, 410, 210);
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

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.borderWidth.setVal (0);
   styleAttrs.padding.setVal (0);
   styleAttrs.backgroundColor = NULL;

   Style *wordStyle = Style::create (&styleAttrs);

   styleAttrs.borderWidth.setVal (1);
   styleAttrs.setBorderColor (Color::create (layout, 0x000080));
   styleAttrs.setBorderStyle (BORDER_SOLID);
   styleAttrs.padding.setVal (1);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.width = createPerLength (0.25);
   styleAttrs.height = createPerLength (0.25);

   Style *imageStyle1 = Style::create (&styleAttrs);
   image1 = new dw::Image ("A longer ALT Text to demonstrate clipping.");
   textblock->addWidget (image1, imageStyle1);
   imageStyle1->unref();

   textblock->addParbreak (10, wordStyle);

   styleAttrs.width = LENGTH_AUTO;
   styleAttrs.height = LENGTH_AUTO;

   Style *imageStyle2 = Style::create (&styleAttrs);
   image2 = new dw::Image ("A longer ALT Text to demonstrate clipping.");
   textblock->addWidget (image2, imageStyle2);
   imageStyle2->unref();

   wordStyle->unref ();
   textblock->flush ();

   window->resizable(viewport);
   window->show();

   Fl::add_timeout (3.0, imageInitTimeout, NULL);
   Fl::add_timeout (0.1, imageDrawTimeout, NULL);

   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
