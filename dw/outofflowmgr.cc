#include "outofflowmgr.hh"
#include "textblock.hh"

#include <limits.h>

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

int OutOfFlowMgr::Float::compareTo(Comparable *other)
{
   Float *otherFloat = (Float*)other;

   if (generatingBlock->wasAllocated()) {
      assert (otherFloat->generatingBlock->wasAllocated());
      return yForContainer() - otherFloat->yForContainer();
   } else {
      assert (generatingBlock == otherFloat->generatingBlock);
      return yReal - otherFloat->yReal;
   }
}

int OutOfFlowMgr::Float::yForTextblock (Textblock *textblock, int y)
{
   if (generatingBlock->wasAllocated()) {
      assert (textblock->wasAllocated());
      return generatingBlock->getAllocation()->y + y
         - textblock->getAllocation()->y;
   } else {
      assert (textblock == generatingBlock);
      return y;
   }
}

int OutOfFlowMgr::Float::yForContainer (int y)
{
   assert (generatingBlock->wasAllocated());
   return y - generatingBlock->getAllocation()->y +
      oofm->containingBlock->getAllocation()->y;
}

bool OutOfFlowMgr::Float::covers (Textblock *textblock, int y, int h)
{
   if (positioned) {
      int reqy, fly; // either widget or canvas coordinates
      if (generatingBlock->wasAllocated()) {
         assert (textblock->wasAllocated());
         reqy = textblock->getAllocation()->y + y;
         fly = generatingBlock->getAllocation()->y + yReal;
      } else {
         assert (textblock == generatingBlock);
         reqy = y;
         fly = yReal;
      }

      int flh = dirty ? 0 : size.ascent + size.descent;

      return fly + flh > reqy && fly < reqy + h;
   } else
      return false;
}

int OutOfFlowMgr::SortedFloatsVector::find (Textblock *textblock, int y)
{
   cleanup ();

   Float key (oofm);
   key.generatingBlock = textblock;
   key.yReal = y;

   return bsearch (&key, false);
}

int OutOfFlowMgr::SortedFloatsVector::findFirst (Textblock *textblock,
                                                 int y, int h)
{
   int i = find (textblock, y);
   if (i > 0 && get(i - 1)->covers (textblock, y, h))
      return i - 1;
   else if (i < size() && get(i)->covers (textblock, y, h))
      return i;
   else
      return -1;
}

void OutOfFlowMgr::SortedFloatsVector::moveTo (SortedFloatsVector *dest)
{
   for (int i = 0; i < size (); i++)
      dest->insert (get (i));
   clear ();
}

OutOfFlowMgr::TBInfo::TBInfo (OutOfFlowMgr *oofm)
{
   leftFloatsGB = new SortedFloatsVector (oofm);
   rightFloatsGB = new SortedFloatsVector (oofm);
}

OutOfFlowMgr::TBInfo::~TBInfo ()
{
   delete leftFloatsGB;
   delete rightFloatsGB;
}

OutOfFlowMgr::OutOfFlowMgr (Textblock *containingBlock)
{
   //printf ("OutOfFlowMgr::OutOfFlowMgr\n");

   this->containingBlock = containingBlock;

   leftFloatsCB = new SortedFloatsVector (this);
   rightFloatsCB = new SortedFloatsVector (this);

   leftFloatsAll = new Vector<Float> (1, true);
   rightFloatsAll = new Vector<Float> (1, true);

   floatsByWidget = new HashTable <TypedPointer <Widget>, Float> (true, false);
   tbInfos = new HashTable <TypedPointer <Textblock>, TBInfo> (true, true);

}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   //printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloatsCB;
   delete rightFloatsCB;

   delete tbInfos;
   delete floatsByWidget;

   // Order is important, since the instances of Float are owned by
   // leftFloatsAll and rightFloatsAll, so these should be deleted
   // last.
   delete leftFloatsAll;
   delete rightFloatsAll;
}

