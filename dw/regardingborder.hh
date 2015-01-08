#ifndef __DW_REGARDINGBORDER_HH__
#define __DW_REGARDINGBORDER_HH__

#include "oofawarewidget.hh"

namespace dw {

/**
 * \brief Base class (rather a tag interface) for those widgets
 *    regarding borders defined by floats, and so allocated on the
 *    full width.
 */
class RegardingBorder: public oof::OOFAwareWidget
{
public:
   static int CLASS_ID;

   RegardingBorder ();
   ~RegardingBorder ();
};

} // namespace dw

#endif // __DW_REGARDINGBORDER_HH__
