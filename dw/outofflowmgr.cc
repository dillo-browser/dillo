/*
 * Dillo Widget
 *
 * Copyright 2013-2014 Sebastian Geerken <sgeerken@dillo.org>
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
   DBG_OBJ_MSGF_O ("resize.oofm", 0, widget,
                   "<b>update</b> (%s, %d, %d, %d, %d)",
                   wasAllocated ? "true" : "false", xCB, yCB, width, height);

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

// ----------------------------------------------------------------------

OutOfFlowMgr::Float::Float (OutOfFlowMgr *oofm, Widget *widget,
                            Textblock *generatingBlock, int externalIndex) :
   WidgetInfo (oofm, widget)
{
   this->generatingBlock = generatingBlock;
   this->externalIndex = externalIndex;

   yReq = yReal = size.width = size.ascent = size.descent = 0;
   dirty = sizeChangedSinceLastAllocation = true;
   indexGBList = indexCBList = -1;

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
   }
}

void OutOfFlowMgr::Float::updateAllocation ()
{
   DBG_OBJ_MSG_O ("resize.oofm", 0, getWidget (), "<b>updateAllocation</b> ()");
   DBG_OBJ_MSG_START_O (getWidget ());

   update (isNowAllocated (), getNewXCB (), getNewYCB (), getNewWidth (),
           getNewHeight ());

   DBG_OBJ_MSG_END_O (getWidget ());
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

   sb->append (", indexGBList = ");
   sb->appendInt (indexGBList);
   sb->append (", indexCBList = ");
   sb->appendInt (indexCBList);
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
            //*gba = getOutOfFlowMgr()->getAllocation(generatingBlock),
            *fla = getWidget()->getAllocation ();
         int reqyCanv = tba->y + y;
         int flyCanv = fla->y;
         int flh = fla->ascent + fla->descent;
         b = flyCanv + flh > reqyCanv && flyCanv < reqyCanv + h;

         DBG_OBJ_MSGF_O ("border", 1, getOutOfFlowMgr (),
                         "not generator (allocated): reqyCanv = %d + %d = %d, "
                         "flyCanv = %d, flh = %d + %d = %d => %s",
                         tba->y, y, reqyCanv, flyCanv,
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
                   fl1->getIndex (type), fl2->getIndex (type), refTB);
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
            oofm->getAllocation(fl1->generatingBlock)->y + fl1->yReal;
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
   int r = -123; // Compiler happiness: GCC 4.7 does not handle this?;

   DBG_OBJ_MSGF_O ("border", 1, oofm,
                   "<b>CompareGBAndExtIndex::compare</b> (#%d -> %p/%d, "
                   "#%d -> %p/#%d)",
                   f1->getIndex (type), f1->generatingBlock, f1->externalIndex,
                   f2->getIndex (type), f2->generatingBlock, f2->externalIndex);
   DBG_OBJ_MSG_START_O (oofm);

   if (f1->generatingBlock == f2->generatingBlock) {
      r = f1->externalIndex - f2->externalIndex;
      DBG_OBJ_MSGF_O ("border", 2, oofm,
                      "(a) generating blocks equal => %d - %d = %d",
                      f1->externalIndex, f2->externalIndex, r);
   } else {
      TBInfo *t1 = oofm->getTextblock (f1->generatingBlock),
         *t2 = oofm->getTextblock (f2->generatingBlock);
      bool rdef = false;

      for (TBInfo *t = t1; t != NULL; t = t->parent)
         if (t->parent == t2) {
            rdef = true;
            r = t->parentExtIndex - f2->externalIndex;
            DBG_OBJ_MSGF_O ("border", 2, oofm,
                            "(b) %p is an achestor of %p; direct child is "
                            "%p (%d) => %d - %d = %d\n",
                            t2->getTextblock (), t1->getTextblock (),
                            t->getTextblock (), t->parentExtIndex,
                            t->parentExtIndex, f2->externalIndex, r);
         }

      for (TBInfo *t = t2; !rdef && t != NULL; t = t->parent)
         if (t->parent == t1) {
            r = f1->externalIndex - t->parentExtIndex;
            rdef = true;
            DBG_OBJ_MSGF_O ("border", 2, oofm,
                            "(c) %p is an achestor of %p; direct child is %p "
                            "(%d) => %d - %d = %d\n",
                            t1->getTextblock (), t2->getTextblock (),
                            t->getTextblock (), t->parentExtIndex,
                            f1->externalIndex, t->parentExtIndex, r);
         }

      if (!rdef) {
         r = t1->index - t2->index;
         DBG_OBJ_MSGF_O ("border", 2, oofm, "(d) other => %d - %d = %d",
                         t1->index, t2->index, r);
      }
   }

   DBG_OBJ_MSGF_O ("border", 2, oofm, "result: %d", r);
   DBG_OBJ_MSG_END_O (oofm);
   return r;
}

int OutOfFlowMgr::SortedFloatsVector::findFloatIndex (Textblock *lastGB,
                                                      int lastExtIndex)
{
   DBG_OBJ_MSGF_O ("border", 0, oofm, "<b>findFloatIndex</b> (%p, %d)",
                   lastGB, lastExtIndex);
   DBG_OBJ_MSG_START_O (oofm);

   Float key (oofm, NULL, lastGB, lastExtIndex);
   key.setIndex (type, -1); // for debugging
   Float::CompareGBAndExtIndex comparator (oofm, type);
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

   DBG_OBJ_MSGF_O ("border", 1, oofm, "=> r = %d", r);
   DBG_OBJ_MSG_END_O (oofm);
   return r;
}

int OutOfFlowMgr::SortedFloatsVector::find (Textblock *textblock, int y,
                                            int start, int end)
{
   DBG_OBJ_MSGF_O ("border", 0, oofm, "<b>find</b> (%p, %d, %d, %d)",
                   textblock, y, start, end);
   DBG_OBJ_MSG_START_O (oofm);

   Float key (oofm, NULL, NULL, 0);
   key.generatingBlock = textblock;
   key.yReal = y;
   key.setIndex (type, -1); // for debugging
   Float::ComparePosition comparator (oofm, textblock, type);
   int result = bsearch (&key, false, start, end, &comparator);

   DBG_OBJ_MSGF_O ("border", 1, oofm, "=> result = %d", result);
   DBG_OBJ_MSG_END_O (oofm);
   return result;
}

int OutOfFlowMgr::SortedFloatsVector::findFirst (Textblock *textblock,
                                                 int y, int h,
                                                 Textblock *lastGB,
                                                 int lastExtIndex)
{
   DBG_OBJ_MSGF_O ("border", 0, oofm, "<b>findFirst</b> (%p, %d, %d, %p, %d)",
                   textblock, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSG_START_O (oofm);

   DBG_IF_RTFL {
      DBG_OBJ_MSG_O ("border", 2, oofm, "searching in list:");
      DBG_OBJ_MSG_START_O (oofm);

      for (int i = 0; i < size(); i++) {
         DBG_OBJ_MSGF_O ("border", 2, oofm,
                         "%d: (%p, i = %d/%d, y = %d/%d, s = (%d * (%d + %d)), "
                         "%s, %s, ext = %d, GB = %p); widget at (%d, %d)",
                         i, get(i)->getWidget (), get(i)->getIndex (type),
                         get(i)->sideSpanningIndex, get(i)->yReq, get(i)->yReal,
                         get(i)->size.width, get(i)->size.ascent,
                         get(i)->size.descent,
                         get(i)->dirty ? "dirty" : "clean",
                         get(i)->sizeChangedSinceLastAllocation ? "scsla"
                         : "sNcsla",
                         get(i)->externalIndex, get(i)->generatingBlock,
                         get(i)->getWidget()->getAllocation()->x,
                         get(i)->getWidget()->getAllocation()->y);
      }

      DBG_OBJ_MSG_END_O (oofm);
   }

   int last = findFloatIndex (lastGB, lastExtIndex);
   DBG_OBJ_MSGF_O ("border", 1, oofm, "last = %d", last);
   assert (last < size());
   int i = find (textblock, y, 0, last), result;
   DBG_OBJ_MSGF_O ("border", 1, oofm, "i = %d", i);

   // Note: The smallest value of "i" is 0, which means that "y" is before or
   // equal to the first float. The largest value is "last + 1", which means
   // that "y" is after the last float. In both cases, the first or last,
   // respectively, float is a candidate. Generally, both floats, before and
   // at the search position, are candidates.

   if (i > 0 && get(i - 1)->covers (textblock, y, h))
      result = i - 1;
   else if (i <= last && get(i)->covers (textblock, y, h))
      result = i;
   else
      result = -1;

   DBG_OBJ_MSGF_O ("border", 1, oofm, "=> result = %d", result);
   DBG_OBJ_MSG_END_O (oofm);
   return result;
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
   vloat->setIndex (type, size() - 1);
}

OutOfFlowMgr::TBInfo::TBInfo (OutOfFlowMgr *oofm, Textblock *textblock,
                              TBInfo *parent, int parentExtIndex) :
   WidgetInfo (oofm, textblock)
{
   this->parent = parent;
   this->parentExtIndex = parentExtIndex;

   leftFloatsGB = new SortedFloatsVector (oofm, LEFT, GB);
   rightFloatsGB = new SortedFloatsVector (oofm, RIGHT, GB);

   wasAllocated = getWidget()->wasAllocated ();
   allocation = *(getWidget()->getAllocation ());
}

OutOfFlowMgr::TBInfo::~TBInfo ()
{
   delete leftFloatsGB;
   delete rightFloatsGB;
}

void OutOfFlowMgr::TBInfo::updateAllocation ()
{
   DBG_OBJ_MSG_O ("resize.oofm", 0, getWidget (), "<b>updateAllocation</b> ()");
   DBG_OBJ_MSG_START_O (getWidget ());

   update (isNowAllocated (), getNewXCB (), getNewYCB (), getNewWidth (),
           getNewHeight ());

   DBG_OBJ_MSG_END_O (getWidget ());
}

OutOfFlowMgr::AbsolutelyPositioned::AbsolutelyPositioned (OutOfFlowMgr *oofm,
                                                          Widget *widget,
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

   leftFloatsCB = new SortedFloatsVector (this, LEFT, CB);
   rightFloatsCB = new SortedFloatsVector (this, RIGHT, CB);

   leftFloatsAll = new Vector<Float> (1, true);
   rightFloatsAll = new Vector<Float> (1, true);

   DBG_OBJ_SET_NUM ("leftFloatsAll.size", leftFloatsAll->size());
   DBG_OBJ_SET_NUM ("rightFloatsAll.size", rightFloatsAll->size());

   floatsByWidget = new HashTable <TypedPointer <Widget>, Float> (true, false);

   tbInfos = new Vector<TBInfo> (1, false);
   tbInfosByTextblock =
      new HashTable <TypedPointer <Textblock>, TBInfo> (true, true);

   leftFloatsMark = rightFloatsMark = 0;
   lastLeftTBIndex = lastRightTBIndex = 0;

   absolutelyPositioned = new Vector<AbsolutelyPositioned> (1, true);

   containingBlockWasAllocated = containingBlock->wasAllocated ();
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

   DBG_OBJ_DELETE ();
}

void OutOfFlowMgr::sizeAllocateStart (Textblock *caller, Allocation *allocation)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>sizeAllocateStart</b> (%p, (%d, %d, %d * (%d + %d)))",
                 caller, allocation->x, allocation->y, allocation->width,
                 allocation->ascent, allocation->descent);
   DBG_OBJ_MSG_START ();

   getTextblock(caller)->allocation = *allocation;
   getTextblock(caller)->wasAllocated = true;

   if (caller == containingBlock) {
      // In the size allocation process, the *first* OOFM method
      // called is sizeAllocateStart, with the containing block as an
      // argument. So this is the correct point to initialize size
      // allocation.

      containingBlockAllocation = *allocation;
      containingBlockWasAllocated = true;

      // Move floats from GB lists to the one CB list.
      moveFromGBToCB (LEFT);
      moveFromGBToCB (RIGHT);

      // These attributes are used to keep track which floats have
      // been allocated (referring to leftFloatsCB and rightFloatsCB).
      lastAllocatedLeftFloat = lastAllocatedRightFloat = -1;
   }

   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::sizeAllocateEnd (Textblock *caller)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>sizeAllocateEnd</b> (%p)", caller);
   DBG_OBJ_MSG_START ();

   // (Later, absolutely positioned blocks have to be allocated.)

   if (caller != containingBlock) {
      // Allocate all floats "before" this textblock.
      sizeAllocateFloats (LEFT, leftFloatsCB->findFloatIndex (caller, -1));
      sizeAllocateFloats (RIGHT, rightFloatsCB->findFloatIndex (caller, -1));
   }

   if (caller == containingBlock) {
      // In the size allocation process, the *last* OOFM method called
      // is sizeAllocateEnd, with the containing block as an
      // argument. So this is the correct point to finish size
      // allocation.

      // Allocate all remaining floats.
      sizeAllocateFloats (LEFT, leftFloatsCB->size () - 1);
      sizeAllocateFloats (RIGHT, rightFloatsCB->size () - 1);

      // Check changes of both textblocks and floats allocation. (All
      // is checked by hasRelationChanged (...).)
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

      // There are cases where some allocated floats (TODO: later also
      // absolutely positioned elements?) exceed the CB allocation.
      bool sizeChanged = doFloatsExceedCB (LEFT) || doFloatsExceedCB (RIGHT);

      // Similar for extremes. (TODO: here also absolutely positioned
      // elements?)
      bool extremesChanged =
         haveExtremesChanged (LEFT) || haveExtremesChanged (RIGHT);

      for (int i = 0; i < leftFloatsCB->size(); i++)
         leftFloatsCB->get(i)->updateAllocation ();

      for (int i = 0; i < rightFloatsCB->size(); i++)
         rightFloatsCB->get(i)->updateAllocation ();

      if (sizeChanged || extremesChanged)
         containingBlock->oofSizeChanged (extremesChanged);
   }

   DBG_OBJ_MSG_END ();
}

bool OutOfFlowMgr::hasRelationChanged (TBInfo *tbInfo, int *minFloatPos,
                                       Widget **minFloat)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
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
      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "has changed: minFloatPos = %d, minFloat = %p",
                    *minFloatPos, *minFloat);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "has not changed");

   DBG_OBJ_MSG_END ();
   return c1 || c2;
}

bool OutOfFlowMgr::hasRelationChanged (TBInfo *tbInfo, Side side,
                                       int *minFloatPos, Widget **minFloat)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
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
         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "not checking (generating!) textblock %p against float "
                       "%p", tbInfo->getWidget (), vloat->getWidget ());
      else {
         Allocation *gba = getAllocation (vloat->generatingBlock);

         int newFlx =
            calcFloatX (vloat, side,
                        gba->x - containingBlockAllocation.x,
                        gba->width, vloat->generatingBlock->getAvailWidth ());
         int newFly = vloat->generatingBlock->getAllocation()->y
            - containingBlockAllocation.y + vloat->yReal;

         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "checking textblock %p against float %p",
                       tbInfo->getWidget (), vloat->getWidget ());
         DBG_OBJ_MSG_START ();

         if (hasRelationChanged (tbInfo->wasThenAllocated (),
                                 tbInfo->getOldXCB (), tbInfo->getOldYCB (),
                                 tbInfo->getNewWidth (),
                                 tbInfo->getNewHeight (),
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
            DBG_OBJ_MSG ("resize.oofm", 0, "No.");

         DBG_OBJ_MSG_END ();
      }

      // All floarts are searched, to find the minimum. TODO: Are
      // floats sorted, so this can be shortened? (The first is the
      // minimum?)
   }

   if (changed)
      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "has changed: minFloatPos = %d, minFloat = %p",
                    *minFloatPos, *minFloat);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "has not changed");

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
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>hasRelationChanged</b> (<i>see below</i>, %s, ...)",
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   if (oldTBAlloc)
      DBG_OBJ_MSGF ("resize.oofm", 1, "old TB: %d, %d; %d * %d",
                    oldTBx, oldTBy, oldTBw, oldTBh);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "old TB: undefined");
   DBG_OBJ_MSGF ("resize.oofm", 1, "new TB: %d, %d; %d * %d",
                 newTBx, newTBy, newTBw, newTBh);

   if (oldFlAlloc)
      DBG_OBJ_MSGF ("resize.oofm", 1, "old Fl: %d, %d; %d * %d",
                    oldFlx, oldFly, oldFlw, oldFlh);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "old Fl: undefined");
   DBG_OBJ_MSGF ("resize.oofm", 1, "new Fl: %d, %d; %d * %d",
                 newFlx, newFly, newFlw, newFlh);

   bool result;
   if (oldTBAlloc && oldFlAlloc) {
      bool oldCov = oldFly + oldFlh > oldTBy && oldFly < oldTBy + oldTBh;
      bool newCov = newFly + newFlh > newTBy && newFly < newTBy + newTBh;

      DBG_OBJ_MSGF ("resize.oofm", 1, "covered? then: %s, now: %s.",
                    oldCov ? "yes" : "no", newCov ? "yes" : "no");
      DBG_OBJ_MSG_START ();

      if (oldCov && newCov) {
         int yOld = oldFly - oldTBy, yNew = newFly - newTBy;
         if (yOld == yNew) {
            DBG_OBJ_MSGF ("resize.oofm", 2,
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

            DBG_OBJ_MSGF ("resize.oofm", 2, "wOld = %d, wNew = %d\n",
                          wOld, wNew);

            if (wOld == wNew) {
               if (oldFlh == newFlh)
                  result = false;
               else {
                  // Only heights of floats changed. Relevant only
                  // from bottoms of float.
                  *floatPos = min (yOld + oldFlh, yNew + newFlh);
                  result = true;
               }
            } else {
               *floatPos = yOld;
               result = true;
            }
         } else {
            DBG_OBJ_MSGF ("resize.oofm", 2,
                          "old (%d - %d = %d) and new (%d - %d = %d) position "
                          "different",
                          oldFly, oldTBy, yOld, newFly, newTBy, yNew);
            *floatPos = min (yOld, yNew);
            result = true;
         }
      } else if (oldCov) {
         *floatPos = oldFly - oldTBy;
         result = true;
         DBG_OBJ_MSGF ("resize.oofm", 2,
                       "returning old position: %d - %d = %d", oldFly, oldTBy,
                       *floatPos);
      } else if (newCov) {
         *floatPos = newFly - newTBy;
         result = true;
         DBG_OBJ_MSGF ("resize.oofm", 2,
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
      DBG_OBJ_MSGF ("resize.oofm", 1, "has changed: floatPos = %d",
                    *floatPos);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "has not changed");

   DBG_OBJ_MSG_END ();

   return result;
}

bool OutOfFlowMgr::doFloatsExceedCB (Side side)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>doFloatsExceedCB</b> (%s)",
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();
   
   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   bool exceeds = false;

   DBG_OBJ_MSG_START ();

   for (int i = 0; i < list->size () && !exceeds; i++) {
      Float *vloat = list->get (i);
      if (vloat->getWidget()->wasAllocated ()) {
         Allocation *fla = vloat->getWidget()->getAllocation ();
         DBG_OBJ_MSGF ("resize.oofm", 2,
                       "Does FlA = (%d, %d, %d * %d) exceed CBA = "
                       "(%d, %d, %d * %d)?",
                       fla->x, fla->y, fla->width, fla->ascent + fla->descent,
                       containingBlockAllocation.x, containingBlockAllocation.y,
                       containingBlockAllocation.width,
                       containingBlockAllocation.ascent
                       + containingBlockAllocation.descent);
         if (fla->x + fla->width >
             containingBlockAllocation.x + containingBlockAllocation.width ||
             fla->y + fla->ascent + fla->descent >
             containingBlockAllocation.y + containingBlockAllocation.ascent
             + containingBlockAllocation.descent) {
            exceeds = true;
            DBG_OBJ_MSG ("resize.oofm", 2, "Yes.");
         } else
            DBG_OBJ_MSG ("resize.oofm", 2, "No.");
      }
   }

   DBG_OBJ_MSG_END ();

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", exceeds ? "true" : "false");
   DBG_OBJ_MSG_END ();

   return exceeds;
}

bool OutOfFlowMgr::haveExtremesChanged (Side side)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>haveExtremesChanged</b> (%s)",
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   // This is quite different from doFloatsExceedCB, since there is no
   // counterpart to getExtremes, as sizeAllocate is a counterpart to
   // sizeRequest. So we have to determine whether the allocation has
   // changed the extremes, which is done by examining the part of the
   // allocation which is part of the extremes calculation (see
   // getFloatsExtremes). Changes of the extremes are handled by the
   // normal queueResize mechanism.

   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   bool changed = false;

   for (int i = 0; i < list->size () && !changed; i++) {
      Float *vloat = list->get (i);
      // When the GB is the CB, an allocation change does not play a
      // role here.
      if (vloat->generatingBlock != containingBlock) {
         if (!vloat->wasThenAllocated () && vloat->isNowAllocated ())
            changed = true;
         else {
            // This method is called within sizeAllocateEnd, where
            // containinBlock->getAllocation() (old value) and
            // containinBlockAllocation (new value) are different.

            Allocation *oldCBA = containingBlock->getAllocation ();
            Allocation *newCBA = &containingBlockAllocation;

            // Compare also to getFloatsExtremes. The box difference
            // of the GB (from style) has not changed in this context,
            // so it is ignored.

            int oldDiffLeft = vloat->getOldXCB ();
            int newDiffLeft = vloat->getNewXCB ();
            int oldDiffRight =
               oldCBA->width - (vloat->getOldXCB () + vloat->getOldWidth ());
            int newDiffRight =
               newCBA->width - (vloat->getNewXCB () + vloat->getNewWidth ());

            if (// regarding minimum
                (side == LEFT && oldDiffLeft != newDiffLeft) ||
                (side == RIGHT && oldDiffRight != newDiffRight) ||
                // regarding maximum
                oldDiffLeft + oldDiffRight != newDiffLeft + newDiffRight)
               changed = true;
         }
      }
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", changed ? "true" : "false");
   DBG_OBJ_MSG_END ();

   return changed;
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
            Float *vloat = src->get (i);
            // "vloat->indexCBList == -1": prevent copying the vloat twice.
            if (vloat->indexCBList == -1 && vloat->mark == mark) {
               dest->put (vloat);
               //printf("[%p] moving %s float %p (%s %p, mark %d) to CB list\n",
               //       containingBlock, side == LEFT ? "left" : "right",
               //       vloat, vloat->widget->getClassName(), vloat->widget,
               //       vloat->mark);
            }
         }
      }

   *floatsMark = 0;

   //printf ("[%p] new %s list:\n",
   //        containingBlock, side == LEFT ? "left" : "right");
   //for (int i = 0; i < dest->size(); i++)
   //   printf ("   %d: %s\n", i, dest->get(i)->toString());
}

void OutOfFlowMgr::sizeAllocateFloats (Side side, int newLastAllocatedFloat)
{
   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   int *lastAllocatedFloat =
      side == LEFT ? &lastAllocatedLeftFloat : &lastAllocatedRightFloat;

   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>sizeAllocateFloats</b> (%s, [%d ->] %d [size = %d])",
                 side == LEFT ? "LEFT" : "RIGHT", *lastAllocatedFloat,
                 newLastAllocatedFloat, list->size ());
   DBG_OBJ_MSG_START ();

   Allocation *cba = &containingBlockAllocation;

   for (int i = *lastAllocatedFloat + 1; i <= newLastAllocatedFloat; i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);

      Allocation *gba = getAllocation (vloat->generatingBlock);
      int availWidth = vloat->generatingBlock->getAvailWidth();

      Allocation childAllocation;
      childAllocation.x = cba->x +
         calcFloatX (vloat, side, gba->x - cba->x, gba->width, availWidth);
      childAllocation.y = gba->y + vloat->yReal;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;

      vloat->getWidget()->sizeAllocate (&childAllocation);
   }

   *lastAllocatedFloat = newLastAllocatedFloat;

   DBG_OBJ_MSG_END ();
}


/**
 * \brief ...
 *
 * gbX is given relative to the CB, as is the return value.
 */