void OutOfFlowMgr::sizeAllocateStart (Allocation *containingBlockAllocation)
{
   this->containingBlockAllocation = *containingBlockAllocation;
}

void OutOfFlowMgr::sizeAllocateEnd ()
{
   // 1. ...  
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfos->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfos->get (key);

      tbInfo->leftFloatsGB->moveTo (leftFloatsCB);
      tbInfo->rightFloatsGB->moveTo (rightFloatsCB);
   }

   // 2. Floats have to be allocated
   sizeAllocateFloats (LEFT);
   sizeAllocateFloats (RIGHT);

   // 3. Textblocks have already been allocated, but we store some
   // information for later use. TODO: Update this comment!
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfos->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfos->get (key);
      Textblock *tb = key->getTypedValue();

      int xCB = tb->getAllocation()->x - containingBlockAllocation.x;
      int yCB = tb->getAllocation()->y - containingBlockAllocation.y;
      int width = tb->getAllocation()->width;
      int height = tb->getAllocation()->ascent + tb->getAllocation()->descent;

      if ((!tbInfo->wasAllocated || tbInfo->xCB != xCB || tbInfo->yCB != yCB ||
           tbInfo->width != width || tbInfo->height != height)) {
         int oldPos, newPos;
         Widget *oldFloat, *newFloat;
         // To calculate the minimum, both allocations, old and new,
         // have to be tested.
      
         // Old allocation:
         bool c1 = isTextblockCoveredByFloats (tb,
                                               tbInfo->xCB
                                               + containingBlockAllocation.x,
                                               tbInfo->yCB
                                               + containingBlockAllocation.y,
                                               tbInfo->width, tbInfo->height,
                                               &oldPos, &oldFloat);
         // new allocation:
         int c2 = isTextblockCoveredByFloats (tb, tb->getAllocation()->x,
                                              tb->getAllocation()->y, width,
                                              height, &newPos, &newFloat);
         if (c1 || c2) {
            if (!c1)
               tb->borderChanged (newPos, newFloat);
            else if (!c2)
               tb->borderChanged (oldPos, oldFloat);
            else {
               if (oldPos < newPos)
                  tb->borderChanged (oldPos, oldFloat);
               else
                  tb->borderChanged (newPos, newFloat);
            }
         }
      }
      
      tbInfo->wasAllocated = true;
      tbInfo->xCB = xCB;
      tbInfo->yCB = yCB;
      tbInfo->width = width;
      tbInfo->height = height;
   }
}

bool OutOfFlowMgr::isTextblockCoveredByFloats (Textblock *tb, int tbx, int tby,
                                               int tbWidth, int tbHeight,
                                               int *floatPos, Widget **vloat)
{
   int leftPos, rightPos;
   Widget *leftFloat, *rightFloat;
   bool c1 = isTextblockCoveredByFloats (leftFloatsCB, tb, tbx, tby,
                                         tbWidth, tbHeight, &leftPos,
                                         &leftFloat);
   bool c2 = isTextblockCoveredByFloats (rightFloatsCB, tb, tbx, tby,
                                         tbWidth, tbHeight, &rightPos,
                                         &rightFloat);
   if (c1 || c2) {
      if (!c1) {
         *floatPos = rightPos;
         *vloat = rightFloat;
      } else if (!c2) {
         *floatPos = leftPos;
         *vloat = leftFloat;
      } else {
         if (leftPos < rightPos) {
            *floatPos = leftPos;
            *vloat = leftFloat;
         } else{ 
            *floatPos = rightPos;
            *vloat = rightFloat;
         }
      }
   }

   return c1 || c2;
}

