#ifndef __RULER_HH__
#define __RULER_HH__

#include "regardingborder.hh"

namespace dw {

/**
 * \brief Widget for drawing (horizontal) rules.
 *
 * This is really an empty widget, the HTML parser puts a border
 * around it, and drawing is done in dw::core::Widget::drawWidgetBox.
 *
 * Ruler implements RegardingBorder; this way, it is simpler to fit
 * the ruler exactly within the space between floats. Currently, the
 * drawn area of the ruler is too large (but most of the superfluous
 * part is hidden by the floats); this problem will not solved but in
 * "dillo_grows", where RegardingBorder is a sub class of
 * OOFAwareWidget.
 */
class Ruler: public RegardingBorder
{
protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void getExtremesImpl (core::Extremes *extremes);
   void containerSizeChangedForChildren ();
   bool usesAvailWidth ();
   void draw (core::View *view, core::Rectangle *area);

public:
   static int CLASS_ID;

   Ruler ();
   ~Ruler ();

   bool isBlockLevel ();

   core::Iterator *iterator (core::Content::Type mask, bool atEnd);
};

} // namespace dw

#endif // __RULER_HH__