int OutOfFlowMgr::calcFloatX (Float *vloat, Side side, int gbX, int gbWidth,
                              int gbAvailWidth)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>calcFloatX</b> (%p, %s, %d, %d, %d)",
                 vloat->getWidget (), side == LEFT ? "LEFT" : "RIGHT", gbX,
                 gbWidth, gbAvailWidth);
   DBG_OBJ_MSG_START ();

   int gbActualWidth, x;

   switch (side) {
   case LEFT:
      // Left floats are always aligned on the left side of the
      // generator (content, not allocation).
      x = gbX + vloat->generatingBlock->getStyle()->boxOffsetX();
      DBG_OBJ_MSGF ("resize.oofm", 1, "left: x = %d + %d = %d",
                    gbX, vloat->generatingBlock->getStyle()->boxOffsetX(), x);
      break;

   case RIGHT:
      // In some cases, the actual (allocated) width is too large; we
      // use the "available" width here.
      gbActualWidth = min (gbWidth, gbAvailWidth);
      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "right: gbActualWidth = min (%d, %d) = %d",
                    gbWidth, gbAvailWidth, gbActualWidth);

      // Similar for right floats, but in this case, floats are
      // shifted to the right when they are too big (instead of
      // shifting the generator to the right).
      x = max (gbX + gbActualWidth - vloat->size.width
               - vloat->generatingBlock->getStyle()->boxRestWidth(),
               // Do not exceed CB allocation:
               0);
      DBG_OBJ_MSGF ("resize.oofm", 1, "x = max (%d + %d - %d - %d, 0) = %d",
                    gbX, gbActualWidth, vloat->size.width,
                    vloat->generatingBlock->getStyle()->boxRestWidth(), x);
      break;

   default:
      assertNotReached ();
      x = 0;
      break;
   }

   DBG_OBJ_MSG_END ();
   return x;
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
      Rectangle childArea;
      if (vloat->getWidget()->intersects (area, &childArea))
         vloat->getWidget()->draw (view, &childArea);
   }
}