bool OutOfFlowMgr::isTextblockCoveredByFloats (SortedFloatsVector *list,
                                               Textblock *tb, int tbx, int tby,
                                               int tbWidth, int tbHeight,
                                               int *floatPos, Widget **vloat)
{
   *floatPos = INT_MAX;
   bool covered = false;

   for (int i = 0; i < list->size(); i++) {
      // TODO binary search
      Float *v = list->get(i);

      // This method is called within OOFM::sizeAllocate, which is
      // called in Textblock::sizeAllocateImpl for the containing
      // block, so that the only textblock which is not necessary
      // allocates is the containing block.
      assert (v->generatingBlock->wasAllocated() ||
              v->generatingBlock == containingBlock);

      // In this case we have to refer to the allocation passed to
      // Textblock::sizeAllocateImpl.
      Allocation *generatingBlockAllocation =
         v->generatingBlock == containingBlock ?
         &containingBlockAllocation : v->generatingBlock->getAllocation();

      // Idea: the distinction could be removed, and the code so made
      // simpler, by moving this second part of OOFM::sizeAllocate
      // into an idle function. This would also make the question
      // obsolete whether it is allowed to call queueResize within
      // sizeAllocate. Unfortunately, idle funtions currently only
      // refer to layouts, not to widgets.
      
      if (tb != v->generatingBlock && v->positioned) {
         int flh = v->dirty ? 0 : v->size.ascent + v->size.descent;
         int y1 = generatingBlockAllocation->y + v->yReal;
         int y2 = y1 + flh;
         
         // TODO: Also regard horizontal dimension (same for tellPositionOrNot).
         if (y2 > tby && y1 < tby + tbHeight) {
            covered = true;
            if (y1 - tby < *floatPos) {
               *floatPos = y1 - tby;
               *vloat = v->widget;
            }
         }
      }

      // All floarts are searched, to find the minimum. TODO: Are
      // floats sorted, so this can be shortene? (The first is the
      // minimum?)
   }

   return covered;
}

void OutOfFlowMgr::sizeAllocateFloats (Side side)
{
   SortedFloatsVector *list = side == LEFT ? leftFloatsCB : rightFloatsCB;

   for (int i = 0; i < list->size(); i++) {
      // TODO Missing: check newly calculated positions, collisions,
      // and queue resize, when neccessary. TODO: See step 2?

      Float *vloat = list->get(i);
      assert (vloat->positioned);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      switch (side) {
      case LEFT:
         childAllocation.x = vloat->generatingBlock->getAllocation()->x
            + vloat->generatingBlock->getStyle()->boxOffsetX();
         break;

      case RIGHT:
         childAllocation.x =
            vloat->generatingBlock->getAllocation()->x
            + min (vloat->generatingBlock->getAllocation()->width,
                   vloat->generatingBlock->getAvailWidth())
            - vloat->size.width
            - vloat->generatingBlock->getStyle()->boxRestWidth();
         break;
      }

      childAllocation.y =
         vloat->generatingBlock->getAllocation()->y + vloat->yReal;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;
      
      vloat->widget->sizeAllocate (&childAllocation);

      //printf ("allocate %s #%d -> (%d, %d), %d x (%d + %d)\n",
      //        right ? "right" : "left", i, childAllocation.x,
      //        childAllocation.y, childAllocation.width,
      //        childAllocation.ascent, childAllocation.descent);
   }
}



void OutOfFlowMgr::draw (View *view, Rectangle *area)
{
   draw (leftFloatsCB, view, area);
   draw (rightFloatsCB, view, area);
}

void OutOfFlowMgr::draw (SortedFloatsVector *list, View *view, Rectangle *area)
{
   // This could be improved, since the list is sorted: search the
   // first float fitting into the area, and iterate until one is
   // found below the area.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      assert (vloat->positioned);

      core::Rectangle childArea;
      if (vloat->widget->intersects (area, &childArea))
         vloat->widget->draw (view, &childArea);
   }
}

bool OutOfFlowMgr::isWidgetOutOfFlow (core::Widget *widget)
{
   // Will be extended for absolute positions.
   return widget->getStyle()->vloat != FLOAT_NONE;
}

