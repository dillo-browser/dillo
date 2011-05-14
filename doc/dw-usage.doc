/** \page dw-usage Dillo Widget Usage

This document describes the usage of Dw, without going too much into
detail.


<h2>Getting Started</h2>

In this section, a small runnable example is described, based on the
FLTK implementation.

As described in \ref dw-overview, the following objects are needed:

<ul>
<li> dw::core::Layout,
<li> an implementation of dw::core::Platform (we will use
     dw::fltk::FltkPlatform),
<li> at least one implementation of dw::core::View (dw::fltk::FltkViewport),
     and
<li> some widgets (for this example, only a simple dw::Textblock).
</ul>

First of all, the necessary \#include's:

\code
#include <FL/Fl_Window.H>
#include <FL/Fl.H>

#include "dw/core.hh"
#include "dw/fltkcore.hh"
#include "dw/fltkviewport.hh"
#include "dw/textblock.hh"
\endcode

Everything is put into one function:

\code
int main(int argc, char **argv)
{
\endcode

As the first object, the platform is instantiated:

\code
   dw::fltk::FltkPlatform *platform = new dw::fltk::FltkPlatform ();
\endcode

Then, the layout is created, with the platform attached:

\code
   dw::core::Layout *layout = new dw::core::Layout (platform);
\endcode

For the view, we first need a FLTK window:

\code
   Fl_Window *window = new Fl_Window(200, 300, "Dw Example");
   window->begin();
\endcode

After this, we can create a viewport, and attach it to the layout:

\code
   dw::fltk::FltkViewport *viewport =
      new dw::fltk::FltkViewport (0, 0, 200, 300);
   layout->attachView (viewport);
\endcode

Each widget needs a style (dw::core::style::Style, see dw::core::style),
so we construct it here. For this, we need to fill a
dw::core::style::StyleAttrs structure with values, and call
dw::core::style::Style::create (latter is done further below):

\code
   dw::core::style::StyleAttrs styleAttrs;
   styleAttrs.initValues ();
   styleAttrs.margin.setVal (5);
\endcode

dw::core::style::StyleAttrs::initValues sets several default
values. The last line sets a margin of 5 pixels. Next, we need a
font. Fonts are created in a similar way, first, the attributes are
defined:

\code
   dw::core::style::FontAttrs fontAttrs;
   fontAttrs.name = "Bitstream Charter";
   fontAttrs.size = 14;
   fontAttrs.weight = 400;
   fontAttrs.style = dw::core::style::FONT_STYLE_NORMAL;
   fontAttrs.letterSpacing = 0;
   fontAttrs.fontVariant = dw::core::style::FONT_VARIANT_NORMAL;
\endcode

Now, the font can be created:

\code
   styleAttrs.font = dw::core::style::Font::create (layout, &fontAttrs);
\endcode

As the last attributes, the background and forground colors are
defined, here dw::core::style::Color::createSimple must be called:

\code
   styleAttrs.color =
      dw::core::style::Color::create (layout, 0x000000);
   styleAttrs.backgroundColor =
      dw::core::style::Color::create (layout, 0xffffff);
\endcode

Finally, the style for the widget is created:

\code
   dw::core::style::Style *widgetStyle =
      dw::core::style::Style::create (layout, &styleAttrs);
\endcode

Now, we create a widget, assign a style to it, and set it as the
toplevel widget of the layout:

\code
   dw::Textblock *textblock = new dw::Textblock (false);
   textblock->setStyle (widgetStyle);
   layout->setWidget (textblock);
\endcode

The style is not needed anymore (a reference is added in
dw::core::Widget::setStyle), so it should be unreferred:

\code
   widgetStyle->unref();
\endcode

Now, some text should be added to the textblock. For this, we first
need another style. \em styleAttrs can still be used for this. We set
the margin to 0, and the background color to "transparent":

\code
   styleAttrs.margin.setVal (0);
   styleAttrs.backgroundColor = NULL;

   dw::core::style::Style *wordStyle =
      dw::core::style::Style::create (layout, &styleAttrs);
\endcode

This loop adds some paragraphs:

\code
   for(int i = 1; i <= 10; i++) {
      char buf[4];
      sprintf(buf, "%d.", i);

      char *words[] = { "This", "is", "the", buf, "paragraph.",
                        "Here", "comes", "some", "more", "text",
                        "to", "demonstrate", "word", "wrapping.",
                        NULL };

      for(int j = 0; words[j]; j++) {
         textblock->addText(strdup(words[j]), wordStyle);
\endcode

Notice the \em strdup, dw::Textblock::addText will feel responsible
for the string, and free the text at the end. (This has been done to
avoid some overhead in the HTML parser.)

The rest is simple, it also includes spaces (which also have styles):

\code
         textblock->addSpace(wordStyle);
      }
\endcode

Finally, a paragraph break is added, which is 10 pixels high:

\code
      textblock->addParbreak(10, wordStyle);
   }
\endcode

Again, this style should be unreferred:

\code
   wordStyle->unref();
\endcode

After adding text, this method should always be called (for faster
adding large text blocks):

\code
   textblock->flush ();
\endcode

Some FLTK stuff to finally show the window:

\code
   window->resizable(viewport);
   window->show();
   int errorCode = Fl::run();
\endcode

For cleaning up, it is sufficient to destroy the layout:

\code
   delete layout;
\endcode

And the rest

\code
   return errorCode;
}
\endcode

If you compile and start the program, you should see the following:

\image html dw-example-screenshot.png

Try to scroll, or to resize the window, you will see, that everything
is done automatically.

Of course, creating new widgets, adding text to widgets etc. can also
be done while the program is running, i.e. after fltk::run has been
called, within timeouts, idles, I/O functions etc. Notice that Dw is
not thread safe, so that everything should be done within one thread.

With the exception, that you have to call dw::Textblock::flush,
everything gets immediately visible, within reasonable times; Dw has
been optimized for frequent updates.


<h2>List of all Widgets</h2>

These widgets are used within dillo:

<ul>
<li>dw::core::ui::Embed
<li>dw::AlignedTextblock
<li>dw::Bullet
<li>dw::Ruler
<li>dw::Image
<li>dw::ListItem
<li>dw::Table
<li>dw::TableCell
<li>dw::Textblock
</ul>

If you want to create a new widget, refer to \ref dw-layout-widgets.


<h2>List of Views</h2>

There are three dw::core::View implementations for FLTK:

<ul>
<li> dw::fltk::FltkViewport implements a viewport, which is used in the
     example above.

<li> dw::fltk::FltkPreview implements a preview window, together with
     dw::fltk::FltkPreviewButton, it is possible to have a scaled down
     overview of the whole canvas.

<li> dw::fltk::FltkFlatView is a "flat" view, i.e. it does not support
     scrolling. It is used for HTML buttons, see
     dw::fltk::ui::FltkComplexButtonResource and especially
     dw::fltk::ui::FltkComplexButtonResource::createNewWidget for details.
</ul>

More informations about views in general can be found in \ref
dw-layout-views.


<h2>Iterators</h2>

For examining generally the contents of widgets, there are iterators
(dw::core::Iterator), created by the method
dw::core::Widget::iterator (see there for more details).

These simple iterators only iterate through one widget, and return
child widgets as dw::core::Content::WIDGET. The next call of
dw::core::Iterator::next will return the piece of contents \em after
(not within) this child widget.

If you want to iterate through the whole widget trees, there are two
possibilities:

<ol>
<li> Use a recursive function. Of course, with this approach, you are
     limited by the program flow.

<li> Maintain a stack of iterators, so you can freely pass this stack
     around. This is already implemented, as dw::core::DeepIterator.
</ol>

As an example, dw::core::SelectionState represents the selected region
as two instances of dw::core::DeepIterator.


<h2>Finding Text</h2>

See dw::core::Layout::findtextState and dw::core::FindtextState
(details in the latter). There are delegation methods:

<ul>
<li> dw::core::Layout::search and
<li> dw::core::Layout::resetSearch.
</ul>


<h2>Anchors and Scrolling</h2>

In some cases, it is necessary to scroll to a given position, or to
an anchor, programmatically.

<h3>Anchors</h3>

Anchors are defined by widgets, e.g. dw::Textblock defines them, when
dw::Textblock::addAnchor is called. To jump to a specific anchor
within the current widget tree, use dw::core::Layout::setAnchor.

This can be done immediately after assignig a toplevel widget, even
when the anchor has not yet been defined. The layout will remember the
anchor, and jump to the respective position, as soon as possible. Even
if the anchor position changes (e.g., when an anchor is moved
downwards, since some space is needed for an image in the text above),
the position is corrected.

As soon as the user scrolls the viewport, this correction is not done
anymore. If in dillo, the user request a page with an anchor, which is
quite at the bottom of the page, he may be get interested in the text
at the beginning of the page, and so scrolling down. If then, after
the anchor has been read and added to the dw::Textblock, this anchor
would be jumped at, the user would become confused.

The anchor is dismissed, too, when the toplevel widget is removed
again.

\todo Currently, anchors only define vertical positions.

<h3>Scrolling</h3>

To scroll to a given position, use the method
dw::core::Layout::scrollTo. It expects several parameters:

<ul>
<li>a horizontal adjustment parameter, defined by dw::core::HPosition,
<li>a vertical adjustment parameter, defined by dw::core::VPosition, and
<li>a rectangle (\em x, \em y, \em width and \em heigh) of the region
    to be adjusted.
</ul>

If you just want to move the canvas coordinate (\em x, \em y) into the
upper left corner of the viewport, you can call:

\code
dw::core::Layout *layout;
// ...
layout->scrollTo(dw::core::HPOS_LEFT, dw::core::VPOS_TOP, 0, 0, 0, 0);
\endcode

By using dw::core::HPOS_NO_CHANGE or dw::core::VPOS_NO_CHANGE, you can
change only one dimension. dw::core::HPOS_INTO_VIEW and
dw::core::VPOS_INTO_VIEW will cause the viewport to move as much as
necessary, that the region is visible in the viewport (this is
e.g. used for finding text).


<h2>Further Documentations</h2>

<ul>
<li> dw::core::style
<li> dw::core::ui
<li> \ref dw-images-and-backgrounds
</ul>

*/
