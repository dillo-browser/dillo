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

#include "core.hh"

namespace dw {
namespace core {

using namespace lout::container;
using namespace lout::object;

void ImgRendererDist::setBuffer (core::Imgbuf *buffer, bool resize)
{
   for (typed::Iterator <TypedPointer <ImgRenderer> > it =
           children->iterator (); it.hasNext (); ) {
      TypedPointer <ImgRenderer> *tp = it.getNext ();
      tp->getTypedValue()->setBuffer (buffer, resize);
   }
}

void ImgRendererDist::drawRow (int row)
{
   for (typed::Iterator <TypedPointer <ImgRenderer> > it =
           children->iterator (); it.hasNext (); ) {
      TypedPointer <ImgRenderer> *tp = it.getNext ();
      tp->getTypedValue()->drawRow (row);
   }
}


void ImgRendererDist::finish ()
{
   for (typed::Iterator <TypedPointer <ImgRenderer> > it =
           children->iterator (); it.hasNext (); ) {
      TypedPointer <ImgRenderer> *tp = it.getNext ();
      tp->getTypedValue()->finish ();
   }
}

void ImgRendererDist::fatal ()
{
   for (typed::Iterator <TypedPointer <ImgRenderer> > it =
           children->iterator (); it.hasNext (); ) {
      TypedPointer <ImgRenderer> *tp = it.getNext ();
      tp->getTypedValue()->fatal ();
   }
}


} // namespace core
} // namespace dw
