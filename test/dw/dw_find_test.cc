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
#include <FL/Fl_Box.H>
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
static Fl_Window *window;
static FltkViewport *viewport;
static Fl_Button *findButton, *resetButton;
static Fl_Widget *resultLabel;

static void findCallback (Fl_Widget *widget, void *data)
{
   //switch(layout->search ("worm", true)) {
   switch(layout->search ("WORM", false, false)) {
       case FindtextState::SUCCESS:
          resultLabel->label("SUCCESS");
          break;

       case FindtextState::RESTART:
          resultLabel->label("RESTART");
          break;

       case FindtextState::NOT_FOUND:
          resultLabel->label("NOT_FOUND");
          break;
   }

   resultLabel->redraw ();
}

static void resetCallback (Fl_Widget *widget, void *data)
{
   layout->resetSearch ();
   resultLabel->label("---");
   resultLabel->redraw ();
}

int main(int argc, char **argv)
{
   platform = new FltkPlatform ();
   layout = new Layout (platform);

   window = new Fl_Window(200, 300, "Dw Find Test");
   window->box(FL_NO_BOX);
   window->begin();

   viewport = new FltkViewport (0, 0, 200, 280);
   viewport->end();
   layout->attachView (viewport);

   findButton = new Fl_Button(0, 280, 50, 20, "Find");
   findButton->callback (findCallback, NULL);
   findButton->when (FL_WHEN_RELEASE);
   findButton->clear_visible_focus ();

   resetButton = new Fl_Button(50, 280, 50, 20, "Reset");
   resetButton->callback (resetCallback, NULL);
   resetButton->when (FL_WHEN_RELEASE);
   resetButton->clear_visible_focus ();

   resultLabel = new Fl_Box(100, 280, 100, 20, "---");
   resultLabel->box(FL_FLAT_BOX);

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = FONT_VARIANT_NORMAL;

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);
   styleAttrs.margin.setVal (10);
   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xffffff);
   Style *topWidgetStyle = Style::create (&styleAttrs);

   styleAttrs.margin.setVal (0);
   styleAttrs.margin.left = 30;
   styleAttrs.backgroundColor = NULL;
   Style *widgetStyle = Style::create (&styleAttrs);

   styleAttrs.margin.left = 0;
   Style *wordStyle = Style::create (&styleAttrs);

   Textblock *textblock = new Textblock (false);
   textblock->setStyle (topWidgetStyle);
   layout->setWidget (textblock);

   Stack <Textblock> *stack = new Stack <Textblock> (false);
   stack->push (textblock);

   for(int i = 0; i < 10; i++)
      for(int j = 0; j < 10; j++) {
         Textblock *current;
         if(j < 5) {
            current = new Textblock (false);
            stack->getTop()->addWidget (current, widgetStyle);
            stack->push (current);
         } else {
            stack->getTop()->flush ();
            stack->pop ();
            current = stack->getTop ();
         }

         current->addText ((i == j ? "worm" : "apple"), wordStyle);
         current->addLinebreak (wordStyle);
      }

   stack->getTop()->flush ();

   topWidgetStyle->unref ();
   widgetStyle->unref ();
   wordStyle->unref ();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
