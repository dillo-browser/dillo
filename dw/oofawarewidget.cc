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
   for (int i = 0; i < NUM_OOFM; i++) {
      if(outOfFlowMgr[i]) {
         // I feel more comfortable by letting the OOF aware widget delete 
         // these widgets, instead of doing this in ~OutOfFlowMgr.
         for (int j = 0; j < outOfFlowMgr[i]->getNumWidgets (); j++)
            delete outOfFlowMgr[i]->getWidget (j);
         
         delete outOfFlowMgr[i];
      }
   }

   DBG_OBJ_DELETE ();
}

void OOFAwareWidget::notifySetAsTopLevel()
{
   oofContainer[OOFM_FLOATS] = oofContainer[OOFM_ABSOLUTE]
      = oofContainer[OOFM_FIXED] = this;
}

bool OOFAwareWidget::isOOFContainer (Widget *widget, int oofmIndex)
{
   // TODO The methods isPossibleContainer() and isPossibleContainerParent()
   // are only used in few cases. Does not matter currently, however.

   switch (oofmIndex) {
   case OOFM_FLOATS:
      return widget->instanceOf (OOFAwareWidget::CLASS_ID) &&
         (// For floats, only some OOF aware widgets are considered as
          // containers.
          ((OOFAwareWidget*)widget)->isPossibleContainer (OOFM_FLOATS) &&
          // The second condition: that this block is "out of flow", in a
          // wider sense.
          (// The toplevel widget is "out of flow", since there is no
           // parent, and so no context.
           widget->getParent() == NULL ||
           // A similar reasoning applies to a widget with an
           // unsuitable parent (typical example: a table cell (this
           // is also a text block, so possible float container)
           // within a table widget, which is not a suitable float
           // container parent).
           !(widget->getParent()->instanceOf (OOFAwareWidget::CLASS_ID) &&
             ((OOFAwareWidget*)widget->getParent())
                 ->isPossibleContainerParent (OOFM_FLOATS)) ||
           // Inline blocks are containing blocks, too.
           widget->getStyle()->display == core::style::DISPLAY_INLINE_BLOCK ||
           // Finally, "out of flow" in a narrower sense: floats;
           // absolutely and fixedly positioned elements; furthermore,
           // relatively positioned elements must already be
           // considered here, since they may constitute a stacking
           // context.
           testWidgetOutOfFlow (widget) || testWidgetPositioned (widget)));
      
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
         (widget->getParent() == NULL || testWidgetPositioned (widget));
      
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
         if (isOOFContainer (widget, oofmIndex)) {
            assert (widget->instanceOf (OOFAwareWidget::CLASS_ID));
            oofContainer[oofmIndex] = (OOFAwareWidget*)widget;
         }
   
      DBG_OBJ_ARRSET_PTR ("oofContainer", oofmIndex, oofContainer[oofmIndex]);

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

void OOFAwareWidget::correctRequisitionByOOF (Requisition *requisition,
                                              void (*splitHeightFun) (int, int*,
                                                                      int*))
{
   for (int i = 0; i < NUM_OOFM; i++) {
      if (outOfFlowMgr[i]) {
         int oofWidth, oofHeight;
         DBG_OBJ_MSGF ("resize", 1,
                       "before considering widgets by OOFM #%d: %d * (%d + %d)",
                       i, requisition->width, requisition->ascent,
                       requisition->descent);

         outOfFlowMgr[i]->getSize (requisition, &oofWidth, &oofHeight);

         if (oofWidth > requisition->width) {
            if (adjustExtraSpaceWhenCorrectingRequisitionByOOF ())
               extraSpace.right = max (extraSpace.right,
                                       oofWidth - requisition->width);
            requisition->width = oofWidth;
         }

         if (oofHeight > requisition->ascent + requisition->descent) {
            if (adjustExtraSpaceWhenCorrectingRequisitionByOOF ())
               extraSpace.bottom = max (extraSpace.bottom,
                                        oofHeight - (requisition->ascent +
                                                     requisition->descent));
            splitHeightFun (oofHeight,
                            &requisition->ascent, &requisition->descent);
         }
      }
   }
}

void OOFAwareWidget::correctExtremesByOOF (Extremes *extremes)
{
   for (int i = 0; i < NUM_OOFM; i++) {
      if (outOfFlowMgr[i]) {
         int oofMinWidth, oofMaxWidth;
         outOfFlowMgr[i]->getExtremes (extremes, &oofMinWidth, &oofMaxWidth);
         
         DBG_OBJ_MSGF ("resize", 1, "OOFM (#%d) correction: %d / %d",
                       i, oofMinWidth, oofMaxWidth);

         extremes->minWidth = max (extremes->minWidth, oofMinWidth);
         extremes->minWidthIntrinsic = max (extremes->minWidthIntrinsic,
                                            oofMinWidth);
         extremes->maxWidth = max (extremes->maxWidth, oofMaxWidth);
         extremes->maxWidthIntrinsic = max (extremes->maxWidthIntrinsic,
                                            oofMinWidth);
      }
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

void OOFAwareWidget::containerSizeChangedForChildrenOOF ()
{
   for (int i = 0; i < NUM_OOFM; i++)
      if (outOfFlowMgr[i])
         outOfFlowMgr[i]->containerSizeChangedForChildren ();
}

void OOFAwareWidget::drawOOF (View *view, Rectangle *area)
{
   for (int i = 0; i < NUM_OOFM; i++)
      if(outOfFlowMgr[i])
         outOfFlowMgr[i]->draw(view, area);
}

Widget *OOFAwareWidget::getWidgetOOFAtPoint (int x, int y, int level)
{
   for (int i = 0; i < NUM_OOFM; i++) {
      Widget *oofWidget =
         outOfFlowMgr[i] ?
         outOfFlowMgr[i]->getWidgetAtPoint (x, y, level) : NULL;
      if (oofWidget)
         return oofWidget;
   }

   return NULL;
}


int OOFAwareWidget::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   if (isWidgetOOF(child)) {
      assert (getWidgetOutOfFlowMgr(child) &&
              getWidgetOutOfFlowMgr(child)->dealingWithSizeOfChild (child));
      return getWidgetOutOfFlowMgr(child)->getAvailWidthOfChild (child,
                                                                 forceValue);
   } else
      return Widget::getAvailWidthOfChild (child, forceValue);
}

int OOFAwareWidget::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   if (isWidgetOOF(child)) {
      assert (getWidgetOutOfFlowMgr(child) &&
              getWidgetOutOfFlowMgr(child)->dealingWithSizeOfChild (child));
      return getWidgetOutOfFlowMgr(child)->getAvailHeightOfChild (child,
                                                                  forceValue);
   } else
      return Widget::getAvailWidthOfChild (child, forceValue);
}

void OOFAwareWidget::removeChild (Widget *child)
{
   // Sub classes should implement this method (and Textblock and
   // Table do so), so this point is only reached from
   // ~OOFAwareWidget, which removes widgets out of flow.
   assert (isWidgetOOF (child));
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

bool OOFAwareWidget::isPossibleContainer (int oofmIndex)
{
   return oofmIndex != OOFM_FLOATS;
}

bool OOFAwareWidget::isPossibleContainerParent (int oofmIndex)
{
   return oofmIndex != OOFM_FLOATS;
}

bool OOFAwareWidget::adjustExtraSpaceWhenCorrectingRequisitionByOOF ()
{
   return true;
}

} // namespace oof

} // namespace dw
