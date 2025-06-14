/*
 * Dillo Widget
 *
 * Copyright 2013-2014 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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

#include "ooffloatsmgr.hh"
#include "oofawarewidget.hh"
#include "../lout/debug.hh"

#include <limits.h>

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

namespace oof {

OOFFloatsMgr::WidgetInfo::WidgetInfo (OOFFloatsMgr *oofm, Widget *widget)
{
   this->oofm = oofm;
   this->widget = widget;
}

// ----------------------------------------------------------------------

OOFFloatsMgr::Float::Float (OOFFloatsMgr *oofm, Widget *widget,
                            OOFAwareWidget *generatingBlock, int externalIndex)
   : WidgetInfo (oofm, widget)
{
   this->generator = generatingBlock;
   this->externalIndex = externalIndex;

   yReq = yReal = size.width = size.ascent = size.descent = 0;
   dirty = true;
   index = -1;

   // Sometimes a float with widget = NULL is created as a key; this
   // is not interesting for RTFL.
   if (widget) {
      DBG_OBJ_SET_PTR_O (widget, "<Float>.generatingBlock", generatingBlock);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.externalIndex", externalIndex);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.yReq", yReq);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.yReal", yReal);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.size.width", size.width);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.size.ascent", size.ascent);
      DBG_OBJ_SET_NUM_O (widget, "<Float>.size.descent", size.descent);
      DBG_OBJ_SET_BOOL_O (widget, "<Float>.dirty", dirty);
   }
}

void OOFFloatsMgr::Float::intoStringBuffer(StringBuffer *sb)
{
   sb->append ("{ widget = ");
   sb->appendPointer (getWidget ());

   if (getWidget ()) {
      sb->append (" (");
      sb->append (getWidget()->getClassName ());
      sb->append (")");
   }

   sb->append (", index = ");
   sb->appendInt (index);
   sb->append (", sideSpanningIndex = ");
   sb->appendInt (sideSpanningIndex);
   sb->append (", generator = ");
   sb->appendPointer (generator);
   sb->append (", yReq = ");
   sb->appendInt (yReq);
   sb->append (", yReal = ");
   sb->appendInt (yReal);
   sb->append (", size = { ");
   sb->appendInt (size.width);
   sb->append (" * ");
   sb->appendInt (size.ascent);
   sb->append (" + ");
   sb->appendInt (size.descent);
   sb->append (" }, dirty = ");
   sb->appendBool (dirty);
   sb->append (", sizeChangedSinceLastAllocation = ");
   sb->append (" }");
}

/**
 * `y` is given relative to the container.
 */
bool OOFFloatsMgr::Float::covers (int y, int h)
{
   DBG_OBJ_ENTER_O ("border", 0, getOOFFloatsMgr (), "covers",
                    "%d, %d [vloat: %p]", y, h, getWidget ());

   getOOFFloatsMgr()->ensureFloatSize (this);
   bool b = yReal + size.ascent + size.descent > y && yReal < y + h;

   DBG_OBJ_LEAVE_VAL_O (getOOFFloatsMgr (), "%s", b ? "true" : "false");
   return b;
}

int OOFFloatsMgr::Float::ComparePosition::compare (Object *o1, Object *o2)
{
   return ((Float*)o1)->yReal - ((Float*)o2)->yReal;
}

int OOFFloatsMgr::Float::CompareSideSpanningIndex::compare (Object *o1,
                                                            Object *o2)
{
   return ((Float*)o1)->sideSpanningIndex - ((Float*)o2)->sideSpanningIndex;
}

int OOFFloatsMgr::Float::CompareGBAndExtIndex::compare (Object *o1, Object *o2)
{
   Float *f1 = (Float*)o1, *f2 = (Float*)o2;
   int r = -123; // Compiler happiness: GCC 4.7 does not handle this?;

   DBG_OBJ_ENTER_O ("border", 1, oofm, "CompareGBAndExtIndex::compare",
                    "#%d -> %p/%d, #%d -> %p/#%d",
                    f1->index, f1->generator, f1->externalIndex,
                    f2->index, f2->generator, f2->externalIndex);

   if (f1->generator == f2->generator) {
      r = f1->externalIndex - f2->externalIndex;
      DBG_OBJ_MSGF_O ("border", 2, oofm,
                      "(a) generating blocks equal => %d - %d = %d",
                      f1->externalIndex, f2->externalIndex, r);
   } else {
      TBInfo *t1 = oofm->getOOFAwareWidget (f1->generator),
         *t2 = oofm->getOOFAwareWidget (f2->generator);
      bool rdef = false;

      for (TBInfo *t = t1; t != NULL; t = t->parent)
         if (t->parent == t2) {
            rdef = true;
            r = t->parentExtIndex - f2->externalIndex;
            DBG_OBJ_MSGF_O ("border", 2, oofm,
                            "(b) %p is an achestor of %p; direct child is "
                            "%p (%d) => %d - %d = %d\n",
                            t2->getOOFAwareWidget (), t1->getOOFAwareWidget (),
                            t->getOOFAwareWidget (), t->parentExtIndex,
                            t->parentExtIndex, f2->externalIndex, r);
         }

      for (TBInfo *t = t2; !rdef && t != NULL; t = t->parent)
         if (t->parent == t1) {
            r = f1->externalIndex - t->parentExtIndex;
            rdef = true;
            DBG_OBJ_MSGF_O ("border", 2, oofm,
                            "(c) %p is an achestor of %p; direct child is %p "
                            "(%d) => %d - %d = %d\n",
                            t1->getOOFAwareWidget (), t2->getOOFAwareWidget (),
                            t->getOOFAwareWidget (), t->parentExtIndex,
                            f1->externalIndex, t->parentExtIndex, r);
         }

      if (!rdef) {
         r = t1->index - t2->index;
         DBG_OBJ_MSGF_O ("border", 2, oofm, "(d) other => %d - %d = %d",
                         t1->index, t2->index, r);
      }
   }

   DBG_OBJ_LEAVE_VAL_O (oofm, "%d", r);
   return r;
}

