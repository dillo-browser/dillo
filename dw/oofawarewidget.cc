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
#include "oofposrelmgr.hh"
#include "oofposfixedmgr.hh"

using namespace dw;
using namespace dw::core;
using namespace dw::core::style;
using namespace lout::object;
using namespace lout::misc;
using namespace lout::container::typed;

namespace dw {

namespace oof {

const char *OOFAwareWidget::OOFM_NAME[NUM_OOFM] = {
   "FLOATS", "ABSOLUTE", "RELATIVE", "FIXED"
};

int OOFAwareWidget::CLASS_ID = -1;

OOFAwareWidget::OOFAwareWidget ()
{
   DBG_OBJ_CREATE ("dw::oof::OOFAwareWidget");
   registerName ("dw::oof::OOFAwareWidget", &CLASS_ID);

   for (int i = 0; i < NUM_OOFM; i++) {
      oofContainer[i] = NULL;
      DBG_OBJ_ARRSET_PTR ("oofContainer", i, oofContainer[i]);
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

const char *OOFAwareWidget::stackingLevelText (int level)
{
   switch (level) {
   case SL_START:      return "START";
   case SL_BACKGROUND: return "BACKGROUND";
   case SL_SC_BOTTOM:  return "SC_BOTTOM";
   case SL_IN_FLOW:    return "IN_FLOW";
   case SL_OOF_REF:    return "OOF_REF";
   case SL_OOF_CONT:   return "OOF_CONT";
   case SL_SC_TOP:     return "SC_TOP";
   case SL_END:        return "END";
   default:            return "???";
   }
}

void OOFAwareWidget::notifySetAsTopLevel ()
{
   for (int i = 0; i < NUM_OOFM; i++) {
      oofContainer[i] = this;
      DBG_OBJ_ARRSET_PTR ("oofContainer", i, oofContainer[i]);
   }
}

int OOFAwareWidget::getOOFMIndex (Widget *widget)
{
   DBG_OBJ_ENTER_O ("construct", 0, NULL, "getOOFMIndex", "%p", widget);  
   DBG_OBJ_MSGF_O ("construct", 1, NULL, "position = %s, float = %s",
                   widget->getStyle()->position
                   == style::POSITION_STATIC ? "static" :
                   (widget->getStyle()->position
                    == style::POSITION_RELATIVE ? "relative" :
                    (widget->getStyle()->position
                     == style::POSITION_ABSOLUTE ? "absolute" :
                     (widget->getStyle()->position
                      == style::POSITION_FIXED ? "fixed" : "???"))),
                   widget->getStyle()->vloat == style::FLOAT_NONE ? "none" :
                   (widget->getStyle()->vloat == style::FLOAT_LEFT ? "left" :
                    (widget->getStyle()->vloat == style::FLOAT_RIGHT ?
                     "right" : "???")));

   int index = -1;
   if (testWidgetFloat (widget))
      index = OOFM_FLOATS;
   else if (testWidgetAbsolutelyPositioned (widget))
      index = OOFM_ABSOLUTE;
   else if (testWidgetRelativelyPositioned (widget))
      index = OOFM_RELATIVE;
   else if (testWidgetFixedlyPositioned (widget))
      index = OOFM_FIXED;
   else
      lout::misc::assertNotReached ();

   DBG_OBJ_LEAVE_VAL_O (NULL, "%d (%s)", index, OOFM_NAME[index]);
   return index;
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
           widget->getStyle()->display == DISPLAY_INLINE_BLOCK ||
           // Same for blocks with 'overview' set to another value than
           // (the default value) 'visible'.
           widget->getStyle()->overflow != OVERFLOW_VISIBLE ||
           // Finally, "out of flow" in a narrower sense: floats;
           // absolutely and fixedly positioned elements. (No
           // relatively positioned elements; since the latters
           // constitute a stacking context, drawing of floats gets
           // somewhat more complicated; see "interrupting the drawing
           // process" in "dw-stacking-context.doc".
           testWidgetOutOfFlow (widget)));
      
   case OOFM_RELATIVE:
   case OOFM_ABSOLUTE:
      return widget->instanceOf (OOFAwareWidget::CLASS_ID) &&
         (widget->getParent() == NULL ||
          OOFAwareWidget::testWidgetPositioned (widget));

      
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
         new OOFFloatsMgr (oofContainer[OOFM_FLOATS], OOFM_FLOATS);
      DBG_OBJ_ASSOC (oofContainer[OOFM_FLOATS],
                     oofContainer[OOFM_FLOATS]->outOfFlowMgr[OOFM_FLOATS]);
   }