void OutOfFlowMgr::drawAbsolutelyPositioned (View *view, Rectangle *area)
{
   for (int i = 0; i < absolutelyPositioned->size(); i++) {
      AbsolutelyPositioned *abspos = absolutelyPositioned->get(i);
      Rectangle childArea;
      if (abspos->widget->intersects (area, &childArea))
         abspos->widget->draw (view, &childArea);
   }
}

/**
 * This method consideres also the attributes not yet considered by
 * dillo, so that the containing block is determined correctly, which
 * leads sometimes to a cleaner rendering.
 */
bool OutOfFlowMgr::isWidgetOutOfFlow (Widget *widget)
{  
   // This is only half-baked, will perhaps be reactivated:
   //
   //return
   //   widget->getStyle()->vloat != FLOAT_NONE ||
   //   widget->getStyle()->position == POSITION_ABSOLUTE ||
   //   widget->getStyle()->position == POSITION_FIXED;

   return isWidgetHandledByOOFM (widget);
}

bool OutOfFlowMgr::isWidgetHandledByOOFM (Widget *widget)
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
         DBG_OBJ_SET_NUM ("leftFloatsAll.size", leftFloatsAll->size());
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
         DBG_OBJ_SET_NUM ("rightFloatsAll.size", rightFloatsAll->size());
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
      if (vloat->externalIndex >= oldStartIndex) {
         vloat->externalIndex += diff;
         DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.externalIndex",
                            vloat->externalIndex);
      }
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
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>markSizeChange</b> (%d)", ref);
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
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>tellFloatPosition</b> (%p, %d)",
                 widget, yReq);
   DBG_OBJ_MSG_START ();

   assert (yReq >= 0);

   Float *vloat = findFloatByWidget(widget);

   SortedFloatsVector *listSame, *listOpp;
   Side side;
   getFloatsListsAndSide (vloat, &listSame, &listOpp, &side);
   ensureFloatSize (vloat);

   // "yReal" may change due to collisions (see below).
   vloat->yReq = vloat->yReal = yReq;

   DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReq", vloat->yReq);
   DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);

   // Test collisions (on this side). Only previous float is relevant.
   int index = vloat->getIndex (listSame->type), yRealNew;
   if (index >= 1 &&
       collidesV (vloat, listSame->get (index - 1), listSame->type,
                  &yRealNew)) {
      vloat->yReal = yRealNew;
      DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);
   }

   // Test collisions (on the opposite side). Search the last float on
   // the other size before this float; only this is relevant.
   int lastOppFloat =
      listOpp->findLastBeforeSideSpanningIndex (vloat->sideSpanningIndex);
   if (lastOppFloat >= 0 &&
       collidesH (vloat, listOpp->get (lastOppFloat), listSame->type,
                  &yRealNew)) {
      vloat->yReal = yRealNew;
      DBG_OBJ_SET_NUM_O (vloat->getWidget (), "<Float>.yReal", vloat->yReal);
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "vloat->yReq = %d, vloat->yReal = %d",
                 vloat->yReq, vloat->yReal);

   DBG_OBJ_MSG_END ();
}

