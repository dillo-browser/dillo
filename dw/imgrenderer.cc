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
