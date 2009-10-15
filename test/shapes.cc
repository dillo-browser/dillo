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



#include "../dw/core.hh"

using namespace dw::core;
using namespace lout::misc;

int main()
{
   Polygon poly;
   poly.addPoint (50, 10);
   poly.addPoint (90, 90);
   poly.addPoint (10, 90);

   printf("first test\n");
   assert (poly.isPointWithin (50, 50));
   printf("second test\n");
   assert (!poly.isPointWithin (10, 10));
   printf("third test\n");
   assert (!poly.isPointWithin (90, 50));
}
