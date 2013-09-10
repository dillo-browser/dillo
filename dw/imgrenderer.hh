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
   virtual void setBuffer (core::Imgbuf *buffer, bool resize = false) = 0;
   virtual void drawRow (int row) = 0;
};

} // namespace core
} // namespace dw

#endif // __DW_IMGRENDERER_HH__


