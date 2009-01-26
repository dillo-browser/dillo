#ifndef __DW_LISTITEM_HH__
#define __DW_LISTITEM_HH__

#include "core.hh"
#include "alignedtextblock.hh"

namespace dw {

class ListItem: public AlignedTextblock
{
protected:
   int getValue ();
   void setMaxValue (int maxValue, int value);

public:
   static int CLASS_ID;

   ListItem(ListItem *ref, bool limitTextWidth);
   ~ListItem();

   void initWithWidget (core::Widget *widget, core::style::Style *style);
   void initWithText (const char *text, core::style::Style *style);
};

} // namespace dw

#endif // __DW_LISTITEM_HH__
