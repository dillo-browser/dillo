/** \page dw-layout-widgets Layout and Widgets

Both, the layouting and the drawing is delegated to a tree of
widgets. A widget represents a given part of the document, e.g. a text
block, a table, or an image. Widgets may be nested, so layouting and
drawing may be delegated by one widget to its child widgets.

Where to define the borders of a widget, whether to combine different
widgets to one, or to split one widget into multiple ones, should be
considered based on different concerns:

<ul>
<li> First, there are some restrictions of Dw:

    <ul>
    <li> The allocation (this is the space a widget allocates at
         a time) of a dillo widget is always rectangular, and
    <li> the allocation of a child widget must be a within the allocation
         of the parent widget.
    </ul>

<li> Since some widgets are already rather complex, an important goal
     is to keep the implementation of the widget simple.

<li> Furthermore, the granularity should not be too fine, because of the
     overhead each single widget adds.
</ul>

For CSS, there will be a document tree on top of Dw, this will be
flexible enough when mapping the document structure on the widget
structure, so you should not have the document structure in mind.

<h2>Sizes</h2>

\ref dw-widget-sizes


<h2>Styles</h2>

Each widget is assigned a style, see dw::core::style for more
informations.


<h2>Iterators</h2>

Widgets must implement dw::core::Widget::iterator. There are several
common iterators:

<ul>
<li>dw::core::EmptyIterator, and
<li>dw::core::TextIterator.
</ul>

Both hide the constructor, use the \em create method.

These simple iterators only iterate through one widget, it does not
have to iterate recursively through child widgets. Instead, the type
dw::core::Content::WIDGET is returned, and the next call of
dw::core::Iterator::next will return the piece of contents \em after
(not within) this child widget.

This makes implementation much simpler, for recursive iteration, there
is dw::core::DeepIterator.


<h2>Anchors and Scrolling</h2>

\todo This section is not implemented yet, after the implementation,
  the documentation should be reviewed.

Here is a description, what is to be done for a widget
implementation. How to jump to anchors, set scrolling positions
etc. is described in \ref dw-usage.

<h3>Anchors</h3>

Anchors are position markers, which are identified by a name, which is
unique in the widget tree. The widget must care about anchors in three
different situations:

<ol>
<li> Adding an anchor is inititiated by a specific widget method, e.g.
     dw::Textblock::addAnchor. Here, dw::core::Widget::addAnchor must be
     called,

<li> Whenever the position of an anchor is changed,
     dw::core::Widget::changeAnchor is called (typically, this is done
     in the implementation of dw::core::Widget::sizeAllocateImpl).

<li> When a widget is destroyed, the anchor must be removed, by calling
     dw::core::Widget::removeAnchor.
</ol>

All these methods are delegated to dw::core::Layout, which manages
anchors centrally. If the anchor in question has been set to jump to,
the viewport position is automatically adjusted, see \ref
dw-usage.


<h2>Drawing</h2>

In two cases, a widget has to be drawn:

<ol>
<li> as a reaction on an expose event,
<li> if the widget content has changed and it needs to be redrawn.
</ol>

In both cases, drawing is done by the implementation of
dw::core::Widget::draw, which draws into the view.


Each view provides some primitive methods for drawing, most should be
obvious. Note that the views do not know anything about dillo
widgets, and so coordinates have to be passed as canvas coordinates.

A widget may only draw in its own allocation. If this cannot be
achieved, a <i>clipping view</i> can be used, this is described in
\ref dw-layout-views. Generally, drawing should then look like:

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

Clipping views are expensive, so they should be avoided when possible.

The second argument to dw::core::Widget::draw is the region, which has
to be drawn. This may (but needs not) be used for optimization.

If a widget contains child widgets, it must explicitly draw these
children (see also code example above). For this, there is the useful
method dw::core::Widget::intersects, which returns, which area of the
child must be drawn.

<h3>Explicit Redrawing</h3>

If a widget changes its contents, so that it must be redrawn, it must
call dw::core::Widget::queueDrawArea or
dw::core::Widget::queueDraw. The first variant expects a region within
the widget, the second will cause the whole widget to be redrawn. This
will cause an asynchronous call of dw::core::Widget::draw.

If only the size changes, a call to dw::core::Widget::queueResize is
sufficient, this will also queue a complete redraw (see \ref
dw-widget-sizes.)


<h2>Mouse Events</h2>

A widget may process mouse events. The view (\ref dw-layout-views)
passes mouse events to the layout, which then passes them to the
widgets. There are two kinds of mouse events:

<ul>
<li>events returning bool, and
<li>events returning nothing (void).
</ul>

The first group consists of:

<ul>
<li>dw::core::Widget::buttonPressImpl,
<li>dw::core::Widget::buttonReleaseImpl, and
<li>dw::core::Widget::motionNotifyImpl.
</ul>

For these events, a widget returns a boolean value, which denotes,
whether the widget has processed this event (true) or not (false). In
the latter case, the event is delegated according to the following
rules:

<ol>
<li> First, this event is passed to the bottom-most widget, in which
     allocation the mouse position is in.
<li> If the widget does not process this event (returning false), it is
     passed to the parent, and so on.
<li> The final result (whether \em any widget has processed this event) is
     returned to the view.
</ol>

The view may return this to the UI toolkit, which then interprets this
in a similar way (whether the viewport, a UI widget, has processed
this event).

These events return nothing:

<ul>
<li>dw::core::Widget::enterNotifyImpl and
<li>dw::core::Widget::leaveNotifyImpl.
</ul>

since they are bound to a widget.

When processing mouse events, the layout always deals with two
widgets: the widget, the mouse pointer was in, when the previous mouse
event was processed, (below called the "old widget") and the widget,
in which the mouse pointer is now ("new widget").

The following paths are calculated:

<ol>
<li> the path from the old widget to the nearest common ancestor of the old
     and the new widget, and
<li> the path from this ancestor to the new widget.
</ol>

For the widgets along these paths, dw::core::Widget::enterNotifyImpl
and dw::core::Widget::leaveNotifyImpl are called.

<h3>Signals</h3>

If a caller outside of the widget is interested in these events, he
can connect a dw::core::Layout::LinkReceiver. For those events with a
boolean return value, the results of the signal emission is regarded,
i.e. the delegation of an event to the parent of the widget can be
stopped by a signal receiver returning true, even if the widget method
returns false.

First, the widget method is called, then (in any case) the signal is
emitted.

<h3>Selection</h3>

If your widget has selectable contents, it should delegate the events
to dw::core::SelectionState (dw::core::Layout::selectionState).


<h2>Miscellaneous</h2>

<h3>Cursors</h3>

Each widget has a cursor, which is set by
dw::core::Widget::setCursor. If a cursor is assigned to a part of a
widget, this widget must process mouse events, and call
dw::core::Widget::setCursor explicitly.

(This will change, cursors should become part of
dw::core::style::Style.)

<h3>Background</h3>

Backgrounds are part of styles
(dw::core::style::Style::backgroundColor). If a widget assigns
background colors to parts of a widget (as dw::Table does for rows),
it must call dw::core::Widget::setBgColor for the children inside this
part.

*/
