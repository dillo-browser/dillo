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
#include "../dw/table.hh"
#include "../dw/tablecell.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace dw::fltk;

int main(int argc, char **argv)
{
   FltkPlatform *platform = new FltkPlatform ();
   Layout *layout = new Layout (platform);

   Fl_Window *window = new Fl_Window(200, 300, "Dw Table Aligned");
   window->box(FL_NO_BOX);
   window->begin();

   FltkViewport *viewport = new FltkViewport (0, 0, 200, 300);
   layout->attachView (viewport);

   StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
   styleAttrs.borderWidth.setVal (1);
   styleAttrs.setBorderStyle (BORDER_OUTSET);
   styleAttrs.setBorderColor (Color::create (layout, 0x808080));

   FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = FONT_VARIANT_NORMAL;
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);

   styleAttrs.color = Color::create (layout, 0x000000);
   styleAttrs.backgroundColor = Color::create (layout, 0xa0a0a0);
   styleAttrs.hBorderSpacing = 5;
   styleAttrs.vBorderSpacing = 5;

   Style *tableStyle = Style::create (&styleAttrs);

   Table *table = new Table (false);
   table->setStyle (tableStyle);
   layout->setWidget (table);

   tableStyle->unref();

   styleAttrs.borderWidth.setVal (1);
   styleAttrs.setBorderStyle (BORDER_INSET);

   Style *cellStyle = Style::create (&styleAttrs);

   styleAttrs.borderWidth.setVal (0);
   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;
   styleAttrs.cursor = CURSOR_TEXT;
   styleAttrs.textAlignChar = '.';

   Style *wordStyle = Style::create (&styleAttrs);

   TableCell *ref = NULL;
   for(int i = 0; i < 10; i++) {
      //for(int i = 0; i < 1; i++) {
      TableCell *cell = new TableCell (ref, false);
      cell->setStyle (cellStyle);
      ref = cell;
      table->addRow (wordStyle);
      table->addCell (cell, 1, 1);

      char buf[16];
      for(int j = 0; j < i; j++)
         buf[j] = '0' + j;
      buf[i] = '.';
      for(int j = i + 1; j < 11; j++)
         buf[j] = '0' + (j - 1);
      buf[11] = 0;

      cell->addText (buf, wordStyle);
      cell->flush ();
   }

   wordStyle->unref();
   cellStyle->unref();

   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();

   delete layout;

   return errorCode;
}
