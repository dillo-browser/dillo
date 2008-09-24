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
#include <fltk/Browser.h>
#include <fltk/MultiBrowser.h>
#include <fltk/Item.h>
#include <fltk/run.h>

using namespace fltk;

int main (int argc, char *argv[])
{
   Window *window = new Window (300, 300, "FLTK Browser");
   window->begin ();
   Browser *browser = new MultiBrowser (0, 0, 300, 300);
   browser->begin ();

   for (int i = 0; i < 10; i++) {
      new Item ("first");
      new Item ("second");
      new Item ("third");
   }
   
   window->resizable(browser);
   window->show();
   return run();
}
