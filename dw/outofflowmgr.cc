#include "outofflowmgr.hh"

//#include <math.h> // testing

using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

OutOfFlowMgr::OutOfFlowMgr (ContainingBlock *containingBlock)
{
   //printf ("OutOfFlowMgr::OutOfFlowMgr\n");
   this->containingBlock = containingBlock;

   leftFloats = new Vector<Float> (1, true);
   rightFloats = new Vector<Float> (1, true);
}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   //printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloats;
   delete rightFloats;
}

void OutOfFlowMgr::sizeAllocate (Allocation *containingBlockAllocation)
{
   // TODO Much copy and paste.

   for (int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      assert (vloat->y != -1);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      childAllocation.x = containingBlockAllocation->x + vloat->borderWidth;
      childAllocation.y = containingBlockAllocation->y + vloat->y;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;

      vloat->widget->sizeAllocate (&childAllocation);

      //printf ("allocate left #%d -> (%d, %d), %d x (%d + %d)\n",
      //        i, childAllocation.x, childAllocation.y, childAllocation.width,
      //        childAllocation.ascent, childAllocation.descent);
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      assert (vloat->y != -1);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      childAllocation.x =
         containingBlockAllocation->x + containingBlockAllocation->width
         - (vloat->size.width + vloat->borderWidth);
      childAllocation.y = containingBlockAllocation->y + vloat->y;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;

      vloat->widget->sizeAllocate (&childAllocation);

      //printf ("allocate right #%d -> (%d, %d), %d x (%d + %d)\n",
      //        i, childAllocation.x, childAllocation.y, childAllocation.width,
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
      assert (vloat->y != -1);

      core::Rectangle childArea;
      if (vloat->widget->intersects (area, &childArea))
         vloat->widget->draw (view, &childArea);
   }
}

void OutOfFlowMgr::queueResize(int ref)
{
}

bool OutOfFlowMgr::isWidgetOutOfFlow (core::Widget *widget)
{
   // Will be extended for absolute positions.
   return widget->getStyle()->vloat != FLOAT_NONE;
}

void OutOfFlowMgr::addWidget (Widget *widget)
{
   if (widget->getStyle()->vloat != FLOAT_NONE) {
      Float *vloat = new Float ();
      vloat->widget = widget;
      vloat->dirty = true;

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

void OutOfFlowMgr::markSizeChange (int ref)
{
   printf ("[%p] MARK_SIZE_CHANGE (%d)\n", containingBlock, ref);

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
      if (vloat->y != -1)
         containingBlock->borderChanged (vloat->y);
   }
   else
      // later: absolute positions
      assertNotReached();
}


void OutOfFlowMgr::markExtremesChange (int ref)
{
}


void OutOfFlowMgr::tellNoPosition (Widget *widget)
{
   Float *vloat = findFloatByWidget(widget);
   int oldY = vloat->y;
   vloat->y = -1;

   if (oldY != -1)
      containingBlock->borderChanged (oldY);
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

   *oofWidth = cbHeight; // This (or "<=" instead of "=") should be
                         // the case for floats.

   int oofHeightLeft = containingBlock->getCBStyle()->boxDiffWidth();
   int oofHeightRight = containingBlock->getCBStyle()->boxDiffWidth();

   for (int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      assert (vloat->y != -1);
      ensureFloatSize (vloat);
      oofHeightLeft = max (oofHeightLeft,
                           vloat->y + vloat->size.ascent + vloat->size.descent
                           + containingBlock->getCBStyle()->boxRestHeight());
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      assert (vloat->y != -1);
      ensureFloatSize (vloat);
      oofHeightRight = max (oofHeightRight,
                            vloat->y + vloat->size.ascent + vloat->size.descent
                            + containingBlock->getCBStyle()->boxRestHeight());
   }

   *oofHeight = max (oofHeightLeft, oofHeightRight);
}


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   assert (y >= 0);

   // TODO Test collisions; when floats overlap, the vloat->y must be larger
   // than y.

   Float *vloat = findFloatByWidget(widget);
   int oldY = vloat->y;

   int realY = y;
   Vector<Float> *list = getFloatList (widget);   
   bool collides;

   do {
      collides = false;

      for (int i = 0; i < list->size(); i++) {
         Float *v = list->get(i);
         if (v != vloat) {
            ensureFloatSize (v);
            if (v->y != -1 && realY >= v->y && 
                realY < v->y + v->size.ascent + v->size.descent) {
               collides = true;
               realY = v->y + v->size.ascent + v->size.descent;
               break;
            }
         }
      }    
   } while (collides);

   vloat->y = realY;
   
   if (oldY == -1)
      containingBlock->borderChanged (vloat->y);
   else if (vloat->y != oldY)
      containingBlock->borderChanged (min (oldY, vloat->y));
}
   
int OutOfFlowMgr::getLeftBorder (int y)
{
   //return 40 * sin ((double)y / 30);

   for(int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      ensureFloatSize (vloat);

      if(vloat->y != -1 && y >= vloat->y &&
         y < vloat->y + vloat->size.ascent + vloat->size.descent) {
         //printf ("   LEFT: %d ==> %d (%d + %d)\n", y,
         //        vloat->width, vloat->ascent, vloat->descent);
         return vloat->size.width + vloat->borderWidth;
      }
   }

   //printf ("   LEFT: %d ==> %d (no float)\n", y, 0);
   return 0;
}

int OutOfFlowMgr::getRightBorder (int y)
{
   //return 40 * cos ((double)y / 30);

   for(int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      ensureFloatSize (vloat);

      if(vloat->y != -1 && y >= vloat->y &&
         y < vloat->y + vloat->size.ascent + vloat->size.descent) {
         //printf ("   RIGHT: %d ==> %d (%d + %d)\n", y,
         //        vloat->width, vloat->ascent, vloat->descent);
         return vloat->size.width + vloat->borderWidth;
      }
   }

   //printf ("   RIGHT: %d ==> %d (no float)\n", y, 0);
   return 0;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty) {
      vloat->widget->sizeRequest (&vloat->size);
      vloat->borderWidth = calcBorderDiff (vloat);

      //printf ("   Float at %d: %d x (%d + %d)\n",
      //        vloat->y, vloat->width, vloat->ascent, vloat->descent);

      vloat->dirty = false;
   }
}

int OutOfFlowMgr::calcBorderDiff (Float *vloat)
{
   switch (vloat->widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return containingBlock->getCBStyle()->boxOffsetX();
      
   case FLOAT_RIGHT:
      return containingBlock->getCBStyle()->boxRestWidth();

   default:
      // Only used for floats.
      assertNotReached();
      return 0; // compiler happiness
   }
}

} // namespace dw