void OutOfFlowMgr::addWidget (Widget *widget, Textblock *generatingBlock)
{
   if (widget->getStyle()->vloat != FLOAT_NONE) {
      TBInfo *tbInfo = registerCaller (generatingBlock);

      Float *vloat = new Float (this);
      vloat->widget = widget;
      vloat->generatingBlock = generatingBlock;
      vloat->dirty = true;
      vloat->positioned = false;
      vloat->yReq = vloat->yReal = INT_MIN;

      switch (widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         leftFloatsAll->put (vloat);
         widget->parentRef = createRefLeftFloat (leftFloatsAll->size() - 1);

         if (generatingBlock->wasAllocated()) {
            leftFloatsCB->insert (vloat);
            //printf ("[%p] adding float %p to CB list\n",
            //        containingBlock, vloat);
         } else {
            tbInfo->leftFloatsGB->insert (vloat);
            //printf ("[%p] adding float %p to GB list\n",
            //        containingBlock, vloat);
         }
         break;

      case FLOAT_RIGHT:
         rightFloatsAll->put (vloat);
         widget->parentRef = createRefRightFloat (rightFloatsAll->size() - 1);

         if (generatingBlock->wasAllocated()) {
            rightFloatsCB->insert (vloat);
            //printf ("[%p] adding float %p to CB list\n",
            //        containingBlock, vloat);
         } else {
            tbInfo->rightFloatsGB->insert (vloat);
            //printf ("[%p] adding float %p to GB list\n",
            //        containingBlock, vloat);
         }

         break;

      default:
         assertNotReached();
      }

      floatsByWidget->put (new TypedPointer<Widget> (widget), vloat);
   } else
      // Will continue here for absolute positions.
      assertNotReached();
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
   //printf ("[%p] MARK_SIZE_CHANGE (%d)\n", containingBlock, ref);

   if (isRefFloat (ref)) {
      Float *vloat;
      
      if (isRefLeftFloat (ref))
         vloat = leftFloatsAll->get (getFloatIndexFromRef (ref));
      else if (isRefRightFloat (ref))
         vloat = rightFloatsAll->get (getFloatIndexFromRef (ref));
      else {
         assertNotReached();
         vloat = NULL; // compiler happiness
      }
      
      vloat->dirty = true;
      // In some cases, the vertical position is not yet defined. Nothing
      // necessary then.
      if (vloat->positioned)
         vloat->generatingBlock->borderChanged (vloat->yReal, vloat->widget);
   } else
      // later: absolute positions
      assertNotReached();
}


void OutOfFlowMgr::markExtremesChange (int ref)
{
   // Nothing to do here.
}

Widget *OutOfFlowMgr::getWidgetAtPoint (int x, int y, int level)
{
   Widget *childAtPoint = getWidgetAtPoint (leftFloatsCB, x, y, level);
   if (childAtPoint == NULL)
      childAtPoint = getWidgetAtPoint (rightFloatsCB, x, y, level);
   return childAtPoint;
}

Widget *OutOfFlowMgr::getWidgetAtPoint (SortedFloatsVector *list,
                                        int x, int y, int level)
{
   for (int i = 0; i < list->size(); i++) {
      // Could use binary search to be faster.
      Float *vloat = list->get(i);
      Widget *childAtPoint = vloat->widget->getWidgetAtPoint (x, y, level + 1);
      if (childAtPoint)
         return childAtPoint;
   }

   return NULL;
}


/**
 * This method is called by Textblock::rewrap at the beginning, to
 * avoid wrong positions.
 */
void OutOfFlowMgr::tellNoPosition (Widget *widget)
{
   tellPositionOrNot (widget, INT_MIN, false);
}


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   assert (y >= 0);
   tellPositionOrNot (widget, y, true);
}

