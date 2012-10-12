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

      Allocation childAllocation;
      childAllocation.x = containingBlockAllocation->x
         + containingBlock->getCBStyle()->boxOffsetX();
      childAllocation.y = containingBlockAllocation->y + vloat->y;
      childAllocation.width =
         vloat->width - containingBlock->getCBStyle()->boxOffsetX();
      childAllocation.ascent = vloat->ascent;
      childAllocation.descent = vloat->descent;

      vloat->widget->sizeAllocate (&childAllocation);

      //printf ("allocate left #%d -> (%d, %d), %d x (%d + %d)\n",
      //        i, childAllocation.x, childAllocation.y, childAllocation.width,
      //        childAllocation.ascent, childAllocation.descent);
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      assert (vloat->y != -1);

      Allocation childAllocation;
      childAllocation.x = containingBlockAllocation->x
         + containingBlockAllocation->width - vloat->width;
      childAllocation.y = containingBlockAllocation->y + vloat->y;
      childAllocation.width =
         vloat->width - containingBlock->getCBStyle()->boxRestWidth();
      childAllocation.ascent = vloat->ascent;
      childAllocation.descent = vloat->descent;

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
   Vector<Float> *list = NULL;
   
   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      list = leftFloats;
      break;

   case FLOAT_RIGHT:
      list = rightFloats;
      break;

   default:
      assertNotReached();
   }

   for(int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      if(vloat->widget == widget)
         return vloat;
   }

   assertNotReached();
   return NULL;
}


void OutOfFlowMgr::markSizeChange (int ref)
{
   printf ("[%p] MARK_SIZE_CHANGE (%d)\n", containingBlock, ref);

   if (isRefLeftFloat (ref))
      leftFloats->get (getFloatIndexFromRef (ref))->dirty = true;
   else if (isRefRightFloat (ref))
      rightFloats->get (getFloatIndexFromRef (ref))->dirty = true;
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


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   assert (y >= 0);

   // TODO Test collisions; when floats overlap, the vloat->y must be larger
   // than y.

   Float *vloat = findFloatByWidget(widget);
   int oldY = vloat->y;
   vloat->y = y;
   
   if (oldY == -1)
      containingBlock->borderChanged (y);
   else if (y != oldY)
      containingBlock->borderChanged (min (oldY, y));
}
   
int OutOfFlowMgr::getLeftBorder (int y)
{
   //return 40 * sin ((double)y / 30);

   for(int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      ensureFloatSize (vloat);

      if(vloat->y != - 1 && y >= vloat->y &&
         y < vloat->y + vloat->ascent + vloat->descent) {
         //printf ("   LEFT: %d ==> %d (%d + %d)\n", y,
         //        vloat->width, vloat->ascent, vloat->descent);
         return vloat->width;
      }
   }

   //printf ("   LEFT: %d ==> %d\n", y, 0);
   return 0;
}

int OutOfFlowMgr::getRightBorder (int y)
{
   //return 40 * cos ((double)y / 30);

   for(int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      ensureFloatSize (vloat);

      if(vloat->y != - 1 && y >= vloat->y &&
         y < vloat->y + vloat->ascent + vloat->descent)
         //printf ("   RIGHT: %d ==> %d (%d + %d)\n", y,
         //        vloat->width, vloat->ascent, vloat->descent);
         return vloat->width;
   }

   //printf ("   RIGHT: %d ==> %d\n", y, 0);
   return 0;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty) {
      int widthDiff;

      switch (vloat->widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         widthDiff = containingBlock->getCBStyle()->boxOffsetX();
         break;

      case FLOAT_RIGHT:
         widthDiff = containingBlock->getCBStyle()->boxRestWidth();
         break;

      default:
         // Only used for floats.
         assertNotReached();
         widthDiff = 0; // compiler happiness
         break;
      }

      Requisition requisition;
      vloat->widget->sizeRequest (&requisition);
      vloat->width = requisition.width + widthDiff;
      vloat->ascent = requisition.ascent;
      vloat->descent = requisition.descent;

      vloat->dirty = false;
   }
}

} // namespace dw
