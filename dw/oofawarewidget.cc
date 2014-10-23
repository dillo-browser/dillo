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
using namespace dw::core::style;
using namespace lout::object;
using namespace lout::misc;
using namespace lout::container::typed;

namespace dw {

namespace oof {

OOFAwareWidget::OOFStackingIterator::OOFStackingIterator (bool atEnd)
{
   // TODO Consider atEnd.
   majorLevel = OOFStackingIterator::BACKGROUND;
   minorLevel = index = 0;

   widgetsDrawnAfterInterruption = NULL;
}

OOFAwareWidget::OOFStackingIterator::~OOFStackingIterator ()
{
   if (widgetsDrawnAfterInterruption)
      delete widgetsDrawnAfterInterruption;
}

void OOFAwareWidget::OOFStackingIterator::registerWidgetDrawnAfterInterruption
   (Widget *widget)
{
   if (widgetsDrawnAfterInterruption == NULL)
      widgetsDrawnAfterInterruption = new HashSet<TypedPointer<Widget> > (true);

   TypedPointer<Widget> *p = new TypedPointer<Widget> (widget);
   assert (!widgetsDrawnAfterInterruption->contains (p));
   widgetsDrawnAfterInterruption->put (p);
}

bool OOFAwareWidget::OOFStackingIterator::hasWidgetBeenDrawnAfterInterruption
   (Widget *widget)
{
   if (widgetsDrawnAfterInterruption) {
      TypedPointer<Widget> p (widget);
      return widgetsDrawnAfterInterruption->contains (&p);
   } else
      return false;
}

const char *OOFAwareWidget::OOFStackingIterator::majorLevelText (int majorLevel)
{
   switch (majorLevel) {
   case START:      return "START";
   case BACKGROUND: return "BACKGROUND";
   case SC_BOTTOM:  return "SC_BOTTOM";
   case IN_FLOW:    return "IN_FLOW";
   case OOF_REF:    return "OOF_REF";
   case OOF_CONT:   return "OOF_CONT";
   case SC_TOP:     return "SC_TOP";
   case END:        return "END";
   default:         return "???";
   }
}

void OOFAwareWidget::OOFStackingIterator::intoStringBuffer(StringBuffer *sb)
{
   sb->append ("(");
   sb->append (majorLevelText (majorLevel));
   sb->append (", ");
   sb->appendInt (minorLevel);
   sb->append (", ");
   sb->appendInt (index);
   sb->append (")");
}

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

void OOFAwareWidget::notifySetAsTopLevel ()
{
   oofContainer[OOFM_FLOATS] = oofContainer[OOFM_ABSOLUTE]
      = oofContainer[OOFM_FIXED] = this;
}

bool OOFAwareWidget::getOOFMIndex (Widget *widget)
{
   if (testWidgetFloat (widget))
      return OOFM_FLOATS;
   else if (testWidgetAbsolutelyPositioned (widget))
      return OOFM_ABSOLUTE;
   else if (testWidgetFixedlyPositioned (widget))
      return OOFM_FIXED;
   else {
      lout::misc::assertNotReached ();
      return -1;
   }
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
      
   case OOFM_ABSOLUTE:
      // Only the toplevel widget (as for all) as well as absolutely,
      // relatively, and fixedly positioned elements constitute the
      // containing block for absolutely positioned elements, but
      // neither floats nor other elements like table cells, or
      // elements with 'overview' set to another value than 'visible'.
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

   DBG_OBJ_MSGF_O ("draw", 1, (void*)NULL, "%d < %d => %s",
                   cl, gl, cl < gl ? "true" : "false");

   DBG_OBJ_LEAVE ();
   return cl < gl;
}

Widget *OOFAwareWidget::draw (View *view, Rectangle *area,
                              StackingIteratorStack *iteratorStack)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   OOFStackingIterator *osi = (OOFStackingIterator*)iteratorStack->getTop ();
   Widget *retWidget = NULL;

   while (retWidget == NULL && osi->majorLevel < OOFStackingIterator::END) {
      retWidget = drawLevel (view, area, iteratorStack, osi->majorLevel);

      if (retWidget) {
         if (retWidget->getParent () == this) {
            DBG_OBJ_MSGF ("draw", 1, "interrupted at %p, drawing seperately",
                          retWidget);
            DBG_IF_RTFL {
               StringBuffer sb;
               iteratorStack->intoStringBuffer (&sb);
               DBG_OBJ_MSGF ("draw", 2, "iteratorStack: %s", sb.getChars ());
            }

            core::Rectangle retWidgetArea;
            if (retWidget->intersects (area, &retWidgetArea)) {
               // Similar to Widget::drawToplevel. Nested
               // interruptions are not allowed.
               StackingIteratorStack iteratorStack2;
               Widget *retWidget2 =
                  retWidget->drawTotal (view, &retWidgetArea, &iteratorStack2);
               assert (retWidget2 == NULL);
            }

            osi->registerWidgetDrawnAfterInterruption (retWidget);

            retWidget = NULL; // Continue with the current state of "iterator".
            DBG_OBJ_MSG ("draw", 1, "done with interruption");
         }
      } else {
         osi->majorLevel++;
         osi->minorLevel = osi->index = 0;
      }
   }


