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
#include <FL/Fl_Multi_Browser.H>
#include <FL/Fl.H>

int main (int argc, char *argv[])
{
   Fl_Window *window = new Fl_Window (300, 300, "FLTK Browser");
   window->box(FL_NO_BOX);
   window->begin ();
   Fl_Multi_Browser *browser = new Fl_Multi_Browser (0, 0, 300, 300);
   browser->begin ();

   for (int i = 0; i < 10; i++) {
      browser->add("first");
      browser->add("second");
      browser->add("third");
   }

   window->resizable(browser);
   window->show();
   return Fl::run();
}
