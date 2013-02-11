#include "outofflowmgr.hh"
#include "textblock.hh"

using namespace lout::object;
using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

int OutOfFlowMgr::Float::yForContainer (OutOfFlowMgr *oofm, int y)
{
   return y - generatingBlock->getAllocation()->y +
      oofm->containingBlock->getAllocation()->y;
}

OutOfFlowMgr::OutOfFlowMgr (Textblock *containingBlock)
{
   //printf ("OutOfFlowMgr::OutOfFlowMgr\n");

   this->containingBlock = containingBlock;
   availWidth = availAscent = availDescent = -1;   
   leftFloats = new Vector<Float> (1, true);
   rightFloats = new Vector<Float> (1, true);
   tbInfos = new HashTable <TypedPointer <Textblock>, TBInfo> (true, true);
}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   //printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloats;
   delete rightFloats;
   delete tbInfos;
}

void OutOfFlowMgr::sizeAllocate (Allocation *containingBlockAllocation)
{
   // 1. Floats have to be allocated
   sizeAllocateFloats (leftFloats, false, containingBlockAllocation);
   sizeAllocateFloats (rightFloats, true, containingBlockAllocation);

   // 2. Textblocks have already been allocated, but we store some
   // information for later use. TODO: Update this comment!
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfos->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfos->get (key);
      Textblock *tb = key->getTypedValue();

      int xCB = tb->getAllocation()->x - containingBlockAllocation->x;
      int yCB = tb->getAllocation()->y - containingBlockAllocation->y;
      int width = tb->getAllocation()->width;
      int height = tb->getAllocation()->ascent + tb->getAllocation()->descent;

      if ((!tbInfo->wasAllocated || tbInfo->xCB != xCB || tbInfo->yCB != yCB ||
           tbInfo->width != width || tbInfo->height != height) &&
          (// old allocation
           isTextblockCoveredByFloats (containingBlockAllocation, tb,
                                       tbInfo->xCB
                                       + containingBlockAllocation->x,
                                       tbInfo->yCB
                                       + containingBlockAllocation->y,
                                       tbInfo->width, tbInfo->height) ||
           // new allocation
           isTextblockCoveredByFloats (containingBlockAllocation, tb,
                                       tb->getAllocation()->x,
                                       tb->getAllocation()->y,
                                       width, height)))
         // TODO Better let isTextblockCoveredByFloats return a value?
         tb->borderChanged (0);
      
      tbInfo->wasAllocated = true;
      tbInfo->xCB = xCB;
      tbInfo->yCB = yCB;
      tbInfo->width = width;
      tbInfo->height = height;
   }
}

bool OutOfFlowMgr::isTextblockCoveredByFloats (Allocation
                                               *containingBlockAllocation,
                                               Textblock *tb, int tbx, int tby,
                                               int tbWidth, int tbHeight)
{
   return
      isTextblockCoveredByFloats (leftFloats, containingBlockAllocation,
                                  tb, tbx, tby, tbWidth, tbHeight) ||
      isTextblockCoveredByFloats (rightFloats, containingBlockAllocation,
                                  tb, tbx, tby, tbWidth, tbHeight);
}

bool OutOfFlowMgr::isTextblockCoveredByFloats (Vector<Float> *list,
                                               Allocation
                                               *containingBlockAllocation,
                                               Textblock *tb, int tbx, int tby,
                                               int tbWidth, int tbHeight)
{
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);

      // This method is called within OOFM::sizeAllocate, which is
      // called in Textblock::sizeAllocateImpl for the containing
      // block, so that the only textblock which is not necessary
      // allocates is the containing block.
      assert (vloat->generatingBlock->wasAllocated() ||
              vloat->generatingBlock == containingBlock);

      // In this case we have to refer to the allocation passed to
      // Textblock::sizeAllocateImpl.
      Allocation *generatingBlockAllocation =
         vloat->generatingBlock == containingBlock ?
         containingBlockAllocation : vloat->generatingBlock->getAllocation();

      // Idea: the distinction could be removed, and the code so made
      // simpler, by moving this second part of OOFM::sizeAllocate
      // into an idle function. This would also make the question
      // obsolete whether it is allowed to call queueResize within
      // sizeAllocate. Unfortunately, idle funtions currently only
      // refer to layouts, not to widgets.
      
      if (tb != vloat->generatingBlock && vloat->positioned) {
         int flh = vloat->dirty ? 0 : vloat->size.ascent + vloat->size.descent;
         int y1 = generatingBlockAllocation->y + vloat->yReal;
         int y2 = y1 + flh;
         
         // TODO: Also regard horizontal dimension (same for tellPositionOrNot).
         if (y2 > tby && y1 < tby + tbWidth)
            return true;
      }
   }

   return false;
}

