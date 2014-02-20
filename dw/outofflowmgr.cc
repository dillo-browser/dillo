/*
 * Dillo Widget
 *
 * Copyright 2013 Sebastian Geerken <sgeerken@dillo.org>
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


#include "outofflowmgr.hh"
#include "textblock.hh"
#include "../lout/debug.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

OutOfFlowMgr::WidgetInfo::WidgetInfo (OutOfFlowMgr *oofm, Widget *widget)
{
   this->oofm = oofm;
   this->widget = widget;
   wasAllocated = false;
   xCB = yCB = width = height = -1;
}

void OutOfFlowMgr::WidgetInfo::update (bool wasAllocated, int xCB, int yCB,
                                       int width, int height)
{
   this->wasAllocated = wasAllocated;
   this->xCB = xCB;
   this->yCB = yCB;
   this->width = width;
   this->height = height;

   DBG_OBJ_SET_NUM_O (widget, "<WidgetInfo>.xCB", xCB);
   DBG_OBJ_SET_NUM_O (widget, "<WidgetInfo>.yCB", yCB);
   DBG_OBJ_SET_NUM_O (widget, "<WidgetInfo>.width", width);
   DBG_OBJ_SET_NUM_O (widget, "<WidgetInfo>.height", height);
}

void OutOfFlowMgr::WidgetInfo::updateAllocation ()
{
   update (isNowAllocated (), getNewXCB (), getNewYCB (), getNewWidth (),
           getNewHeight ());
}

// ----------------------------------------------------------------------

OutOfFlowMgr::Float::Float (OutOfFlowMgr *oofm, Widget *widget,
                            Textblock *generatingBlock, int externalIndex) :
   WidgetInfo (oofm, widget)
{
   this->generatingBlock = generatingBlock;
   this->externalIndex = externalIndex;

   yReq = yReal = size.width = size.ascent = size.descent = 0;
   dirty = sizeChangedSinceLastAllocation = true;
   inCBList = false;
}

void OutOfFlowMgr::Float::intoStringBuffer(StringBuffer *sb)
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
   sb->append (", generatingBlock = ");
   sb->appendPointer (generatingBlock);
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
   sb->appendBool (sizeChangedSinceLastAllocation);
   sb->append (", inCBList = ");
   sb->appendBool (inCBList);
   sb->append (" }");
}

bool OutOfFlowMgr::Float::covers (Textblock *textblock, int y, int h)
{
   DBG_OBJ_MSGF_O ("border", 0, getOutOfFlowMgr (),
                   "<b>covers</b> (%p, %d, %d) [vloat: %p]",
                   textblock, y, h, getWidget ());
   DBG_OBJ_MSG_START_O (getOutOfFlowMgr ());

   bool b;

   if (textblock == generatingBlock) {
      int reqyGB = y;
      int flyGB = yReal;
      getOutOfFlowMgr()->ensureFloatSize (this);
      int flh = size.ascent + size.descent;
      b = flyGB + flh > reqyGB && flyGB < reqyGB + h;

      DBG_OBJ_MSGF_O ("border", 1, getOutOfFlowMgr (),
                      "for generator: reqyGB = %d, flyGB = %d, "
                      "flh = %d + %d = %d => %s",
                      reqyGB, flyGB, size.ascent, size.descent, flh,
                      b ? "true" : "false");
   } else {
      assert (getOutOfFlowMgr()->wasAllocated (generatingBlock));
      assert (getOutOfFlowMgr()->wasAllocated (textblock));

      if (!getWidget()->wasAllocated ()) {
         DBG_OBJ_MSG_O ("border", 1, getOutOfFlowMgr (),
                        "not generator (not allocated) => false");
         b = false;
      } else {
         Allocation *tba = getOutOfFlowMgr()->getAllocation(textblock),
            *gba = getOutOfFlowMgr()->getAllocation(generatingBlock),
            *fla = getWidget()->getAllocation ();
         int reqyCanv = tba->y + y;
         int flyCanv = gba->y + yReal;
         int flh = fla->ascent + fla->descent;
         b = flyCanv + flh > reqyCanv && flyCanv < reqyCanv + h;
         
         DBG_OBJ_MSGF_O ("border", 1, getOutOfFlowMgr (),
                         "not generator (allocated): reqyCanv = %d + %d = %d, "
                         "flyCanv = %d + %d = %d, flh = %d + %d = %d => %s",
                         tba->y, y, reqyCanv, gba->y, yReal, flyCanv,
                         fla->ascent, fla->descent, flh, b ? "true" : "false");
      }
   }

   DBG_OBJ_MSG_END_O (getOutOfFlowMgr ());

   return b;
}

int OutOfFlowMgr::Float::ComparePosition::compare (Object *o1, Object *o2)
{
   Float *fl1 = (Float*)o1, *fl2 = (Float*)o2;
   int r;

   DBG_OBJ_MSGF_O ("border", 1, oofm,
                   "<b>ComparePosition::compare</b> (#%d, #%d) [refTB = %p]",
                   fl1->index, fl2->index, refTB);
   DBG_OBJ_MSG_START_O (oofm);
   
   if (refTB == fl1->generatingBlock && refTB == fl2->generatingBlock) {
      DBG_OBJ_MSG_O ("border", 2, oofm, "refTB is generating both floats");
      r = fl1->yReal - fl2->yReal;
   } else {
      DBG_OBJ_MSG_O ("border", 2, oofm, "refTB is not generating both floats");
      DBG_OBJ_MSG_START_O (oofm);

      assert (oofm->wasAllocated (fl1->generatingBlock));
      assert (oofm->wasAllocated (fl2->generatingBlock));

      DBG_OBJ_MSGF_O ("border", 2, oofm, "generators are %p and %p",
                      fl1->generatingBlock, fl2->generatingBlock);
      
      // (i) Floats may not yet been allocated (although the
      // generators are). Non-allocated floats do not have an effect
      // yet, they are considered "at the end" of the list.

      // (ii) Float::widget for the key used for binary search. In
      // this case, Float::yReal is used instead (which is set in
      // SortedFloatsVector::find).

      bool a1 = fl1->getWidget () ? fl1->getWidget()->wasAllocated () : true;
      bool a2 = fl2->getWidget () ? fl2->getWidget()->wasAllocated () : true;

      DBG_OBJ_MSGF_O ("border", 2, oofm,
                      "float 1 allocated: %s; float 2 allocated: %s",
                      a1 ? "yes" : "no", a2 ? "yes" : "no");
      
      if (a1 && a2) {
         int fly1 = fl1->getWidget() ? fl1->getWidget()->getAllocation()->y :
            oofm->getAllocation(fl2->generatingBlock)->y + fl1->yReal;
         int fly2 = fl2->getWidget() ? fl2->getWidget()->getAllocation()->y :
            oofm->getAllocation(fl2->generatingBlock)->y + fl2->yReal;
         DBG_OBJ_MSGF_O ("border", 2, oofm, "y diff = %d - %d", fly1, fly2);
         r = fly1 - fly2;
      } else if (a1 && !a2)
         r = -1;
      else if (!a1 && a2)
         r = +1;
      else // if (!a1 && !a2)
         return 0;

      DBG_OBJ_MSG_END_O (oofm);
   }

   DBG_OBJ_MSGF_O ("border", 1, oofm, "result: %d", r);
   DBG_OBJ_MSG_END_O (oofm);
   return r;
}

int OutOfFlowMgr::Float::CompareSideSpanningIndex::compare (Object *o1,
                                                            Object *o2)
{
   return ((Float*)o1)->sideSpanningIndex - ((Float*)o2)->sideSpanningIndex;
}

int OutOfFlowMgr::Float::CompareGBAndExtIndex::compare (Object *o1, Object *o2)
{
   Float *f1 = (Float*)o1, *f2 = (Float*)o2;

   //printf ("[%p] comparing (%p, %d) with (%p, %d) ...\n",
   //        oofm->containingBlock, f1->generatingBlock, f1->externalIndex,
   //        f2->generatingBlock, f2->externalIndex);

   if (f1->generatingBlock == f2->generatingBlock) {
      //printf ("   (a) generating blocks equal => %d - %d = %d\n",
      //        f1->externalIndex, f2->externalIndex,
      //        f1->externalIndex - f2->externalIndex);
      return f1->externalIndex - f2->externalIndex;
   } else {
      TBInfo *t1 = oofm->getTextblock (f1->generatingBlock),
         *t2 = oofm->getTextblock (f2->generatingBlock);

      for (TBInfo *t = t1; t != NULL; t = t->parent)
         if (t->parent == t2) {
            //printf ("   (b) %p is an achestor of %p; direct child is %p (%d)"
            //        " => %d - %d = %d\n", t2->textblock, t1->textblock,
            //        t->textblock, t->parentExtIndex, t->parentExtIndex,
            //        f2->externalIndex, t->parentExtIndex - f2->externalIndex);
            return t->parentExtIndex - f2->externalIndex;
         }

      for (TBInfo *t = t2; t != NULL; t = t->parent)
         if (t->parent == t1) {
            //printf ("   (c) %p is an achestor of %p; direct child is %p (%d)"
            //        " => %d - %d = %d\n", t1->textblock, t2->textblock,
            //        t->textblock, t->parentExtIndex, f1->externalIndex,
            //        t->parentExtIndex, f1->externalIndex - t->parentExtIndex);
            return f1->externalIndex - t->parentExtIndex;
         }

      //printf ("   (d) other => %d - %d = %d\n",
      //        t1->index, t2->index, t1->index - t2->index);
      return t1->index - t2->index;
   }
}

int OutOfFlowMgr::SortedFloatsVector::findFloatIndex (Textblock *lastGB,
                                                      int lastExtIndex)
{
   Float key (oofm, NULL, lastGB, lastExtIndex);
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
   //        oofm->containingBlock, lastGB, lastExtIndex, i, r, size (),
   //        type == GB ? "GB" : "CB", this, side == LEFT ? "left" : "right");
   
   //for (int i = 0; i < size (); i++) {
   //   Float *f = get(i);
   //   TBInfo *t = oofm->getTextblock(f->generatingBlock);
   //   printf ("   %d: (%p [%d, %p], %d)\n", i, f->generatingBlock,
   //           t->index, t->parent ? t->parent->textblock : NULL,
   //           get(i)->externalIndex);
   //}

   return r;
}

int OutOfFlowMgr::SortedFloatsVector::find (Textblock *textblock, int y,
                                            int start, int end)
{
   Float key (oofm, NULL, NULL, 0);
   key.generatingBlock = textblock;
   key.yReal = y;
   key.index = -1; // for debugging
   Float::ComparePosition comparator (oofm, textblock);
   return bsearch (&key, false, start, end, &comparator);
}

int OutOfFlowMgr::SortedFloatsVector::findFirst (Textblock *textblock,
                                                 int y, int h,
                                                 Textblock *lastGB,
                                                 int lastExtIndex)
{
   int last = findFloatIndex (lastGB, lastExtIndex);
   assert (last < size());
   int i = find (textblock, y, 0, last);

   DBG_OBJ_MSGF_O ("border", 1, oofm,
                   "find (%s, %p, allocated: %s, %d, %p, %d) => last = %d, "
                   "result = %d (of %d)\n",
                   type == GB ? "GB" : "CB", textblock,
                   oofm->wasAllocated (textblock) ? "true" : "false", y, lastGB,
                   lastExtIndex, last, i, size());

   // Note: The smallest value of "i" is 0, which means that "y" is before or
   // equal to the first float. The largest value is "last + 1", which means
   // that "y" is after the last float. In both cases, the first or last,
   // respectively, float is a candidate. Generally, both floats, before and
   // at the search position, are candidates.

   if (i > 0 && get(i - 1)->covers (textblock, y, h))
      return i - 1;
   else if (i <= last && get(i)->covers (textblock, y, h))
      return i;
   else
      return -1;
}

int OutOfFlowMgr::SortedFloatsVector::findLastBeforeSideSpanningIndex
   (int sideSpanningIndex)
{
   OutOfFlowMgr::Float::CompareSideSpanningIndex comparator;
   Float key (NULL, NULL, NULL, 0);
   key.sideSpanningIndex = sideSpanningIndex;
   return bsearch (&key, false, &comparator) - 1;
}

void OutOfFlowMgr::SortedFloatsVector::put (Float *vloat)
{
   lout::container::typed::Vector<Float>::put (vloat);
   vloat->index = size() - 1;
   vloat->inCBList = type == CB;
}

OutOfFlowMgr::TBInfo::TBInfo (OutOfFlowMgr *oofm, Textblock *textblock,
                              TBInfo *parent, int parentExtIndex) :
   WidgetInfo (oofm, textblock)
{
   this->parent = parent;
   this->parentExtIndex = parentExtIndex;

   leftFloatsGB = new SortedFloatsVector (oofm, LEFT, SortedFloatsVector::GB);
   rightFloatsGB = new SortedFloatsVector (oofm, RIGHT, SortedFloatsVector::GB);
}

OutOfFlowMgr::TBInfo::~TBInfo ()
{
   delete leftFloatsGB;
   delete rightFloatsGB;
}

OutOfFlowMgr::AbsolutelyPositioned::AbsolutelyPositioned (OutOfFlowMgr *oofm,
                                                          core::Widget *widget,
                                                          Textblock
                                                          *generatingBlock,
                                                          int externalIndex)
{
   this->widget = widget;
   dirty = true;
}

OutOfFlowMgr::OutOfFlowMgr (Textblock *containingBlock)
{
   DBG_OBJ_CREATE ("dw::OutOfFlowMgr");

   this->containingBlock = containingBlock;

   leftFloatsCB = new SortedFloatsVector (this, LEFT, SortedFloatsVector::CB);
   rightFloatsCB = new SortedFloatsVector (this, RIGHT, SortedFloatsVector::CB);

   leftFloatsAll = new Vector<Float> (1, true);
   rightFloatsAll = new Vector<Float> (1, true);

   floatsByWidget = new HashTable <TypedPointer <Widget>, Float> (true, false);

   tbInfos = new Vector<TBInfo> (1, false);
   tbInfosByTextblock =
      new HashTable <TypedPointer <Textblock>, TBInfo> (true, true);

   leftFloatsMark = rightFloatsMark = 0;
   lastLeftTBIndex = lastRightTBIndex = 0;

   absolutelyPositioned = new Vector<AbsolutelyPositioned> (1, true);

   containingBlockWasAllocated = containingBlock->wasAllocated ();
   if (containingBlockWasAllocated)
      containingBlockAllocation = *(containingBlock->getAllocation());

   addWidgetInFlow (containingBlock, NULL, 0);
}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   //printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloatsCB;
   delete rightFloatsCB;

   // Order is important: tbInfosByTextblock is owner of the instances
   // of TBInfo.tbInfosByTextblock
   delete tbInfos;
   delete tbInfosByTextblock;

   delete floatsByWidget;

   // Order is important, since the instances of Float are owned by
   // leftFloatsAll and rightFloatsAll, so these should be deleted
   // last.
   delete leftFloatsAll;
   delete rightFloatsAll;

   delete absolutelyPositioned;
}

void OutOfFlowMgr::sizeAllocateStart (Allocation *containingBlockAllocation)
{
   DBG_OBJ_MSG ("resize.floats", 0, "<b>sizeAllocateStart</b>");
   this->containingBlockAllocation = *containingBlockAllocation;
   containingBlockWasAllocated = true;
}

void OutOfFlowMgr::sizeAllocateEnd ()
{
   DBG_OBJ_MSG ("resize.floats", 0, "<b>sizeAllocateEnd</b>");
   DBG_OBJ_MSG_START ();

   // Move floats from GB lists to the one CB list.
   moveFromGBToCB (LEFT);
   moveFromGBToCB (RIGHT);

   // Floats and absolutely positioned blocks have to be allocated
   sizeAllocateFloats (LEFT);
   sizeAllocateFloats (RIGHT);
   sizeAllocateAbsolutelyPositioned ();

   // Textblocks have already been allocated here.

   // Check changes of both textblocks and floats allocation. (All is checked
   // by hasRelationChanged (...).)
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfosByTextblock->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfosByTextblock->get (key);
      Textblock *tb = key->getTypedValue();

      int minFloatPos;
      Widget *minFloat;
      if (hasRelationChanged (tbInfo, &minFloatPos, &minFloat))
         tb->borderChanged (minFloatPos, minFloat);
   }
  
   // Store some information for later use.
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfosByTextblock->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfosByTextblock->get (key);
      Textblock *tb = key->getTypedValue();

      tbInfo->updateAllocation ();
      tbInfo->availWidth = tb->getAvailWidth ();
   }

   for (int i = 0; i < leftFloatsCB->size(); i++)
      leftFloatsCB->get(i)->updateAllocation ();

   for (int i = 0; i < rightFloatsCB->size(); i++)
      rightFloatsCB->get(i)->updateAllocation ();

   DBG_OBJ_MSG_END ();
}

bool OutOfFlowMgr::hasRelationChanged (TBInfo *tbInfo, int *minFloatPos,
                                       Widget **minFloat)
{
   DBG_OBJ_MSGF ("resize.floats", 0,
                 "<b>hasRelationChanged</b> (<i>widget:</i> %p, ...)",
                 tbInfo->getWidget ());
   DBG_OBJ_MSG_START ();

   int leftMinPos, rightMinPos;
   Widget *leftMinFloat, *rightMinFloat;
   bool c1 =
      hasRelationChanged (tbInfo, LEFT, &leftMinPos, &leftMinFloat);
   bool c2 =
      hasRelationChanged (tbInfo, RIGHT, &rightMinPos, &rightMinFloat);
   if (c1 || c2) {
      if (!c1) {
         *minFloatPos = rightMinPos;
         *minFloat = rightMinFloat;
      } else if (!c2) {
         *minFloatPos = leftMinPos;
         *minFloat = leftMinFloat;
      } else {
         if (leftMinPos < rightMinPos) {
            *minFloatPos = leftMinPos;
            *minFloat = leftMinFloat;
         } else{ 
            *minFloatPos = rightMinPos;
            *minFloat = rightMinFloat;
         }
      }
   }

   if (c1 || c2)
      DBG_OBJ_MSGF ("resize.floats", 1,
                    "has changed: minFloatPos = %d, minFloat = %p",
                    *minFloatPos, *minFloat);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "has not changed");
   
   DBG_OBJ_MSG_END ();
   return c1 || c2;
}

bool OutOfFlowMgr::hasRelationChanged (TBInfo *tbInfo, Side side,
                                       int *minFloatPos, Widget **minFloat)
{
   DBG_OBJ_MSGF ("resize.floats", 0,
                 "<b>hasRelationChanged</b> (<i>widget:</i> %p, %s, ...)",
                 tbInfo->getWidget (), side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   bool changed = false;
   
   for (int i = 0; i < list->size(); i++) {
      // TODO binary search?
      Float *vloat = list->get(i);
      int floatPos;

      if (tbInfo->getTextblock () == vloat->generatingBlock)
         DBG_OBJ_MSGF ("resize.floats", 1,
                       "not checking (generating!) textblock %p against float "
                       "%p", tbInfo->getWidget (), vloat->getWidget ());
      else {
         Allocation *gba = getAllocation (vloat->generatingBlock);

         int newFlx =
            calcFloatX (vloat, side,
                        vloat->generatingBlock->getAllocation()->x
                        - containingBlockAllocation.x,
                        gba->width, vloat->generatingBlock->getAvailWidth ());
         int newFly = vloat->generatingBlock->getAllocation()->y
            - containingBlockAllocation.y + vloat->yReal;
         
         DBG_OBJ_MSGF ("resize.floats", 1,
                       "checking textblock %p against float %p",
                       tbInfo->getWidget (), vloat->getWidget ());
         DBG_OBJ_MSG_START ();
         
         if (hasRelationChanged (tbInfo->wasThenAllocated (),
                                 tbInfo->getOldXCB (), tbInfo->getOldYCB (),
                                 tbInfo->getOldWidth (),
                                 tbInfo->getOldHeight (),
                                 tbInfo->getNewXCB (), tbInfo->getNewYCB (),
                                 tbInfo->getNewWidth (),
                                 tbInfo->getNewHeight (),
                                 vloat->wasThenAllocated (),
                                 // When not allocated before, these values
                                 // are undefined, but this does not matter,
                                 // since they are neither used.
                                 vloat->getOldXCB (), vloat->getOldYCB (),
                                 vloat->getOldWidth (), vloat->getOldHeight (),
                                 newFlx, newFly, vloat->size.width,
                                 vloat->size.ascent + vloat->size.descent,
                                 side, &floatPos)) {
            if (!changed || floatPos < *minFloatPos) {
               *minFloatPos = floatPos;
               *minFloat = vloat->getWidget ();
            }
            changed = true;
         } else
            DBG_OBJ_MSG ("resize.floats", 0, "No.");

         DBG_OBJ_MSG_END ();
      }

      // All floarts are searched, to find the minimum. TODO: Are
      // floats sorted, so this can be shortened? (The first is the
      // minimum?)
   }

   if (changed)
      DBG_OBJ_MSGF ("resize.floats", 1,
                    "has changed: minFloatPos = %d, minFloat = %p",
                    *minFloatPos, *minFloat);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "has not changed");
   
   DBG_OBJ_MSG_END ();
   return changed;
}

/**
 * \brief ...
 *
 * All coordinates are given relative to the CB. *floatPos is relative
 * to the TB, and may be negative.
 */
