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

#include <stdlib.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "../dw/core.hh"
#include "../dw/fltkcore.hh"
#include "../dw/fltkviewport.hh"
#include "../dw/textblock.hh"
#include "../dw/image.hh"

using namespace lout::signal;
using namespace lout::misc;
using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

class ImageStyleDeletionReceiver: public ObservedObject::DeletionReceiver
{
public:
   void deleted (ObservedObject *object);
};

static StyleImage *image1 = NULL, *image2 = NULL;
static Layout *layout;
static int imgRow1 = 0, imgRow2 = 0;
static ImageStyleDeletionReceiver isdr;

void ImageStyleDeletionReceiver::deleted (ObservedObject *object)
{
   if ((StyleImage*)object == image1)
      image1 = NULL;
   else if ((StyleImage*)object == image2)
      image2 = NULL;
   else
      assertNotReached ();
}

static void imageInitTimeout (void *data)
{
   if (image1) {
      Imgbuf *imgbuf1 = layout->createImgbuf (Imgbuf::RGB, 400, 200, 1);
      image1->getMainImgRenderer()->setBuffer (imgbuf1, false);
   }

   if (image2) {
      Imgbuf *imgbuf2 = layout->createImgbuf (Imgbuf::RGB, 100, 100, 1);
      image2->getMainImgRenderer()->setBuffer (imgbuf2, false);
   }
}

static void imageDrawTimeout (void *data)
{
   Imgbuf *imgbuf1 = image1 ? image1->getImgbufSrc () : NULL;
   Imgbuf *imgbuf2 = image2 ? image2->getImgbufSrc () : NULL;

   if (imgbuf1 && imgRow1 < 200) {
      byte buf[3 * 400];
      for(int x = 0; x < 400; x++) {
         buf[3 * x + 0] = 128 + x * 127 / 399;
         buf[3 * x + 1] = 128 + (399 - x) * 127 / 399;
         buf[3 * x + 2] = 128 + imgRow1 * 127 / 199;
      }

      imgbuf1->copyRow (imgRow1, buf);
      image1->getMainImgRenderer()->drawRow (imgRow1);
      imgRow1++;
   }

   if (imgbuf2 && imgRow2 < 100) {
      byte buf[3 * 100];
      for(int x = 0; x < 100; x++) {
         int r = 128 + rand () % 127;
         buf[3 * x + 0] = buf[3 * x + 1] = buf[3 * x + 2] = r;
      }

      imgbuf2->copyRow (imgRow2, buf);
      image2->getMainImgRenderer()->drawRow (imgRow2);
      imgRow2++;
   }

   if(imgRow1 < 200 || imgRow2 < 100)
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

   image1 = StyleImage::create ();
   image1->connectDeletion (&isdr);
   layout->setBgImage (image1, BACKGROUND_REPEAT_Y,
                       BACKGROUND_ATTACHMENT_SCROLL, createPerLength (0.5),
                       createAbsLength (30));

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.x_lang[0] = 'e';
   styleAttrs.x_lang[1] = 'n';

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = FONT_VARIANT_NORMAL;
   styleAttrs.font = style::Font::create (layout, &fontAttrs);

   styleAttrs.color =  Color::create (layout, 0x000000);
   //styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   Style *widgetStyle = Style::create (&styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.backgroundImage = NULL;

   Style *wordStyle = Style::create (&styleAttrs);

   image2 = styleAttrs.backgroundImage = StyleImage::create ();
   image2->connectDeletion (&isdr);
   styleAttrs.backgroundRepeat = BACKGROUND_REPEAT;
   styleAttrs.backgroundPositionX = createPerLength (0);
   styleAttrs.backgroundPositionY = createPerLength (0);
   Style *wordStyleBg = Style::create (&styleAttrs);

   for(int i = 1; i <= 1; i++) {
      char buf[4];
      sprintf(buf, "%d.", i);

      const char *words[] = { "This", "is", "the", buf, "paragraph.",
                              "Here", "comes", "some", "more", "text",
                              "to", "demonstrate", "word", "wrapping.",
                              NULL };

      for(int j = 0; words[j]; j++) {
         textblock->addText(words[j], j == 11 ? wordStyleBg : wordStyle);
         textblock->addSpace(wordStyle);
      }

      textblock->addParbreak(10, wordStyle);
   }

   wordStyle->unref();
   wordStyleBg->unref();

   textblock->flush ();

   window->resizable(viewport);
   window->show();

   Fl::add_timeout (1.0, imageInitTimeout, NULL);
   Fl::add_timeout (0.1, imageDrawTimeout, NULL);

   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
