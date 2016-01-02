/** \page dw-out-of-flow-positioned Handling Elements Out Of Flow: Positioned Elements

<div style="border: 2px solid #ffff00; margin: 1em 0; padding: 0.5em
  1em; background-color: #ffffe0">Positioned elements have been
  deactivated, during the development of \ref dw-size-request-pos, to
  focus on floats. See dw::core::IMPL_POS in file dw/core.hh.</div>


Miscellaneous notes
===================

General
-------
(See also *relative positions* below.)

What about negative positions?

dw::oof::OutOfFlowMgr::tellPosition1 and
dw::oof::OutOfFlowMgr::tellPosition2 could be combined again, and
called in the respective circumstance, depending on
dw::oof::OutOfFlowMgr::mayAffectBordersAtAll.

Relative positions
------------------
**General Overview:** At the original position, a space as large as
the positioned element is left. This is implemented by assigning a
size to the widget *reference*. For this there are two new methods:
dw::oof::OutOfFlowMgr::calcWidgetRefSize and
dw::oof::OOFAwareWidget::widgetRefSizeChanged.

**Bug:** Since the size of a relatively positioned element should be
calculated as if it was in flow, the available width should be
delegated to the *generator*; however, since
dw::oof::OOFPosRelMgr::dealingWithSizeOfChild returns *false* in all
cases, it is delegated to the *container*. **Idea for fix:**
dw::oof::OOFPosRelMgr::dealingWithSizeOfChild should return *false* if
and only if the generator of the child is the container (to prevent an
endless recursion). In other cases,
dw::oof::OOFPosRelMgr::getAvailWidthOfChild and
dw::oof::OOFPosRelMgr::getAvailHeightOfChild should directly call the
respective methods of the *generator*, which must be made public then.

**Performance:** In many cases, the first call of
dw::oof::OOFPosRelMgr::sizeAllocateEnd will queue again the resize
idle, since some elements are not considered in
dw::oof::OOFPosRelMgr::getSize. One case could be removed: if a
positioned element has *left* = *top* = 0, and its total size (the
requisition) is equal to the space left at the original position, the
size of the widget *reference*
(dw::oof::OOFAwareWidget::getRequisitionWithoutOOF, see
dw::oof::OOFPosRelMgr::calcWidgetRefSize).

**Documentation:** Describe why the latter is not covered by
dw::oof::OOFPositionedMgr::doChildrenExceedContainer. (Is this really
the case?)

**Open:** Stacking order? Furthermore: a relatively positioned element
does not always constitute a containing block (see CSS specification).

Fixed positions
---------------
Currently, fixedly positioned elements are positioned relative to the
canvas, not to the viewport. For a complete implementation, see \ref
dw-fixed-positions.

*/