bool OutOfFlowMgr::hasRelationChanged (bool oldTBAlloc,
                                       int oldTBx, int oldTBy, int oldTBw,
                                       int oldTBh, int newTBx, int newTBy,
                                       int newTBw, int newTBh,
                                       bool oldFlAlloc,
                                       int oldFlx, int oldFly, int oldFlw,
                                       int oldFlh, int newFlx, int newFly,
                                       int newFlw, int newFlh,
                                       Side side, int *floatPos)
{
   DBG_OBJ_MSGF ("resize.floats", 0,
                 "<b>hasRelationChanged</b> (<i>see below</i>, %s, ...)",
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   if (oldTBAlloc)
      DBG_OBJ_MSGF ("resize.floats", 1, "old TB: %d, %d; %d * %d",
                    oldTBx, oldTBy, oldTBw, oldTBh);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "old TB: undefined");
   DBG_OBJ_MSGF ("resize.floats", 1, "new TB: %d, %d; %d * %d",
                 newTBx, newTBy, newTBw, newTBh);

   if (oldFlAlloc)
      DBG_OBJ_MSGF ("resize.floats", 1, "old Fl: %d, %d; %d * %d",
                    oldFlx, oldFly, oldFlw, oldFlh);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "old Fl: undefined");
   DBG_OBJ_MSGF ("resize.floats", 1, "new Fl: %d, %d; %d * %d",
                 newFlx, newFly, newFlw, newFlh);

   bool result;
   if (oldTBAlloc && oldFlAlloc) {
      bool oldCov = oldFly + oldFlh > oldTBy && oldFly < oldTBy + oldTBh;
      bool newCov = newFly + newFlh > newTBy && newFly < newTBy + newTBh;

      DBG_OBJ_MSGF ("resize.floats", 1, "covered? then: %s, now: %s.",
                    oldCov ? "yes" : "no", newCov ? "yes" : "no");
      DBG_OBJ_MSG_START ();

      if (oldCov && newCov) {
         int yOld = oldFly - oldTBy, yNew = newFly - newTBy;
         if (yOld == yNew) {
            DBG_OBJ_MSGF ("resize.floats", 2,
                          "old (%d - %d) and new (%d - %d) position equal: %d",
                          oldFly, oldTBy, newFly, newTBy, yOld);

            // Float position has not changed, but perhaps the amout
            // how far the float reaches into the TB. (TODO:
            // Generally, not only here, it could be tested whether
            // the float reaches into the TB at all.)
            int wOld, wNew;
            if (side == LEFT) {
               wOld = oldFlx + oldFlw - oldTBx;
               wNew = newFlx + newFlw - newTBx;
            } else {
               wOld = oldTBx + oldTBw - oldFlx;
               wNew = newTBx + newTBw - newFlx;
            }

            DBG_OBJ_MSGF ("resize.floats", 2, "wOld = %d, wNew = %d\n",
                          wOld, wNew);
          
            if (wOld == wNew) {
               if (oldFlh == newFlh)
                  result = false;
               else {
                  // Only heights of floats changed. Relevant only
                  // from bottoms of float.
                  result = min (yOld + oldFlh, yNew + newFlh);
               }
            } else {
               *floatPos = yOld;
               result = true;
            }
         } else {
            DBG_OBJ_MSGF ("resize.floats", 2,
                          "old (%d - %d = %d) and new (%d - %d = %d) position "
                          "different",
                          oldFly, oldTBy, yOld, newFly, newTBy, yNew);
            *floatPos = min (yOld, yNew);
            result = true;
         }
      } else if (oldCov) {
         *floatPos = oldFly - oldTBy;
         result = true;
         DBG_OBJ_MSGF ("resize.floats", 2,
                       "returning old position: %d - %d = %d", oldFly, oldTBy,
                       *floatPos);
      } else if (newCov) {
         *floatPos = newFly - newTBy;
         result = true;
         DBG_OBJ_MSGF ("resize.floats", 2,
                       "returning new position: %d - %d = %d", newFly, newTBy,
                       *floatPos);
      } else
         result = false;

      DBG_OBJ_MSG_END ();
   } else {
      // Not allocated before: ignore all old values, only check whether
      // TB is covered by Float.
      if (newFly + newFlh > newTBy && newFly < newTBy + newTBh) {
         *floatPos = newFly - newTBy;
         result = true;
      } else
         result = false;
   }

   if (result)
      DBG_OBJ_MSGF ("resize.floats", 1, "has changed: floatPos = %d",
                    *floatPos);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "has not changed");

   DBG_OBJ_MSG_END ();

   return result;
}