int OOFFloatsMgr::SortedFloatsVector::findFloatIndex (OOFAwareWidget *lastGB,
                                                      int lastExtIndex)
{
   DBG_OBJ_ENTER_O ("border", 0, oofm, "findFloatIndex", "%p, %d",
                    lastGB, lastExtIndex);

   Float key (oofm, NULL, lastGB, lastExtIndex);
   key.index = -1; // for debugging
   Float::CompareGBAndExtIndex comparator (oofm);
   int i = bsearch (&key, false, &comparator);

   // At position i is the next larger element, so element i should
   // not included, but i - 1 returned; except if the exact element is
   // found: then include it and so return i.
   int r;
   if (i == size())
      r = i - 1;
   else {
      Float *f = get (i);
      if (comparator.compare (f, &key) == 0)
         r = i;
      else
         r = i - 1;
   }

   //printf ("[%p] findFloatIndex (%p, %d) => i = %d, r = %d (size = %d); "
   //        "in %s list %p on the %s side\n",
   //        oofm->container, lastGB, lastExtIndex, i, r, size (),
   //        type == GB ? "GB" : "CB", this, side == LEFT ? "left" : "right");

   //for (int i = 0; i < size (); i++) {
   //   Float *f = get(i);
   //   TBInfo *t = oofm->getOOFAwareWidget(f->generatingBlock);
   //   printf ("   %d: (%p [%d, %p], %d)\n", i, f->generatingBlock,
   //           t->index, t->parent ? t->parent->textblock : NULL,
   //           get(i)->externalIndex);
   //}

   DBG_OBJ_LEAVE_VAL_O (oofm, "%d", r);
   return r;
}

/**
 * `y` is given relative to the container.
 */
int OOFFloatsMgr::SortedFloatsVector::find (int y, int start, int end)
{
   DBG_OBJ_ENTER_O ("border", 0, oofm, "find", "%d, %d, %d", y, start, end);

   Float key (oofm, NULL, NULL, 0);
   key.yReal = y;
   Float::ComparePosition comparator;
   int result = bsearch (&key, false, start, end, &comparator);

   DBG_OBJ_LEAVE_VAL_O (oofm, "%d", result);
   return result;
}

int OOFFloatsMgr::SortedFloatsVector::findFirst (int y, int h,
                                                 OOFAwareWidget *lastGB,
                                                 int lastExtIndex,
                                                 int *lastReturn)
{
   DBG_OBJ_ENTER_O ("border", 0, oofm, "findFirst", "%d, %d, %p, %d",
                    y, h, lastGB, lastExtIndex);

   int last = findFloatIndex (lastGB, lastExtIndex);
   DBG_OBJ_MSGF_O ("border", 1, oofm, "last = %d", last);
   assert (last < size());

   // If the caller wants to reuse this value:
   if (lastReturn)
      *lastReturn = last;

   int i = find (y, 0, last), result;
   DBG_OBJ_MSGF_O ("border", 1, oofm, "i = %d", i);

   // Note: The smallest value of "i" is 0, which means that "y" is before or
   // equal to the first float. The largest value is "last + 1", which means
   // that "y" is after the last float. In both cases, the first or last,
   // respectively, float is a candidate. Generally, both floats, before and
   // at the search position, are candidates.

   if (i > 0 && get(i - 1)->covers (y, h))
      result = i - 1;
   else if (i <= last && get(i)->covers (y, h))
      result = i;
   else
      result = -1;

   DBG_OBJ_LEAVE_VAL_O (oofm, "%d", result);
   return result;
}

int OOFFloatsMgr::SortedFloatsVector::findLastBeforeSideSpanningIndex
   (int sideSpanningIndex)
{
   OOFFloatsMgr::Float::CompareSideSpanningIndex comparator;
   Float key (NULL, NULL, NULL, 0);
   key.sideSpanningIndex = sideSpanningIndex;
   return bsearch (&key, false, &comparator) - 1;
}

void OOFFloatsMgr::SortedFloatsVector::put (Float *vloat)
{
   lout::container::typed::Vector<Float>::put (vloat);
   vloat->index = size() - 1;
}

int OOFFloatsMgr::TBInfo::ComparePosition::compare (Object *o1, Object *o2)
{
   TBInfo *tbInfo1 = (TBInfo*)o1, *tbInfo2 = (TBInfo*)o2;
   int y1 = tbInfo1->getOOFAwareWidget () == NULL ? tbInfo1->y :
      tbInfo1->getOOFAwareWidget()->getGeneratorY (oofmIndex);
   int y2 = tbInfo2->getOOFAwareWidget () == NULL ? tbInfo2->y :
      tbInfo2->getOOFAwareWidget()->getGeneratorY (oofmIndex);
   return y1 - y2;
}

OOFFloatsMgr::TBInfo::TBInfo (OOFFloatsMgr *oofm, OOFAwareWidget *textblock,
                              TBInfo *parent, int parentExtIndex) :
   WidgetInfo (oofm, textblock)
{
   this->parent = parent;
   this->parentExtIndex = parentExtIndex;

   leftFloats = new Vector<Float> (1, false);
   rightFloats = new Vector<Float> (1, false);
}

OOFFloatsMgr::TBInfo::~TBInfo ()
{
   delete leftFloats;
   delete rightFloats;
}