void OutOfFlowMgr::sizeAllocateFloats (Vector<Float> *list, bool right,
                                       Allocation *containingBlockAllocation)
{
   //int width =
   //   availWidth != -1 ? availWidth : containingBlockAllocation->width;

   for (int i = 0; i < list->size(); i++) {
      // TODO Missing: check newly calculated positions, collisions,
      // and queue resize, when neccessary. TODO: See step 2?

      Float *vloat = list->get(i);
      assert (vloat->positioned);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      if (right)
         // TODO Corrected by availWidth; see commented "width" above.
         childAllocation.x =
            vloat->generatingBlock->getAllocation()->x
            + vloat->generatingBlock->getAllocation()->width
            - vloat->size.width
            - vloat->generatingBlock->getStyle()->boxRestWidth();
      else
         childAllocation.x = vloat->generatingBlock->getAllocation()->x
            + vloat->generatingBlock->getStyle()->boxOffsetX();

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
   draw (leftFloats, view, area);
   draw (rightFloats, view, area);
}

void OutOfFlowMgr::draw (Vector<Float> *list, View *view, Rectangle *area)
{
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      assert (vloat->positioned);

      core::Rectangle childArea;
      if (vloat->widget->intersects (area, &childArea))
         vloat->widget->draw (view, &childArea);
   }
}

void OutOfFlowMgr::queueResize(int ref)
{
   // TODO Is there something to do?
}

bool OutOfFlowMgr::isWidgetOutOfFlow (core::Widget *widget)
{
   // Will be extended for absolute positions.
   return widget->getStyle()->vloat != FLOAT_NONE;
}

void OutOfFlowMgr::addWidget (Widget *widget, Textblock *generatingBlock)
{
   if (widget->getStyle()->vloat != FLOAT_NONE) {
      Float *vloat = new Float ();
      vloat->widget = widget;
      vloat->generatingBlock = generatingBlock;
      vloat->dirty = true;
      vloat->yReq = vloat->positioned;

      switch (widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         leftFloats->put (vloat);
         widget->parentRef = createRefLeftFloat (leftFloats->size() - 1);
         break;

      case FLOAT_RIGHT:
         rightFloats->put (vloat);
         widget->parentRef = createRefRightFloat (rightFloats->size() - 1);
         break;

      default:
         assertNotReached();
      }
   } else
      // Will continue here for absolute positions.
      assertNotReached();
}

OutOfFlowMgr::Float *OutOfFlowMgr::findFloatByWidget (Widget *widget)
{
   Vector<Float> *list = getFloatList (widget);

   for(int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      if(vloat->widget == widget)
         return vloat;
   }

   assertNotReached();
   return NULL;
}

Vector<OutOfFlowMgr::Float> *OutOfFlowMgr::getFloatList (Widget *widget)
{
   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return leftFloats;

   case FLOAT_RIGHT:
      return rightFloats;

   default:
      assertNotReached();
      return NULL;
   }
}

Vector<OutOfFlowMgr::Float> *OutOfFlowMgr::getOppositeFloatList (Widget *widget)
{
   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return rightFloats;

   case FLOAT_RIGHT:
      return leftFloats;

   default:
      assertNotReached();
      return NULL;
   }
}