bool OutOfFlowMgr::isTextblockCoveredByFloat (Float *vloat, Textblock *tb,
                                              int tbx, int tby,
                                              int tbWidth, int tbHeight,
                                              int *floatPos)
{
   assert (wasAllocated (vloat->generatingBlock));
   
   int flh = vloat->dirty ? 0 : vloat->size.ascent + vloat->size.descent;
   int y1 = getAllocation(vloat->generatingBlock)->y + vloat->yReal;
   int y2 = y1 + flh;

   // TODO: Also regard horizontal dimension (same for tellFloatPosition)?
   if (y2 > tby && y1 < tby + tbHeight) {
      *floatPos = y1 - tby;
      return true;
   } else
      return false;
}

void OutOfFlowMgr::checkChangedFloatSizes ()
{
   DBG_OBJ_MSG ("resize.floats", 0, "<b>checkChangedFloatSizes</b>");
   DBG_OBJ_MSG_START ();

   checkChangedFloatSizes (leftFloatsCB);
   checkChangedFloatSizes (rightFloatsCB);

   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::checkChangedFloatSizes (SortedFloatsVector *list)
{
   DBG_OBJ_MSG ("resize.floats", 0,
                "<b>checkChangedFloatSizes</b> (<i>list</i>)");
   DBG_OBJ_MSG_START ();

   // TODO (i) Comment (ii) linear search?
   for (int i = 0; i < list->size(); i++) {
      // TODO binary search
      Float *vloat = list->get(i);

      if (vloat->sizeChangedSinceLastAllocation &&
          wasAllocated (vloat->generatingBlock)) {
         DBG_OBJ_MSGF ("resize.floats", 1, "float %p: checking textblocks",
                       vloat->getWidget ());
         DBG_OBJ_MSG_START ();

         for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
                 tbInfosByTextblock->iterator ();
              it.hasNext (); ) {
            Textblock *tb = it.getNext()->getTypedValue();
            if (wasAllocated (tb)) {
               Allocation *tba = getAllocation (tb);
               int floatPos;

               if (isTextblockCoveredByFloat
                   (vloat, tb, tba->x - containingBlockAllocation.x,
                    tba->y - containingBlockAllocation.y,
                    tba->width, tba->ascent + tba->descent, &floatPos)) {
                  DBG_OBJ_MSGF ("resize.floats", 2, "%p: covereds", tb);
                  tb->borderChanged (floatPos, vloat->getWidget ());
               } else
                  DBG_OBJ_MSGF ("resize.floats", 2, "%p: not covered", tb);
            } else
               DBG_OBJ_MSGF ("resize.floats", 2, "%p: not allocated", tb);
         }

         vloat->sizeChangedSinceLastAllocation = false;

         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::moveFromGBToCB (Side side)
{
   SortedFloatsVector *dest = side == LEFT ? leftFloatsCB : rightFloatsCB;
   int *floatsMark = side == LEFT ? &leftFloatsMark : &rightFloatsMark;

   for (int mark = 0; mark <= *floatsMark; mark++)
      for (lout::container::typed::Iterator<TBInfo> it = tbInfos->iterator ();
           it.hasNext (); ) {
         TBInfo *tbInfo = it.getNext ();
         SortedFloatsVector *src =
            side == LEFT ? tbInfo->leftFloatsGB : tbInfo->rightFloatsGB;
         for (int i = 0; i < src->size (); i++) {
            Float *vloat = src->get(i);
            if (!vloat->inCBList && vloat->mark == mark) {
               dest->put (vloat);
               //printf("[%p] moving %s float %p (%s %p, mark %d) to CB list\n",
               //       containingBlock, side == LEFT ? "left" : "right",
               //       vloat, vloat->widget->getClassName(), vloat->widget,
               //       vloat->mark);
            }
         }
      }

   *floatsMark = 0;

   /* Old code: GB lists do not have to be cleared, but their contents
      are still useful after allocation. Soon to be deleted, not only
      uncommented.
      
   for (lout::container::typed::Iterator<TBInfo> it = tbInfos->iterator ();
        it.hasNext (); ) {
      TBInfo *tbInfo = it.getNext ();
      SortedFloatsVector *src =
         side == LEFT ? tbInfo->leftFloatsGB : tbInfo->rightFloatsGB;
      src->clear ();
   }
   */

   //printf ("[%p] new %s list:\n",
   //        containingBlock, side == LEFT ? "left" : "right");
   //for (int i = 0; i < dest->size(); i++)
   //   printf ("   %d: %s\n", i, dest->get(i)->toString());
}

void OutOfFlowMgr::sizeAllocateFloats (Side side)
{
   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      // "ensureFloatSize (vloat)" was already called before.

      Allocation *gbAllocation = getAllocation(vloat->generatingBlock);
      Allocation *cbAllocation = getAllocation(containingBlock);

      Allocation childAllocation;
      childAllocation.x = cbAllocation->x +
         calcFloatX (vloat, side, gbAllocation->x - cbAllocation->x,
                     gbAllocation->width,
                     vloat->generatingBlock->getAvailWidth());
      childAllocation.y = gbAllocation->y + vloat->yReal;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;
      
      vloat->getWidget()->sizeAllocate (&childAllocation);
   }
}


/**
 * \brief ...
 *
 * gbX is given relative to the CB, as is the return value.
 */
int OutOfFlowMgr::calcFloatX (Float *vloat, Side side, int gbX, int gbWidth,
                              int gbAvailWidth)
{
   int gbActualWidth;

   switch (side) {
   case LEFT:
      // Left floats are always aligned on the left side of the
      // generator (content, not allocation).
      return gbX + vloat->generatingBlock->getStyle()->boxOffsetX();
         break;
         
   case RIGHT:
      // In some cases, the actual (allocated) width is too large; we
      // use the "available" width here.
      gbActualWidth = min (gbWidth, gbAvailWidth);

      // Similar for right floats, but in this case, floats are
      // shifted to the right when they are too big (instead of
      // shifting the generator to the right).
      return max (gbX + gbActualWidth - vloat->size.width
                  - vloat->generatingBlock->getStyle()->boxRestWidth(),
                  // Do not exceed CB allocation:
                  0);
         
   default:
      assertNotReached ();
      return 0;
   }
}


void OutOfFlowMgr::draw (View *view, Rectangle *area)
{
   drawFloats (leftFloatsCB, view, area);
   drawFloats (rightFloatsCB, view, area);
   drawAbsolutelyPositioned (view, area);
}

void OutOfFlowMgr::drawFloats (SortedFloatsVector *list, View *view,
                               Rectangle *area)
{
   // This could be improved, since the list is sorted: search the
   // first float fitting into the area, and iterate until one is
   // found below the area.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      core::Rectangle childArea;
      if (vloat->getWidget()->intersects (area, &childArea))
         vloat->getWidget()->draw (view, &childArea);
   }
}

void OutOfFlowMgr::drawAbsolutelyPositioned (View *view, Rectangle *area)
{
   for (int i = 0; i < absolutelyPositioned->size(); i++) {
      AbsolutelyPositioned *abspos = absolutelyPositioned->get(i);
      core::Rectangle childArea;
      if (abspos->widget->intersects (area, &childArea))
         abspos->widget->draw (view, &childArea);
   }
}

/**
 * This method consideres also the attributes not yet considered by
 * dillo, so that the containing block is determined correctly, which
 * leads sometimes to a cleaner rendering.
 */
bool OutOfFlowMgr::isWidgetOutOfFlow (core::Widget *widget)
{
   return
      widget->getStyle()->vloat != core::style::FLOAT_NONE ||
      widget->getStyle()->position == core::style::POSITION_ABSOLUTE ||
      widget->getStyle()->position == core::style::POSITION_FIXED;
}

bool OutOfFlowMgr::isWidgetHandledByOOFM (core::Widget *widget)
{
   // May be extended for fixed (and relative?) positions.
   return isWidgetFloat (widget);
   // TODO temporary disabled: || isWidgetAbsolutelyPositioned (widget);
}

void OutOfFlowMgr::addWidgetInFlow (Textblock *textblock,
                                    Textblock *parentBlock, int externalIndex)
{
   //printf ("[%p] addWidgetInFlow (%p, %p, %d)\n",
   //        containingBlock, textblock, parentBlock, externalIndex);

   TBInfo *tbInfo =
      new TBInfo (this, textblock,
                  parentBlock ? getTextblock (parentBlock) : NULL,
                  externalIndex);     
   tbInfo->index = tbInfos->size();
   
   tbInfos->put (tbInfo);
   tbInfosByTextblock->put (new TypedPointer<Textblock> (textblock), tbInfo);
}

void OutOfFlowMgr::addWidgetOOF (Widget *widget, Textblock *generatingBlock,
                                 int externalIndex)
{
   DBG_OBJ_MSGF ("construct.oofm", 0, "<b>addWidgetOOF</b> (%p, %p, %d)",
                 widget, generatingBlock, externalIndex);

   if (isWidgetFloat (widget)) {
      TBInfo *tbInfo = getTextblock (generatingBlock);

      Float *vloat = new Float (this, widget, generatingBlock, externalIndex);

      // Note: Putting the float first in the GB list, and then,
      // possibly into the CB list (in that order) will trigger
      // setting Float::inCBList to the right value.

      switch (widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         leftFloatsAll->put (vloat);
         widget->parentRef = createRefLeftFloat (leftFloatsAll->size() - 1);
         tbInfo->leftFloatsGB->put (vloat);

         if (wasAllocated (generatingBlock)) {
            leftFloatsCB->put (vloat);
            //printf ("[%p] adding left float %p (%s %p) to CB list\n",
            //        containingBlock, vloat, widget->getClassName(), widget);
         } else {
            if (tbInfo->index < lastLeftTBIndex)
               leftFloatsMark++;

            vloat->mark = leftFloatsMark;
            //printf ("[%p] adding left float %p (%s %p, mark %d) to GB list "
            //        "(index %d, last = %d)\n",
            //        containingBlock, vloat, widget->getClassName(), widget,
            //        vloat->mark, tbInfo->index, lastLeftTBIndex);

            lastLeftTBIndex = tbInfo->index;
         }
         break;

      case FLOAT_RIGHT:
         rightFloatsAll->put (vloat);
         widget->parentRef = createRefRightFloat (rightFloatsAll->size() - 1);
         tbInfo->rightFloatsGB->put (vloat);

         if (wasAllocated (generatingBlock)) {
            rightFloatsCB->put (vloat);
            //printf ("[%p] adding right float %p (%s %p) to CB list\n",
            //        containingBlock, vloat, widget->getClassName(), widget);
         } else {
            if (tbInfo->index < lastRightTBIndex)
               rightFloatsMark++;

            vloat->mark = rightFloatsMark;
            //printf ("[%p] adding right float %p (%s %p, mark %d) to GB list "
            //        "(index %d, last = %d)\n",
            //        containingBlock, vloat, widget->getClassName(), widget,
            //        vloat->mark, tbInfo->index, lastRightTBIndex);

            lastRightTBIndex = tbInfo->index;
         }

         break;

      default:
         assertNotReached();
      }

      // "sideSpanningIndex" is only compared, so this simple
      // assignment is sufficient; differenciation between GB and CB
      // lists is not neccessary. TODO: Can this also be applied to
      // "index", to simplify the current code? Check: where is
      // "index" used.
      vloat->sideSpanningIndex =
         leftFloatsAll->size() + rightFloatsAll->size() - 1;

      floatsByWidget->put (new TypedPointer<Widget> (widget), vloat);
   } else if (isWidgetAbsolutelyPositioned (widget)) {
      AbsolutelyPositioned *abspos =
         new AbsolutelyPositioned (this, widget, generatingBlock,
                                   externalIndex);
      absolutelyPositioned->put (abspos);
      widget->parentRef =
         createRefAbsolutelyPositioned (absolutelyPositioned->size() - 1);
   } else
      // May be extended.
      assertNotReached();
}

void OutOfFlowMgr::moveExternalIndices (Textblock *generatingBlock,
                                        int oldStartIndex, int diff)
{
   TBInfo *tbInfo = getTextblock (generatingBlock);
   moveExternalIndices (tbInfo->leftFloatsGB, oldStartIndex, diff);
   moveExternalIndices (tbInfo->rightFloatsGB, oldStartIndex, diff);
}

void OutOfFlowMgr::moveExternalIndices (SortedFloatsVector *list,
                                        int oldStartIndex, int diff)
{
   // Could be faster with binary search, but the GB (not CB!) lists
   // should be rather small.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      if (vloat->externalIndex >= oldStartIndex)
         vloat->externalIndex += diff;
   }
}

