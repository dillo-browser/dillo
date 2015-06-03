/** \page dw-images-and-backgrounds Images and Backgrounds in Dw

Image Buffers
=============

Representation of the image data is done by dw::core::Imgbuf, see
there for details. Drawing is done by dw::core::View
(dw::core::View::drawImage).

Since dw::core::Imgbuf provides memory management based on reference
counting, there may be an 1-to-n relation from image renderers (image
widgets or backgrounds, see below) and dw::core::Imgbuf. Since
dw::core::Imgbuf does not know about renderers, but just provides
rendering functionality, the caller must (typically after calling
dw::core::Imgbuf::copyRow) notify all renderers connected to the
buffer.


Image Renderer
==============

Generally, there are no restrictions on how to manage
dw::core::Imgbuf; but to handle image data from web resources, the
interface dw::core::ImgRenderer should be implemented. It is again
wrapped by DilloImage (to make access from the C part possible, since
dw::core::ImgRenderer is written in C++), which is referenced by
DilloWeb. There are two positions where retrieving image data is
initiated:

- Html_load_image: for embedded images (implemented by dw::Image,
  which implements dw::core::ImgRenderer);
- StyleEngine::apply (search for "case
  CSS_PROPERTY_BACKGROUND_IMAGE"): for backgrond images; there are
  some implementations of dw::core::ImgRenderer within the context of
  dw::core::style::StyleImage.

Both are described in detail below. Notice that the code is quite
similar; only the implementation of dw::core::ImgRenderer differs.

At this time, dw::core::ImgRenderer has got two methods (see more
documentation there):

- dw::core::ImgRenderer::setBuffer,
- dw::core::ImgRenderer::drawRow,
- dw::core::ImgRenderer::finish, and
- dw::core::ImgRenderer::fatal.


Images
======

This is the simplest renderer, displaying an image. For each row to be
drawn,

- first dw::core::Imgbuf::copyRow, and then
- for each dw::Image, dw::Image::drawRow must be called, with the same
  argument (no scaling is necessary).

dw::Image automatically scales the dw::core::Imgbuf, the root buffer
should be passed to dw::Image::setBuffer.

\see dw::Image for more details.


Background Images
=================

Since background images are style resources, they are associated with
dw::core::style::Style, as dw::core::style::StyleImage, which is
handled in a similar way (reference counting etc.) as
dw::core::style::Color and dw::core::style::Font, although it is
concrete and not platform-dependant.

The actual renderer (passed to Web) is an instance of
dw::core::ImgRendererDist (distributes all calls to a set of other
instances of dw::core::ImgRenderer), which contains two kinds of
renderers:

- one instance of dw::core::style::StyleImage::StyleImgRenderer, which
  does everything needed for dw::core::style::StyleImage, and
- other renderers, used externally (widgets etc.), which are added by
  dw::core::style::StyleImage::putExternalImgRenderer (and removed by
  dw::core::style::StyleImage::removeExternalImgRenderer).

This diagram gives an comprehensive overview:

\dot
digraph G {
   node [shape=record, fontname=Helvetica, fontsize=10];
   edge [arrowhead="open", dir="both", arrowtail="none",
         labelfontname=Helvetica, labelfontsize=10, color="#404040",
         labelfontcolor="#000080"];
   fontname=Helvetica; fontsize=10;

   subgraph cluster_dw_style {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;

      Style [URL="\ref dw::core::style::Style"];
      StyleImage [URL="\ref dw::core::style::StyleImage"];
      Imgbuf [URL="\ref dw::core::Imgbuf", color="#a0a0a0"];
      StyleImgRenderer
         [URL="\ref dw::core::style::StyleImage::StyleImgRenderer"];
      ImgRenderer [URL="\ref dw::core::ImgRenderer", color="#ff8080"];
      ImgRendererDist [URL="\ref dw::core::ImgRendererDist"];
      ExternalImgRenderer
         [URL="\ref dw::core::style::StyleImage::ExternalImgRenderer",
          color="#a0a0a0"];
      ExternalWidgetImgRenderer
         [URL="\ref dw::core::style::StyleImage::ExternalWidgetImgRenderer",
          color="#a0a0a0"];
   }

   subgraph cluster_dw_layout {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;

      Layout [URL="\ref dw::core::Layout"];
      LayoutImgRenderer [URL="\ref dw::core::Layout::LayoutImgRenderer"];
   }

   subgraph cluster_dw_widget {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;

      Widget [URL="\ref dw::core::Widget", color="#a0a0a0"];
      WidgetImgRenderer [URL="\ref dw::core::Widget::WidgetImgRenderer"];
   }

   subgraph cluster_dw_textblock {
      style="dashed"; color="#000080"; fontname=Helvetica; fontsize=10;

      Textblock [URL="\ref dw::Textblock"];
      Word [URL="\ref dw::Textblock::Word"];
      WordImgRenderer [URL="\ref dw::Textblock::WordImgRenderer"];
      SpaceImgRenderer [URL="\ref dw::Textblock::SpaceImgRenderer"];
   }

   Style -> StyleImage [headlabel="*", taillabel="1"];
   StyleImage -> Imgbuf [headlabel="*", taillabel="1"];
   StyleImage -> StyleImgRenderer [headlabel="1", taillabel="1"];
   StyleImage -> ImgRendererDist [headlabel="1", taillabel="1"];
   ImgRendererDist -> StyleImgRenderer [headlabel="1", taillabel="1"];
   ImgRendererDist -> ImgRenderer [headlabel="1", taillabel="*"];

   ImgRenderer -> ImgRendererDist [arrowhead="none", arrowtail="empty",
                                   dir="both", style="dashed"];
   ImgRenderer -> StyleImgRenderer [arrowhead="none", arrowtail="empty",
                                   dir="both", style="dashed"];
   ImgRenderer -> ExternalImgRenderer [arrowhead="none", arrowtail="empty",
                                      dir="both", style="dashed"];
   ExternalImgRenderer -> ExternalWidgetImgRenderer [arrowhead="none",
                                 arrowtail="empty", dir="both", style="dashed"];

   Layout -> LayoutImgRenderer [headlabel="1", taillabel="0..1"];
   ExternalImgRenderer -> LayoutImgRenderer [arrowhead="none",
                                 arrowtail="empty", dir="both", style="dashed"];

   Widget -> WidgetImgRenderer [headlabel="1", taillabel="0..1"];
   ExternalWidgetImgRenderer -> WidgetImgRenderer [arrowhead="none",
                                 arrowtail="empty", dir="both", style="dashed"];

   Textblock -> Word [headlabel="1", taillabel="*"];
   Word -> WordImgRenderer [headlabel="1", taillabel="0..1"];
   Word -> SpaceImgRenderer [headlabel="1", taillabel="0..1"];
   ExternalWidgetImgRenderer -> WordImgRenderer [arrowhead="none",
                                 arrowtail="empty", dir="both", style="dashed"];
   WordImgRenderer -> SpaceImgRenderer [arrowhead="none", arrowtail="empty",
                                        dir="both", style="dashed"];
}
\enddot

<center>[\ref uml-legend "legend"]</center>


Memory management
-----------------

dw::core::style::StyleImage extends lout::signal::ObservedObject, so
that deleting this instance can be connected to code dealing with
cache clients etc. See StyleImageDeletionReceiver and how it is
attached in StyleEngine::apply ("case CSS_PROPERTY_BACKGROUND_IMAGE").


Bugs and Things Needing Improvement
===================================

(Mostly related to image backgrounds, when not otherwise mentioned.)

High Priority
-------------

**Configurability, security/privacy aspects, etc.,** which are
currently available for image widgets, should be adopted. Perhaps some
more configuration options specially for background images.


Medium Priority
---------------

**Background-attachment** is not yet implemented, and will be postponed.

**Calls to dw::core::ImgRenderer::fatal** are incomplete. As an
example, it is not called, when connecting to a server fails. (And so,
as far as I see, no cache client is started.)


Low Priority
------------

**Alpha support:** (not related to image backgrounds) currently alpha
support (and also colormap management) is done in dicache, while
dw::Image is only created with type RGB. This leads to several problems:

- One dicache entry (representing an image related to the same URL),
  which has only one background color, may refer to different images
  with different background colors.
- The dicache only handles background colors, not background images.

The solution is basicly simple: keep alpha support out of dicache;
instead implement RGBA in dw::Image. As it seems, the main problem is
alpha support in FLTK/X11.


Solved (Must Be Documented)
---------------------------

*Drawing background images row by row may become slow. As an
alternative, dw::core::ImgRenderer::finish could be used. However,
drawing row by row could become an option.* There is now
dw::core::style::drawBackgroundLineByLine, which can be changed in the
code, and is set to *false*. The old code still exists, so changing
this to *true* activates again drawing line by line.

(For image widgets, this could also become an option: in contexts,
when image data is retrieved in a very fast way.)

*/