bool OutOfFlowMgr::collidesV (Float *vloat, Float *other, SFVType type,
                              int *yReal)
{
   // Only checks vertical (possible) collisions, and only refers to
   // vloat->yReal; never to vloat->allocation->y, even when the GBs are
   // different. Used only in tellPosition.

   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>collides</b> (#%d [%p], #%d [%p], ...)",
                 vloat->getIndex (type), vloat->getWidget (),
                 other->getIndex (type), other->getWidget ());
   DBG_OBJ_MSG_START ();

   bool result;

   DBG_OBJ_MSGF ("resize.oofm", 1, "initial yReal = %d", vloat->yReal);

   if (vloat->generatingBlock == other->generatingBlock) {
      ensureFloatSize (other);
      int otherBottomGB =
         other->yReal + other->size.ascent + other->size.descent;

      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "same generators: otherBottomGB = %d + (%d + %d) = %d",
                    other->yReal, other->size.ascent, other->size.descent,
                    otherBottomGB);

      if (vloat->yReal <  otherBottomGB) {
         *yReal = otherBottomGB;
         result = true;
      } else
         result = false;
   } else {
      assert (wasAllocated (vloat->generatingBlock));
      assert (wasAllocated (other->generatingBlock));

      // If the other float is not allocated, there is no collision. The
      // allocation of this float (vloat) is not used at all.
      if (!other->getWidget()->wasAllocated ())
         result = false;
      else {
         Allocation *gba = getAllocation (vloat->generatingBlock),
            *flaOther = other->getWidget()->getAllocation ();
         int otherBottomGB =
            flaOther->y + flaOther->ascent + flaOther->descent - gba->y;

         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "different generators: "
                       "otherBottomGB = %d + (%d + %d) - %d = %d",
                       flaOther->y, flaOther->ascent, flaOther->descent, gba->y,
                       otherBottomGB);

         if (vloat->yReal <  otherBottomGB) {
            *yReal = otherBottomGB;
            result = true;
         } else
            result = false;
      }
   }

   if (result)
      DBG_OBJ_MSGF ("resize.oofm", 1, "collides: new yReal = %d", *yReal);
   else
      DBG_OBJ_MSG ("resize.oofm", 1, "does not collide");

   DBG_OBJ_MSG_END ();
   return result;
}