OutOfFlowMgr::Float *OutOfFlowMgr::findFloatByWidget (Widget *widget)
{
   TypedPointer <Widget> key (widget);
   Float *vloat = floatsByWidget->get (&key);
   assert (vloat != NULL);
   return vloat;
}

void OutOfFlowMgr::markSizeChange (int ref)
{
   DBG_OBJ_MSGF ("resize", 0, "<b>markSizeChange</b> (%d)", ref);
   DBG_OBJ_MSG_START ();

   if (isRefFloat (ref)) {
      Float *vloat;
      
      if (isRefLeftFloat (ref)) {
         int i = getFloatIndexFromRef (ref);
         vloat = leftFloatsAll->get (i);
         //printf ("   => left float %d\n", i);
      } else if (isRefRightFloat (ref)) {
         int i = getFloatIndexFromRef (ref);
         vloat = rightFloatsAll->get (i);
         //printf ("   => right float %d\n", i);
      } else {
         assertNotReached();
         vloat = NULL; // compiler happiness
      }
      
      vloat->dirty = vloat->sizeChangedSinceLastAllocation = true;

      // The generating block is told directly about this. (Others later, in
      // sizeAllocateEnd.) Could be faster (cf. hasRelationChanged, which
      // differentiates many special cases), but the size is not known yet,
      vloat->generatingBlock->borderChanged (vloat->yReal, vloat->getWidget ());
   } else if (isRefAbsolutelyPositioned (ref)) {
      int i = getAbsolutelyPositionedIndexFromRef (ref);
      absolutelyPositioned->get(i)->dirty = true;
   } else
      assertNotReached();

   DBG_OBJ_MSG_END ();
}


