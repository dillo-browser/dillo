#ifndef __DW_OOFAWAREWIDGET_HH__
#define __DW_OOFAWAREWIDGET_HH__

#include "core.hh"

namespace dw {

namespace oof {

/**
 * \brief Base class for widgets which can act as container and
 *     generator for widgets out of flow.
 *
 * (Perhaps it should be diffenciated between the two roles, container
 * and generator, but this would make multiple inheritance necessary.)
 */
class OOFAwareWidget: public core::Widget
{
public:
   virtual void borderChanged (int y, core::Widget *vloat);
   virtual void oofSizeChanged (bool extremesChanged);
   virtual int getLineBreakWidth (); // Should perhaps be renamed.
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFAWAREWIDGET_HH__
