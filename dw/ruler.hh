#ifndef __RULER_HH__
#define __RULER_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Widget for drawing (horizontal) rules.
 *
 * This is really an empty widget, the HTML parser puts a border
 * around it, and drawing is done in dw::core::Widget::drawWidgetBox.
 * The only remarkable point is that the HAS_CONTENT flag is
 * cleared.
 */
class Ruler: public core::Widget
{
protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void draw (core::View *view, core::Rectangle *area);

public:
   Ruler ();

   core::Iterator *iterator (core::Content::Type mask, bool atEnd);
};

} // namespace dw

#endif // __RULER_HH__