void OutOfFlowMgr::markExtremesChange (int ref)
{
   // Nothing to do here.
}

Widget *OutOfFlowMgr::getWidgetAtPoint (int x, int y, int level)
{
   Widget *childAtPoint = getFloatWidgetAtPoint (leftFloatsCB, x, y, level);
   if (childAtPoint == NULL)
      childAtPoint = getFloatWidgetAtPoint (rightFloatsCB, x, y, level);
   if (childAtPoint == NULL)
      childAtPoint = getAbsolutelyPositionedWidgetAtPoint (x, y, level);
   return childAtPoint;
}

Widget *OutOfFlowMgr::getFloatWidgetAtPoint (SortedFloatsVector *list,
                                             int x, int y, int level)
{
   for (int i = 0; i < list->size(); i++) {
      // Could use binary search to be faster.
      Float *vloat = list->get(i);
      Widget *childAtPoint =
         vloat->getWidget()->getWidgetAtPoint (x, y, level + 1);
      if (childAtPoint)
         return childAtPoint;
   }

   return NULL;
}

Widget *OutOfFlowMgr::getAbsolutelyPositionedWidgetAtPoint (int x, int y,
                                                            int level)
{
   for (int i = 0; i < absolutelyPositioned->size(); i++) {
      AbsolutelyPositioned *abspos = absolutelyPositioned->get(i);
      Widget *childAtPoint = abspos->widget->getWidgetAtPoint (x, y, level + 1);
      if (childAtPoint)
         return childAtPoint;
   }

   return NULL;
}

void OutOfFlowMgr::tellPosition (Widget *widget, int yReq)
{
   if (isWidgetFloat (widget))
      tellFloatPosition (widget, yReq);

   // Nothing to do for absolutely positioned blocks.
}


void OutOfFlowMgr::tellFloatPosition (Widget *widget, int yReq)
{
   DBG_OBJ_MSGF ("resize.floats", 0, "<b>tellFloatPosition</b> (%p, %d)",
                 widget, yReq);
   DBG_OBJ_MSG_START ();

   assert (yReq >= 0);

   Float *vloat = findFloatByWidget(widget);
   int oldY = vloat->yReal;

   SortedFloatsVector *listSame, *listOpp;
   Side side;
   getFloatsListsAndSide (vloat, &listSame, &listOpp, &side);
   ensureFloatSize (vloat);

   // "yReal" may change due to collisions (see below).
   vloat->yReq = vloat->yReal = yReq;

   // Test collisions (on this side). Only previous float is relevant.
   int yRealNew;
   if (vloat->index >= 1 &&
       collides (vloat, listSame->get (vloat->index - 1), &yRealNew)) {
      vloat->yReal = yRealNew;
   }

   // Test collisions (on the opposite side). Search the last float on
   // the other size before this float; only this is relevant.
   int lastOppFloat =
      listOpp->findLastBeforeSideSpanningIndex (vloat->sideSpanningIndex);
   if (lastOppFloat >= 0) {
      Float *last = listOpp->get (lastOppFloat);
      if (collides (vloat, last, &yRealNew)) {
         // Here, test also horizontal values.
         bool collidesH;
         if (vloat->generatingBlock == last->generatingBlock)
            collidesH = vloat->size.width + last->size.width +
               vloat->generatingBlock->getStyle()->boxDiffWidth()
               > vloat->generatingBlock->getAvailWidth();
         else {
            // Here (different generating blocks) it can be assumed
            // that the allocations are defined, otherwise, the float
            // "last" would not be found in "listOpp".
            assert (wasAllocated (vloat->generatingBlock));
            assert (wasAllocated (last->generatingBlock));
            Float *left, *right;
            if (widget->getStyle()->vloat == FLOAT_LEFT) {
               left = vloat;
               right = last;
            } else {
               left = last;
               right = vloat;
            }
            
            // right border of the left float (canvas coordinates)
            int rightOfLeft =
               left->generatingBlock->getAllocation()->x
               + left->generatingBlock->getStyle()->boxOffsetX()
               + left->size.width;
            // left border of the right float (canvas coordinates)
            int leftOfRight =
               right->generatingBlock->getAllocation()->x
               + min (right->generatingBlock->getAllocation()->width,
                      right->generatingBlock->getAvailWidth())
               - right->generatingBlock->getStyle()->boxRestWidth()
               - right->size.width;
            
            collidesH = rightOfLeft > leftOfRight;
         }
         
         if (collidesH)
            vloat->yReal = yRealNew;
      }
   }

   DBG_OBJ_MSGF ("resize.floats", 1,
                 "oldY = %d, vloat->yReq = %d, vloat->yReal = %d",
                 oldY, vloat->yReq, vloat->yReal);

   DBG_OBJ_MSG_END ();
}