bool OutOfFlowMgr::collidesH (Float *vloat, Float *other, SFVType type,
                              int *yReal)
{
   bool collidesH;

   if (!collidesV (vloat, other, type, yReal))
      collidesH = false;
   else {
      if (vloat->generatingBlock == other->generatingBlock)
         collidesH = vloat->size.width + other->size.width
            + vloat->generatingBlock->getStyle()->boxDiffWidth()
            > vloat->generatingBlock->getAvailWidth();
      else {
         assert (wasAllocated (vloat->generatingBlock));
         assert (wasAllocated (other->generatingBlock));

         // Again, if the other float is not allocated, there is no
         // collision. Compare to collidesV. (But vloat->size is used
         // here.)
         if (!other->getWidget()->wasAllocated ())
            collidesH = false;
         else {
            Allocation *gba = getAllocation (vloat->generatingBlock);
            int vloatX =
               calcFloatX (vloat,
                           vloat->getWidget()->getStyle()->vloat == FLOAT_LEFT ?
                           LEFT : RIGHT,
                           gba->x, gba->width,
                           vloat->generatingBlock->getAvailWidth ());

            // Generally: right border of the left float > left border
            // of the right float (all in canvas coordinates).
            if (vloat->getWidget()->getStyle()->vloat == FLOAT_LEFT)
               // "vloat" is left, "other" is right
               collidesH = vloatX + vloat->size.width
                  > other->getWidget()->getAllocation()->x;
            else
               // "other" is left, "vloat" is right
               collidesH = other->getWidget()->getAllocation()->x
                  + other->getWidget()->getAllocation()->width
                  > vloatX;
         }
      }
   }

   return collidesH;
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

void OutOfFlowMgr::getSize (Requisition *cbReq, int *oofWidth, int *oofHeight)
{
   DBG_OBJ_MSG ("resize.oofm", 0, "<b>getSize</b> ()");
   DBG_OBJ_MSG_START ();

   int oofWidthAbsPos, oofHeightAbsPos;
   getAbsolutelyPositionedSize (cbReq, &oofWidthAbsPos, &oofHeightAbsPos);

   int oofWidthtLeft, oofWidthRight, oofHeightLeft, oofHeightRight;
   getFloatsSize (cbReq, LEFT, &oofWidthtLeft, &oofHeightLeft);
   getFloatsSize (cbReq, RIGHT, &oofWidthRight, &oofHeightRight);

   *oofWidth = max (oofWidthtLeft, oofWidthRight, oofWidthAbsPos);
   *oofHeight = max (oofHeightLeft, oofHeightRight, oofHeightAbsPos);

   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> (a: %d, l: %d, r: %d => %d) * (a: %d, l: %d, r: %d => %d)",
                 oofWidthAbsPos, oofWidthtLeft, oofWidthRight, *oofWidth,
                 oofHeightAbsPos, oofHeightLeft, oofHeightRight, *oofHeight);
   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::getFloatsSize (Requisition *cbReq, Side side, int *width,
                                  int *height)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>getFloatsSize</b> ((%d * (%d + %d), %s, ...)",
                 cbReq->width, cbReq->ascent, cbReq->descent,
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   SortedFloatsVector *list = getFloatsListForTextblock (containingBlock, side);

   *width = *height = 0;

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      if (vloat->generatingBlock == containingBlock ||
          wasAllocated (vloat->generatingBlock)) {
         ensureFloatSize (vloat);
         int x, y;

         if (vloat->generatingBlock == containingBlock) {
            x = calcFloatX (vloat, side, 0, cbReq->width,
                            vloat->generatingBlock->getAvailWidth ());
            y = vloat->yReal;
         } else {
            Allocation *gba = getAllocation(vloat->generatingBlock);
            x = calcFloatX (vloat, side,
                            gba->x - containingBlockAllocation.x, gba->width,
                            vloat->generatingBlock->getAvailWidth ());
            y = gba->y - containingBlockAllocation.y + vloat->yReal;
         }

         *width = max (*width, x +  vloat->size.width);
         *height = max (*height, y + vloat->size.ascent + vloat->size.descent);

         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "considering float %p generated by %p: (%d + %d) * "
                       "(%d + (%d + %d)) => %d * %d",
                       vloat->getWidget (), vloat->generatingBlock,
                       x, vloat->size.width,
                       y, vloat->size.ascent, vloat->size.descent,
                       *width, *height);
      } else
         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "considering float %p generated by %p: not allocated",
                       vloat->getWidget (), vloat->generatingBlock);
   }

   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::getExtremes (Extremes *cbExtr, int *oofMinWidth,
                                int *oofMaxWidth)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>getExtremes</b> ((%d / %d), ...)",
                 cbExtr->minWidth, cbExtr->maxWidth);
   DBG_OBJ_MSG_START ();

   int oofMinWidthAbsPos, oofMaxWidthAbsPos;
   getAbsolutelyPositionedExtremes (cbExtr, &oofMinWidthAbsPos,
                                    &oofMaxWidthAbsPos);

   int oofMinWidthtLeft, oofMinWidthRight, oofMaxWidthLeft, oofMaxWidthRight;
   getFloatsExtremes (cbExtr, LEFT, &oofMinWidthtLeft, &oofMaxWidthLeft);
   getFloatsExtremes (cbExtr, RIGHT, &oofMinWidthRight, &oofMaxWidthRight);

   *oofMinWidth = max (oofMinWidthtLeft, oofMinWidthRight, oofMinWidthAbsPos);
   *oofMaxWidth = max (oofMaxWidthLeft, oofMaxWidthRight, oofMaxWidthAbsPos);

   DBG_OBJ_MSGF ("resize.oofm", 1,
                 "=> (a: %d, l: %d, r: %d => %d) / (a: %d, l: %d, r: %d => %d)",
                 oofMinWidthAbsPos, oofMinWidthtLeft, oofMinWidthRight,
                 *oofMinWidth, oofMaxWidthAbsPos, oofMaxWidthLeft,
                 oofMaxWidthRight, *oofMaxWidth);
   DBG_OBJ_MSG_END ();
}

