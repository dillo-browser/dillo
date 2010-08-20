/** \page dw-layout-views Layout and Views

Rendering of Dw is done in a way resembling the model-view pattern, at
least formally.  Actually, the counterpart of the model, the layout
(dw::core::Layout), does a bit more than a typical model, namely the
layouting (delegated to the widget tree, see \ref dw-layout-widgets),
and the view does a bit less than a typical view, i.e. only the actual
drawing.

Additionally, there is a structure representing common properties of
the platform. A platform is typically related to the underlying UI
toolkit, but other uses may be thought of.

This design helps to archieve two important goals:

<ul>
<li> Abstraction of the actual drawing, by different implementations
     of dw::core::View.

<li> It makes portability simple.
</ul>


<h2>Viewports</h2>

Although the design implies that the usage of viewports should be
fully transparent to the layout module, this cannot be fully achieved,
for the following reasons:

<ul>
<li> Some features, which are used on the level of dw::core::Widget,
     e.g. anchors, refer to scrolling positions.

<li> Size hints (see \ref dw-layout-widgets) depend on the viewport
     sizes, e.g. when the user changes the window size, and so also
     the size of a viewport, the text within should be rewrapped.
</ul>

Therefore, dw::core::Layout keeps track of the viewport size, the
viewport position, and even the thickness of the scrollbars, they are
relevant, see below for more details.
If a viewport is not used, however, the size is not defined.

Whether a given dw::core::View implementation is a viewport or not, is
defined by the return value of dw::core::View::usesViewport. If this
method returns false, the following methods need not to be implemented
at all:

<ul>
<li> dw::core::View::getHScrollbarThickness,
<li> dw::core::View::getVScrollbarThickness,
<li> dw::core::View::scrollTo, and
<li> dw::core::View::setViewportSize.
</ul>

<h3>Scrolling Positions</h3>

The scrolling position is the canvas position at the upper left corner
of the viewport. Views using viewports must

<ol>
<li> change this value on request (dw::core::View::scrollTo), and
<li> tell other changes to the layout, e.g. caused by user events
     (dw::core::Layout::scrollPosChanged).
</ol>

Applications of scrolling positions (anchors, test search etc.) are
handled by the layout, in a way fully transparent to the view.

<h3>Scrollbars</h3>

A feature of the viewport size model are scrollbars. There may be a
vertical scrollbar and a horizontal scrollbar, displaying the
relationship between canvas and viewport height or width,
respectively. If they are not needed, they are hidden, to save screen
space.

Since scrollbars decrease the usable space of a view, dw::core::Layout
must know how much space they take. The view returns, via
dw::core::View::getHScrollbarThickness and
dw::core::View::getVScrollbarThickness, how thick they will be, when
visible.

Viewport sizes, which denote the size of the viewport widgets, include
scrollbar thicknesses. When referring to the viewport \em excluding
the scrollbars space, we will call it "usable viewport size", this is
the area, which is used to display the canvas.

<h2>Drawing</h2>

A view must implement several drawing methods, which work on the whole
canvas. If it is necessary to convert them (e.g. into
dw::fltk::FltkViewport), this is done in a way fully transparent to
dw::core::Widget and dw::core::Layout, instead, this is done by the
view implementation.

There exist following situations:

<ul>
<li> A view gets an expose event: It will delegate this to the
     layout (dw::core::Layout::draw), which will then pass it to the
     widgets (dw::core::Widget::draw), with the view as a parameter.
     Eventually, the widgets will call drawing methods of the view.

<li> A widget requests a redraw: In this case, the widget will
     delegate this to the layout (dw::core::Layout::queueDraw), which
     delegates it to the view (dw::core::View::queueDraw).
     Typically, the view will queue these requests for efficiency.

<li> A widget requests a resize: This case is described below, in short,
     dw::core::View::queueDrawTotal is called for the view.
</ul>

If the draw method of a widget is implemented in a way that it may
draw outside of the widget's allocation, it should draw into a
<i>clipping view.</i> A clipping view is a view related to the actual
view, which guarantees that the parts drawn outside are discarded. At
the end, the clipping view is merged into the actual view. Sample
code:

\code
void Foo::draw (dw::core::View *view, dw::core::Rectangle *area)
{
   // 1. Create a clipping view.
   dw::core::View clipView =
      view->getClippingView (allocation.x, allocation.y,
                             allocation.width, getHeight ());

   // 2. Draw into clip_view
   clipView->doSomeDrawing (...);

   // 3. Draw the children, they receive the clipping view as argument.
   dw::core::Rectangle *childArea
   for (<all relevant children>) {
      if (child->intersects (area, &childArea))
         child->draw (clipView, childArea);
   }

   // 4. Merge
   view->mergeClippingView (clipView);
}
\endcode

A drawing process is always embedded into calls of
dw::core::View::startDrawing and dw::core::View::finishDrawing. An
implementation of this may e.g. use backing pixmaps, to prevent
flickering.


<h2>Sizes</h2>

In the simplest case, the view does not have any influence on
the canvas size, so it is told about changes of the
canvas size by a call to dw::core::View::setCanvasSize. This happens
in the following situations:

<ul>
<li> dw::core::Layout::addWidget,
<li> dw::core::Layout::removeWidget (called by dw::core::Widget::~Widget),
     and
<li> dw::core::Layout::queueResize (called by
     dw::core::Widget::queueResize, when a widget itself requests a size
     change).
</ul>

<h3>Viewports</h3>

There are two cases where the viewport size changes:

<ul>
<li> As an reaction on a user event, e.g. when the user changes the
     window size. In this case, the view delegates this
     change to the layout, by calling
     dw::core::Layout::viewportSizeChanged.

<li> The viewport size may also depend on the visibility of UI
     widgets, which depend on the world size, e.g scrollbars,
     generally called "viewport markers". This is described in a separate
     section.
</ul>

After the creation of the layout, the viewport size is undefined. When
a view is attached to a layout, and this view can already specify
its viewport size, it may call
dw::core::Layout::viewportSizeChanged within the implementation of
dw::core::Layout::setLayout. If not, it may do this as soon as the
viewport size is known.

Generally, the scrollbars have to be considered. If e.g. an HTML page
is rather small, it looks like this:

\image html dw-viewport-without-scrollbar.png

If some more data is retrieved, so that the height exceeds the
viewport size, the text has to be rewrapped, since the available width
gets smaller, due to the vertical scrollbar:

\image html dw-viewport-with-scrollbar.png

Notice the different line breaks.

This means circular dependencies between these different sizes:

<ol>
<li> Whether the scrollbars are visible or not, determines the
     usable space of the viewport.

<li> From the usable space of the viewport, the size hints for the
     toplevel are calculated.

<li> The size hints for the toplevel widgets may have an effect on its
     size, which is actually the canvas size.

<li> The canvas size determines the visibility of the scrollbarss.
</ol>

To make an implementation simpler, we simplify the model:

<ol>
<li> For the calls to dw::core::Widget::setAscent and
     dw::core::Widget::setDescent, we will always exclude the
     horizontal scrollbar thickness (i.e. assume the horizontal
     scrollbar is used, although the visibility is determined correctly).

<li> For the calls to dw::core::Widget::setWidth, we will calculate
     the usable viewport width, but with the general assumption, that
     the widget generally gets higher.
</ol>

This results in the following rules:

<ol>
<li> Send always (when it changes) dw::core::Layout::viewportHeight
     minus the maximal value of dw::core::View::getHScrollbarThickness as
     argument to dw::core::Widget::setAscent, and 0 as argument to
     dw::core::Widget::setDescent.

<li> There is a flag, dw::core::Layout::canvasHeightGreater, which is set
     to false in the following cases:

     <ul>
     <li> dw::core::Layout::addWidget,
     <li> dw::core::Layout::removeWidget (called by dw::core::Widget::~Widget),
          and
     <li> dw::core::Layout::viewportSizeChanged.
     </ul>

     Whenever the canvas size is calculated (dw::core::Layout::resizeIdle),
     and dw::core::Layout::canvasHeightGreater is false, a test is made,
     whether the widget has in the meantime grown that high, that the second
     argument should be set to true (i.e. the vertical scrollbar gets visible).
     As soon as and dw::core::Layout::canvasHeightGreater is true, no such test
     is done anymore.
</ol>

*/
