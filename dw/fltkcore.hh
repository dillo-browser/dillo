#ifndef __DW_FLTK_CORE_HH__
#define __DW_FLTK_CORE_HH__

#define __INCLUDED_FROM_DW_FLTK_CORE_HH__

namespace dw {
namespace fltk {
namespace ui {

class FltkResource;

} // namespace ui
} // namespace fltk
} // namespace dw

#include <FL/Fl_Widget.H>

#include "core.hh"
#include "fltkimgbuf.hh"
#include "fltkplatform.hh"
#include "fltkui.hh"

namespace dw {
namespace fltk {

inline void freeall ()
{
   FltkImgbuf::freeall ();
}

} // namespace fltk
} // namespace dw

#undef __INCLUDED_FROM_DW_FLTK_CORE_HH__

#endif // __DW_FLTK_CORE_HH__
