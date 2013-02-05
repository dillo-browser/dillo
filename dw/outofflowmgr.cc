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
   sizeAllocate (leftFloats, false, containingBlockAllocation);
   sizeAllocate (rightFloats, true, containingBlockAllocation);
}

void OutOfFlowMgr::sizeAllocate(Vector<Float> *list, bool right,
                                Allocation *containingBlockAllocation)
{
   //int width =
   //   availWidth != -1 ? availWidth : containingBlockAllocation->width;

   // 1, Floats have to be allocated
   for (int i = 0; i < list->size(); i++) {
      // TODO Missing: check newly calculated positions, collisions,
      // and queue resize, when neccessary.

      Float *vloat = list->get(i);
      assert (vloat->yReal != -1);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      if (right)
         // TODO Corrected by availWidth; see commented "width" above.
         childAllocation.x =
            vloat->generatingBlock->getAllocation()->x
            + vloat->generatingBlock->getAllocation()->width
            - vloat->size.width;
      else
         childAllocation.x = vloat->generatingBlock->getAllocation()->x;

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

   // 2. Textblocks have already been allocated, but we store some
   // information for later use.
   for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
           tbInfos->iterator ();
        it.hasNext (); ) {
      TypedPointer <Textblock> *key = it.getNext ();
      TBInfo *tbInfo = tbInfos->get (key);
      tbInfo->wasAllocated = true;
      tbInfo->x = key->getTypedValue()->getAllocation()->x;
      tbInfo->y = key->getTypedValue()->getAllocation()->y;
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
      assert (vloat->yReal != -1);

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
      vloat->yReq = vloat->yReal = -1;

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
      if (vloat->yReal != -1)
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
   tellPositionOrNot (widget, -1);
}


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   assert (y >= 0);
   tellPositionOrNot (widget, y);
}

void OutOfFlowMgr::tellPositionOrNot (Widget *widget, int y)
{
   Float *vloat = findFloatByWidget(widget);
   if (y == vloat->yReq)
      // Nothing happened.
      return;

   if (y != -1)
      ensureFloatSize (vloat);
   int oldY = vloat->yReal;
   vloat->yReq = y;

   if (y != -1) {
      // TODO Test collisions (check old code).

      // It is assumed that there are no floats below this float
      // within this generator. For this reason, no other floats have
      // to be adjusted.
   }

   vloat->yReal = y; // Due to collisions, vloat->y may be different from y.

   // Only this float has been changed (see above), so only this float
   // has to be tested against all textblocks.
   if (vloat->generatingBlock->wasAllocated () &&
       // A change from "no position" to "no position" is uninteresting.
       !(oldY == -1 && vloat->yReal == -1)) {
      int yChange;
      if (vloat->yReal == -1)
         // position -> no position
         yChange = oldY;
      else if (oldY == -1)
         // no position -> position
         yChange = vloat->yReal;
      else
         // position -> position
         yChange = min (oldY, vloat->yReal);
      
      // TODO This (and similar code) is not very efficient.
      for (lout::container::typed::Iterator<TypedPointer <Textblock> > it =
              tbInfos->iterator ();
           it.hasNext (); ) {
         TypedPointer <Textblock> *key = it.getNext ();
         Textblock *textblock = key->getTypedValue();

         if (textblock->wasAllocated () &&
             // For y == -1, there will be soon a rewrap (see
             // Textblock::rewrap), or, for y != -1 , the generating
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

            if (oldY != -1) {
               int y1 = vloat->generatingBlock->getAllocation()->y + oldY;
               int y2 = y1 + flh;
               covered = y2 > tby1 && y1 < tby2;
            }

            if (!covered && vloat->yReal != -1) {
               int y1 =
                  vloat->generatingBlock->getAllocation()->y + vloat->yReal;
               int y2 = y1 + flh;
               covered = y2 > tby1 && y1 < tby2;
            }

            if (covered) {
               int yTextblock =
                  vloat->generatingBlock->getAllocation() + yChange
                  - textblock->getAllocation();
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
      if (vloat->y != -1) {
         ensureFloatSize (vloat);
         oofHeightLeft =
            max (oofHeightLeft,
                 vloat->y + vloat->size.ascent + vloat->size.descent
                 + containingBlock->asWidget()->getStyle()->boxRestHeight());
      }
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      if (vloat->y != -1) {
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
   return getBorder (textblock, leftFloats, y, h);
}

/**
 * Get the right border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * See also getLeftBorder(int, int);
 */
int OutOfFlowMgr::getRightBorder (Textblock *textblock, int y, int h)
{
   return getBorder (textblock, rightFloats, y, h);
}

int OutOfFlowMgr::getBorder (Textblock *textblock, Vector<Float> *list,
                             int y, int h)
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
      int yWidget;
      if (vloat->yReal == -1)
         yWidget = -1;
      else if (textblock == vloat->generatingBlock)
         yWidget = vloat->yReal;
      else {
         if (textblock->wasAllocated() &&
             vloat->generatingBlock->wasAllocated())
            yWidget = vloat->yReal + vloat->generatingBlock->getAllocation()->y
               - textblock->getAllocation()->y;
         else
            yWidget = -1;
      }

      if (yWidget != -1 && y + h >= yWidget &&
          y < yWidget + vloat->size.ascent + vloat->size.descent)
         // It is not sufficient to find the first float, since a line
         // (with height h) may cover the region of multiple float, of
         // which the widest has to be choosen.
         border = max (border, vloat->size.width);

      // To be a bit more efficient, the loop could be stopped when
      // (i) at least one float has been found, and (ii) the next float is
      // below y + h.
   }

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
      
      if (vloat->y != -1 && y + h >= vloat->y &&
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
