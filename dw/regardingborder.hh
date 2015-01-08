#ifndef __DW_REGARDINGBORDER_HH__
#define __DW_REGARDINGBORDER_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Base class (rather a tag interface) for those widgets
 *    regarding borders defined by floats, and so allocated on the
 *    full width.
 *
 * Will, when integrated to the "dillo_grows" repository, become a sub
 * class of OOFAwareWidget.
 */
class RegardingBorder: public core::Widget
{
public:
   static int CLASS_ID;

   RegardingBorder ();
   ~RegardingBorder ();
};

} // namespace dw

#endif // __DW_REGARDINGBORDER_HH__
