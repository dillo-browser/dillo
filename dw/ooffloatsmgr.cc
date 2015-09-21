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
   wasAllocated = false;
   xCB = yCB = width = height = -1;
}

void OOFFloatsMgr::WidgetInfo::update (bool wasAllocated, int xCB, int yCB,
                                       int width, int height)
{
   DBG_OBJ_ENTER_O ("resize.oofm", 0, widget, "update", "%s, %d, %d, %d, %d",
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

   DBG_OBJ_LEAVE_O (widget);
}

// ----------------------------------------------------------------------

OOFFloatsMgr::Float::Float (OOFFloatsMgr *oofm, Widget *widget,
                            OOFAwareWidget *generatingBlock, int externalIndex)
   : WidgetInfo (oofm, widget)
{
   this->generatingBlock = generatingBlock;
   this->externalIndex = externalIndex;

   yReq = yReal = size.width = size.ascent = size.descent = 0;
   dirty = sizeChangedSinceLastAllocation = true;
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
      DBG_OBJ_SET_BOOL_O (widget, "<Float>.sizeChangedSinceLastAllocation",
                         sizeChangedSinceLastAllocation);
   }
}

void OOFFloatsMgr::Float::updateAllocation ()
{
   DBG_OBJ_ENTER0_O ("resize.oofm", 0, getWidget (), "updateAllocation");

   update (isNowAllocated (), getNewXCB (), getNewYCB (), getNewWidth (),
           getNewHeight ());

   DBG_OBJ_LEAVE_O (getWidget ());
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

bool OOFFloatsMgr::Float::covers (OOFAwareWidget *textblock, int y, int h)
{
   DBG_OBJ_ENTER_O ("border", 0, getOOFFloatsMgr (), "covers",
                    "%p, %d, %d [vloat: %p]",
                    textblock, y, h, getWidget ());

   bool b;

   if (textblock == generatingBlock) {
      int reqyGB = y;
      int flyGB = yReal;
      getOOFFloatsMgr()->ensureFloatSize (this);
      int flh = size.ascent + size.descent;
      b = flyGB + flh > reqyGB && flyGB < reqyGB + h;

      DBG_OBJ_MSGF_O ("border", 1, getOOFFloatsMgr (),
                      "for generator: reqyGB = %d, flyGB = %d, "
                      "flh = %d + %d = %d => %s",
                      reqyGB, flyGB, size.ascent, size.descent, flh,
                      b ? "true" : "false");
   } else {
      // (If the textblock were not allocated, the GB list would have
      // been choosen instead of the CB list, and so this else-branch
      // would not have been not executed.)
      assert (getOOFFloatsMgr()->wasAllocated (textblock));

      if (!getWidget()->wasAllocated ()) {
         DBG_OBJ_MSG_O ("border", 1, getOOFFloatsMgr (),
                        "not generator (not allocated) => false");
         b = false;
      } else {
         Allocation *tba = getOOFFloatsMgr()->getAllocation(textblock),
            *fla = getWidget()->getAllocation ();
         int reqyCanv = tba->y + y;
         int flyCanv = fla->y;
         int flh = fla->ascent + fla->descent;
         b = flyCanv + flh > reqyCanv && flyCanv < reqyCanv + h;

         DBG_OBJ_MSGF_O ("border", 1, getOOFFloatsMgr (),
                         "not generator (allocated): reqyCanv = %d + %d = %d, "
                         "flyCanv = %d, flh = %d + %d = %d => %s",
                         tba->y, y, reqyCanv, flyCanv,
                         fla->ascent, fla->descent, flh, b ? "true" : "false");
      }
   }

   DBG_OBJ_LEAVE_O (getOOFFloatsMgr ());

   return b;
}

int OOFFloatsMgr::Float::ComparePosition::compare (Object *o1, Object *o2)
{
   Float *fl1 = (Float*)o1, *fl2 = (Float*)o2;
   int r;

   DBG_OBJ_ENTER_O ("border", 1, oofm,
                    "ComparePosition/compare", "(#%d, #%d) [refTB = %p]",
                    fl1->index, fl2->index, refTB);

   if (refTB == fl1->generatingBlock && refTB == fl2->generatingBlock) {
      DBG_OBJ_MSG_O ("border", 2, oofm, "refTB is generating both floats");
      r = fl1->yReal - fl2->yReal;
   } else {
      DBG_OBJ_MSG_O ("border", 2, oofm, "refTB is not generating both floats");
      DBG_OBJ_MSG_START_O (oofm);

      DBG_OBJ_MSGF_O ("border", 2, oofm, "generators are %p and %p",
                      fl1->generatingBlock, fl2->generatingBlock);

      // (i) Floats may not yet been allocated. Non-allocated floats
      // do not have an effect yet, they are considered "at the end"
      // of the list.

      // (ii) Float::widget is NULL for the key used for binary
      // search. In this case, Float::yReal is used instead (which is
      // set in SortedFloatsVector::find, too). The generator is the
      // textblock, and should be allocated. (If not, the GB list
      // would have been choosen instead of the CB list, and so this
      // else-branch would not have been not executed.)

      bool a1 = fl1->getWidget () ? fl1->getWidget()->wasAllocated () : true;
      bool a2 = fl2->getWidget () ? fl2->getWidget()->wasAllocated () : true;

      DBG_OBJ_MSGF_O ("border", 2, oofm,
                      "float 1 (%p) allocated: %s; float 2 (%p) allocated: %s",
                      fl1->getWidget (), a1 ? "yes" : "no", fl2->getWidget (),
                      a2 ? "yes" : "no");

      if (a1 && a2) {
         int fly1, fly2;
         
         if (fl1->getWidget()) {
            fly1 = fl1->getWidget()->getAllocation()->y;
            DBG_OBJ_MSGF_O ("border", 2, oofm, "fly1 = %d", fly1);
         } else {
            assert (oofm->wasAllocated (fl1->generatingBlock));
            fly1 = oofm->getAllocation(fl1->generatingBlock)->y + fl1->yReal;
            DBG_OBJ_MSGF_O ("border", 2, oofm, "fly1 = %d + %d = %d",
                            oofm->getAllocation(fl1->generatingBlock)->y,
                            fl1->yReal, fly1);
         }

         if (fl2->getWidget()) {
            fly2 = fl2->getWidget()->getAllocation()->y;
            DBG_OBJ_MSGF_O ("border", 2, oofm, "fly2 = %d", fly2);
         } else {
            assert (oofm->wasAllocated (fl2->generatingBlock));
            fly2 = oofm->getAllocation(fl2->generatingBlock)->y + fl2->yReal;
            DBG_OBJ_MSGF_O ("border", 2, oofm, "fly2 = %d + %d = %d",
                            oofm->getAllocation(fl2->generatingBlock)->y,
                            fl2->yReal, fly2);
         }

         r = fly1 - fly2;

         DBG_OBJ_MSGF_O ("border", 2, oofm, "r = %d - %d = %d", fly1, fly2, r);
      } else if (a1 && !a2)
         r = -1;
      else if (!a1 && a2)
         r = +1;
      else // if (!a1 && !a2)
         return 0;

      DBG_OBJ_MSG_END_O (oofm);
   }

   DBG_OBJ_MSGF_O ("border", 1, oofm, "result: %d", r);
   DBG_OBJ_LEAVE_O (oofm);
   return r;
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
                    f1->index, f1->generatingBlock, f1->externalIndex,
                    f2->index, f2->generatingBlock,
                    f2->externalIndex);

   if (f1->generatingBlock == f2->generatingBlock) {
      r = f1->externalIndex - f2->externalIndex;
      DBG_OBJ_MSGF_O ("border", 2, oofm,
                      "(a) generating blocks equal => %d - %d = %d",
                      f1->externalIndex, f2->externalIndex, r);
   } else {
      TBInfo *t1 = oofm->getOOFAwareWidget (f1->generatingBlock),
         *t2 = oofm->getOOFAwareWidget (f2->generatingBlock);
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

   DBG_OBJ_MSGF_O ("border", 2, oofm, "result: %d", r);
   DBG_OBJ_LEAVE_O (oofm);
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

   DBG_OBJ_MSGF_O ("border", 1, oofm, "=> r = %d", r);
   DBG_OBJ_LEAVE_O (oofm);
   return r;
}

int OOFFloatsMgr::SortedFloatsVector::find (OOFAwareWidget *textblock, int y,
                                            int start, int end)
{
   DBG_OBJ_ENTER_O ("border", 0, oofm, "find", "%p, %d, %d, %d",
                    textblock, y, start, end);

   Float key (oofm, NULL, NULL, 0);
   key.generatingBlock = textblock;
   key.yReal = y;
   key.index = -1; // for debugging
   Float::ComparePosition comparator (oofm, textblock);
   int result = bsearch (&key, false, start, end, &comparator);

   DBG_OBJ_MSGF_O ("border", 1, oofm, "=> result = %d", result);
   DBG_OBJ_LEAVE_O (oofm);
   return result;
}

int OOFFloatsMgr::SortedFloatsVector::findFirst (OOFAwareWidget *textblock,
                                                 int y, int h,
                                                 OOFAwareWidget *lastGB,
                                                 int lastExtIndex,
                                                 int *lastReturn)
{
   DBG_OBJ_ENTER_O ("border", 0, oofm, "findFirst", "%p, %d, %d, %p, %d",
                    textblock, y, h, lastGB, lastExtIndex);

   DBG_IF_RTFL {
      DBG_OBJ_MSG_O ("border", 2, oofm, "searching in list:");
      DBG_OBJ_MSG_START_O (oofm);

      for (int i = 0; i < size(); i++) {
         DBG_OBJ_MSGF_O ("border", 2, oofm,
                         "%d: (%p, i = %d/%d, y = %d/%d, s = (%d * (%d + %d)), "
                         "%s, %s, ext = %d, GB = %p); widget at (%d, %d)",
                         i, get(i)->getWidget (), get(i)->index,
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

   // If the caller wants to reuse this value:
   if (lastReturn)
      *lastReturn = last;

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
   DBG_OBJ_LEAVE_O (oofm);
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

OOFFloatsMgr::TBInfo::TBInfo (OOFFloatsMgr *oofm, OOFAwareWidget *textblock,
                              TBInfo *parent, int parentExtIndex) :
   WidgetInfo (oofm, textblock)
{
   this->parent = parent;
   this->parentExtIndex = parentExtIndex;

   leftFloats = new Vector<Float> (1, false);
   rightFloats = new Vector<Float> (1, false);

   wasAllocated = getWidget()->wasAllocated ();
   allocation = *(getWidget()->getAllocation ());
   clearPosition = 0;
}

OOFFloatsMgr::TBInfo::~TBInfo ()
{
   delete leftFloats;
   delete rightFloats;
}

void OOFFloatsMgr::TBInfo::updateAllocation ()
{
   DBG_OBJ_ENTER0_O ("resize.oofm", 0, getWidget (), "updateAllocation");

   update (isNowAllocated (), getNewXCB (), getNewYCB (), getNewWidth (),
           getNewHeight ());

   DBG_OBJ_LEAVE_O (getWidget ());
}

OOFFloatsMgr::OOFFloatsMgr (OOFAwareWidget *container)
{
   DBG_OBJ_CREATE ("dw::OOFFloatsMgr");

   this->container = (OOFAwareWidget*)container;

   leftFloats = new SortedFloatsVector (this, LEFT);
   rightFloats = new SortedFloatsVector (this, RIGHT);

   DBG_OBJ_SET_NUM ("leftFloats.size", leftFloats->size());
   DBG_OBJ_SET_NUM ("rightFloats.size", rightFloats->size());

   floatsByWidget = new HashTable <TypedPointer <Widget>, Float> (true, false);

   tbInfos = new Vector<TBInfo> (1, false);
   tbInfosByOOFAwareWidget =
      new HashTable <TypedPointer <OOFAwareWidget>, TBInfo> (true, true);

   leftFloatsMark = rightFloatsMark = 0;
   lastLeftTBIndex = lastRightTBIndex = 0;

   containerWasAllocated = container->wasAllocated ();
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

   delete leftFloats;
   delete rightFloats;

   delete floatsByWidget;

   DBG_OBJ_DELETE ();
}

void OOFFloatsMgr::sizeAllocateStart (OOFAwareWidget *caller,
                                      Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateStart",
                  "%p, (%d, %d, %d * (%d + %d))",
                  caller, allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   // Some callers are not registered, especially tables. (Where the
   // floats manager is actually empty?)
   TBInfo *oofAWInfo = getOOFAwareWidgetWhenRegistered (caller);
   if (oofAWInfo) {
      oofAWInfo->allocation = *allocation;
      oofAWInfo->wasAllocated = true;
   }

   if (caller == container) {
      // In the size allocation process, the *first* OOFM method
      // called is sizeAllocateStart, with the containing block as an
      // argument. So this is the correct point to initialize size
      // allocation.

      containerWasAllocated = true;
      containerAllocation = *allocation;
   }

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::sizeAllocateEnd (OOFAwareWidget *caller)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateEnd", "%p", caller);

   if (isOOFAwareWidgetRegistered (caller)) {     
      // The checks below do not cover "clear position" in all cases,
      // so this is done here separately. This position is stored in
      // TBInfo and calculated at this points; changes will be noticed
      // to the textblock.
      TBInfo *tbInfo = getOOFAwareWidget (caller);
      int newClearPosition = calcClearPosition (caller);
      if (newClearPosition != tbInfo->clearPosition) {
         tbInfo->clearPosition = newClearPosition;
         caller->clearPositionChanged ();
      }

      if (caller == container) {
         // In the size allocation process, the *last* OOFM method called
         // is sizeAllocateEnd, with the containing block as an
         // argument. So this is the correct point to finish size
         // allocation.
         
         // TODO: After SRDOP, the comment above is probably non-sense.

         sizeAllocateFloats (LEFT);
         sizeAllocateFloats (RIGHT);

         // Store some information for later use.
         // TODO still used with SRDOP?
         for (lout::container::typed::Iterator<TypedPointer <OOFAwareWidget> >
                 it = tbInfosByOOFAwareWidget->iterator ();
              it.hasNext (); ) {
            TypedPointer <OOFAwareWidget> *key = it.getNext ();
            TBInfo *tbInfo = tbInfosByOOFAwareWidget->get (key);
            OOFAwareWidget *tb = key->getTypedValue();

            tbInfo->updateAllocation ();
            tbInfo->lineBreakWidth = tb->getLineBreakWidth ();
         }

         // There are cases where some allocated floats exceed the CB size.
         bool sizeChanged = doFloatsExceedCB (LEFT) || doFloatsExceedCB (RIGHT);

         // Similar for extremes.
         bool extremesChanged =
            haveExtremesChanged (LEFT) || haveExtremesChanged (RIGHT);

         for (int i = 0; i < leftFloats->size(); i++)
            leftFloats->get(i)->updateAllocation ();

         for (int i = 0; i < rightFloats->size(); i++)
            rightFloats->get(i)->updateAllocation ();

         if (sizeChanged || extremesChanged)
            container->oofSizeChanged (extremesChanged);
      }
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

bool OOFFloatsMgr::doFloatsExceedCB (Side side)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "doFloatsExceedCB", "%s",
                  side == LEFT ? "LEFT" : "RIGHT");

   // This method is called to determine whether the *requisition* of
   // the CB must be recalculated. So, we check the float allocations
   // against the *requisition* of the CB, which may (e. g. within
   // tables) differ from the new allocation. (Generally, a widget may
   // allocated at a different size.)
   core::Requisition cbReq;
   container->sizeRequest (&cbReq);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   bool exceeds = false;

   DBG_OBJ_MSG_START ();

   for (int i = 0; i < list->size () && !exceeds; i++) {
      Float *vloat = list->get (i);
      if (vloat->getWidget()->wasAllocated ()) {
         Allocation *fla = vloat->getWidget()->getAllocation ();
         DBG_OBJ_MSGF ("resize.oofm", 2,
                       "Does FlA = (%d, %d, %d * %d) exceed CB req+alloc = "
                       "(%d, %d, %d * %d)?",
                       fla->x, fla->y, fla->width, fla->ascent + fla->descent,
                       containerAllocation.x, containerAllocation.y,
                       cbReq.width, cbReq.ascent + cbReq.descent);
         if (fla->x + fla->width > containerAllocation.x + cbReq.width ||
             fla->y + fla->ascent + fla->descent
             > containerAllocation.y + cbReq.ascent + cbReq.descent) {
            exceeds = true;
            DBG_OBJ_MSG ("resize.oofm", 2, "Yes.");
         } else
            DBG_OBJ_MSG ("resize.oofm", 2, "No.");
      }
   }

   DBG_OBJ_MSG_END ();

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %s", exceeds ? "true" : "false");
   DBG_OBJ_LEAVE ();

   return exceeds;
}

bool OOFFloatsMgr::haveExtremesChanged (Side side)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "haveExtremesChanged", "%s",
                  side == LEFT ? "LEFT" : "RIGHT");

   // This is quite different from doFloatsExceedCB, since there is no
   // counterpart to getExtremes, as sizeAllocate is a counterpart to
   // sizeRequest. So we have to determine whether the allocation has
   // changed the extremes, which is done by examining the part of the
   // allocation which is part of the extremes calculation (see
   // getFloatsExtremes). Changes of the extremes are handled by the
   // normal queueResize mechanism.

   // (This may refer to the old implementation of doFloatsExceedCB,
   // which checked the container *allocation*, not the *requisition*.
   //
   // TODO: (i) Correct the comment. (ii) Is this implementation still
   // correct, considering the reasoning behind the change of
   // doFloatsExceedCB?
   //
   // See also OOFPositionedMgr::haveExtremesChanged.)

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   bool changed = false;

   for (int i = 0; i < list->size () && !changed; i++) {
      Float *vloat = list->get (i);
      // When the GB is the CB, an allocation change does not play a
      // role here.
      if (vloat->generatingBlock != container) {
         if (!vloat->wasThenAllocated () && vloat->isNowAllocated ())
            changed = true;
         else {
            // This method is called within sizeAllocateEnd, where
            // containinBlock->getAllocation() (old value) and
            // containinBlockAllocation (new value) are different.

            Allocation *oldCBA = container->getAllocation ();
            Allocation *newCBA = &containerAllocation;

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
   DBG_OBJ_LEAVE ();

   return changed;
}

void OOFFloatsMgr::sizeAllocateFloats (Side side)
{
   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;

   DBG_OBJ_ENTER ("resize.oofm", 0, "sizeAllocateFloats", "%s",
                  side == LEFT ? "LEFT" : "RIGHT");

   Allocation *cba = &containerAllocation;

   for (int i = 0; i < list->size (); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);

      Allocation *gba = getAllocation (vloat->generatingBlock);

      Allocation childAllocation;
      childAllocation.x = cba->x + calcFloatX (vloat, side, gba->x - cba->x,
                                               getGBWidthForAllocation (vloat));
      childAllocation.y = gba->y + vloat->yReal;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;

      vloat->getWidget()->sizeAllocate (&childAllocation);
   }

   DBG_OBJ_LEAVE ();
}

// Used as argument "gbWidth" for calcFloatX(), in the context of allocation.
int OOFFloatsMgr::getGBWidthForAllocation (Float *vloat)
{
   // See comments in getFloatsSize() for a detailed rationale ...
   if (container->mustBeWidenedToAvailWidth ())
      return vloat->generatingBlock->getLineBreakWidth ();
   else
      // ... but notice this difference: not GB width + float width is
      // used, but only GB width, since the float width has already
      // been included in getFloatsSize().
      return min (getAllocation(vloat->generatingBlock)->width,
                  vloat->generatingBlock->getLineBreakWidth ());
}

/**
 * \brief ...
 *
 * gbX is given relative to the CB, as is the return value.
 */
int OOFFloatsMgr::calcFloatX (Float *vloat, Side side, int gbX, int gbWidth)
{
   DBG_OBJ_ENTER ("resize.common", 0, "calcFloatX", "%p, %s, %d, %d",
                  vloat->getWidget (), side == LEFT ? "LEFT" : "RIGHT", gbX,
                  gbWidth);
   int x;

   switch (side) {
   case LEFT:
      // Left floats are always aligned on the left side of the
      // generator (content, not allocation) ...
      x = gbX + vloat->generatingBlock->getStyle()->boxOffsetX();
      DBG_OBJ_MSGF ("resize.common", 1, "left: x = %d + %d = %d",
                    gbX, vloat->generatingBlock->getStyle()->boxOffsetX(), x);
      // ... but when the float exceeds the line break width of the
      // container, it is corrected (but not left of the container).
      // This way, we save space and, especially within tables, avoid
      // some problems.
      if (wasAllocated (container) &&
          x + vloat->size.width > container->getLineBreakWidth ()) {
         x = max (0, container->getLineBreakWidth () - vloat->size.width);
         DBG_OBJ_MSGF ("resize.common", 1,
                       "corrected to: max (0, %d - %d) = %d",
                       container->getLineBreakWidth (), vloat->size.width, x);
      }
      break;

   case RIGHT:
      // Similar for right floats, but in this case, floats are
      // shifted to the right when they are too big (instead of
      // shifting the generator to the right).

      x = max (gbX + gbWidth - vloat->size.width
               - vloat->generatingBlock->getStyle()->boxRestWidth(),
               // Do not exceed CB allocation:
               0);
      DBG_OBJ_MSGF ("resize.common", 1, "x = max (%d + %d - %d - %d, 0) = %d",
                    gbX, gbWidth, vloat->size.width,
                    vloat->generatingBlock->getStyle()->boxRestWidth(), x);
      break;

   default:
      assertNotReached ();
      x = 0;
      break;
   }

   DBG_OBJ_LEAVE ();
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

   int subRef;

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
   // is sufficient; differenciation between GB and CB lists is not
   // neccessary.
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

void OOFFloatsMgr::markSizeChange (int ref)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "markSizeChange", "%d", ref);

   Float *vloat;

   if (isSubRefLeftFloat (ref))
      vloat = leftFloats->get (getFloatIndexFromSubRef (ref));
   else if (isSubRefRightFloat (ref))
      vloat = rightFloats->get (getFloatIndexFromSubRef (ref));
   else {
      assertNotReached();
      vloat = NULL; // compiler happiness
   }

   vloat->dirty = vloat->sizeChangedSinceLastAllocation = true;
   
   DBG_OBJ_SET_BOOL_O (vloat->getWidget (), "<Float>.dirty", vloat->dirty);
   DBG_OBJ_SET_BOOL_O (vloat->getWidget (),
                       "<Float>.sizeChangedSinceLastAllocation",
                       vloat->sizeChangedSinceLastAllocation);

   // The generating block is told directly about this. (Others later, in
   // sizeAllocateEnd.) Could be faster (cf. hasRelationChanged, which
   // differentiates many special cases), but the size is not known yet,
   vloat->generatingBlock->borderChanged (vloat->yReal, vloat->getWidget ());

   DBG_OBJ_LEAVE ();
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

   DBG_OBJ_LEAVE ();
}

void OOFFloatsMgr::tellPosition2 (Widget *widget, int x, int y)
{
   // Nothing to do.
}

void OOFFloatsMgr::tellIncompletePosition1 (Widget *generator, Widget *widget,
                                            int x, int y)
{
   assertNotReached ();
}

void OOFFloatsMgr::tellIncompletePosition2 (Widget *generator, Widget *widget,
                                            int x, int y)
{
   assertNotReached ();
}

bool OOFFloatsMgr::collidesV (Float *vloat, Float *other, int *yReal)
{
   // Only checks vertical (possible) collisions, and only refers to
   // vloat->yReal; never to vloat->allocation->y, even when the GBs are
   // different. Used only in tellPosition.

   DBG_OBJ_ENTER ("resize.oofm", 0, "collidesV", "#%d [%p], #%d [%p], ...",
                  vloat->index, vloat->getWidget (), other->index,
                  other->getWidget ());

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
      // If the other float is not allocated, there is no collision. The
      // allocation of this float (vloat) is not used at all.
      if (!other->getWidget()->wasAllocated ())
         result = false;
      else {
         assert (wasAllocated (vloat->generatingBlock));
         Allocation *gba = getAllocation (vloat->generatingBlock),
            *flaOther = other->getWidget()->getAllocation ();

         // TODO: The following comment is wrong after SRDOP. What is still
         // relevant (especially in the second paragraph)? (Also, usage of
         // allocation has to be reworked generally.)
         //
         // We distinguish two cases (by different values of useAllocation):
         // (i) within tellPosition, GB allocation + yReal is used for the
         // y position of the other float, while (ii) in checkAllocatedFloat-
         // Collisions, the float allocation is used. The latter is necessary
         // by the definition of this method, the former increases performance,
         // as compared to using the float allocation, in some cases, as in
         // this:
         //
         // When '<div><div style="float:left">[Some text]</div></div>' is
         // repeated n times, the resize idle function (Layout::resizeIdle)
         // would be repeated roughly n times, when also in case (i) the float
         // allocation is used, since for the collision test of float n with
         // float n - 1, the allocation of float n - 1 does not yet reflect the
         // collision test between n - 1 and n - 2, but yReal does for n - 1.
         //
         // On the other hand, the GB allocations will most likely more stable
         // than the float allocations.
         //
         // Cases where this is incorrect will hopefully be rare, and, in any
         // case, corrected in sizeAllocateEnd, either because hasRelation-
         // Changed returns true, or in checkAllocatedFloatCollisions.

         int otherFloatY =
            getAllocation(other->generatingBlock)->y + other->yReal;
         int otherBottomGB =
            otherFloatY + flaOther->ascent + flaOther->descent - gba->y;

         DBG_OBJ_MSGF ("resize.oofm", 1,
                       "different generators: "
                       "otherBottomGB = %d + (%d + %d) - %d = %d",
                       otherFloatY, flaOther->ascent, flaOther->descent, gba->y,
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

   DBG_OBJ_LEAVE ();
   return result;
}


bool OOFFloatsMgr::collidesH (Float *vloat, Float *other)
{
   // Only checks horizontal collision. For a complete test, use
   // collidesV (...) && collidesH (...).
   bool collidesH;

   if (vloat->generatingBlock == other->generatingBlock)
      collidesH = vloat->size.width + other->size.width
         + vloat->generatingBlock->boxDiffWidth()
         > vloat->generatingBlock->getLineBreakWidth();
   else {
      // Again, if the other float is not allocated, there is no
      // collision. Compare to collidesV. (But vloat->size is used
      // here.)
      if (!other->getWidget()->wasAllocated ())
         collidesH = false;
      else {
         assert (wasAllocated (vloat->generatingBlock));
         Allocation *gba = getAllocation (vloat->generatingBlock);
         int vloatX =
            calcFloatX (vloat,
                        vloat->getWidget()->getStyle()->vloat == FLOAT_LEFT ?
                        LEFT : RIGHT,
                        gba->x, getGBWidthForAllocation (vloat));

         // Generally: right border of the left float > left border of
         // the right float (all in canvas coordinates).
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
                    vloat->getWidget (), vloat->generatingBlock, container);
                    
      if (vloat->generatingBlock == container ||
          wasAllocated (vloat->generatingBlock)) {
         ensureFloatSize (vloat);
         int x, y;
         
         int effWidth;
         if (container->mustBeWidenedToAvailWidth ())
            // For most textblocks, the line break width is used for
            // calculating the x position. (This changed for GROWS,
            // where the width of a textblock is often smaller that
            // the line break.)
            effWidth = vloat->generatingBlock->getLineBreakWidth ();
         else
            // For some textblocks, like inline blocks, the line break
            // width would be too large for right floats in some
            // cases.
            //
            //  (i) Consider a small inline block with only a few words
            //      in one line, narrower that line break width minus
            //      float width. In this case, the sum should be used.
            //
            // (ii) If there is more than one line, the line break
            //      will already be exceeded, and so be smaller that
            //      GB width + float width.
            effWidth = min (cbReq->width + vloat->size.width,
                            vloat->generatingBlock->getLineBreakWidth ());

         if (vloat->generatingBlock == container) {
            x = calcFloatX (vloat, side, 0, effWidth);
            y = vloat->yReal;
         } else {
            Allocation *gba = getAllocation(vloat->generatingBlock);
            x = calcFloatX (vloat, side, gba->x - containerAllocation.x,
                            effWidth);
            y = gba->y - containerAllocation.y + vloat->yReal;
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
                    vloat->getWidget (), vloat->generatingBlock,
                    container);
      
      if (vloat->generatingBlock == container ||
          wasAllocated (vloat->generatingBlock)) {
         Extremes extr;
         vloat->getWidget()->getExtremes (&extr);
         
         // The calculation of extremes must be kept consistent with
         // getFloatsSize(). Especially this means for the *minimal* width:
         //
         // - The right border (difference between float and
         //   container) does not have to be considered (see
         //   getFloatsSize()).
         //
         // - This is also the case for the left border, as seen in
         //   calcFloatX() ("... but when the float exceeds the line
         //   break width" ...).

         *minWidth = max (*minWidth, extr.minWidth);

         // For the maximal width, borders must be considered.
      
         if (vloat->generatingBlock == container)
            *maxWidth =
               max (*maxWidth,
                    extr.maxWidth
                    + vloat->generatingBlock->getStyle()->boxDiffWidth());
         else {
            Allocation *gba = getAllocation (vloat->generatingBlock);
            *maxWidth =
               max (*maxWidth,
                    extr.maxWidth
                    + vloat->generatingBlock->getStyle()->boxDiffWidth()
                    + max (containerAllocation.width - gba->width, 0));
         }
         
         DBG_OBJ_MSGF ("resize.oofm", 1, "%d / %d => %d / %d",
                       extr.minWidth, extr.maxWidth, *minWidth, *maxWidth);
      } else
         DBG_OBJ_MSG ("resize.oofm", 1, "not allocated");
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

int OOFFloatsMgr::getLeftBorder (OOFAwareWidget *textblock, int y, int h,
                                 OOFAwareWidget *lastGB, int lastExtIndex)
{
   int b = getBorder (textblock, LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "left border (%p, %d, %d, %p, %d) => %d",
                 textblock, y, h, lastGB, lastExtIndex, b);
   return b;
}

int OOFFloatsMgr::getRightBorder (OOFAwareWidget *textblock, int y, int h,
                                  OOFAwareWidget *lastGB, int lastExtIndex)
{
   int b = getBorder (textblock, RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "right border (%p, %d, %d, %p, %d) => %d",
                 textblock, y, h, lastGB, lastExtIndex, b);
   return b;
}

int OOFFloatsMgr::getBorder (OOFAwareWidget *textblock, Side side, int y, int h,
                             OOFAwareWidget *lastGB, int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "getBorder", "%p, %s, %d, %d, %p, %d",
                  textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
                  lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int last;   
   int first = list->findFirst (textblock, y, h, lastGB, lastExtIndex, &last);

   DBG_OBJ_MSGF ("border", 1, "first = %d", first);

   if (first == -1) {
      // No float.
      DBG_OBJ_LEAVE ();
      return 0;
   } else {
      // It is not sufficient to find the first float, since a line
      // (with height h) may cover the region of multiple float, of
      // which the widest has to be choosen.
      int border = 0;
      bool covers = true;

      // We are not searching until the end of the list, but until the
      // float defined by lastGB and lastExtIndex.
      for (int i = first; covers && i <= last; i++) {
         Float *vloat = list->get(i);
         covers = vloat->covers (textblock, y, h);
         DBG_OBJ_MSGF ("border", 1, "float %d (%p) covers? %s.",
                       i, vloat->getWidget(), covers ? "<b>yes</b>" : "no");

         if (covers) {
            int thisBorder;
            if (vloat->generatingBlock == textblock) {
               int borderIn = side == LEFT ?
                  vloat->generatingBlock->boxOffsetX() :
                  vloat->generatingBlock->boxRestWidth();
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
                  thisBorder =
                     tba->x + textblock->getLineBreakWidth () - fla->x;
                  DBG_OBJ_MSGF ("border", 1,
                                "not GB: thisBorder = %d + %d - %d "
                                "= %d",
                                tba->x, textblock->getLineBreakWidth (), fla->x,
                                thisBorder);
               }
            }

            border = max (border, thisBorder);
            DBG_OBJ_MSGF ("border", 1, "=> border = %d", border);
         }
      }

      DBG_OBJ_LEAVE ();
      return border;
   }
}

bool OOFFloatsMgr::hasFloatLeft (OOFAwareWidget *textblock, int y, int h,
                                 OOFAwareWidget *lastGB, int lastExtIndex)
{
   bool b = hasFloat (textblock, LEFT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float left (%p, %d, %d, %p, %d) => %s",
                 textblock, y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OOFFloatsMgr::hasFloatRight (OOFAwareWidget *textblock, int y, int h,
                                  OOFAwareWidget *lastGB, int lastExtIndex)
{
   bool b = hasFloat (textblock, RIGHT, y, h, lastGB, lastExtIndex);
   DBG_OBJ_MSGF ("border", 0, "has float right (%p, %d, %d, %p, %d) => %s",
                 textblock, y, h, lastGB, lastExtIndex, b ? "true" : "false");
   return b;
}

bool OOFFloatsMgr::hasFloat (OOFAwareWidget *textblock, Side side, int y, int h,
                             OOFAwareWidget *lastGB, int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "hasFloat", "%p, %s, %d, %d, %p, %d",
                  textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
                  lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int first = list->findFirst (textblock, y, h, lastGB, lastExtIndex, NULL);

   DBG_OBJ_MSGF ("border", 1, "first = %d", first);
   DBG_OBJ_LEAVE ();
   return first != -1;
}

int OOFFloatsMgr::getLeftFloatHeight (OOFAwareWidget *textblock, int y, int h,
                                      OOFAwareWidget *lastGB, int lastExtIndex)
{
   return getFloatHeight (textblock, LEFT, y, h, lastGB, lastExtIndex);
}

int OOFFloatsMgr::getRightFloatHeight (OOFAwareWidget *textblock, int y, int h,
                                       OOFAwareWidget *lastGB, int lastExtIndex)
{
   return getFloatHeight (textblock, RIGHT, y, h, lastGB, lastExtIndex);
}

int OOFFloatsMgr::getFloatHeight (OOFAwareWidget *textblock, Side side, int y,
                                  int h, OOFAwareWidget *lastGB,
                                  int lastExtIndex)
{
   DBG_OBJ_ENTER ("border", 0, "getFloatHeight", "%p, %s, %d, %d, %p, %d",
                  textblock, side == LEFT ? "LEFT" : "RIGHT", y, h,
                  lastGB, lastExtIndex);

   SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;
   int first = list->findFirst (textblock, y, h, lastGB, lastExtIndex, NULL);
   assert (first != -1);   /* This method must not be called when there is no
                              float on the respective side. */

   Float *vloat = list->get(first);
   int yRelToFloat;

   if (vloat->generatingBlock == textblock) {
      yRelToFloat = y - vloat->yReal;
      DBG_OBJ_MSGF ("border", 1, "caller is CB: yRelToFloat = %d - %d = %d",
                    y, vloat->yReal, yRelToFloat);
   } else {
      // The respective widgets are allocated; otherwise, hasFloat() would have
      // returned false.
      assert (wasAllocated (textblock));
      assert (vloat->getWidget()->wasAllocated ());

      Allocation *tba = getAllocation(textblock),
         *fla = vloat->getWidget()->getAllocation ();
      yRelToFloat = tba->y + y - fla->y;

      DBG_OBJ_MSGF ("border", 1,
                    "caller is not CB: yRelToFloat = %d + %d - %d = %d",
                    tba->y, y, fla->y, yRelToFloat);
   }

   ensureFloatSize (vloat);
   int height = vloat->size.ascent + vloat->size.descent - yRelToFloat;

   DBG_OBJ_MSGF ("border", 1, "=> (%d + %d) - %d = %d",
                 vloat->size.ascent, vloat->size.descent, yRelToFloat, height);
   DBG_OBJ_LEAVE ();
   return height;
}

/**
 * Returns position relative to the textblock "tb".
 */
int OOFFloatsMgr::getClearPosition (OOFAwareWidget *textblock)
{
   return getOOFAwareWidget(textblock)->clearPosition;
}

int OOFFloatsMgr::calcClearPosition (OOFAwareWidget *textblock)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getClearPosition", "%p", textblock);

   int pos;

   if (textblock->getStyle()) {
      bool left = false, right = false;
      switch (textblock->getStyle()->clear) {
      case CLEAR_NONE: break;
      case CLEAR_LEFT: left = true; break;
      case CLEAR_RIGHT: right = true; break;
      case CLEAR_BOTH: left = right = true; break;
      default: assertNotReached ();
      }

      pos = max (left ? calcClearPosition (textblock, LEFT) : 0,
                 right ? calcClearPosition (textblock, RIGHT) : 0);
   } else
      pos = 0;

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", pos);
   DBG_OBJ_LEAVE ();

   return pos;
}

bool OOFFloatsMgr::affectsLeftBorder (core::Widget *widget)
{
   return widget->getStyle()->vloat == core::style::FLOAT_LEFT;
}

bool OOFFloatsMgr::affectsRightBorder (core::Widget *widget)
{
   return widget->getStyle()->vloat == core::style::FLOAT_RIGHT;
};

bool OOFFloatsMgr::mayAffectBordersAtAll ()
{
   return true;
}

int OOFFloatsMgr::calcClearPosition (OOFAwareWidget *textblock, Side side)
{
   DBG_OBJ_ENTER ("resize.oofm", 0, "getClearPosition", "%p, %s",
                  textblock, side == LEFT ? "LEFT" : "RIGHT");

   int pos;

   if (!wasAllocated (textblock))
      // There is no relation yet to floats generated by other
      // textblocks, and this textblocks floats are unimportant for
      // the "clear" property.
      pos = 0;
   else {
      SortedFloatsVector *list = side == LEFT ? leftFloats : rightFloats;

      // Search the last float before (therfore -1) this textblock.
      int i = list->findFloatIndex (textblock, -1);
      if (i < 0) {
         pos = 0;
         DBG_OBJ_MSG ("resize.oofm", 1, "no float");
      } else {
         Float *vloat = list->get(i);
         assert (vloat->generatingBlock != textblock);
         if (!wasAllocated (vloat->generatingBlock))
            pos = 0; // See above.
         else {
            ensureFloatSize (vloat);
            pos = max (getAllocation(vloat->generatingBlock)->y + vloat->yReal
                       + vloat->size.ascent + vloat->size.descent
                       - getAllocation(textblock)->y,
                       0);
            DBG_OBJ_MSGF ("resize.oofm", 1,
                          "float %p => max (%d + %d + (%d + %d) - %d, 0)",
                          vloat->getWidget (),
                          getAllocation(vloat->generatingBlock)->y,
                          vloat->yReal, vloat->size.ascent, vloat->size.descent,
                          getAllocation(textblock)->y);
         }
      }
   }

   DBG_OBJ_MSGF ("resize.oofm", 1, "=> %d", pos);
   DBG_OBJ_LEAVE ();

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
      vloat->cbLineBreakWidth = container->getLineBreakWidth ();
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
   assertNotReached ();
   return 0;
}

int OOFFloatsMgr::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   assertNotReached ();
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