   if (oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE] == NULL) {
      oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE] =
         new OOFPosAbsMgr (oofContainer[OOFM_ABSOLUTE]);
      DBG_OBJ_ASSOC (oofContainer[OOFM_ABSOLUTE],
                     oofContainer[OOFM_ABSOLUTE]->outOfFlowMgr[OOFM_ABSOLUTE]);
   }

   if (oofContainer[OOFM_RELATIVE]->outOfFlowMgr[OOFM_RELATIVE] == NULL) {
      oofContainer[OOFM_RELATIVE]->outOfFlowMgr[OOFM_RELATIVE] =
         new OOFPosRelMgr (oofContainer[OOFM_RELATIVE]);
      DBG_OBJ_ASSOC (oofContainer[OOFM_RELATIVE],
                     oofContainer[OOFM_RELATIVE]->outOfFlowMgr[OOFM_RELATIVE]);
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
   DBG_OBJ_ENTER ("resize", 0, "correctRequisitionByOOF", "%d * (%d + %d), ...",
                  requisition->width, requisition->ascent,
                  requisition->descent);

   requisitionWithoutOOF = *requisition;

   for (int i = 0; i < NUM_OOFM; i++) {
      if (outOfFlowMgr[i]) {
         DBG_OBJ_MSGF ("resize", 1, "OOFM for %s", OOFM_NAME[i]);
         DBG_OBJ_MSG_START ();

         int oofWidth, oofHeight;

         outOfFlowMgr[i]->getSize (requisition, &oofWidth, &oofHeight);
         DBG_OBJ_MSGF ("resize", 1, "result: %d * %d", oofWidth, oofHeight);

         if (oofWidth > requisition->width) {
            if (outOfFlowMgr[i]->containerMustAdjustExtraSpace () &&
                adjustExtraSpaceWhenCorrectingRequisitionByOOF ()) {
               extraSpace.right = max (extraSpace.right,
                                       oofWidth - requisition->width);
               DBG_OBJ_SET_NUM ("extraSpace.right", extraSpace.right);
            }

            requisition->width = oofWidth;
         }

         if (oofHeight > requisition->ascent + requisition->descent) {
            if (outOfFlowMgr[i]->containerMustAdjustExtraSpace () &&
                adjustExtraSpaceWhenCorrectingRequisitionByOOF ()) {
               extraSpace.bottom = max (extraSpace.bottom,
                                        oofHeight - (requisition->ascent +
                                                     requisition->descent));
               DBG_OBJ_SET_NUM ("extraSpace.bottom", extraSpace.bottom);
            }
   
            splitHeightFun (oofHeight,
                            &requisition->ascent, &requisition->descent);
         }

         if (!adjustExtraSpaceWhenCorrectingRequisitionByOOF ()) {
            requisitionWithoutOOF.width = max (requisitionWithoutOOF.width,
                                               oofWidth);
            if (oofHeight >
                requisitionWithoutOOF.ascent + requisitionWithoutOOF.descent)
               splitHeightFun (oofHeight, &requisitionWithoutOOF.ascent,
                               &requisitionWithoutOOF.descent);
         }

         DBG_OBJ_MSGF ("resize", 1, "after correction: %d * (%d + %d)",
                       requisition->width, requisition->ascent,
                       requisition->descent);
         DBG_OBJ_MSG_END ();
      } else
         DBG_OBJ_MSGF ("resize", 1, "no OOFM for %s", OOFM_NAME[i]);
   }

   DBG_OBJ_SET_NUM ("requisitionWithoutOOF.width", requisitionWithoutOOF.width);
   DBG_OBJ_SET_NUM ("requisitionWithoutOOF.ascent",
                    requisitionWithoutOOF.ascent);
   DBG_OBJ_SET_NUM ("requisitionWithoutOOF.descent",
                    requisitionWithoutOOF.descent);

   DBG_OBJ_LEAVE ();
}