void OutOfFlowMgr::tellPositionOrNot (Widget *widget, int y, bool positioned)
{
   Float *vloat = findFloatByWidget(widget);
   //printf ("[%p] TELL_POSITION_OR_NOT: vloat = %p, y = %d (%d => %d), "
   //        "positioned = %s (%s)\n", containingBlock, vloat,
   //        y, vloat->yReq, vloat->yReal,
   //        positioned ? "true" : "false",
   //        vloat->positioned ? "true" : "false");
      

   if ((!positioned && !vloat->positioned) ||
       (positioned && vloat->positioned && y == vloat->yReq))
      // Nothing changed.
      return;

   SortedFloatsVector *listSame, *listOpp;
   getFloatsLists (vloat, &listSame, &listOpp);

   if (positioned)
      ensureFloatSize (vloat);
   int oldY = vloat->yReal;
   bool oldPositioned = vloat->positioned;

   // "yReal" may change due to collisions (see below).
   vloat->yReq = vloat->yReal = y;
   vloat->positioned = positioned;

   if (positioned) {
      // Test collisions (on both sides.

      // It is assumed that there are no floats below this float
      // within this generator. For this reason, (i) search is simple
      // (candidate is at the end of the list), and (ii) no other
      // floats have to be adjusted.

      if (listSame->size() > 1) {
         Float *last = listSame->get (listSame->size () - 1);
         if (last == vloat) // TODO Should this not always be the case?
            last = listSame->get (listSame->size () - 2);
         
         if (last->covers (vloat->generatingBlock, vloat->yReal,
                           vloat->size.ascent + vloat->size.descent))
            vloat->yReal = last->yForTextblock (vloat->generatingBlock)
               + last->size.ascent + last->size.descent;
      }

      if (listOpp->size() > 0) {
         Float *last = listOpp->get (listOpp->size () - 1);
         if (last->covers (vloat->generatingBlock, vloat->yReal,
                           vloat->size.ascent + vloat->size.descent)) {
            // Here, test also horizontal values.
            bool collidesH;
            if (vloat->generatingBlock == last->generatingBlock)
               collidesH = vloat->size.width + last->size.width +
                  vloat->generatingBlock->getStyle()->boxDiffWidth()
                  > vloat->generatingBlock->getAvailWidth();
            else {
               // Here (different generating blocks) it can be assumed
               // that the allocations are defined (because
               // Float::covers(...) would have returned false,
               // otherwise).
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
               vloat->yReal = last->yForTextblock (vloat->generatingBlock)
                  + last->size.ascent + last->size.descent;
         }
      }
   }

   listSame->change (vloat);

   // Only this float has been changed (see above), so only this float
   // has to be tested against all textblocks.
   if (vloat->generatingBlock->wasAllocated () &&
       // A change from "no position" to "no position" is uninteresting.
       !(oldPositioned && !vloat->positioned)) {
      int yChange;

      if (!vloat->positioned) {
         // position -> no position
         assert (oldPositioned);
         yChange = oldY;
      } else if (!oldPositioned) {
         // no position -> position
         assert (vloat->positioned);
         yChange = vloat->yReal;
      } else {
         // position -> position
         assert (oldPositioned && vloat->positioned);
         yChange = min (oldY, vloat->yReal);
      }
      
      // TODO This (and similar code) is not very efficient.
      for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
              tbInfos->iterator ();
           it.hasNext (); ) {
         TypedPointer <Textblock> *key = it.getNext ();
         Textblock *textblock = key->getTypedValue();

         if (textblock->wasAllocated () &&
             // If not positioned, there will be soon a rewrap (see
             // Textblock::rewrap), or, if positioned , the generating
             // block takes care of the possible change (see
             // Textblock::wrapWidgetOofRef), respectively; so, only
             // the other textblocks must be told about this.
             textblock != vloat->generatingBlock) {
            int tby1 = textblock->getAllocation()->y;
            int tby2 = tby1 + textblock->getAllocation()->ascent
               + textblock->getAllocation()->descent;
            int flh = 
               vloat->dirty ? 0 : vloat->size.ascent + vloat->size.descent;
            bool covered = false;

            if (oldPositioned) {
               int y1 = vloat->generatingBlock->getAllocation()->y + oldY;
               int y2 = y1 + flh;
               covered = y2 > tby1 && y1 < tby2;
            }

            if (!covered && vloat->positioned) {
               int y1 =
                  vloat->generatingBlock->getAllocation()->y + vloat->yReal;
               int y2 = y1 + flh;
               covered = y2 > tby1 && y1 < tby2;
            }

            if (covered) {
               int yTextblock =
                  vloat->generatingBlock->getAllocation()->y + yChange
                  - textblock->getAllocation()->y;
               textblock->borderChanged (yTextblock, vloat->widget);
            }
         }
      }
   }
}