bool OutOfFlowMgr::collides (Float *vloat, Float *other, int *yReal)
{
   DBG_OBJ_MSGF ("resize.floats", 0,
                 "<b>collides</b> (#%d [%p], #%d [%p], ...)",
                 vloat->index, vloat->getWidget (), other->index,
                 other->getWidget ());
   DBG_OBJ_MSG_START ();

   bool result = false;

   DBG_OBJ_MSGF ("resize.floats", 1, "initial yReal = %d", vloat->yReal);

   if (vloat->generatingBlock == other->generatingBlock) {
      ensureFloatSize (other);
      int otherBottomGB =
         other->yReal + other->size.ascent + other->size.descent;

      DBG_OBJ_MSGF ("resize.floats", 1,
                    "same generators: otherBottomGB = %d + (%d + %d) = %d",
                    other->yReal, other->size.ascent, other->size.descent,
                    otherBottomGB);

      if (vloat->yReal <  otherBottomGB) {
         *yReal = otherBottomGB;
         result = true;
      }
   } else {
      assert (wasAllocated (vloat->generatingBlock));
      assert (vloat->getWidget()->wasAllocated ());
      assert (other->getWidget()->wasAllocated ());

      Allocation *gba = getAllocation (vloat->generatingBlock),
         *fla = vloat->getWidget()->getAllocation (),
         *flaOther = other->getWidget()->getAllocation ();
      int otherBottomCanvas =
         flaOther->y + flaOther->ascent + flaOther->descent;

      DBG_OBJ_MSGF ("resize.floats", 1,
                    "different generators: this float at %d, "
                    "otherBottomCanvas = %d + (%d + %d) = %d",
                    fla->y, flaOther->y, flaOther->ascent, flaOther->descent,
                    otherBottomCanvas);

      if (fla->y < otherBottomCanvas) {
         *yReal = otherBottomCanvas - gba->y;
         result = true;
      }
   }

   if (result)
      DBG_OBJ_MSGF ("resize.floats", 1, "collides: new yReal = %d", *yReal);
   else
      DBG_OBJ_MSG ("resize.floats", 1, "does not collide");

   DBG_OBJ_MSG_END ();
   return result;
}

void OutOfFlowMgr::getFloatsListsAndSide (Float *vloat,
                                          SortedFloatsVector **listSame,
                                          SortedFloatsVector **listOpp,
                                          Side *side)
{
   TBInfo *tbInfo = getTextblock (vloat->generatingBlock);
      
   switch (vloat->getWidget()->getStyle()->vloat) {
   case FLOAT_LEFT:
      if (wasAllocated (vloat->generatingBlock)) {
         if (listSame) *listSame = leftFloatsCB;
         if (listOpp) *listOpp = rightFloatsCB;
      } else {
         if (listSame) *listSame = tbInfo->leftFloatsGB;
         if (listOpp) *listOpp = tbInfo->rightFloatsGB;
      }
      if (side) *side = LEFT;
      break;

   case FLOAT_RIGHT:
      if (wasAllocated (vloat->generatingBlock)) {
         if (listSame) *listSame = rightFloatsCB;
         if (listOpp) *listOpp = leftFloatsCB;
      } else {
         if (listSame) *listSame = tbInfo->rightFloatsGB;
         if (listOpp) *listOpp = tbInfo->leftFloatsGB;
      }
      if (side) *side = RIGHT;
      break;

   default:
      assertNotReached();
   }
}

void OutOfFlowMgr::getSize (int cbWidth, int cbHeight,
                            int *oofWidth, int *oofHeight)
{
   // CbWidth and cbHeight *do* contain padding, border, and
   // margin. See call in dw::Textblock::sizeRequest. (Notice that
   // this has changed from an earlier version.)

   // Also notice that Float::y includes margins etc.

   // TODO Is it correct to add padding, border, and margin to the
   // containing block? Check CSS spec.

   //printf ("[%p] GET_SIZE (%d, %d, ...): %d / %d floats...\n",
   //        containingBlock, cbWidth, cbHeight,
   //        leftFloatsCB->size(), rightFloatsCB->size());

   int oofWidthAbsPos, oofHeightAbsPos;
   getAbsolutelyPositionedSize (&oofWidthAbsPos, &oofHeightAbsPos);

   int oofWidthtLeft, oofWidthRight, oofHeightLeft, oofHeightRight;
   getFloatsSize (leftFloatsCB, LEFT, &oofWidthtLeft, &oofHeightLeft);
   getFloatsSize (rightFloatsCB, RIGHT, &oofWidthRight, &oofHeightRight);

   *oofWidth = max (oofWidthtLeft, oofWidthRight, oofWidthAbsPos);
   *oofHeight = max (oofHeightLeft, oofHeightRight, oofHeightAbsPos);

   //printf ("   => %d x %d => %d x %d (%d / %d)\n",
   //        cbWidth, cbHeight, *oofWidth, *oofHeight,
   //        oofHeightLeft, oofHeightRight);
}

void OutOfFlowMgr::getFloatsSize (SortedFloatsVector *list, Side side,
                                  int *width, int *height)
{
   *width = 0;
   *height = containingBlock->getStyle()->boxDiffHeight();

   // Idea for a faster implementation: find the last float; this
   // should be the relevant one, since the list is sorted.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);

      // 1. Height: May play a role when the float is too wide.
      // Minimum taken, since the relevant case is when the containing
      // block is otherwise not wide enough.
      int borderDiff = getMinBorderDiff (vloat, side);
      *width = max (*width, vloat->size.width + borderDiff);

      // 2. Height: Plays a role for floats hanging over at the bottom
      // of the page.

      // Notice that all positions are relative to the generating
      // block, but we need them relative to the containing block.

      // Position of generating block, relative to containing
      // block. Greater or equal than 0, so dealing with 0 when it
      // cannot yet be calculated is safe. (No distiction whether it
      // is defined or not is necessary.)

      int yGBinCB;

      if (vloat->generatingBlock == containingBlock)
         // Simplest case: the generator is the container.
         yGBinCB = 0;
      else {
         if (wasAllocated (containingBlock)) {
            if (wasAllocated (vloat->generatingBlock))
               // Simple case: both containing block and generating
               // block are defined.
               yGBinCB = getAllocation(vloat->generatingBlock)->y
                  - containingBlock->getAllocation()->y;
            else
               // Generating block not yet allocation; the next
               // allocation will, when necessary, trigger
               // sizeRequest. (TODO: Is this really the case?)
               yGBinCB = 0;
         } else
            // Nothing can be done now, but the next allocation
            // will trigger sizeAllocate. (TODO: Is this really the
            // case?)
            yGBinCB = 0;
      }
      
      *height =
         max (*height,
              yGBinCB + vloat->yReal + vloat->size.ascent + vloat->size.descent
              + containingBlock->getStyle()->boxRestHeight());
      //printf ("   float %d: (%d + %d) + (%d + %d + %d) => %d\n",
      //        i, yGBinCB, vloat->yReal, vloat->size.ascent,
      //        vloat->size.descent,
      //        containingBlock->getStyle()->boxRestHeight(), *height);
   }
}

void OutOfFlowMgr::getExtremes (int cbMinWidth, int cbMaxWidth,
                                int *oofMinWidth, int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;
   accumExtremes (leftFloatsCB, LEFT, oofMinWidth, oofMaxWidth);
   accumExtremes (rightFloatsCB, RIGHT, oofMinWidth, oofMaxWidth);
   // TODO Absolutely positioned elements
}

void OutOfFlowMgr::accumExtremes (SortedFloatsVector *list, Side side,
                                  int *oofMinWidth, int *oofMaxWidth)
{
   // Idea for a faster implementation: use incremental resizing?

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      int minBorderDiff = getMinBorderDiff (vloat, side);
      int maxBorderDiff = getMaxBorderDiff (vloat, side);
      Extremes extr;
      vloat->getWidget()->getExtremes (&extr);

      *oofMinWidth = max (*oofMinWidth, extr.minWidth + minBorderDiff);
      *oofMaxWidth = max (*oofMaxWidth, extr.maxWidth + maxBorderDiff);
   }
}