void OOFAwareWidget::correctExtremesByOOF (Extremes *extremes)
{
   DBG_OBJ_ENTER ("resize", 0, "correctExtremesByOOF", "%d (%d) / %d (%d)",
                  extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   for (int i = 0; i < NUM_OOFM; i++) {
      if (outOfFlowMgr[i]) {
         DBG_OBJ_MSGF ("resize", 1, "OOFM for %s", OOFM_NAME[i]);
         DBG_OBJ_MSG_START ();

         int oofMinWidth, oofMaxWidth;
         outOfFlowMgr[i]->getExtremes (extremes, &oofMinWidth, &oofMaxWidth);
         DBG_OBJ_MSGF ("resize", 1, "result: %d / %d",
                       oofMinWidth, oofMaxWidth);

         extremes->minWidth = max (extremes->minWidth, oofMinWidth);
         extremes->minWidthIntrinsic = max (extremes->minWidthIntrinsic,
                                            oofMinWidth);
         extremes->maxWidth = max (extremes->maxWidth, oofMaxWidth);
         extremes->maxWidthIntrinsic = max (extremes->maxWidthIntrinsic,
                                            oofMinWidth);
         extremes->adjustmentWidth = max (extremes->adjustmentWidth,
                                          oofMinWidth);

         DBG_OBJ_MSGF ("resize", 1, "after correction: %d (%d) / %d (%d)",
                       extremes->minWidth, extremes->minWidthIntrinsic,
                       extremes->maxWidth, extremes->maxWidthIntrinsic);
         DBG_OBJ_MSG_END ();
      } else
         DBG_OBJ_MSGF ("resize", 1, "no OOFM for %s", OOFM_NAME[i]);
   }

   DBG_OBJ_LEAVE ();
}

void OOFAwareWidget::sizeAllocateStart (Allocation *allocation)
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

bool OOFAwareWidget::doesWidgetOOFInterruptDrawing (Widget *widget)
{
   DBG_OBJ_ENTER ("draw", 0, "doesWidgetOOFInterruptDrawing", "%p", widget);

   // This is the generator of the widget.
   int oofmIndex = getOOFMIndex (widget);
   DBG_OBJ_MSGF ("draw", 1, "oofmIndex = %d", oofmIndex);

   int cl = oofContainer[oofmIndex]->stackingContextWidget->getLevel (),
      gl = stackingContextWidget->getLevel ();

   DBG_OBJ_MSGF ("draw", 1,"%d < %d => %s", cl, gl, cl < gl ? "true" : "false");

   DBG_OBJ_LEAVE ();
   return cl < gl;
}

void OOFAwareWidget::draw (View *view, Rectangle *area, DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   for (int level = SL_START + 1; level < SL_END; level++)
      drawLevel (view, area, level, context);

   DBG_OBJ_LEAVE ();
}

void OOFAwareWidget::drawLevel (View *view, Rectangle *area, int level,
                                DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "OOFAwareWidget::drawLevel",
                  "(%d, %d, %d * %d), %s",
                  area->x, area->y, area->width, area->height,
                  stackingLevelText (level));

   switch (level) {
   case SL_START:
      break;

   case SL_BACKGROUND:
      drawWidgetBox (view, area, false);
      break;

   case SL_SC_BOTTOM:
      if (stackingContextMgr)
         stackingContextMgr->drawBottom (view, area, context);
      break;

   case SL_IN_FLOW:
      // Should be implemented in the sub class.
      break;

   case SL_OOF_REF:
      // Should be implemented in the sub class (when references are hold).
      break;

   case SL_OOF_CONT:
      drawOOF (view, area, context);
      break;

   case SL_SC_TOP:
      if (stackingContextMgr)
         stackingContextMgr->drawTop (view, area, context);
      break;

   case SL_END:
      break;

   default:
      fprintf (stderr, "OOFAwareWidget::drawLevel: invalid level %s (%d)",
               stackingLevelText (level), level);
      break;
   }

   DBG_OBJ_LEAVE ();
}

