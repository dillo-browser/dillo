#ifndef __DW_IMGBUF_HH__
#define __DW_IMGBUF_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

#include "../lout/debug.hh"

namespace dw {
namespace core {

/**
 * \brief The platform independent interface for image buffers.
 *
 * %Image buffers depend on the platform (see \ref dw-images-and-backgrounds),
 * but have this general, platform independent interface. The purpose of
 * an image buffer is
 *
 * <ol>
 * <li> storing the image data,
 * <li> handling scaled versions of this buffer, and
 * <li> drawing.
 * </ol>
 *
 * The latter must be done independently from the window.
 *
 * <h3>Creating</h3>
 *
 * %Image buffers are created by calling dw::core::Platform::createImgbuf.
 *
 * <h3>Storing %Image Data</h3>
 *
 * dw::core::Imgbuf supports five image types, which are listed in the table
 * below. The representation defines, how the colors are stored within
 * the data, which is passed to dw::core::Imgbuf::copyRow.
 *
 * <table>
 * <tr><th>Type (dw::core::Imgbuf::Type) <th>Bytes per
 *                                           Pixel <th>Representation
 * <tr><td>dw::core::Imgbuf::RGB           <td>3   <td>red, green, blue
 * <tr><td>dw::core::Imgbuf::RGBA          <td>4   <td>red, green, blue, alpha
 * <tr><td>dw::core::Imgbuf::GRAY          <td>1   <td>gray value
 * <tr><td>dw::core::Imgbuf::INDEXED       <td>1   <td>index to colormap
 * <tr><td>dw::core::Imgbuf::INDEXED_ALPHA <td>1    <td>index to colormap
 * </table>
 *
 * The last two types need a colormap, which is set by
 * dw::core::Imgbuf::setCMap, which must be called before
 * dw::core::Imgbuf::copyRow. This function expects the colors as 32 bit
 * unsigned integers, which have the format 0xrrbbgg (for indexed
 * images), or 0xaarrggbb (for indexed alpha), respectively.
 *
 *
 * <h3>Scaling</h3>
 *
 * The buffer with the original size, which was created by
 * dw::core::Platform::createImgbuf, is called root buffer. Imgbuf provides
 * the ability to scale buffers. Generally, both root buffers, as well as
 * scaled buffers, may be shared, memory management is done by reference
 * counters.
 *
 * Via dw::core::Imgbuf::getScaledBuf, you can retrieve a scaled buffer.
 * Generally, something like this must work always, in an efficient way:
 *
 * \code
 * dw::core::Imgbuf *curBuf, *oldBuf;
 * int width, heigt,
 * // ...
 * oldBuf = curBuf;
 * curBuf = oldBuf->getScaledBuf(oldBuf, width, height);
 * oldBuf->unref();
 * \endcode
 *
 * \em oldBuf may both be a root buffer, or a scaled buffer.
 *
 * The root buffer keeps a list of all children, and all methods
 * operating on the image data (dw::core::Imgbuf::copyRow and
 * dw::core::Imgbuf::setCMap) are delegated to the scaled buffers, when
 * processed, and inherited, when a new scaled buffer is created. This
 * means, that they must only be performed for the root buffer.
 *
 * A possible implementation could be (dw::fltk::FltkImgbuf does it this way):
 *
 * <ul>
 * <li> If the method is called with an already scaled image buffer, this is
 *      delegated to the root buffer.
 *
 * <li> If the given size is the original size, the root buffer is
 *      returned, with an increased reference counter.
 *
 * <li> Otherwise, if this buffer has already been scaled to the given
 *      size, return this scaled buffer, with an increased reference
 *      counter.
 *
 * <li> Otherwise, return a new scaled buffer with reference counter 1.
 * </ul>
 *
 * Special care is to be taken, when the root buffer is not used anymore,
 * i.e. after dw::core::Imgbuf::unref the reference counter is 0, but there
 * are still scaled buffers. Since all methods operating on the image data
 * (dw::core::Imgbuf::copyRow and dw::core::Imgbuf::setCMap) are called for
 * the root buffer, the root buffer is still needed, and so must not be
 * deleted at this point. This is, how dw::fltk::FltkImgbuf solves this
 * problem:
 *
 * <ul>
 * <li> dw::fltk::FltkImgbuf::unref does, for root buffers, check, not only
 *      whether dw::fltk::FltkImgbuf::refCount is 0, but also, whether
 *      there are children left. When the latter is the case, the buffer
 *      is not deleted.
 *
 * <li> There is an additional check in dw::fltk::FltkImgbuf::detachScaledBuf,
 *      which deals with the case, that dw::fltk::FltkImgbuf::refCount is 0,
 *      and the last scaled buffer is removed.
 * </ul>
 *
 * In the following example:
 *
 * \code
 * dw::fltk::FltkPlatform *platform = new dw::fltk::FltkPlatform ();
 * dw::core::Layout *layout = new dw::core::Layout (platform);
 *
 * dw::core::Imgbuf *rootbuf =
 *    layout->createImgbuf (dw::core::Imgbuf::RGB, 100, 100);
 * dw::core::Imgbuf *scaledbuf = rootbuf->getScaledBuf (50, 50);
 * rootbuf->unref ();
 * scaledbuf->unref ();
 * \endcode
 *
 * the root buffer is not deleted, when dw::core::Imgbuf::unref is called,
 * since a scaled buffer is left. After calling dw::core::Imgbuf::unref for
 * the scaled buffer, it is deleted, and after it, the root buffer.
 *
 * <h3>Drawing</h3>
 *
 * dw::core::Imgbuf provides no methods for drawing, instead, this is
 * done by the views (implementation of dw::core::View).
 *
 * There are two situations, when drawing is necessary:
 *
 * <ol>
 * <li> To react on expose events, the function dw::core::View::drawImage
 *      should be used, with the following parameters:
 *      <ul>
 *      <li> of course, the image buffer,
 *      <li> where the root of the image would be displayed (as \em xRoot
 *           and \em yRoot), and
 *      <li> the region within the image, which should be displayed (\em x,
 *           \em y, \em width, \em height).
 *      </ul>
 *
 * <li> When a row has been copied, it has to be drawn. To determine the
 *      area, which has to be drawn, the dw::core::Imgbuf::getRowArea
 *      should be used. The result can then passed
 *      to dw::core::View::drawImage.
 * </ol>
 *
 * \sa \ref dw-images-and-backgrounds
 */
class Imgbuf: public lout::object::Object, public lout::signal::ObservedObject
{
public:
   enum Type { RGB, RGBA, GRAY, INDEXED, INDEXED_ALPHA };

