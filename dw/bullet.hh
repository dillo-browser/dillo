#ifndef __BULLET_HH__
#define __BULLET_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Displays different kind of bullets.
 *
 * Perhaps, in the future, Unicode characters are used for bullets, so this
 * widget is not used anymore.
 */
class Bullet: public core::Widget
{
protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void draw (core::View *view, core::Rectangle *area);
   core::Iterator *iterator (core::Content::Type mask, bool atEnd);

public:
   Bullet ();
};

} // namespace dw

#endif // __BULLET_HH__