/**
 * Minimal difference between generating block and to containing
 * block, Greater or equal than 0, so dealing with 0 when it cannot
 * yet be calculated is safe. (No distiction whether it is defined or
 * not is necessary.)
 */
int OutOfFlowMgr::getMinBorderDiff (Float *vloat, Side side)
{
   if (vloat->generatingBlock == containingBlock)
      // Simplest case: the generator is the container.
      // Since the way, how left and right floats are positioned when
      // there is not much space, is not symmetric, the *minimal* value
      // considered for the margin/border/padding of the generating block
      // is *zero* for floats on the right.
      return
         side == LEFT ? vloat->generatingBlock->getStyle()->boxOffsetX() : 0;
   else {
      if (wasAllocated (containingBlock)) {
         if (wasAllocated (vloat->generatingBlock)) {
            // Simple case: both containing block and generating block
            // are defined.
            Allocation *gba = getAllocation(vloat->generatingBlock);
            Allocation *cba = getAllocation(containingBlock);
            if (side == LEFT)
               return gba->x - cba->x +
                  vloat->generatingBlock->getStyle()->boxOffsetX();
            else
               // For margin/border/padding see comment above. Also,
               // in the worst case, the float can take the whole CB
               // (not GB!) width. Therefore:
               return 0;
         } else
            // Generating block not yet allocation; the next
            // allocation will, when necessary, trigger
            // getExtremes. (TODO: Is this really the case?)
            return 0;
      } else
         // Nothing can be done now, but the next allocation will
         // trigger getExtremes. (TODO: Is this really the case?)
         return 0;
   }
}

/**
 * Maximal difference between generating block and to containing
 * block, i. e. sum on both sides.
 */
int OutOfFlowMgr::getMaxBorderDiff (Float *vloat, Side side)
{
   if (vloat->generatingBlock == containingBlock)
      // Simplest case: the generator is the container.
      return vloat->generatingBlock->getStyle()->boxDiffWidth();
   else {
      if (wasAllocated (containingBlock)) {
         if (wasAllocated (vloat->generatingBlock))
            // Simple case: both containing block and generating block
            // are defined.
            return getAllocation(containingBlock)->width -
               getAllocation(vloat->generatingBlock)->width +
               vloat->generatingBlock->getStyle()->boxDiffWidth();
         else
            // Generating block not yet allocation; the next
            // allocation will, when necessary, trigger
            // getExtremes. (TODO: Is this really the case?)
            return 0;
      } else
         // Nothing can be done now, but the next allocation will
         // trigger getExtremes. (TODO: Is this really the case?)
         return 0;
   }
}

OutOfFlowMgr::TBInfo *OutOfFlowMgr::getTextblock (Textblock *textblock)
{
   TypedPointer<Textblock> key (textblock);
   TBInfo *tbInfo = tbInfosByTextblock->get (&key);
   assert (tbInfo);
   return tbInfo;
}
  
/**
 * Get the left border for the vertical position of *y*, for a height
 * of *h", based on floats; relative to the allocation of the calling
 * textblock.
 *
 * The border includes marging/border/padding of the calling textblock
 * but is 0 if there is no float, so a caller should also consider
 * other borders.
 */
int OutOfFlowMgr::getLeftBorder (Textblock *textblock, int y, int h,
                                 Textblock *lastGB, int lastExtIndex)
{
   int b = getBorder (textblock, LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "left border (%p, %d, %d, %p, %d) => %d",
                 textblock, y, h, lastGB, lastExtIndex, b);
   return b;
}

/**
 * Get the right border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * See also getLeftBorder(int, int);
 */
int OutOfFlowMgr::getRightBorder (Textblock *textblock, int y, int h,
                                  Textblock *lastGB, int lastExtIndex)
{
   int b = getBorder (textblock, RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "right border (%p, %d, %d, %p, %d) => %d",
                 textblock, y, h, lastGB, lastExtIndex, b);
   return b;
}

int OutOfFlowMgr::getBorder (Textblock *textblock, Side side, int y, int h,
                             Textblock *lastGB, int lastExtIndex)
{
   DBG_OBJ_MSGF ("border", 0, "<b>getBorder</b> (%p, %s, %d, %d, %p, %d)",
                 textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
                 lastGB, lastExtIndex);
   DBG_OBJ_MSG_START ();

   SortedFloatsVector *list = getFloatsListForTextblock (textblock, side);

   DBG_IF_RTFL {
      DBG_OBJ_MSG ("border", 1, "searching in list:");
      DBG_OBJ_MSG_START ();
      
      for (int i = 0; i < list->size(); i++) {
         Float *f = list->get(i);
         DBG_OBJ_MSGF ("border", 1,
                       "%d: (%p, i = %d/%d, y = %d/%d, s = (%d * (%d + %d), "
                       "%s, %s); widget at (%d, %d)",
                       i, f->getWidget (), f->index, f->sideSpanningIndex,
                       f->yReq, f->yReal, f->size.width, f->size.ascent,
                       f->size.descent, f->dirty ? "dirty" : "clean",
                       f->sizeChangedSinceLastAllocation ? "scsla" : "sNcsla",
                       f->getWidget()->getAllocation()->x,
                       f->getWidget()->getAllocation()->y);
      }
      
      DBG_OBJ_MSG_END ();
   }
   
   int first = list->findFirst (textblock, y, h, lastGB, lastExtIndex);
   
   DBG_OBJ_MSGF ("border", 1, "first = %d", first);
   
   if (first == -1) {
      // No float.
      DBG_OBJ_MSG_END ();
      return 0;
   } else {
      // It is not sufficient to find the first float, since a line
      // (with height h) may cover the region of multiple float, of
      // which the widest has to be choosen.
      int border = 0;
      bool covers = true;
      // TODO Also check against lastGB and lastExtIndex
      for (int i = first; covers && i < list->size(); i++) {
         Float *vloat = list->get(i);
         covers = vloat->covers (textblock, y, h);
         DBG_OBJ_MSGF ("border", 1, "float %d (%p) covers? %s.",
                       i, vloat->getWidget(), covers ? "<b>yes</b>" : "no");
         
         if (covers) {
            int thisBorder;
            if (vloat->generatingBlock == textblock) {
               int borderIn = side == LEFT ?
                  vloat->generatingBlock->getStyle()->boxOffsetX() :
                  vloat->generatingBlock->getStyle()->boxRestWidth();
               thisBorder = vloat->size.width + borderIn;
            } else {
               assert (wasAllocated (vloat->generatingBlock));
               assert (vloat->getWidget()->wasAllocated ());

               Allocation *tba = getAllocation(textblock),
                  *fla = vloat->getWidget()->getAllocation ();
               thisBorder = side == LEFT ?
                  fla->x + fla->width - tba->x :
                  tba->x + tba->width - fla->x;
            }

            border = max (border, thisBorder);
            DBG_OBJ_MSGF ("border", 1, "=> border = %d", border);
         }
      }
      
      DBG_OBJ_MSG_END ();
      return border;
   }
}


OutOfFlowMgr::SortedFloatsVector *OutOfFlowMgr::getFloatsListForTextblock
   (Textblock *textblock, Side side)
{
   if (wasAllocated (textblock))
      return side == LEFT ? leftFloatsCB : rightFloatsCB;
   else {
      TBInfo *tbInfo = getTextblock (textblock);
      return side == LEFT ? tbInfo->leftFloatsGB : tbInfo->rightFloatsGB;
   }
}


bool OutOfFlowMgr::hasFloatLeft (Textblock *textblock, int y, int h,
                                 Textblock *lastGB, int lastExtIndex)
{
   return hasFloat (textblock, LEFT, y, h, lastGB, lastExtIndex);
}

bool OutOfFlowMgr::hasFloatRight (Textblock *textblock, int y, int h,
                                  Textblock *lastGB, int lastExtIndex)
{
   return hasFloat (textblock, RIGHT, y, h, lastGB, lastExtIndex);
}

bool OutOfFlowMgr::hasFloat (Textblock *textblock, Side side, int y, int h,
                             Textblock *lastGB, int lastExtIndex)
{
   //printf ("[%p] hasFloat (%p, %s, %d, %d, %p, %d)\n",
   //        containingBlock, textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
   //        lastGB, lastExtIndex);
   SortedFloatsVector *list = getFloatsListForTextblock (textblock, side);
   return list->findFirst (textblock, y, h, lastGB, lastExtIndex) != -1;
}

/**
 * Returns position relative to the textblock "tb".
 */
int OutOfFlowMgr::getClearPosition (Textblock *tb)
{
   if (tb->getStyle()) {
      bool left = false, right = false;
      switch (tb->getStyle()->clear) {
      case core::style::CLEAR_NONE: break;
      case core::style::CLEAR_LEFT: left = true; break;
      case core::style::CLEAR_RIGHT: right = true; break;
      case core::style::CLEAR_BOTH: left = right = true; break;
      default: assertNotReached ();
      }
      
      return max (left ? getClearPosition (tb, LEFT) : 0,
                  right ? getClearPosition (tb, RIGHT) : 0);
   } else
      return 0;
}

