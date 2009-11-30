/** \page dw-changes Changes to the GTK+-based Release Version

<h2>Changes in Dw</h2>

Related to the FLTK port, there have been many changes, this is a
(hopefully complete) list:

<ul>
<li> Rendering abstraction, read \ref dw-overview and  \ref dw-layout-views
     for details. Some important changes:

     <ul>
     <li> The underlying platform (e.g. the UI toolkit) is fully abstract,
          there are several platform independent structures replacing
          GTK+ structures, e.g. dw::core::Event.

     <li> The central class managing the widget tree is not anymore
          GtkDwViewport,  but dw::core::Layout.

     <li> Drawing is done via dw::core::View, a pointer is passed to
          dw::core::Widget::draw.

     <li> The distinction between viewport coordinates and canvas
          coordinates (formerly world coordinates) has been mostly
          removed. (Only for views, it sometimes plays a role, see
          \ref dw-layout-views).
</ul>

<li> Cursors have been moved to dw::core::style, see
     dw::core::style::Style::cursor. dw::core::Widget::setCursor is now
     protected (and so only called by widget implementations).

<li> World coordinates are now called canvas coordinates.

<li> There is now a distinction between dw::core::style::StyleAttrs and
     dw::core::style::Style.

<li> There is no base class for container widgets anymore. The former
     DwContainer::for_all has been removed, instead this functionality
     is now done via iterators (dw::core::Widget::iterator,
     dw::core::Iterator).

<li> DwPage is now called dw::Textblock, and DwAlignedPage
     dw::AlignedTextblock.

<li> dw::Textblock, all sub classes of it, and dw::Table do not read
     "limit_text_width" from the preferences, but get it as an argument.
     (May change again.)

<li> dw::Table has been rewritten.

<li> Instead of border_spacing in the old DwStyle, there are two attributes,
     dw::core::style::Style::hBorderSpacing and
     dw::core::style::Style::vBorderSpacing, since CSS allowes to specify
     two values. Without CSS, both attributes should have the same value.

<li> Images are handled differently, see \ref dw-images-and-backgrounds.

<li> Embedded UI widgets (formerly GtkWidget's) are handled differently,
     see dw::core::ui.

<li> DwButton has been removed, instead, embedded UI widgets are used. See
     dw::core::ui and dw::core::ui::ComplexButtonResource.
</ul>

Dw is now written C++, the transition should be obvious. All "Dw"
prefixes have been removed, instead, namespaces are used now:

<ul>
<li>dw::core contains the core,
<li>dw::core::style styles,
<li>dw::core::ui embedded UI resources,
<li>dw::fltk classes related to FLTK, and
<li>::dw the widgets.
</ul>

<h2>Documentation</h2>

The old documentation has been moved to:

<table>
<tr><th colspan="2">Old           <th>New
<tr><td rowspan="2">Dw.txt
    <td>general part              <td>\ref dw-overview, \ref dw-usage,
                                      \ref dw-layout-widgets,
                                      \ref dw-widget-sizes
<tr><td>remarks on specific widgets  <td>respective source files: dw::Bullet,
                                      dw::core::ui::Embed
<tr><td rowspan="2">DwImage.txt
    <td>signals                   <td>dw::core::Layout::LinkReceiver
<tr><td>rest                      <td>dw::Image,
                                      \ref dw-images-and-backgrounds
<tr><td colspan="2">Imgbuf.txt    <td>dw::core::Imgbuf,
                                  \ref dw-images-and-backgrounds
<tr><td colspan="2">DwPage.txt    <td>dw::Textblock
<tr><td colspan="2">DwRender.txt  <td>\ref dw-overview, \ref dw-layout-views,
                                      dw::core::ui
<tr><td colspan="2">DwStyle.txt   <td>dw::core::style
<tr><td colspan="2">DwTable.txt   <td>dw::Table
<tr><td colspan="2">DwWidget.txt  <td>dw::core::Widget, \ref dw-layout-widgets,
                                      \ref dw-widget-sizes
<tr><td colspan="2">Selection.txt <td>dw::core::SelectionState
</table>

*/
