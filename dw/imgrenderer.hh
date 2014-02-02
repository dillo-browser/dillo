#ifndef __DW_IMGRENDERER_HH__
#define __DW_IMGRENDERER_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

/**
 * \brief ...
 *
 * \sa \ref dw-images-and-backgrounds
 */
class ImgRenderer
{
public:
   virtual ~ImgRenderer () { }

   /**
    * \brief Called, when an image buffer is attached.
    *
    * This is typically the case when all meta data (size, depth) has been read.
    */
   virtual void setBuffer (core::Imgbuf *buffer, bool resize = false) = 0;

   /**
    * \brief Called, when data from a row is available and has been copied into
    *    the image buffer.
    *
    * The implementation will typically queue the respective area for drawing.
    */
   virtual void drawRow (int row) = 0;

   /**
    * \brief Called, when all image data has been retrieved.
    *
    * The implementation may use this instead of "drawRow" for drawing, to
    * limit the number of draws.
    */
   virtual void finish () = 0;

   /**
    * \brief Called, when there are problems with the retrieval of image data.
    *
    * The implementation may use this to indicate an error.
    */
   virtual void fatal () = 0;
};

/**
 * \brief Implementation of ImgRenderer, which distributes all calls
 * to a set of other implementations of ImgRenderer.
 *
 * The order of the call children is not defined, especially not
 * identical to the order in which they have been added.
 */
class ImgRendererDist: public ImgRenderer
{
   lout::container::typed::HashSet <lout::object::TypedPointer <ImgRenderer> >
      *children;

public:
   inline ImgRendererDist ()
   { children = new lout::container::typed::HashSet
         <lout::object::TypedPointer <ImgRenderer> > (true); }
   ~ImgRendererDist () { delete children; }

   void setBuffer (core::Imgbuf *buffer, bool resize);
   void drawRow (int row);
   void finish ();
   void fatal ();

   void put (ImgRenderer *child)
   { children->put (new lout::object::TypedPointer <ImgRenderer> (child)); }
   void remove (ImgRenderer *child)
   { lout::object::TypedPointer <ImgRenderer> tp (child);
     children->remove (&tp); }
};

} // namespace core
} // namespace dw

#endif // __DW_IMGRENDERER_HH__