OOFFloatsMgr::OOFFloatsMgr (OOFAwareWidget *container, int oofmIndex)
{
   DBG_OBJ_CREATE ("dw::oof::OOFFloatsMgr");

   this->container = container;
   this->oofmIndex = oofmIndex;

   leftFloats = new SortedFloatsVector (this, LEFT, true);
   rightFloats = new SortedFloatsVector (this, RIGHT, true);

   DBG_OBJ_SET_NUM ("leftFloats.size", leftFloats->size());
   DBG_OBJ_SET_NUM ("rightFloats.size", rightFloats->size());

   floatsByWidget = new HashTable <TypedPointer <Widget>, Float> (true, false);

   tbInfos = new Vector<TBInfo> (1, false);
   tbInfosByOOFAwareWidget =
      new HashTable <TypedPointer <OOFAwareWidget>, TBInfo> (true, true);

   leftFloatsMark = rightFloatsMark = 0;
   lastLeftTBIndex = lastRightTBIndex = 0;

   SizeChanged = true;
   DBG_OBJ_SET_BOOL ("SizeChanged", SizeChanged);

   containerAllocation = *(container->getAllocation());

   addWidgetInFlow (this->container, NULL, 0);
}

OOFFloatsMgr::~OOFFloatsMgr ()
{
   // Order is important: tbInfosByOOFAwareWidget is owner of the instances
   // of TBInfo.tbInfosByOOFAwareWidget. Also, leftFloats and rightFloats are
   // owners of the floats.
   delete tbInfos;
   delete tbInfosByOOFAwareWidget;

   delete floatsByWidget;

   delete leftFloats;
   delete rightFloats;
   
   DBG_OBJ_DELETE ();
}

void OOFFloatsMgr::sizeAllocateStart (OOFAwareWidget *caller,
                                      Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateStart",
                  "%p, (%d, %d, %d * (%d + %d))",
                  caller, allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   if (caller == container)
      containerAllocation = *allocation;
   
   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::sizeAllocateEnd (OOFAwareWidget *caller)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateEnd", "%p", caller);

   if (caller == container) {
      sizeAllocateFloats (LEFT);
      sizeAllocateFloats (RIGHT);
   }

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int i = 0; i < leftFloats->size (); i++)
      leftFloats->get(i)->getWidget()->containerSizeChanged ();
   for (int i = 0; i < rightFloats->size (); i++)
      rightFloats->get(i)->getWidget()->containerSizeChanged ();

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::sizeAllocateFloats (Side side)
{
   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;

   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateFloats", "%s",
                  side == LEFT ? "LEFT" : "RIGHT");

   for (int i = 0; i < list->size (); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      childAllocation.x = containerAllocation.x + calcFloatX (vloat);
      childAllocation.y = containerAllocation.y + vloat->yReal;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;

      vloat->getWidget()->sizeAllocate (&childAllocation);
   }

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Return position of a float relative to the container.
 */
int OOFFloatsMgr::calcFloatX (Float *vloat)
{
   DBG_OBJ_ENTER ("resize.common", 0, "calcFloatX", "%p", vloat->getWidget ());
   int x, effGeneratorWidth;
   OOFAwareWidget *generator = vloat->generator;

   ensureFloatSize (vloat);

   switch (vloat->getWidget()->getStyle()->vloat) {
   case FLOAT_LEFT:
      // Left floats are always aligned on the left side of the generator
      // (content, not allocation) ...
      x = generator->getGeneratorX (oofmIndex)
         + generator->marginBoxOffsetX();

      // ... but when the float exceeds the line break width of the container,
      // it is corrected (but not left of the container).  This way, we save
      // space and, especially within tables, avoid some problems.
      if (x + vloat->size.width > container->getMaxGeneratorWidth ())
         x = max (0, container->getMaxGeneratorWidth () - vloat->size.width);
      break;

   case FLOAT_RIGHT:
      // Similar for right floats, but in this case, floats are shifted to the
      // right when they are too big (instead of shifting the generator to the
      // right).

      // (The following code for calculating effGeneratorWidth, is quite
      // specific for textblocks; this also applies for the comments. Both may
      // be generalized, but actually, only textblocks play a role here.)
   
      if (vloat->generator->usesMaxGeneratorWidth ())
         // For most textblocks, the line break width is used for calculating
         // the x position. (This changed for GROWS, where the width of a
         // textblock is often smaller that the line break.)
         effGeneratorWidth = vloat->generator->getMaxGeneratorWidth ();
      else
         // For some textblocks, like inline blocks, the line break width would
         // be too large for right floats in some cases.
         //
         //  (i) Consider a small inline block with only a few words in one
         //      line, narrower that line break width minus float width. In this
         //      case, the sum should be used.
         //
         // (ii) If there is more than one line, the line break will already be
         //      exceeded, and so be smaller that GB width + float width.
         effGeneratorWidth =
            min (vloat->generator->getGeneratorWidth () + vloat->size.width,
                 vloat->generator->getMaxGeneratorWidth ());

      x = max (generator->getGeneratorX (oofmIndex) + effGeneratorWidth
               - vloat->size.width - generator->marginBoxRestWidth(),
               // Do not exceed container allocation:
               0);
      break;

   default:
      assertNotReached ();
      x = 0;
      break;
   }

   DBG_OBJ_LEAVE_VAL ("%d", x);
   return x;
}

void OOFFloatsMgr::draw (View *view, Rectangle *area, DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "draw", "%d, %d, %d * %d",
                  area->x, area->y, area->width, area->height);

   drawFloats (leftFloats, view, area, context);
   drawFloats (rightFloats, view, area, context);

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::drawFloats (SortedFloatsVector *list, View *view,
                               Rectangle *area, DrawingContext *context)
{
   // This could be improved, since the list is sorted: search the
   // first float fitting into the area, and iterate until one is
   // found below the area.

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      Widget *childWidget = vloat->getWidget ();
     
      Rectangle childArea;
      if (!context->hasWidgetBeenProcessedAsInterruption (childWidget) &&
         !StackingContextMgr::handledByStackingContextMgr (childWidget) &&
          childWidget->intersects (container, area, &childArea))
         childWidget->draw (view, &childArea, context);
   }
}

