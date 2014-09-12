/*
 * Dillo Widget
 *
 * Copyright 2014 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "oofawarewidget.hh"
#include "ooffloatsmgr.hh"
#include "oofposabsmgr.hh"
#include "oofposfixedmgr.hh"
#include "textblock.hh"

// TODO: Avoid reference to Textblock by replacing
// "instanceOf (Textblock::CLASS_ID)" by some new methods.

using namespace dw;
using namespace dw::core;
using namespace lout::misc;

namespace dw {

namespace oof {

int OOFAwareWidget::CLASS_ID = -1;

OOFAwareWidget::OOFAwareWidget ()
{
   DBG_OBJ_CREATE ("dw::oof::OOFAwareWidget");
   registerName ("dw::oof::OOFAwareWidget", &CLASS_ID);

   for (int i = 0; i < NUM_OOFM; i++) {
      oofContainer[i] = NULL;
      outOfFlowMgr[i] = NULL;
   }
}

OOFAwareWidget::~OOFAwareWidget ()
{
   DBG_OBJ_DELETE ();
}

void OOFAwareWidget::notifySetAsTopLevel()
{
   oofContainer[OOFM_FLOATS] = oofContainer[OOFM_ABSOLUTE]
      = oofContainer[OOFM_FIXED] = this;
}

bool OOFAwareWidget::isContainingBlock (Widget *widget, int oofmIndex)
{
   switch (oofmIndex) {
   case OOFM_FLOATS:
      return
         // For floats, only textblocks are considered as containing
         // blocks.
         widget->instanceOf (Textblock::CLASS_ID) &&
         // The second condition: that this block is "out of flow", in a
         // wider sense.
         (// The toplevel widget is "out of flow", since there is no
          // parent, and so no context.
          widget->getParent() == NULL ||
          // A similar reasoning applies to a widget with another parent
          // than a textblock (typical example: a table cell (this is
          // also a text block) within a table widget).
          !widget->getParent()->instanceOf (Textblock::CLASS_ID) ||
          // Inline blocks are containing blocks, too.
          widget->getStyle()->display == core::style::DISPLAY_INLINE_BLOCK ||
          // Finally, "out of flow" in a narrower sense: floats; absolutely
          // and fixedly positioned elements.
          testWidgetOutOfFlow (widget));
      
   case OOFM_ABSOLUTE:
      // Only the toplevel widget (as for all) as well as absolutely,
      // relatively, and fixedly positioned elements constitute the
      // containing block for absolutely positioned elements, but
      // neither floats nor other elements like table cells.
      //
      // (Notice that relative positions are not yet supported, but
      // only tested to get the correct containing block. Furthermore,
      // it seems that this test would be incorrect for floats.)
      // 
      // We also test whether this widget is a textblock: is this
      // necessary? (What about other absolutely widgets containing
      // children, like tables? TODO: Check CSS spec.)

      return widget->instanceOf (OOFAwareWidget::CLASS_ID) &&
         (widget->getParent() == NULL ||
          testWidgetAbsolutelyPositioned (widget) ||
          testWidgetRelativelyPositioned (widget) ||
          testWidgetFixedlyPositioned (widget));
      
   case OOFM_FIXED:
      // The single container for fixedly positioned elements is the
      // toplevel (canvas; actually the viewport). (The toplevel
      // widget should always be a textblock; at least this is the
      // case in dillo.)
      return widget->getParent() == NULL;

   default:
      // compiler happiness
      lout::misc::assertNotReached ();
      return false;
   }
}

void OOFAwareWidget::notifySetParent ()
{
   // Search for containing blocks.
   for (int oofmIndex = 0; oofmIndex < NUM_OOFM; oofmIndex++) {
      oofContainer[oofmIndex] = NULL;

      for (Widget *widget = this;
           widget != NULL && oofContainer[oofmIndex] == NULL;
           widget = widget->getParent ())
         if (isContainingBlock (widget, oofmIndex)) {
            assert (widget->instanceOf (OOFAwareWidget::CLASS_ID));
            oofContainer[oofmIndex] = (OOFAwareWidget*)widget;
         }
   
      DBG_OBJ_ARRSET_PTR ("containingBlock", oofmIndex,
                          containingBlock[oofmIndex]);

      assert (oofContainer[oofmIndex] != NULL);
   }
}

void OOFAwareWidget::initOutOfFlowMgrs ()
{
   if (oofContainer[OOFM_FLOATS]->outOfFlowMgr[OOFM_FLOATS] == NULL) {
      oofContainer[OOFM_FLOATS]->outOfFlowMgr[OOFM_FLOATS] =
         new OOFFloatsMgr (oofContainer[OOFM_FLOATS]);
      DBG_OBJ_ASSOC (oofContainer[OOFM_FLOATS],
                     oofContainer[OOFM_FLOATS]->outOfFlowMgr[OOFM_FLOATS]);
   }

   if (oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE] == NULL) {
      oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE] =
         new OOFPosAbsMgr (oofContainer[OOFM_ABSOLUTE]);
      DBG_OBJ_ASSOC (oofContainer[OOFM_ABSOLUTE],
                     oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE]);
   }

   if (oofContainer[OOFM_FIXED]->outOfFlowMgr[OOFM_FIXED] == NULL) {
      oofContainer[OOFM_FIXED]->outOfFlowMgr[OOFM_FIXED] =
         new OOFPosFixedMgr (oofContainer[OOFM_FIXED]);
      DBG_OBJ_ASSOC (oofContainer[OOFM_FIXED],
                     oofContainer[OOFM_FIXED]->outOfFlowMgr[OOFM_FIXED]);
   }
}

void OOFAwareWidget::sizeAllocateStart (core::Allocation *allocation)
{

   for (int i = 0; i < NUM_OOFM; i++)
      if (oofContainer[i]->outOfFlowMgr[i])
         oofContainer[i]->outOfFlowMgr[i]->sizeAllocateStart (this, allocation);
}

void OOFAwareWidget::sizeAllocateEnd ()
{
   for (int i = 0; i < NUM_OOFM; i++)
      if (oofContainer[i]->outOfFlowMgr[i])
         oofContainer[i]->outOfFlowMgr[i]->sizeAllocateEnd (this);
}

void OOFAwareWidget::borderChanged (int y, Widget *vloat)
{
   assertNotReached ();
}

void OOFAwareWidget::oofSizeChanged (bool extremesChanged)
{
   DBG_OBJ_ENTER ("resize", 0, "oofSizeChanged", "%s",
                  extremesChanged ? "true" : "false");
   queueResize (-1, extremesChanged);

   // Extremes changes may become also relevant for the children.
   if (extremesChanged)
      containerSizeChanged ();

   DBG_OBJ_LEAVE ();
}

int OOFAwareWidget::getLineBreakWidth ()
{
   assertNotReached ();
   return 0;
}

} // namespace oof

} // namespace dw
