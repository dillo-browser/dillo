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
#include "../dw/ui.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::core::ui;
using namespace dw::fltk;

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   ::fltk::Window *window = new ::fltk::Window(410, 210, "Dw Resource test");
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
   styleAttrs.font = Font::create (layout, &fontAttrs);

   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);

   Style *widgetStyle = Style::create (layout, &styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);

   widgetStyle->unref();

   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;

   SelectionResource *res = layout->getResourceFactory()->createListResource
      (ListResource::SELECTION_AT_MOST_ONE, 4);
   //SelectionResource *res =
   //   layout->getResourceFactory()->createOptionMenuResource ();
   
   Embed *embed = new Embed (res);
   textblock->addWidget (embed, widgetStyle);
   textblock->addSpace (widgetStyle);

   widgetStyle->unref();

   for(int i = 0; i < 50; i++)
      res->addItem ("Hello, world!", true, false);

   textblock->flush ();

   window->resizable(viewport);
   window->show();

   int errorCode = ::fltk::run();

   delete layout;

   return errorCode;
}