void OutOfFlowMgr::markSizeChange (int ref)
{
   //printf ("[%p] MARK_SIZE_CHANGE (%d)\n", containingBlock, ref);

   if (isRefFloat (ref)) {
      Float *vloat;
      
      if (isRefLeftFloat (ref))
         vloat = leftFloats->get (getFloatIndexFromRef (ref));
      else if (isRefRightFloat (ref))
         vloat = rightFloats->get (getFloatIndexFromRef (ref));
      else {
         assertNotReached();
         vloat = NULL; // compiler happiness
      }
      
      vloat->dirty = true;
      // In some cases, the vertical position is not yet defined. Nothing
      // necessary then.
      if (vloat->positioned)
         vloat->generatingBlock->borderChanged (vloat->yReal);
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
   Widget *childAtPoint = getWidgetAtPoint (leftFloats, x, y, level);
   if (childAtPoint == NULL)
      childAtPoint = getWidgetAtPoint (rightFloats, x, y, level);
   return childAtPoint;
}

Widget *OutOfFlowMgr::getWidgetAtPoint (Vector<Float> *list,
                                        int x, int y, int level)
{
   for (int i = 0; i < list->size(); i++) {
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
   tellPositionOrNot (widget, 0, false);
}


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   assert (y >= 0);
   tellPositionOrNot (widget, y, true);
}

void OutOfFlowMgr::tellPositionOrNot (Widget *widget, int y, bool positioned)
{
   Float *vloat = findFloatByWidget(widget);
   if ((!positioned && !vloat->positioned) ||
       (positioned && vloat->positioned && y == vloat->yReq))
      // Nothing happened.
      return;
   
   if (positioned)
      ensureFloatSize (vloat);
   int oldY = vloat->yReal;
   bool oldPositioned = vloat->positioned;
   vloat->yReq = y;

   if (positioned) {
      // TODO Test collisions (check old code).

      // It is assumed that there are no floats below this float
      // within this generator. For this reason, no other floats have
      // to be adjusted.
   }

   vloat->yReal = y; // Due to collisions, vloat->y may be different from y.
   vloat->positioned = positioned;

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
               textblock->borderChanged (yTextblock);
            }
         }
      }
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

#if 0
   // TODO Latest change: Check and re-activate.

   int oofHeightLeft = containingBlock->asWidget()->getStyle()->boxDiffWidth();
   int oofHeightRight = containingBlock->asWidget()->getStyle()->boxDiffWidth();

   for (int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      if (vloat->positioned) {
         ensureFloatSize (vloat);
         oofHeightLeft =
            max (oofHeightLeft,
                 vloat->y + vloat->size.ascent + vloat->size.descent
                 + containingBlock->asWidget()->getStyle()->boxRestHeight());
      }
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      if (vloat->positioned) {
         ensureFloatSize (vloat);
         oofHeightRight =
            max (oofHeightRight,
                 vloat->y + vloat->size.ascent + vloat->size.descent
                 + containingBlock->asWidget()->getStyle()->boxRestHeight());
      }
   }

   *oofHeight = max (oofHeightLeft, oofHeightRight);
#endif
}

void OutOfFlowMgr::getExtremes (int cbMinWidth, int cbMaxWidth,
                                int *oofMinWidth, int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;
   accumExtremes (leftFloats, oofMinWidth, oofMaxWidth);
   accumExtremes (rightFloats, oofMinWidth, oofMaxWidth);
}

void OutOfFlowMgr::accumExtremes (Vector<Float> *list, int *oofMinWidth,
                                  int *oofMaxWidth)
{
   // TODO Latest change: Check and re-activate.

#if 0
   for (int i = 0; i < list->size(); i++) {
      Float *v = list->get(i);
      Extremes extr;
      v->widget->getExtremes (&extr);
      // TODO Calculation of borders is repeated quite much.
      int borderDiff = calcLeftBorderDiff (v) + calcRightBorderDiff (v);
      *oofMinWidth = max (*oofMinWidth, extr.minWidth + borderDiff);
      *oofMaxWidth = max (*oofMaxWidth, extr.maxWidth + borderDiff);
   }
#endif
}
   
/**
 * Get the left border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * The border includes marging/border/padding of the containging
 * block, but is 0 if there is no float, so a caller should also
 * consider other borders.
 */
int OutOfFlowMgr::getLeftBorder (Textblock *textblock, int y, int h)
{
   return getBorder (textblock, leftFloats, false, y, h);
}

/**
 * Get the right border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * See also getLeftBorder(int, int);
 */
int OutOfFlowMgr::getRightBorder (Textblock *textblock, int y, int h)
{
   return getBorder (textblock, rightFloats, true, y, h);
}