void OOFFloatsMgr::addWidgetInFlow (OOFAwareWidget *textblock,
                                    OOFAwareWidget *parentBlock,
                                    int externalIndex)
{
   //printf ("[%p] addWidgetInFlow (%p, %p, %d)\n",
   //        container, textblock, parentBlock, externalIndex);

   TBInfo *tbInfo =
      new TBInfo (this, textblock,
                  parentBlock ? getOOFAwareWidget (parentBlock) : NULL,
                  externalIndex);
   tbInfo->index = tbInfos->size();

   tbInfos->put (tbInfo);
   tbInfosByOOFAwareWidget->put (new TypedPointer<OOFAwareWidget> (textblock),
                                 tbInfo);
}

int OOFFloatsMgr::addWidgetOOF (Widget *widget, OOFAwareWidget *generatingBlock,
                                 int externalIndex)
{
   DBG_OBJ_ENTER ("construct.oofm", 0, "addWidgetOOF", "%p, %p, %d",
                  widget, generatingBlock, externalIndex);

   int subRef = 0;

   TBInfo *tbInfo = getOOFAwareWidget (generatingBlock);
   Float *vloat = new Float (this, widget, generatingBlock, externalIndex);

   // Note: Putting the float first in the GB list, and then, possibly
   // into the CB list (in that order) will trigger setting
   // Float::inCBList to the right value.

   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      leftFloats->put (vloat);
      DBG_OBJ_SET_NUM ("leftFloats.size", leftFloats->size());
      DBG_OBJ_ARRATTRSET_PTR ("leftFloats", leftFloats->size() - 1,
                              "widget", vloat->getWidget ());
      tbInfo->leftFloats->put (vloat);
      
      subRef = createSubRefLeftFloat (leftFloats->size() - 1);

      break;
      
   case FLOAT_RIGHT:
      rightFloats->put (vloat);
      DBG_OBJ_SET_NUM ("rightFloats.size", rightFloats->size());
      DBG_OBJ_ARRATTRSET_PTR ("rightFloats", rightFloats->size() - 1,
                              "widget", vloat->getWidget ());
      tbInfo->rightFloats->put (vloat);
      
      subRef = createSubRefRightFloat (rightFloats->size() - 1);
      break;

   default:
      assertNotReached();
   }

   // "sideSpanningIndex" is only compared, so this simple assignment
   // is sufficient; differentiation between GB and CB lists is not
   // necessary.
   vloat->sideSpanningIndex =
      leftFloats->size() + rightFloats->size() - 1;
      
   floatsByWidget->put (new TypedPointer<Widget> (widget), vloat);

   DBG_OBJ_LEAVE_VAL ("%d", subRef);
   return subRef;
}

void OOFFloatsMgr::calcWidgetRefSize (Widget *widget, Requisition *size)
{
   size->width = size->ascent = size->descent = 0;
}

void OOFFloatsMgr::moveExternalIndices (OOFAwareWidget *generatingBlock,
                                        int oldStartIndex, int diff)
{
   TBInfo *tbInfo = getOOFAwareWidget (generatingBlock);
   moveExternalIndices (tbInfo->leftFloats, oldStartIndex, diff);
   moveExternalIndices (tbInfo->rightFloats, oldStartIndex, diff);
}

void OOFFloatsMgr::moveExternalIndices (Vector<Float> *list, int oldStartIndex,
                                        int diff)
{
   // Could be faster with binary search, but the number of floats per generator
   // should be rather small.
   for (int i = 0; i < list->size (); i++) {
      Float *vloat = list->get (i);
      if (vloat->externalIndex >= oldStartIndex) {
         vloat->externalIndex += diff;
         DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.externalIndex",
                            vloat->externalIndex);
      }
   }
}

OOFFloatsMgr::Float *OOFFloatsMgr::findFloatByWidget (Widget *widget)
{
   TypedPointer <Widget> key (widget);
   Float *vloat = floatsByWidget->get (&key);
   assert (vloat != NULL);
   return vloat;
}

/*
 * Currently this is a compound recursion for textblocks:
 * markSizeChange -> updateReference -> queueResize -> markSizeChange
 * One way to see it is as a widget tree coverage problem. i.e. to cover all
 * the nodes that need a resize when a float changes its size.
 * The coverage logic of it is shared between resize code and mark code (here).
 * 
 * This implementation works for all the test cases so far. It relies on the
 * fact that Widget::queueResize should be called whenever a widget changes its
 * size. When "SizeChanged" is true, we notify the parent, when not, just the
 * following textblocks.
 */
void OOFFloatsMgr::markSizeChange (int ref)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "markSizeChange", "%d", ref);

   // When "SizeChanged" is true, we know this float changed its size.
   // This helps to prune redundant passes.
   // "SizeChanged" is set by getSize(), which is called by sizeRequest().

   SortedFloatsVector *list;
   list = isSubRefLeftFloat(ref) ? leftFloats : rightFloats;
   Float *vloat = list->get (getFloatIndexFromSubRef (ref));

   vloat->dirty = true;
   DBG_OBJ_SET_BOOL_O (vloat->getWidget (), "<Float>.dirty", vloat->dirty);

   updateGenerators (vloat);

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Update all generators which are affected by a given float.
 */
void OOFFloatsMgr::updateGenerators (Float *vloat)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "updateGenerators", "#%d [%p]",
                  vloat->index, vloat->getWidget ());

   assert (vloat->getWidget()->getWidgetReference() != NULL);

   int first = getOOFAwareWidget(vloat->generator)->index;
   DBG_OBJ_MSGF ("resize.oofm", 1, "updating from %d", first);

   //printf("IN markSizeChange %p ref %d SzCh=%d\n", this, ref,
   //       (int)SizeChanged);

   if (SizeChanged)
      tbInfos->get(first)->getOOFAwareWidget()
         ->updateReference (vloat->getWidget()->getWidgetReference()
                            ->parentRef);

   for (int i = first + 1; i < tbInfos->size(); i++)
      tbInfos->get(i)->getOOFAwareWidget()->updateReference(0);

   SizeChanged = false; // Done.

   DBG_OBJ_LEAVE ();
}