   inline Imgbuf () {
      DBG_OBJ_CREATE ("dw::core::Imgbuf");
      DBG_OBJ_BASECLASS (lout::object::Object);
      DBG_OBJ_BASECLASS (lout::signal::ObservedObject);
   }

   /*
    * Methods called from the image decoding
    */

   virtual void setCMap (int *colors, int num_colors) = 0;
   virtual void copyRow (int row, const byte *data) = 0;
   virtual void newScan () = 0;

   /*
    * Methods called from dw::Image
    */

   virtual Imgbuf* getScaledBuf (int width, int height) = 0;
   virtual void getRowArea (int row, dw::core::Rectangle *area) = 0;
   virtual int getRootWidth () = 0;
   virtual int getRootHeight () = 0;


   /**
    * Creates an image buffer with same parameters (type, gamma etc.)
    * except size.
    */
   virtual Imgbuf *createSimilarBuf (int width, int height) = 0;

   /**
    * Copies another image buffer into this image buffer.
    */
   virtual void copyTo (Imgbuf *dest, int xDestRoot, int yDestRoot,
                        int xSrc, int ySrc, int widthSrc, int heightSrc) = 0;

   /*
    * Reference counting.
    */

   virtual void ref () = 0;
   virtual void unref () = 0;

   /**
    * \todo Comment
    */
   virtual bool lastReference () = 0;


   /**
    * \todo Comment
    */
   virtual void setDeleteOnUnref (bool deleteOnUnref) = 0;

   /**
    * \todo Comment
    */
   virtual bool isReferred () = 0;
};

} // namespace core
} // namespace dw

#endif // __DW_IMGBUF_HH__
