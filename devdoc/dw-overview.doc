/** \page dw-overview Dillo Widget Overview

Note: If you are already familiar with the Gtk+-based version of Dw,
read \ref dw-changes.


The module Dw (Dillo Widget) is responsible for the low-level rendering of
all resources, e.g. images, plain text, and HTML pages (this is the
most complex type). Dw is \em not responsible for parsing HTML, or
decoding image data. Furthermore, the document tree, which is planned
for CSS, is neither a part of Dw, instead, it is a new module on top
of Dw.

The rendering, as done by Dw, is split into two phases:

<ul>
<li> the \em layouting, this means calculating the exact positions of
     words, lines, etc. (in pixel position), and
<li> the \em drawing, i.e. making the result of the layouting visible
     on the screen.
</ul>

The result of the layouting allocates an area, which is called
\em canvas.

<h2>Structure</h2>

The whole Dw module can be split into the following parts:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="open", fontname=Helvetica, fontsize=10,
         labelfontname=Helvetica, labelfontsize=10,
         color="#404040", labelfontcolor="#000080"];

   subgraph cluster_core {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
      label="Platform independent core";

      Layout [URL="\ref dw::core::Layout"];
      Platform [URL="\ref dw::core::Platform", color="#ff8080"];
      View [URL="\ref dw::core::View", color="#ff8080"];
      Widget [URL="\ref dw::core::Widget", color="#a0a0a0"];
   }

   subgraph cluster_fltk {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
      label="FLTK specific part (as an\nexample for the platform specific\n\
implementations)";

      subgraph cluster_fltkcore {
         style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
         label="FLTK core";

         FltkPlatform [URL="\ref dw::fltk::FltkPlatform"];
         FltkView [URL="\ref dw::fltk::FltkView", color="#ff8080"];
      }

      FltkViewport [URL="\ref dw::fltk::FltkViewport"];
      FltkPreview [URL="\ref dw::fltk::FltkPreview"];
   }

   subgraph cluster_widgets {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;
      label="Platform independent widgets";

      Textblock [URL="\ref dw::Textblock"];
      AlignedTextblock [URL="\ref dw::AlignedTextblock", color="#a0a0a0"];
      Table [URL="\ref dw::Table"];
      Image [URL="\ref dw::Image"];
      etc1 [label="..."];
      etc2 [label="..."];
   }

   Layout -> Platform [headlabel="1", taillabel="1"];
   Layout -> View [headlabel="*", taillabel="1"];

   Layout -> Widget [headlabel="1", taillabel="1", label="topLevel"];
   Widget -> Widget [headlabel="*", taillabel="1", label="children"];

   Widget -> Textblock [arrowhead="none", arrowtail="empty", dir="both"];
   Widget -> Table [arrowhead="none", arrowtail="empty", dir="both"];
   Widget -> Image [arrowhead="none", arrowtail="empty", dir="both"];
   Widget -> etc1 [arrowhead="none", arrowtail="empty", dir="both"];
   Textblock -> AlignedTextblock [arrowhead="none", arrowtail="empty",
                                  dir="both"];
   AlignedTextblock -> etc2 [arrowhead="none", arrowtail="empty", dir="both"];

   Platform -> FltkPlatform [arrowhead="none", arrowtail="empty", dir="both",
                             style="dashed"];
   FltkPlatform -> FltkView [headlabel="*", taillabel="1"];

   View -> FltkView [arrowhead="none", arrowtail="empty", dir="both"];
   FltkView -> FltkViewport [arrowhead="none", arrowtail="empty", dir="both",
                             style="dashed"];
   FltkView -> FltkPreview [arrowhead="none", arrowtail="empty", dir="both",
                            style="dashed"];
}
\enddot

<center>[\ref uml-legend "legend"]</center>

\em Platform means in most cases the underlying UI toolkit
(e.g. FLTK). A layout is bound to a specific platform, but multiple
platforms may be handled in one program.

A short overview:

<ul>
<li> dw::core::Layout is the central class, it manages the widgets and the
     view, and provides delegation methods for the platform.

<li> The layouting is done by a tree of widgets (details are described in
     \ref dw-layout-widgets), also the drawing, which is finally delegated
     to the view.

<li> The view (implementation of dw::core::View) provides primitive methods
     for drawing, but also have an influence on
     the canvas size (via size hints). See \ref dw-layout-views for details.

<li> Some platform dependencies are handled by implementations
     of dw::core::Platform.
</ul>


<h3>Header Files</h3>

The structures mentioned above can be found in the following header
files:

<ul>
<li> Anything from the Dw core in core.hh. Do not include the single files.

<li> The single widgets can be found in the respective header files, e.g.
     image.hh for dw::Image.

<li> The core of the FLTK implementation is defined in fltkcore.hh. This
     includes dw::fltk::FltkPlatform, dw::fltk::FltkView, but not the concrete
     view implementations.

<li> The views can be found in single header files, e.g fltkviewport.hh for
     dw::fltk::FltkViewport.
</ul>


<h2>Further Documentations</h2>

A complete map can be found at \ref dw-map.

<ul>
<li> For learning, how to use Dw, read \ref dw-usage and related documents,
     dw::core::style, dw::core::ui and \ref dw-images-and-backgrounds.
<li> Advanced topics are described in \ref dw-layout-widgets,
     \ref dw-widget-sizes and \ref dw-layout-views.
</ul>

*/