void OutOfFlowMgr::getFloatsLists (Float *vloat, SortedFloatsVector **listSame,
                                   SortedFloatsVector **listOpp)
{
   TBInfo *tbInfo = registerCaller (vloat->generatingBlock);
      
   switch (vloat->widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      if (vloat->generatingBlock->wasAllocated()) {
         if (listSame) *listSame = leftFloatsCB;
         if (listOpp) *listOpp = rightFloatsCB;
      } else {
         if (listSame) *listSame = tbInfo->leftFloatsGB;
         if (listOpp) *listOpp = tbInfo->rightFloatsGB;
      }
      break;

   case FLOAT_RIGHT:
      if (vloat->generatingBlock->wasAllocated()) {
         if (listSame) *listSame = rightFloatsCB;
         if (listOpp) *listOpp = leftFloatsCB;
      } else {
         if (listSame) *listSame = tbInfo->rightFloatsGB;
         if (listOpp) *listOpp = tbInfo->leftFloatsGB;
      }
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

   *oofWidth = cbWidth; /* This (or "<=" instead of "=") should be
                           the case for floats. */

   int oofHeightLeft = getFloatsSize (leftFloatsCB);
   int oofHeightRight = getFloatsSize (rightFloatsCB);
   *oofHeight = max (oofHeightLeft, oofHeightRight);
}

int OutOfFlowMgr::getFloatsSize (SortedFloatsVector *list)
{
   int height = containingBlock->getStyle()->boxDiffHeight();

   // Idea for a faster implementation: find the last float; this
   // should be the relevant one, since the list is sorted.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      if (vloat->positioned) {
         ensureFloatSize (vloat);

         // Notice that all positions are relative to the generating
         // block, but we need them relative to the containing block.

         // Position of generating block, relative to containing
         // block. Greater or equal than 0, so dealing with 0 when it
         // cannot yet be calculated is safe. (No distiction whether
         // it is defined or not is necessary.)
         int yGBinCB;

         if (vloat->generatingBlock == containingBlock)
            // Simplest case: the generator is the container.
            yGBinCB = 0;
         else {
            if (containingBlock->wasAllocated()) {
               if (vloat->generatingBlock->wasAllocated())
                  // Simple case: both containing block and generating
                  // block are defined.
                  yGBinCB = vloat->generatingBlock->getAllocation()->y
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

         height =
            max (height,
                 yGBinCB + vloat->yReal
                 + vloat->size.ascent + vloat->size.descent
                 + containingBlock->getStyle()->boxRestHeight());
      }
   }

   return height;
}

void OutOfFlowMgr::getExtremes (int cbMinWidth, int cbMaxWidth,
                                int *oofMinWidth, int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;
   accumExtremes (leftFloatsCB, oofMinWidth, oofMaxWidth);
   accumExtremes (rightFloatsCB, oofMinWidth, oofMaxWidth);
}

void OutOfFlowMgr::accumExtremes (SortedFloatsVector *list, int *oofMinWidth,
                                  int *oofMaxWidth)
{
   // Idea for a faster implementation: use incremental resizing?

   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      // Difference between generating block and to containing block,
      // sum on both sides. Greater or equal than 0, so dealing with 0
      // when it cannot yet be calculated is safe. (No distiction
      // whether it is defined or not is necessary.)
      int borderDiff;

      if (vloat->generatingBlock == containingBlock)
         // Simplest case: the generator is the container.
         borderDiff = 0;
      else {
         if (containingBlock->wasAllocated()) {
            if (vloat->generatingBlock->wasAllocated())
               // Simple case: both containing block and generating
               // block are defined.
               borderDiff = containingBlock->getAllocation()->width -
                  vloat->generatingBlock->getAllocation()->width;
            else
               // Generating block not yet allocation; the next
               // allocation will, when necessary, trigger
               // getExtremes. (TODO: Is this really the case?)
               borderDiff = 0;
         } else
            // Nothing can be done now, but the next allocation will
            // trigger getExtremes. (TODO: Is this really the case?)
            borderDiff = 0;
      }

      Extremes extr;
      vloat->widget->getExtremes (&extr);

      *oofMinWidth = max (*oofMinWidth, extr.minWidth + borderDiff);
      *oofMaxWidth = max (*oofMaxWidth, extr.maxWidth + borderDiff);
   }
}