   DBG_OBJ_MSGF ("draw", 1, "=> %p", retWidget);
   DBG_OBJ_LEAVE ();
   return retWidget;
}

Widget *OOFAwareWidget::drawLevel (View *view, Rectangle *area,
                                   StackingIteratorStack *iteratorStack,
                                   int majorLevel)
{
   DBG_OBJ_ENTER ("draw", 0, "OOFAwareWidget/drawLevel",
                  "(%d, %d, %d * %d), %s",
                  area->x, area->y, area->width, area->height,
                  OOFStackingIterator::majorLevelText (majorLevel));

   Widget *retWidget = NULL;

   switch (majorLevel) {
   case OOFStackingIterator::START:
      break;

   case OOFStackingIterator::BACKGROUND:
      drawWidgetBox (view, area, false);
      break;

   case OOFStackingIterator::SC_BOTTOM:
      if (stackingContextMgr) {
         OOFStackingIterator *osi =
            (OOFStackingIterator*)iteratorStack->getTop ();
         retWidget =
            stackingContextMgr->drawBottom (view, area, iteratorStack,
                                            &osi->minorLevel, &osi->index);
      }
      break;

   case OOFStackingIterator::IN_FLOW:
      // Should be implemented in the sub class.
      break;

   case OOFStackingIterator::OOF_REF:
      // Should be implemented in the sub class (when references are hold).
      break;

   case OOFStackingIterator::OOF_CONT:
      retWidget = drawOOF (view, area, iteratorStack);
      break;

   case OOFStackingIterator::SC_TOP:
      if (stackingContextMgr) {
         OOFStackingIterator *osi =
            (OOFStackingIterator*)iteratorStack->getTop ();
         retWidget =
            stackingContextMgr->drawTop (view, area, iteratorStack,
                                         &osi->minorLevel, &osi->index);
      }
      break;

   case OOFStackingIterator::END:
      break;
   }

   DBG_OBJ_MSGF ("draw", 1, "=> %p", retWidget);
   DBG_OBJ_LEAVE ();
   return retWidget;
}

Widget *OOFAwareWidget::drawOOF (View *view, Rectangle *area,
                                 StackingIteratorStack *iteratorStack)
{
   OOFStackingIterator *osi = (OOFStackingIterator*)iteratorStack->getTop ();
   assert (osi->majorLevel == OOFStackingIterator::OOF_CONT);

   Widget *retWidget = NULL;

   while (retWidget == NULL && osi->minorLevel < NUM_OOFM) {
      if(outOfFlowMgr[osi->minorLevel])
         retWidget =
            outOfFlowMgr[osi->minorLevel]->draw (view, area, iteratorStack,
                                                 &(osi->index));
      if (retWidget == NULL) {
         osi->minorLevel++;
         osi->index = 0;
      }
   }
   
   return retWidget;
}

Widget *OOFAwareWidget::getWidgetOOFAtPoint (int x, int y)
{
   for (int i = 0; i < NUM_OOFM; i++) {
      Widget *oofWidget =
         outOfFlowMgr[i] ?
         outOfFlowMgr[i]->getWidgetAtPoint (x, y) : NULL;
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

Widget *OOFAwareWidget::getWidgetAtPoint (int x, int y)
{
   if (x >= allocation.x &&
       y >= allocation.y &&
       x <= allocation.x + allocation.width &&
       y <= allocation.y + getHeight ()) {

      if (stackingContextMgr) {
         Widget *scmWidget =
            stackingContextMgr->getTopWidgetAtPoint (x, y);
         if (scmWidget)
            return scmWidget;
      }

      Widget *oofWidget = getWidgetOOFAtPoint (x, y);
      if (oofWidget)
         return oofWidget;

      Widget *childAtPoint = NULL;
      core::Iterator *it =
         iterator ((Content::Type)
                   (Content::WIDGET_IN_FLOW | Content::WIDGET_OOF_CONT),
                   false);

      while (childAtPoint == NULL && it->next ()) {
         Widget *child = it->getContent()->widget;
         if (!StackingContextMgr::handledByStackingContextMgr (child) &&
             child->wasAllocated ())
            childAtPoint = child->getWidgetAtPoint (x, y);
      }

      it->unref ();

      if (childAtPoint)
         return childAtPoint;

      if (stackingContextMgr) {
         Widget *scmWidget =
            stackingContextMgr->getBottomWidgetAtPoint (x, y);
         if (scmWidget)
            return scmWidget;
      }
       
      return this;
   } else
      return NULL;
}


Object *OOFAwareWidget::stackingIterator (bool atEnd)
{
   return new OOFStackingIterator (atEnd);
}

void OOFAwareWidget::borderChanged (int y, Widget *vloat)
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