/**
 * `y` is given relative to the container.
 */
int OOFFloatsMgr::findTBInfo (int y)
{
   DBG_OBJ_ENTER ("findTBInfo", 0, "findTBInfo", "%d", y);

   TBInfo key (this, NULL, NULL, 0);
   key.y = y;
   TBInfo::ComparePosition comparator (oofmIndex);
   int index = tbInfos->bsearch (&key, false, &comparator);

   // "bsearch" returns next greater, but we are interested in the last which
   // is less or equal.
   int result = index > 0 ? index - 1 : index;

   DBG_OBJ_LEAVE_VAL ("%d", result);
   return result;
}


void OOFFloatsMgr::markExtremesChange (int ref)
{
   // Nothing to do here.
}

Widget *OOFFloatsMgr::getWidgetAtPoint (int x, int y,
                                        GettingWidgetAtPointContext *context)
{
   Widget *widgetAtPoint = NULL;

   widgetAtPoint = getFloatWidgetAtPoint (rightFloats, x, y, context);
   if (widgetAtPoint == NULL)
      widgetAtPoint = getFloatWidgetAtPoint (leftFloats, x, y, context);

   return widgetAtPoint;
}

Widget *OOFFloatsMgr::getFloatWidgetAtPoint (SortedFloatsVector *list, int x,
                                             int y,
                                             GettingWidgetAtPointContext
                                             *context)
{
   // Could use binary search to be faster (similar to drawing).
   Widget *widgetAtPoint = NULL;
   
   for (int i = list->size() - 1; widgetAtPoint == NULL && i >= 0; i--) {
      Widget *childWidget = list->get(i)->getWidget ();
      if (!context->hasWidgetBeenProcessedAsInterruption (childWidget) &&
          !StackingContextMgr::handledByStackingContextMgr (childWidget))
         widgetAtPoint = childWidget->getWidgetAtPoint (x, y, context);
   }
   
   return widgetAtPoint;
}

void OOFFloatsMgr::tellPosition1 (Widget *widget, int x, int y)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "tellPosition1", "%p, %d, %d",
                  widget, x, y);

   assert (y >= 0);

   Float *vloat = findFloatByWidget(widget);

   SortedFloatsVector *listSame, *listOpp;
   Side side;
   getFloatsListsAndSide (vloat, &listSame, &listOpp, &side);
   ensureFloatSize (vloat);

   int oldYReal = vloat->yReal;

   // "yReal" may change due to collisions (see below).
   vloat->yReq = vloat->yReal = y;

   DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReq", vloat->yReq);
   DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);

   // Test collisions (on this side). Although there are (rare) cases
   // where it could make sense, the horizontal dimensions are not
   // tested; especially since searching and border calculation would
   // be confused. For this reaspn, only the previous float is
   // relevant. (Cf. below, collisions on the other side.)
   int yRealNew;
   if (vloat->index >= 1 &&
       collidesV (vloat, listSame->get (vloat->index - 1), &yRealNew)) {
      vloat->yReal = yRealNew;
      DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);
   }

   // Test collisions (on the opposite side). There are cases when
   // more than one float has to be tested. Consider the following
   // HTML snippet ("id" attribute only used for simple reference
   // below, as #f1, #f2, and #f3):
   //
   //    <div style="float:left" id="f1">
   //       Left left left left left left left left left left.
   //    </div>
   //    <div style="float:left" id="f2">Also left.</div>
   //    <div style="float:right" id="f3">Right.</div>
   //
   // When displayed with a suitable window width (only slightly wider
   // than the text within #f1), this should look like this:
   //
   //    ---------------------------------------------------------
   //    | Left left left left left left left left left left.    |
   //    | Also left.                                     Right. |
   //    ---------------------------------------------------------
   //
   // Consider float #f3: a collision test with #f2, considering
   // vertical dimensions, is positive, but not the test with
   // horizontal dimensions (because #f2 and #f3 are too
   // narrow). However, a collision has to be tested with #f1;
   // otherwise #f3 and #f1 would overlap.

   int oppFloatIndex =
      listOpp->findLastBeforeSideSpanningIndex (vloat->sideSpanningIndex);
   // Generally, the rules are simple: loop as long as the vertical
   // dimensions test is positive (and, of course, there are floats),
   // ...
   for (bool foundColl = false;
        !foundColl && oppFloatIndex >= 0 &&
           collidesV (vloat, listOpp->get (oppFloatIndex), &yRealNew);
        oppFloatIndex--) {
      // ... but stop the loop as soon as the horizontal dimensions
      // test is positive.
      if (collidesH (vloat, listOpp->get (oppFloatIndex))) {
         vloat->yReal = yRealNew;
         DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);
         foundColl = true;
      }
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "vloat->yReq = %d, vloat->yReal = %d",
                 vloat->yReq, vloat->yReal);

   // In some cases, an explicit update is necessary, as in this example:
   //
   // <body>
   //     <div id="a">
   //         <div id="b" style="float:left">main</div>
   //     </div>
   //     <div id="c" style="clear:both">x</div>
   //     <div id="d">footer</div>
   // </body>
   //
   // Without an explicit update, #c would keep an old value for extraSpace.top,
   // based on the old value of vloat->yReal.
   //
   // Notice that #c would be updated otherwise, if it had at least one word
   // content.

   if (vloat->yReal != oldYReal)
      updateGenerators (vloat);

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::tellPosition2 (Widget *widget, int x, int y)
{
   // Nothing to do.
}

void OOFFloatsMgr::tellIncompletePosition1 (Widget *generator, Widget *widget,
                                            int x, int y)
{
   notImplemented ("OOFFloatsMgr::tellIncompletePosition1");
}