OutOfFlowMgr::TBInfo *OutOfFlowMgr::registerCaller (Textblock *textblock)
{
   TypedPointer<Textblock> key (textblock);
   TBInfo *tbInfo = tbInfos->get (&key);
   if (tbInfo == NULL) {
      tbInfo = new TBInfo (this);
      tbInfo->wasAllocated = false;
      tbInfos->put (new TypedPointer<Textblock> (textblock), tbInfo);
   }

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
int OutOfFlowMgr::getLeftBorder (Textblock *textblock, int y, int h)
{
   return getBorder (textblock, LEFT, y, h);
}

/**
 * Get the right border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * See also getLeftBorder(int, int);
 */
int OutOfFlowMgr::getRightBorder (Textblock *textblock, int y, int h)
{
   return getBorder (textblock, RIGHT, y, h);
}

int OutOfFlowMgr::getBorder (Textblock *textblock, Side side, int y, int h)
{
   TBInfo *tbInfo = registerCaller (textblock);

   SortedFloatsVector *list;
   if (textblock->wasAllocated())
      list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   else
      list = side == LEFT ? tbInfo->leftFloatsGB : tbInfo->rightFloatsGB;

   int first = list->findFirst (textblock, y, h);
   if (first == -1)
      // No float.
      return 0;
   else {
      // It is not sufficient to find the first float, since a line
      // (with height h) may cover the region of multiple float, of
      // which the widest has to be choosen.
      int border = 0;
      bool covers = true;
      for (int i = first; covers && i < list->size(); i++) {
         Float *vloat = list->get(i);
         covers = vloat->covers (textblock, y, h);
         if (covers) {
            int borderDiff = getBorderDiff (textblock, vloat, side);
            int borderIn = side == LEFT ?
               vloat->generatingBlock->getStyle()->boxOffsetX() :
               vloat->generatingBlock->getStyle()->boxRestWidth();
            border = max (border, vloat->size.width + borderIn + borderDiff);
         }
      }

      return border;
   }
}

bool OutOfFlowMgr::hasFloatLeft (Textblock *textblock, int y, int h)
{
   return hasFloat (textblock, LEFT, y, h);
}

bool OutOfFlowMgr::hasFloatRight (Textblock *textblock, int y, int h)
{
   return hasFloat (textblock, RIGHT, y, h);
}

bool OutOfFlowMgr::hasFloat (Textblock *textblock, Side side, int y, int h)
{
   // Compare to getBorder(). Actually much copy and paste.

   TBInfo *tbInfo = registerCaller (textblock);

   SortedFloatsVector *list;
   if (textblock->wasAllocated())
      list = side == LEFT ? leftFloatsCB : rightFloatsCB;
   else
      list = side == LEFT ? tbInfo->leftFloatsGB : tbInfo->rightFloatsGB;

   // TODO binary search
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);

      int yWidget;
      if (getYWidget (textblock, vloat, &yWidget)
          && y + h > yWidget
          && y < yWidget + vloat->size.ascent + vloat->size.descent) {
         // As opposed to getBorder, finding the first float is
         // sufficient.

         //printf ("[%p] float on %s side (%d, %d): (%d, %d)\n",
         //        textblock, right ? "right" : "left", y, h,
         //        yWidget, vloat->size.ascent + vloat->size.descent);
         return true;
      }
   }

   //printf ("[%p] no float on %s side (%d, %d)\n",
   //        textblock, right ? "right" : "left", y, h);

   return false;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty) {
      // TODO Ugly. Soon to be replaced by cleaner code? See also
      // comment in Textblock::calcWidgetSize.
      if (vloat->widget->usesHints ()) {
         if (isAbsLength (vloat->widget->getStyle()->width))
            vloat->widget->setWidth
               (absLengthVal (vloat->widget->getStyle()->width));
         else if (isPerLength (vloat->widget->getStyle()->width))
            vloat->widget->setWidth
               (containingBlock->getAvailWidth()
                * perLengthVal (vloat->widget->getStyle()->width));
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
      vloat->widget->getExtremes (&extremes);

      vloat->widget->sizeRequest (&vloat->size);

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
          
      vloat->widget->setWidth (width);
      vloat->widget->sizeRequest (&vloat->size);
      
      //printf ("   Float %p: %d x (%d + %d)\n", vloat, vloat->size.width,
      //        vloat->size.ascent, vloat->size.descent);
          
      vloat->dirty = false;
   }
}