void OOFAwareWidget::drawOOF (View *view, Rectangle *area,
                              DrawingContext *context)
{
   for (int i = 0; i < NUM_OOFM; i++) {
      if(outOfFlowMgr[i])
         outOfFlowMgr[i]->draw (view, area, context);
   }
} 

Widget *OOFAwareWidget::getWidgetAtPoint (int x, int y,
                                          GettingWidgetAtPointContext *context)
{
   DBG_OBJ_ENTER ("events", 0, "getWidgetAtPoint", "%d, %d", x, y);
   Widget *widgetAtPoint = NULL;

   if (inAllocation (x, y)) {
      for (int level = SL_END - 1; widgetAtPoint == NULL && level > SL_START;
           level--)
         widgetAtPoint = getWidgetAtPointLevel (x, y, level, context);
   }
   
   DBG_OBJ_MSGF ("events", 1, "=> %p", widgetAtPoint);
   DBG_OBJ_LEAVE ();
   return widgetAtPoint;
}

Widget *OOFAwareWidget::getWidgetAtPointLevel (int x, int y, int level,
                                               GettingWidgetAtPointContext
                                               *context)
{
   DBG_OBJ_ENTER ("events", 0, "OOFAwareWidget::getWidgetAtPointLevel",
                  "%d, %d, %s", x, y, stackingLevelText (level));

   Widget *widgetAtPoint = NULL;

   switch (level) {
   case SL_BACKGROUND:
      if (inAllocation (x, y))
         widgetAtPoint = this;
      break;

   case SL_SC_BOTTOM:
      if (stackingContextMgr)
         widgetAtPoint =
            stackingContextMgr->getBottomWidgetAtPoint (x, y, context);
      break;

   case SL_IN_FLOW:
      // Should be implemented in the sub class.
      assertNotReached ();
      break;

   case SL_OOF_REF:
      // Should be implemented in the sub class (when references are hold).
      break;

   case SL_OOF_CONT:
      widgetAtPoint = getWidgetOOFAtPoint (x, y, context);
      break;

   case SL_SC_TOP:
      if (stackingContextMgr)
         widgetAtPoint = 
            stackingContextMgr->getTopWidgetAtPoint (x, y, context);
      break;

   default:
      fprintf (stderr,
               "OOFAwareWidget::getWidgetAtPointLevel: invalid level %s (%d)",
               stackingLevelText (level), level);
      break;
   }

   DBG_OBJ_MSGF ("events", 1, "=> %p", widgetAtPoint);
   DBG_OBJ_LEAVE ();
   return widgetAtPoint;
}

Widget *OOFAwareWidget::getWidgetOOFAtPoint (int x, int y,
                                             GettingWidgetAtPointContext
                                             *context)
{
   Widget *widgetAtPoint = NULL;

   for (int i = NUM_OOFM -1; widgetAtPoint == NULL && i >= 0; i--) {
      if(outOfFlowMgr[i])
         widgetAtPoint = outOfFlowMgr[i]->getWidgetAtPoint (x, y, context);
   }

   return widgetAtPoint;
}

void OOFAwareWidget::removeChild (Widget *child)
{
   // Sub classes should implement this method (and Textblock and
   // Table do so), so this point is only reached from
   // ~OOFAwareWidget, which removes widgets out of flow.
   assert (isWidgetOOF (child));
}

void OOFAwareWidget::borderChanged (int oofmIndex, int y, Widget *widgetOOF)
{
   assertNotReached ();
}

void OOFAwareWidget::widgetRefSizeChanged (int externalIndex)
{
   assertNotReached ();
}

void OOFAwareWidget::clearPositionChanged ()
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

int OOFAwareWidget::getGeneratorX (int oofmIndex)
{
   assertNotReached ();
   return 0;
}

int OOFAwareWidget::getGeneratorY (int oofmIndex)
{
   assertNotReached ();
   return 0;
}

int OOFAwareWidget::getGeneratorWidth ()
{
   assertNotReached ();
   return 0;
}

int OOFAwareWidget::getMaxGeneratorWidth ()
{
   assertNotReached ();
   return 0;
}

bool OOFAwareWidget::usesMaxGeneratorWidth ()
{
   assertNotReached ();
   return false;
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