void OOFFloatsMgr::tellIncompletePosition2 (Widget *generator, Widget *widget,
                                            int x, int y)
{
   notImplemented ("OOFFloatsMgr::tellIncompletePosition2");
}

bool OOFFloatsMgr::collidesV (Float *vloat, Float *other, int *yReal)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "collidesV", "#%d [%p], #%d [%p], ...",
                  vloat->index, vloat->getWidget (), other->index,
                  other->getWidget ());

   bool result;

   DBG_OBJ_MSGF ("resize.oofm", 1, "initial yReal = %d", vloat->yReal);

   ensureFloatSize (other);
   int otherBottomGB = other->yReal + other->size.ascent + other->size.descent;

   DBG_OBJ_MSGF ("resize.oofm", 1, "otherBottomGB = %d + (%d + %d) = %d",
                 other->yReal, other->size.ascent, other->size.descent,
                 otherBottomGB);

   if (vloat->yReal <  otherBottomGB) {
      *yReal = otherBottomGB;
      result = true;
   } else
      result = false;

   if (result)
      DBG_OBJ_LEAVE_VAL ("%s, %d", "true", *yReal);
   else
      DBG_OBJ_LEAVE_VAL ("%s", "false");

   return result;
}


bool OOFFloatsMgr::collidesH (Float *vloat, Float *other)
{
   // Only checks horizontal collision. For a complete test, use collidesV (...)
   // && collidesH (...).
   bool collidesH;

   int vloatX = calcFloatX (vloat), otherX = calcFloatX (other);

   // Generally: right border of the left float > left border of the right float
   // (all in canvas coordinates).
   if (vloat->getWidget()->getStyle()->vloat == FLOAT_LEFT) {
      // "vloat" is left, "other" is right
      ensureFloatSize (vloat);
      collidesH = vloatX + vloat->size.width > otherX;
   } else {
      // "other" is left, "vloat" is right
      ensureFloatSize (other);
      collidesH = otherX + other->size.width > vloatX;
   }

   return collidesH;
}

void OOFFloatsMgr::getFloatsListsAndSide (Float *vloat,
                                          SortedFloatsVector **listSame,
                                          SortedFloatsVector **listOpp,
                                          Side *side)
{
   switch (vloat->getWidget()->getStyle()->vloat) {
   case FLOAT_LEFT:
      if (listSame) *listSame = leftFloats;
      if (listOpp) *listOpp = rightFloats;
      if (side) *side = LEFT;
      break;

   case FLOAT_RIGHT:
      if (listSame) *listSame = rightFloats;
      if (listOpp) *listOpp = leftFloats;
      if (side) *side = RIGHT;
      break;

   default:
      assertNotReached();
   }
}

void OOFFloatsMgr::getSize (Requisition *cbReq, int *oofWidth, int *oofHeight)
{
   DBG_OBJ_ENTER0 ("resize.oofm", 0, "getSize");

   int oofWidthtLeft, oofWidthRight, oofHeightLeft, oofHeightRight;
   getFloatsSize (cbReq, LEFT, &oofWidthtLeft, &oofHeightLeft);
   getFloatsSize (cbReq, RIGHT, &oofWidthRight, &oofHeightRight);

   // Floats must be within the *content* area of the containing
   // block, not its *margin* area (which is equivalent to the
   // requisition / allocation). For this reason, boxRestWidth() and
   // boxRestHeight() are added here.

   *oofWidth =
      max (oofWidthtLeft, oofWidthRight) + container->boxRestWidth ();
   *oofHeight =
      max (oofHeightLeft, oofHeightRight) + container->boxRestHeight ();

   SizeChanged = true;

   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> (l: %d, r: %d => %d) * (l: %d, r: %d => %d)",
                 oofWidthtLeft, oofWidthRight, *oofWidth, oofHeightLeft,
                 oofHeightRight, *oofHeight);
   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::getFloatsSize (Requisition *cbReq, Side side, int *width,
                                  int *height)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getFloatsSize", "(%d * (%d + %d), %s, ...",
                  cbReq->width, cbReq->ascent, cbReq->descent,
                  side == LEFT ? "LEFT" : "RIGHT");

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;

   *width = *height = 0;

   DBG_OBJ_MSGF ("resize.oofm", 1, "%d floats on this side", list->size());

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "float %p has generator %p (container is %p)",
                    vloat->getWidget (), vloat->generator, container);
                    
      ensureFloatSize (vloat);
      
      *width = max (*width, calcFloatX (vloat) + vloat->size.width);
      *height = max (*height,
                     vloat->yReal + vloat->size.ascent + vloat->size.descent);
   }
   
   DBG_OBJ_LEAVE ();
}

bool OOFFloatsMgr::containerMustAdjustExtraSpace ()
{
   return false;
}

