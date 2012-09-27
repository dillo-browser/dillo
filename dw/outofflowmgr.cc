#include "outofflowmgr.hh"

//#include <math.h> // testing

using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

OutOfFlowMgr::OutOfFlowMgr (Widget *containingBlock)
{
   printf ("OutOfFlowMgr::OutOfFlowMgr\n");
   this->containingBlock = containingBlock;

   leftFloats = new Vector<Float> (1, true);
   rightFloats = new Vector<Float> (1, true);
}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloats;
   delete rightFloats;
}

void OutOfFlowMgr::sizeAllocate (Allocation *containingBoxAllocation)
{
}

void OutOfFlowMgr::draw (View *view, Rectangle *area)
{
}

void OutOfFlowMgr::queueResize(int ref)
{
}

bool OutOfFlowMgr::isWidgetOutOfFlow (core::style::Style *style)
{
   // Will be extended for absolute positions.
   return style->vloat != FLOAT_NONE;
}

void OutOfFlowMgr::addWidget (Widget *widget, Widget *generator)
{
   if (widget->getStyle()->vloat != FLOAT_NONE) {
      Float *vloat = new Float ();
      vloat->widget = widget;
      vloat->generator = generator;
      vloat->y = -1;

      Requisition requisition;
      widget->sizeRequest (&requisition);
      vloat->width = requisition.width;
      vloat->height = requisition.ascent + requisition.descent;

      switch (widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         vloat->width += containingBlock->getStyle()->boxOffsetX();
         leftFloats->put (vloat);
         break;

      case FLOAT_RIGHT:
         vloat->width += containingBlock->getStyle()->boxRestWidth();
         rightFloats->put (vloat);
         break;

      default:
         assertNotReached();
      }

      widget->parentRef = 1; // TODO
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

void OutOfFlowMgr::tellNoPosition (Widget *widget)
{
   findFloatByWidget(widget)->y = -1;
}

void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   // TODO test collisions
   findFloatByWidget(widget)->y = y;
}

void OutOfFlowMgr::markChange (int ref)
{
}
   
int OutOfFlowMgr::getLeftBorder (int y)
{
   //return 40 * sin ((double)y / 30);

   for(int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      if(y != - 1 && y >= vloat->y && y < vloat->y + vloat->height)
         return vloat->width;
   }

   return 0;
}

int OutOfFlowMgr::getRightBorder (int y)
{
   //return 40 * cos ((double)y / 30);

   for(int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      if(y != - 1 && y >= vloat->y && y < vloat->y + vloat->height)
         return vloat->width;
   }

   return 0;
}

} // namespace dw