void OutOfFlowMgr::getFloatsExtremes (Extremes *cbExtr, Side side,
                                      int *minWidth, int *maxWidth)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>getFloatsExtremes</b> ((%d / %d), %s, ...)",
                 cbExtr->minWidth, cbExtr->maxWidth,
                 side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   *minWidth = *maxWidth = 0;

   SortedFloatsVector *list = getFloatsListForTextblock (containingBlock, side);


   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      int leftDiff, rightDiff;

      if (getFloatDiffToCB (vloat, &leftDiff, &rightDiff)) {
         Extremes extr;
         vloat->getWidget()->getExtremes (&extr);

         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "considering float %p generated by %p: %d / %d",
                       vloat->getWidget (), vloat->generatingBlock,
                       extr.minWidth, extr.maxWidth);

         if (isAbsLength (vloat->getWidget()->getStyle()->width)) {
            int width = absLengthVal (vloat->getWidget()->getStyle()->width);
            if (extr.minWidth < width)
               extr.minWidth = width;
            if (extr.maxWidth > width)
               // maxWidth not smaller than minWidth
               extr.maxWidth = max (width, extr.minWidth);
                    
            DBG_OBJ_MSGF ("resize.oofm", 1,
                          "corrected by absolute width %d: %d / %d",
                          width, extr.minWidth, extr.maxWidth);
         }

         // TODO: Or zero (instead of rightDiff) for right floats?
         *minWidth =
            max (*minWidth,
                 extr.minWidth + (side == LEFT ? leftDiff : rightDiff));
         *maxWidth = max (*maxWidth, extr.maxWidth + leftDiff + rightDiff);

         DBG_OBJ_MSGF ("resize.oofm", 1, " => %d / %d", *minWidth, *maxWidth);
      } else
         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "considering float %p generated by %p: not allocated",
                       vloat->getWidget (), vloat->generatingBlock);
   }

   DBG_OBJ_MSG_END ();
}