/**
 * Returns true, when position can be determined; in this case, the
 * position is tored in *yWidget.
 */
bool OutOfFlowMgr::getYWidget (Textblock *textblock, Float *vloat, int *yWidget)
{
   if (!vloat->positioned)
      return false;
   else if (textblock == vloat->generatingBlock) {
      *yWidget = vloat->yReal;
      return true;
   } else {
      if (textblock->wasAllocated() && vloat->generatingBlock->wasAllocated()) {
         *yWidget = vloat->yReal + vloat->generatingBlock->getAllocation()->y
            - textblock->getAllocation()->y;
         return true;
      } else {
         // Should not happen, when the correct list is choosen.
         assertNotReached ();
         return false;
      }
   }
}

/**
 * Return the between generator and calling textblock. (TODO Exact
 * definition. See getBorder(), where it is used.)
 *
 * Assumes that the position can be determined (getYWidget() returns true).
 */
int OutOfFlowMgr::getBorderDiff (Textblock *textblock, Float *vloat, Side side)
{
   assert (vloat->positioned);

   if (textblock == vloat->generatingBlock)
      return 0;
   else {
      assert (textblock->wasAllocated() &&
              vloat->generatingBlock->wasAllocated());
      
      switch (side) {
      case LEFT:
         return vloat->generatingBlock->getAllocation()->x
            - textblock->getAllocation()->x;

      case RIGHT:
         return
            textblock->getAllocation()->x + textblock->getAllocation()->width
            - (vloat->generatingBlock->getAllocation()->x +
               min (vloat->generatingBlock->getAllocation()->width,
                    vloat->generatingBlock->getAvailWidth()));

      default:
         assertNotReached();
         return 0;
      }
   }
}

// TODO Latest change: Check also Textblock::borderChanged: looks OK,
// but the comment ("... (i) with canvas coordinates ...") looks wrong
// (and looks as having always been wrong).

// Another issue: does it make sense to call Textblock::borderChanged
// for generators, when the respective widgets have not been called
// yet?

} // namespace dw