int OutOfFlowMgr::getBorder (Textblock *textblock, Vector<Float> *list,
                             bool right, int y, int h)
{
   int border = 0;

   TypedPointer<Textblock> key (textblock);
   TBInfo *tbInfo = tbInfos->get (&key);
   if (tbInfo == NULL) {
      tbInfo = new TBInfo;
      tbInfo->wasAllocated = false;
      tbInfos->put (new TypedPointer<Textblock> (textblock), tbInfo);
   }

   // To be a bit more efficient, one could use linear search to find
   // the first affected float.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);
      int yWidget, borderDiff;
      bool positioned;

      if (!vloat->positioned)
         positioned = false;
      else if (textblock == vloat->generatingBlock) {
         positioned = true;
         yWidget = vloat->yReal;
         borderDiff = 0;
      } else {
         if (textblock->wasAllocated() &&
             vloat->generatingBlock->wasAllocated()) {
            positioned = true;
            yWidget = vloat->yReal + vloat->generatingBlock->getAllocation()->y
               - textblock->getAllocation()->y;

            if (right)
               borderDiff =
                  textblock->getAllocation()->x +
                  textblock->getAllocation()->width -
                  (vloat->generatingBlock->getAllocation()->x +
                   vloat->generatingBlock->getAllocation()->width);
            else
               borderDiff =
                  textblock->getAllocation()->x -
                  vloat->generatingBlock->getAllocation()->x;

            //printf ("[%p]       %d = %d + %d - %d\n",
            //        textblock, yWidget, vloat->yReal,
            //        vloat->generatingBlock->getAllocation()->y,
            //        textblock->getAllocation()->y);
         } else
            positioned = false;
      }

      //intf ("[%p]    float #%d (%p): tba = %s, gba = %s, y = %d => %s / %d\n",
      //        textblock, i, vloat->widget,
      //        textblock->wasAllocated() ? "true" : "false",
      //        vloat->generatingBlock->wasAllocated() ? "true" : "false",
      //        vloat->yReal, positioned ? "true" : "false", yWidget);

      if (positioned && y + h >= yWidget &&
          y < yWidget + vloat->size.ascent + vloat->size.descent) {
         // It is not sufficient to find the first float, since a line
         // (with height h) may cover the region of multiple float, of
         // which the widest has to be choosen.
         int borderIn;
         if (right)
            borderIn = vloat->generatingBlock->getStyle()->boxRestWidth();
         else
            borderIn = vloat->generatingBlock->getStyle()->boxOffsetX();

         border = max (border, vloat->size.width + borderDiff + borderIn);
      }

      // To be a bit more efficient, the loop could be stopped when
      // (i) at least one float has been found, and (ii) the next float is
      // below y + h.
   }

   //printf ("[%p] %s border (%d, %d) = %d\n",
   //        textblock, right ? "right" : "left", y, h, border);

   return border;
}

bool OutOfFlowMgr::hasFloatLeft (Textblock *textblock, int y, int h)
{
   return hasFloat (textblock, leftFloats, y, h);
}

bool OutOfFlowMgr::hasFloatRight (Textblock *textblock, int y, int h)
{
   return hasFloat (textblock, rightFloats, y, h);
}

bool OutOfFlowMgr::hasFloat (Textblock *textblock, Vector<Float> *list,
                             int y, int h)
{
   // TODO Latest change: Many changes neccessary. Re-actiavate.

#if 0
   // Compare to getBorder().
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);
      
      if (vloat->positioned && y + h >= vloat->y &&
          y < vloat->y + vloat->size.ascent + vloat->size.descent)
         return true;
   }
#endif

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
               (availWidth * perLengthVal (vloat->widget->getStyle()->width));
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
      if (availWidth != -1 && width > availWidth)
         width = availWidth;
      // Finally, consider extremes (as described above).
      if (width < extremes.minWidth)
          width = extremes.minWidth;
      if (width > extremes.maxWidth)
         width = extremes.maxWidth;
          
      vloat->widget->setWidth (width);
      vloat->widget->sizeRequest (&vloat->size);
      
      //printf ("   Float at %d: %d x (%d + %d)\n",
      //        vloat->y, vloat->width, vloat->ascent, vloat->descent);
          
      vloat->dirty = false;
   }
}

// TODO Latest change: Check also Textblock::borderChanged: looks OK,
// but the comment ("... (i) with canvas coordinates ...") looks wrong
// (and looks as having always been wrong).

// Another issue: does it make sense to call Textblock::borderChanged
// for generators, when the respective widgets have not been called
// yet?

} // namespace dw