int OutOfFlowMgr::getClearPosition (Textblock *tb, Side side)
{
   if (!wasAllocated (tb))
      // There is no relation yet to floats generated by other
      // textblocks, and this textblocks floats are unimportant for
      // the "clear" property.
      return 0;

   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;

   int i = list->findFloatIndex (tb, 0);
   if (i < 0)
      return 0;

   Float *vloat = list->get(i);
   // We pass this texblock and 0 (first word, or smallest external
   // index, respectively), but search for the last float before this
   // position. Therefore this check.
   if (vloat->generatingBlock== tb)
      i--;
   if (i < 0)
      return 0;

   vloat = list->get(i);
   ensureFloatSize (vloat);
   return
      vloat->yReal + vloat->size.ascent + vloat->size.descent -
      getAllocation(tb)->y;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty ||
       // If the size of the containing block has changed (represented
       // currently by the available width), a recalculation of a
       // relative float width may also be necessary.
       (isPerLength (vloat->getWidget()->getStyle()->width) &&
        vloat->cbAvailWidth != containingBlock->getAvailWidth ())) {
      DBG_OBJ_MSGF ("resize.floats", 0,
                    "<b>ensureFloatSize</b> (%p): recalculation",
                    vloat->getWidget ());
      DBG_OBJ_MSG_START ();

      // TODO Ugly. Soon to be replaced by cleaner code? See also
      // comment in Textblock::calcWidgetSize.
      if (vloat->getWidget()->usesHints ()) {
         if (isAbsLength (vloat->getWidget()->getStyle()->width))
            vloat->getWidget()->setWidth
               (absLengthVal (vloat->getWidget()->getStyle()->width));
         else if (isPerLength (vloat->getWidget()->getStyle()->width))
            vloat->getWidget()->setWidth
               (multiplyWithPerLength (containingBlock->getAvailWidth(),
                                       vloat->getWidget()->getStyle()->width));
      }

      // This is a bit hackish: We first request the size, then set
      // the available width (also considering the one of the
      // containing block, and the extremes of the float), then
      // request the size again, which may of course have a different
      // result. This is a fix for the bug:
      //
      //    Text in floats, which are wider because of an image, are
      //    broken at a too narrow width. Reproduce:
      //    test/floats2.html. After the image has been loaded, the
      //    text "Some text in a float." should not be broken
      //    anymore.
      //
      // If the call of setWidth not is neccessary, the second call
      // will read the size from the cache, so no redundant
      // calculation is necessary.

      // Furthermore, extremes are considered; especially, floats are too
      // wide, sometimes.
      Extremes extremes;
      vloat->getWidget()->getExtremes (&extremes);

      vloat->getWidget()->sizeRequest (&vloat->size);

      // Set width  ...
      int width = vloat->size.width;
      // Consider the available width of the containing block (when set):
      if (width > containingBlock->getAvailWidth())
         width = containingBlock->getAvailWidth();
      // Finally, consider extremes (as described above).
      if (width < extremes.minWidth)
          width = extremes.minWidth;
      if (width > extremes.maxWidth)
         width = extremes.maxWidth;
          
      vloat->getWidget()->setWidth (width);
      vloat->getWidget()->sizeRequest (&vloat->size);
      
      //printf ("   float %p (%s %p): %d x (%d + %d)\n",
      //        vloat, vloat->getWidget()->getClassName(), vloat->getWidget(),
      //        vloat->size.width, vloat->size.ascent, vloat->size.descent);
          
      vloat->cbAvailWidth = containingBlock->getAvailWidth ();
      vloat->dirty = false;

      DBG_OBJ_MSGF ("resize.floats", 1, "new size: %d * (%d + %d)",
                    vloat->size.width, vloat->size.ascent, vloat->size.descent);

      // "sizeChangedSinceLastAllocation" is reset in sizeAllocateEnd()

      DBG_OBJ_MSG_END ();
   }
}

void OutOfFlowMgr::getAbsolutelyPositionedSize (int *oofWidthAbsPos,
                                                int *oofHeightAbsPos)
{
   *oofWidthAbsPos = *oofHeightAbsPos = 0;

   for (int i = 0; i < absolutelyPositioned->size(); i++) {
      AbsolutelyPositioned *abspos = absolutelyPositioned->get (i);
      ensureAbsolutelyPositionedSizeAndPosition (abspos);
      *oofWidthAbsPos = max (*oofWidthAbsPos, abspos->xCB + abspos->width);
      *oofHeightAbsPos = max (*oofHeightAbsPos, abspos->yCB + abspos->height);
   }
}

void OutOfFlowMgr::ensureAbsolutelyPositionedSizeAndPosition
   (AbsolutelyPositioned *abspos)
{
   // TODO Similar to floats, changes of the available size of the
   // containing block are not noticed; which is a bit more severe as
   // for floats. Find a general solution for both floats and
   // absolutely positioned blocks.

   // TODO Compare to ensureFloatSize: some parts are
   // missing. Nevertheless, a simpler approach should be focussed on.

   if (abspos->dirty) {
      Style *style = abspos->widget->getStyle();
      int availWidth = containingBlock->getAvailWidth();
      int availHeight =
         containingBlock->getAvailAscent() + containingBlock->getAvailDescent();

      if (style->left == LENGTH_AUTO)
         abspos->xCB = 0;
      else
         abspos->xCB =
            calcValueForAbsolutelyPositioned (abspos, style->left, availWidth);
      
      if (style->top == LENGTH_AUTO)
         abspos->yCB = 0;
      else
         abspos->yCB =
            calcValueForAbsolutelyPositioned (abspos, style->top, availHeight);

      abspos->width = -1; // undefined
      if (style->width != LENGTH_AUTO)
         abspos->width = calcValueForAbsolutelyPositioned (abspos, style->width,
                                                           availWidth);
      else if (style->right != LENGTH_AUTO) {
         int right = calcValueForAbsolutelyPositioned (abspos, style->right,
                                                       availWidth);
         abspos->width = max (0, availWidth - (abspos->xCB + right));
      }

      abspos->height = -1; // undefined
      if (style->height != LENGTH_AUTO)
         abspos->height = calcValueForAbsolutelyPositioned (abspos,
                                                            style->height,
                                                            availHeight);
      else if (style->bottom != LENGTH_AUTO) {
         int bottom = calcValueForAbsolutelyPositioned (abspos, style->bottom,
                                                        availHeight);
         abspos->height = max (0, availHeight - (abspos->yCB + bottom));
      }

      if (abspos->width != -1)
         abspos->widget->setWidth (abspos->width);
      
      if (abspos->height != -1) {
         abspos->widget->setAscent (abspos->height);
         abspos->widget->setDescent (0); // TODO
      }

      if (abspos->width == -1 || abspos->height == -1) {
         Requisition req;
         abspos->widget->sizeRequest (&req);

         if (abspos->width == -1)
            abspos->width = req.width;

         if (abspos->height == -1)
            abspos->height = req.ascent + req.descent;
      }
            
      abspos->dirty = false;
   }
}

int OutOfFlowMgr::calcValueForAbsolutelyPositioned
   (AbsolutelyPositioned *abspos, Length styleLen, int refLen)
{
   assert (styleLen != LENGTH_AUTO);
   if (isAbsLength (styleLen))
      return absLengthVal (styleLen);
   else if (isPerLength (styleLen))
      return multiplyWithPerLength (refLen, styleLen);
   else {
      assertNotReached ();
      return 0; // compiler happiness
   }
}

void OutOfFlowMgr::sizeAllocateAbsolutelyPositioned ()
{
   for (int i = 0; i < absolutelyPositioned->size(); i++) {
      Allocation *cbAllocation = getAllocation(containingBlock);
      AbsolutelyPositioned *abspos = absolutelyPositioned->get (i);
      ensureAbsolutelyPositionedSizeAndPosition (abspos);

      Allocation childAllocation;
      childAllocation.x = cbAllocation->x + abspos->xCB;
      childAllocation.y = cbAllocation->y + abspos->yCB;
      childAllocation.width = abspos->width;
      childAllocation.ascent = abspos->height;
      childAllocation.descent = 0; // TODO
      
      abspos->widget->sizeAllocate (&childAllocation);

      printf ("[%p] allocating child %p at: (%d, %d), %d x (%d + %d)\n",
              containingBlock, abspos->widget, childAllocation.x,
              childAllocation.y, childAllocation.width, childAllocation.ascent,
              childAllocation.descent);
   }
}

// TODO Latest change: Check also Textblock::borderChanged: looks OK,
// but the comment ("... (i) with canvas coordinates ...") looks wrong
// (and looks as having always been wrong).

// Another issue: does it make sense to call Textblock::borderChanged
// for generators, when the respective widgets have not been called
// yet?

} // namespace dw