void OOFFloatsMgr::getExtremes (Extremes *cbExtr, int *oofMinWidth,
                                int *oofMaxWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getExtremes", "(%d / %d), ...",
                  cbExtr->minWidth, cbExtr->maxWidth);

   int oofMinWidthtLeft, oofMinWidthRight, oofMaxWidthLeft, oofMaxWidthRight;
   getFloatsExtremes (cbExtr, LEFT, &oofMinWidthtLeft, &oofMaxWidthLeft);
   getFloatsExtremes (cbExtr, RIGHT, &oofMinWidthRight, &oofMaxWidthRight);

   *oofMinWidth = max (oofMinWidthtLeft, oofMinWidthRight);
   *oofMaxWidth = max (oofMaxWidthLeft, oofMaxWidthRight);

   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> (l: %d, r: %d => %d) / (l: %d, r: %d => %d)",
                 oofMinWidthtLeft, oofMinWidthRight, *oofMinWidth,
                 oofMaxWidthLeft, oofMaxWidthRight, *oofMaxWidth);
   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::getFloatsExtremes (Extremes *cbExtr, Side side,
                                      int *minWidth, int *maxWidth)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getFloatsExtremes", "(%d / %d), %s, ...",
                  cbExtr->minWidth, cbExtr->maxWidth,
                  side == LEFT ? "LEFT" : "RIGHT");

   *minWidth = *maxWidth = 0;

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   DBG_OBJ_MSGF ("resize.oofm", 1, "%d floats to be examined", list->size());

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "float %p has generator %p (container is %p)",
                    vloat->getWidget (), vloat->generator,
                    container);
      
      Extremes extr;
      vloat->getWidget()->getExtremes (&extr);
      
      // The calculation of extremes must be kept consistent with
      // getFloatsSize(). Especially this means for the *minimal* width:
      //
      // - The right border (difference between float and container) does not
      //   have to be considered (see getFloatsSize()).
      //
      // - This is also the case for the left border, as seen in calcFloatX()
      //   ("... but when the float exceeds the line break width" ...).
      
      *minWidth = max (*minWidth, extr.minWidth);
      
      // For the maximal width, borders must be considered.
      *maxWidth = max (*maxWidth,
                       extr.maxWidth
                       + vloat->generator->marginBoxDiffWidth(),
                       + max (container->getGeneratorWidth ()
                              - vloat->generator->getGeneratorWidth (),
                              0));
      
      DBG_OBJ_MSGF ("resize.oofm", 1, "%d / %d => %d / %d",
                    extr.minWidth, extr.maxWidth, *minWidth, *maxWidth);
   }

   DBG_OBJ_LEAVE ();
}

OOFFloatsMgr::TBInfo *OOFFloatsMgr::getOOFAwareWidgetWhenRegistered
   (OOFAwareWidget *widget)
{
   DBG_OBJ_ENTER ("oofm.common", 0, "getOOFAwareWidgetWhenRegistered", "%p",
                  widget);
   TypedPointer<OOFAwareWidget> key (widget);
   TBInfo *tbInfo = tbInfosByOOFAwareWidget->get (&key);
   DBG_OBJ_MSGF ("oofm.common", 1, "found? %s", tbInfo ? "yes" : "no");
   DBG_OBJ_LEAVE ();
   return tbInfo;
}

OOFFloatsMgr::TBInfo *OOFFloatsMgr::getOOFAwareWidget (OOFAwareWidget *widget)
{
   DBG_OBJ_ENTER ("oofm.common", 0, "getOOFAwareWidget", "%p", widget);
   TBInfo *tbInfo = getOOFAwareWidgetWhenRegistered (widget);
   assert (tbInfo);
   DBG_OBJ_LEAVE ();
   return tbInfo;
}

int OOFFloatsMgr::getLeftBorder (int y, int h, OOFAwareWidget *lastGB,
                                 int lastExtIndex)
{
   int b = getBorder (LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "left border (%d, %d, %p, %d) => %d",
                 y, h, lastGB, lastExtIndex, b);
   return b;
}

int OOFFloatsMgr::getRightBorder (int y, int h, OOFAwareWidget *lastGB,
                                  int lastExtIndex)
{
   int b = getBorder (RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "right border (%d, %d, %p, %d) => %d",
                 y, h, lastGB, lastExtIndex, b);
   return b;
}

int OOFFloatsMgr::getBorder (Side side, int y, int h, OOFAwareWidget *lastGB,
                             int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "getBorder", "%s, %d, %d, %p, %d",
                  side == LEFT ? "LEFT" : "RIGHT", y, h, lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int last;   
   int first = list->findFirst (y, h, lastGB, lastExtIndex, &last);
   int border = 0;

   DBG_OBJ_MSGF ("border", 1, "first = %d", first);

   if (first != -1) {
      // It is not sufficient to find the first float, since a line
      // (with height h) may cover the region of multiple float, of
      // which the widest has to be chosen.
      bool covers = true;

      // We are not searching until the end of the list, but until the
      // float defined by lastGB and lastExtIndex.
      for (int i = first; covers && i <= last; i++) {
         Float *vloat = list->get(i);
         covers = vloat->covers (y, h);
         DBG_OBJ_MSGF ("border", 1, "float %d (%p) covers? %s.",
                       i, vloat->getWidget(), covers ? "<b>yes</b>" : "no");

         if (covers) {
            int d;
            switch (side) {
            case LEFT:
               d = vloat->generator->getGeneratorX (oofmIndex)
                  + vloat->generator->marginBoxOffsetX ();
               break;

            case RIGHT:
               // There is no simple possibility to get the difference between
               // container and generator at the right border (as it is at the
               // left border, see above). We have to calculate the difference
               // between the maximal widths.
               d = container->getMaxGeneratorWidth ()
                  - (vloat->generator->getGeneratorX (oofmIndex)
                     + vloat->generator->getMaxGeneratorWidth ())
                  + vloat->generator->marginBoxRestWidth ();
               break;

            default:
               assertNotReached ();
               d = 0;
               break;
            }

            int thisBorder = vloat->size.width + d;
            DBG_OBJ_MSGF ("border", 1, "thisBorder = %d + %d = %d",
                          vloat->size.width, d, thisBorder);
            
            border = max (border, thisBorder);
         }
      }
   }

   DBG_OBJ_LEAVE_VAL ("%d", border);
   return border;
}

