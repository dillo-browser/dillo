#include "outofflowmgr.hh"

#include <math.h> // only for testing

namespace dw {

void OutOfFlowMgr::sizeAllocate(core::Allocation *containingBoxAllocation)
{
}

void OutOfFlowMgr::draw (core::View *view, core::Rectangle *area)
{
}

void OutOfFlowMgr::queueResize(int ref)
{
}

void OutOfFlowMgr::addWidget (core::Widget *widget, core::style::Style *style)
{
   // widget->parentRef = ...
}

void OutOfFlowMgr::markChange (int ref)
{
}
   
int OutOfFlowMgr::getLeftBorder (int y)
{
   return 40 * sin ((double)y / 30);
}

int OutOfFlowMgr::getRightBorder (int y)
{
   return 40 * cos ((double)y / 30);
}

} // namespace dw