// Returns "false" when borders cannot yet determined; *leftDiff and
// *rightDiff are undefined in this case.
bool OutOfFlowMgr::getFloatDiffToCB (Float *vloat, int *leftDiff,
                                     int *rightDiff)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>getDiffToCB</b> (float %p [generated by %p], ...)",
                 vloat->getWidget (), vloat->generatingBlock);
   DBG_OBJ_MSG_START ();

   bool result;

   if (vloat->generatingBlock == containingBlock) {
      *leftDiff = vloat->generatingBlock->getStyle()->boxOffsetX();
      *rightDiff = vloat->generatingBlock->getStyle()->boxRestWidth();
      result = true;
      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "GB == CB => leftDiff = %d, rightDiff = %d",
                    *leftDiff, *rightDiff);
   } else if (wasAllocated (vloat->generatingBlock)) {
      Allocation *gba = getAllocation(vloat->generatingBlock);
      *leftDiff = gba->x - containingBlockAllocation.x
         + vloat->generatingBlock->getStyle()->boxOffsetX();
      *rightDiff =
         (containingBlockAllocation.x + containingBlockAllocation.width)
         - (gba->x + gba->width)
         + vloat->generatingBlock->getStyle()->boxRestWidth();
      result = true;
      DBG_OBJ_MSGF ("resize.oofm", 1,
                    "GB != CB => leftDiff = %d - %d + %d = %d, "
                    "rightDiff = (%d + %d) - (%d + %d) + %d = %d",
                    gba->x, containingBlockAllocation.x,
                    vloat->generatingBlock->getStyle()->boxOffsetX(),
                    *leftDiff, containingBlockAllocation.x,
                    containingBlockAllocation.width, gba->x, gba->width,
                    vloat->generatingBlock->getStyle()->boxRestWidth(),
                    *rightDiff);
   } else {
      DBG_OBJ_MSG ("resize.oofm", 1, "GB != CB, and float not allocated");
      result = false;
   }

   DBG_OBJ_MSG_END ();
   return result;
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
 * of *h*, based on floats.
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
               DBG_OBJ_MSGF ("border", 1, "GB: thisBorder = %d + %d = %d",
                             vloat->size.width, borderIn, thisBorder);
            } else {
               assert (wasAllocated (vloat->generatingBlock));
               assert (vloat->getWidget()->wasAllocated ());

               Allocation *tba = getAllocation(textblock),
                  *fla = vloat->getWidget()->getAllocation ();
               if (side == LEFT) {
                  thisBorder = fla->x + fla->width - tba->x;
                  DBG_OBJ_MSGF ("border", 1,
                                "not GB: thisBorder = %d + %d - %d = %d",
                                fla->x, fla->width, tba->x, thisBorder);
               } else {
                  // See also calcFloatX.
                  int tbAvWidth = textblock->getAvailWidth ();
                  thisBorder = tba->x + min (tba->width, tbAvWidth) - fla->x;
                  DBG_OBJ_MSGF ("border", 1,
                                "not GB: thisBorder = %d + min (%d, %d) - %d "
                                "= %d",
                                tba->x, tba->width, tbAvWidth, fla->x,
                                thisBorder);
               }
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
   bool b = hasFloat (textblock, LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float left (%p, %d, %d, %p, %d) => %s",
                 textblock, y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OutOfFlowMgr::hasFloatRight (Textblock *textblock, int y, int h,
                                  Textblock *lastGB, int lastExtIndex)
{
   bool b = hasFloat (textblock, RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float right (%p, %d, %d, %p, %d) => %s",
                 textblock, y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OutOfFlowMgr::hasFloat (Textblock *textblock, Side side, int y, int h,
                             Textblock *lastGB, int lastExtIndex)
{
   DBG_OBJ_MSGF ("border", 0, "<b>hasFloat</b> (%p, %s, %d, %d, %p, %d)",
                 textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
                 lastGB, lastExtIndex);
   DBG_OBJ_MSG_START ();

   SortedFloatsVector *list = getFloatsListForTextblock (textblock, side);
   int first = list->findFirst (textblock, y, h, lastGB, lastExtIndex);

   DBG_OBJ_MSGF ("border", 1, "first = %d", first);
   DBG_OBJ_MSG_END ();
   return first != -1;
}

/**
 * Returns position relative to the textblock "tb".
 */
int OutOfFlowMgr::getClearPosition (Textblock *tb)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>getClearPosition</b> (%p)", tb);
   DBG_OBJ_MSG_START ();

   int pos;

   if (tb->getStyle()) {
      bool left = false, right = false;
      switch (tb->getStyle()->clear) {
      case CLEAR_NONE: break;
      case CLEAR_LEFT: left = true; break;
      case CLEAR_RIGHT: right = true; break;
      case CLEAR_BOTH: left = right = true; break;
      default: assertNotReached ();
      }

      pos = max (left ? getClearPosition (tb, LEFT) : 0,
                 right ? getClearPosition (tb, RIGHT) : 0);
   } else
      pos = 0;

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", pos);
   DBG_OBJ_MSG_END ();

   return pos;
}

int OutOfFlowMgr::getClearPosition (Textblock *tb, Side side)
{
   DBG_OBJ_MSGF ("resize.oofm", 0, "<b>getClearPosition</b> (%p, %s)",
                 tb, side == LEFT ? "LEFT" : "RIGHT");
   DBG_OBJ_MSG_START ();

   int pos;

   if (!wasAllocated (tb))
      // There is no relation yet to floats generated by other
      // textblocks, and this textblocks floats are unimportant for
      // the "clear" property.
      pos = 0;
   else {
      SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;

      // Search the last float before (therfore -1) this textblock.
      int i = list->findFloatIndex (tb, -1);
      if (i < 0) {
         pos = 0;
         DBG_OBJ_MSG ("resize.oofm", 1, "no float");
      } else {
         Float *vloat = list->get(i);
         assert (vloat->generatingBlock != tb);
         ensureFloatSize (vloat);
         pos = vloat->yReal + vloat->size.ascent + vloat->size.descent -
            getAllocation(tb)->y;
         DBG_OBJ_MSGF ("resize.oofm", 1, "float %p => %d + (%d + %d) - %d",
                       vloat->getWidget (), vloat->yReal, vloat->size.ascent,
                       vloat->size.descent, getAllocation(tb)->y);
      }
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", pos);
   DBG_OBJ_MSG_END ();

   return pos;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty ||
       vloat->cbAvailWidth != containingBlock->getAvailWidth () &&
       (// If the size of the containing block has changed (represented
        // currently by the available width), a recalculation of a relative
        // float width may also be necessary.
        isPerLength (vloat->getWidget()->getStyle()->width) ||
        // Similar for "auto" widths of textblocks etc.
        (vloat->getWidget()->usesHints () && 
         vloat->getWidget()->getStyle()->width == LENGTH_AUTO))) {
      DBG_OBJ_MSGF ("resize.oofm", 0,
                    "<b>ensureFloatSize</b> (%p): recalculation",
                    vloat->getWidget ());
      DBG_OBJ_MSG_START ();

      Extremes extremes;
      vloat->getWidget()->getExtremes (&extremes);
      DBG_OBJ_MSGF ("resize.oofm", 1, "getExtremes => %d / %d",
                    extremes.minWidth, extremes.maxWidth);

      // TODO Ugly. Soon to be replaced by cleaner code? See also
      // comment in Textblock::calcWidgetSize.

      if (vloat->getWidget()->usesHints ()) {
         // For widths defined by CSS, similar adjustments (extremes
         // etc.) like below are necessary, to prevent CPU hogging.

         Length cssWidth = vloat->getWidget()->getStyle()->width;
         if (isAbsLength (cssWidth)) {
            int width = absLengthVal (cssWidth);
            DBG_OBJ_MSGF ("resize.oofm", 1, "about to set absolute width: %d",
                          width);
            width = adjustFloatWidth (width, &extremes);
            vloat->getWidget()->setWidth (width);
         } else if (cssWidth == LENGTH_AUTO || isPerLength (cssWidth)) {
            // It is important that the width of the *CB* is not
            // larger than its minimal width, when the latter is set
            // as size hint; otherwise we have an endless queueResize
            // cycle (resulting in CPU hogging) when the CB is part of
            // a narrow table column. To prevent this, the width of
            // the *float* has to be limited (cf. also getExtremes).

            int availWidth, leftDiff, rightDiff;
            if (getFloatDiffToCB (vloat, &leftDiff, &rightDiff))
               availWidth = containingBlock->getAvailWidth()
                  - (vloat->getWidget()->getStyle()->vloat == FLOAT_LEFT ?
                     leftDiff : rightDiff);
            else
               // Not allocated: next allocation will take care.
               availWidth = containingBlock->getAvailWidth();

            int width;
            
            if (cssWidth == LENGTH_AUTO) {
               width = availWidth;
               DBG_OBJ_MSGF ("resize.oofm", 1, "setting width 'auto': %d",
                             width);
            } else {
               width = multiplyWithPerLength (availWidth, cssWidth);

               // Some more corrections (nonsense percentage values):
               if (width < 1)
                  width = 1;
               if (width > availWidth)
                  width = availWidth;
               
               DBG_OBJ_MSGF ("resize.oofm", 1,
                             "about to set percentage width: %d * %g -> %d",
                             availWidth, perLengthVal (cssWidth), width);
               width = adjustFloatWidth (width, &extremes);
            }

            vloat->getWidget()->setWidth (width);
         } else
            DBG_OBJ_MSG ("resize.oofm", 1,
                         "setting width: <b>relative length? may be a bug</b>");
      } else
         DBG_OBJ_MSG ("resize.oofm", 1, "setting no width: uses no hints");

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
      //
      // Furthermore, extremes are considered; especially, floats are too
      // wide, sometimes.

      vloat->getWidget()->sizeRequest (&vloat->size);
      DBG_OBJ_MSGF ("resize.oofm", 1, "sizeRequest (1) => %d * (%d + %d)",
                    vloat->size.width, vloat->size.ascent, vloat->size.descent);

      // Set width  ...
      int width = vloat->size.width;
      DBG_OBJ_MSGF ("resize.oofm", 1, "new width: %d", width);
      width = adjustFloatWidth (width, &extremes);
      vloat->getWidget()->setWidth (width);
      vloat->getWidget()->sizeRequest (&vloat->size);
      DBG_OBJ_MSGF ("resize.oofm", 1, "sizeRequest (2) => %d * (%d + %d)",
                    vloat->size.width, vloat->size.ascent, vloat->size.descent);

      vloat->cbAvailWidth = containingBlock->getAvailWidth ();
      vloat->dirty = false;

      DBG_OBJ_MSGF ("resize.oofm", 1, "final size: %d * (%d + %d)",
                    vloat->size.width, vloat->size.ascent, vloat->size.descent);

      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.width",
                         vloat->size.width);
      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.ascent",
                         vloat->size.ascent);
      DBG_OBJ_SET_NUM_O (vloat->getWidget(), "<Float>.size.descent",
                         vloat->size.descent);

      // "sizeChangedSinceLastAllocation" is reset in sizeAllocateEnd()

      DBG_OBJ_MSG_END ();
   }
}

int OutOfFlowMgr::adjustFloatWidth (int width, Extremes *extremes)
{
   DBG_OBJ_MSGF ("resize.oofm", 0,
                 "<b>adjustFloatWidth</b> (%d, (%d, %d)) [CB->availWidth = %d]",
                 width, extremes->minWidth, extremes->maxWidth,
                 containingBlock->getAvailWidth());
   DBG_OBJ_MSG_START ();

   // Consider the available width of the containing block (when set):
   if (width > containingBlock->getAvailWidth()) {
      width = containingBlock->getAvailWidth();
      DBG_OBJ_MSGF ("resize.oofm", 1, "adjusted to availWidth: %d", width);
   }
   // Finally, consider extremes (as described above).
   if (width < extremes->minWidth) {
      width = extremes->minWidth;
      DBG_OBJ_MSGF ("resize.oofm", 1, "adjusted to minWidth: %d", width);
   }
   if (width > extremes->maxWidth) {
      width = extremes->maxWidth;
      DBG_OBJ_MSGF ("resize.oofm", 1, "adjusted to maxWidth: %d", width);
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", width);
   DBG_OBJ_MSG_END ();

   return width;
}

void OutOfFlowMgr::getAbsolutelyPositionedSize (Requisition *cbReq, int *width,
                                                int *height)
{
   // TODO
   *width = *height = 0;
}

void OutOfFlowMgr::getAbsolutelyPositionedExtremes (Extremes *cbExtr,
                                                    int *minWidth,
                                                    int *maxWidth)
{
   // TODO
   *minWidth = *maxWidth = 0;
}

void OutOfFlowMgr::ensureAbsolutelyPositionedSizeAndPosition
   (AbsolutelyPositioned *abspos)
{
   // No work is done anymore on this, since widget sizes will be
   // redesigned before absolute positions are finished.

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

} // namespace dw