bool OOFFloatsMgr::hasFloatLeft (int y, int h, OOFAwareWidget *lastGB,
                                 int lastExtIndex)
{
   bool b = hasFloat (LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float left (%d, %d, %p, %d) => %s",
                 y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OOFFloatsMgr::hasFloatRight (int y, int h, OOFAwareWidget *lastGB,
                                  int lastExtIndex)
{
   bool b = hasFloat (RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float right (%d, %d, %p, %d) => %s",
                 y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OOFFloatsMgr::hasFloat (Side side, int y, int h, OOFAwareWidget *lastGB,
                             int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "hasFloat", "%s, %d, %d, %p, %d",
                  side == LEFT ? "LEFT" : "RIGHT", y, h, lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int first = list->findFirst (y, h, lastGB, lastExtIndex, NULL);

   DBG_OBJ_MSGF ("border", 1, "first = %d", first);
   DBG_OBJ_LEAVE ();
   return first != -1;
}

int OOFFloatsMgr::getLeftFloatHeight (int y, int h, OOFAwareWidget *lastGB,
                                      int lastExtIndex)
{
   return getFloatHeight (LEFT, y, h, lastGB, lastExtIndex);
}

int OOFFloatsMgr::getRightFloatHeight (int y, int h, OOFAwareWidget *lastGB,
                                       int lastExtIndex)
{
   return getFloatHeight (RIGHT, y, h, lastGB, lastExtIndex);
}

int OOFFloatsMgr::getFloatHeight (Side side, int y, int h,
                                  OOFAwareWidget *lastGB, int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "getFloatHeight", "%s, %d, %d, %p, %d",
                  side == LEFT ? "LEFT" : "RIGHT", y, h, lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int first = list->findFirst (y, h, lastGB, lastExtIndex, NULL);
   assert (first != -1);   /* This method must not be called when there is no
                              float on the respective side. */

   Float *vloat = list->get (first);
   int yRelToFloat = y - vloat->yReal;
   DBG_OBJ_MSGF ("border", 1, "caller is CB: yRelToFloat = %d - %d = %d",
                 y, vloat->yReal, yRelToFloat);

   ensureFloatSize (vloat);
   int height = vloat->size.ascent + vloat->size.descent - yRelToFloat;

   DBG_OBJ_MSGF ("border", 1, "=> (%d + %d) - %d = %d",
                 vloat->size.ascent, vloat->size.descent, yRelToFloat, height);
   DBG_OBJ_LEAVE ();
   return height;
}

int OOFFloatsMgr::getClearPosition (OOFAwareWidget *widget)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getClearPosition", "%p", widget);

   int pos;

   if (widget->getStyle()) {
      bool left = false, right = false;
      switch (widget->getStyle()->clear) {
      case CLEAR_NONE: break;
      case CLEAR_LEFT: left = true; break;
      case CLEAR_RIGHT: right = true; break;
      case CLEAR_BOTH: left = right = true; break;
      default: assertNotReached ();
      }

      pos = max (left ? getClearPosition (widget, LEFT) : 0,
                 right ? getClearPosition (widget, RIGHT) : 0);
   } else
      pos = 0;

   DBG_OBJ_LEAVE_VAL ("%d", pos);
   return pos;
}

bool OOFFloatsMgr::affectsLeftBorder (core::Widget *widget)
{
   return widget->getStyle()->vloat == core::style::FLOAT_LEFT;
}

bool OOFFloatsMgr::affectsRightBorder (core::Widget *widget)
{
   return widget->getStyle()->vloat == core::style::FLOAT_RIGHT;
}

bool OOFFloatsMgr::mayAffectBordersAtAll ()
{
   return true;
}

int OOFFloatsMgr::getClearPosition (OOFAwareWidget *widget, Side side)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getClearPosition", "%p, %s",
                  widget, side == LEFT ? "LEFT" : "RIGHT");

   int pos;
   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;

   // Search the last float before (therefore -1) this widget.
   int i = list->findFloatIndex (widget, -1);
   if (i < 0)
      pos = 0;
   else {
      Float *vloat = list->get(i);
      assert (vloat->generator != widget);
      ensureFloatSize (vloat);
      int yRel = widget->getGeneratorY (oofmIndex);
      pos = max (vloat->yReal + vloat->size.ascent + vloat->size.descent - yRel,
                 0);
      DBG_OBJ_MSGF ("resize.oofm", 1, "pos = max (%d + %d + %d - %d, 0)",
                    vloat->yReal, vloat->size.ascent, vloat->size.descent,
                    yRel);
   }
   
   DBG_OBJ_LEAVE_VAL ("%d", pos);

   return pos;
}

void OOFFloatsMgr::ensureFloatSize (Float *vloat)
{
   // Historical note: relative sizes (e. g. percentages) are already
   // handled by (at this time) Layout::containerSizeChanged, so
   // Float::dirty will be set.

   DBG_OBJ_ENTER ("resize.oofm", 0, "ensureFloatSize", "%p",
                  vloat->getWidget ());

   if (vloat->dirty)  {
      DBG_OBJ_MSG ("resize.oofm", 1, "dirty: recalculation");

      vloat->getWidget()->sizeRequest (&vloat->size);
      vloat->dirty = false;
      DBG_OBJ_SET_BOOL_O (vloat->getWidget (), "<Float>.dirty", vloat->dirty);

      DBG_OBJ_MSGF ("resize.oofm", 1, "size: %d * (%d + %d)",
                    vloat->size.width, vloat->size.ascent, vloat->size.descent);

      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.width",
                         vloat->size.width);
      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.ascent",
                         vloat->size.ascent);
      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.descent",
                         vloat->size.descent);

      // "sizeChangedSinceLastAllocation" is reset in sizeAllocateEnd()
   }

   DBG_OBJ_LEAVE ();
}

bool OOFFloatsMgr::dealingWithSizeOfChild (core::Widget *child)
{
   return false;
}

int OOFFloatsMgr::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   notImplemented ("OOFFloatsMgr::getAvailWidthOfChild");
   return 0;
}

int OOFFloatsMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   notImplemented ("OOFFloatsMgr::getAvailHeightOfChild");
   return 0;
}

int OOFFloatsMgr::getNumWidgets ()
{
   return leftFloats->size() + rightFloats->size();
}

Widget *OOFFloatsMgr::getWidget (int i)
{
   if (i < leftFloats->size())
      return leftFloats->get(i)->getWidget ();
   else
      return rightFloats->get(i - leftFloats->size())->getWidget ();
}

} // namespace oof

} // namespace dw
