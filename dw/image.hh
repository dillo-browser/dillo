#ifndef __DW_IMAGE_HH__
#define __DW_IMAGE_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Represents a list of client-side image maps.
 *
 * All image maps of a HTML page (in the future, also image maps from
 * different HTML pages) are stored in a list, which is passed to the
 * image, so that it is possible to deal with maps, which are defined
 * after the image within the HTML page.
 *
 * Maps are referred by instances of object::Object. These keys are
 * typically URLs, so the type representing URLS should be derived from
 * object::Object.
 *
 * \todo Some methods within the key class have to be implemented, this
 *       is not clear at this time.
 */
class ImageMapsList
{
private:
   class ImageMap: public lout::object::Object {
      private:
         class ShapeAndLink: public lout::object::Object {
         public:
            core::Shape *shape;
            int link;

            ~ShapeAndLink () { if (shape) delete shape; };
         };

         lout::container::typed::List <ShapeAndLink> *shapesAndLinks;
         int defaultLink;
      public:
         ImageMap ();
         ~ImageMap ();

         void draw (core::View *view, core::style::Style *style, int x, int y);
         void add (core::Shape *shape, int link);
         void setDefaultLink (int link) { defaultLink = link; };
         int link (int x, int y);
   };

   lout::container::typed::HashTable <lout::object::Object, ImageMap>
      *imageMaps;
   ImageMap *currentMap;

public:
   ImageMapsList ();
   ~ImageMapsList ();

   void startNewMap (lout::object::Object *key);
   void addShapeToCurrentMap (core::Shape *shape, int link);
   void setCurrentMapDefaultLink (int link);
   void drawMap(lout::object::Object *key, core::View *view,
                core::style::Style *style, int x, int y);
   int link (lout::object::Object *key, int x, int y);
};

/**
 * \brief Displays an instance of dw::core::Imgbuf.
 *
 * The dw::core::Imgbuf is automatically scaled, when needed, but dw::Image
 * does not keep a reference on the root buffer.
 *
 *
 * <h3>Signals</h3>
 *
 * For image maps, dw::Image uses the signals defined in
 * dw::core::Layout::LinkReceiver. For client side image maps, -1 is
 * passed for the coordinates, for server side image maps, the respective
 * coordinates are used. See section "Image Maps" below.
 *
 *
 * <h3>%Image Maps</h3>
 *
 * <h4>Client Side %Image Maps</h4>
 *
 * You must first create a list of image maps (dw::ImageMapList), which can
 * be used for multiple images. The caller is responsible for freeing the
 * dw::ImageMapList.
 *
 * Adding a map is done by dw::ImageMapsList::startNewMap. The key is an
 * instance of a sub class of object::Object. In the context of HTML, this is
 * a URL, which defines this map globally, by combining the URL of the
 * document, this map is defined in, with the value of the attribute "name" of
 * the \<MAP\> element, as a fragment.
 *
 * dw::ImageMapsList::addShapeToCurrentMap adds a shape to the current
 * map. The \em link argument is a number, which is later passed to
 * the dw::core::Layout::LinkReceiver.
 *
 * This map list is then, together with the key for the image, passed to
 * dw::Image::setUseMap. For HTML, a URL with the value of the "ismap"
 * attribute of \<IMG\> should be used.
 *
 * dw::Image will search the correct map, when needed. If it is not found
 * at this time, but later defined, it will be found and used later. This is
 * the case, when an HTML \<MAP\> is defined below the \<IMG\> in the
 * document.
 *
 * Currently, only maps defined in the same document as the image may be
 * used, since the dw::ImageMapsList is stored in the HTML link block, and
 * contains only the image maps defined in the document.
 *
 * <h4>Server Side %Image Maps</h4>
 *
 * To use images for server side image maps, you must call
 * dw::Image::setIsMap, and the dw::Image::style must contain a valid link
 * (dw::core::style::Style::x_link). After this, motions and clicks are
 * delegated to dw::core::Layout::LinkReceiver.
 *
 * \sa\ref dw-images-and-backgrounds
 */
class Image: public core::Widget, public core::ImgRenderer
{
private:
   char *altText;
   core::Imgbuf *buffer;
   int altTextWidth;
   bool clicking;
   int currLink;
   ImageMapsList *mapList;
   Object *mapKey;
   bool isMap;

protected:
   void sizeRequestImpl (core::Requisition *requisition);
   void sizeAllocateImpl (core::Allocation *allocation);

   void draw (core::View *view, core::Rectangle *area);

   bool buttonPressImpl (core::EventButton *event);
   bool buttonReleaseImpl (core::EventButton *event);
   void enterNotifyImpl (core::EventCrossing *event);
   void leaveNotifyImpl (core::EventCrossing *event);
   bool motionNotifyImpl (core::EventMotion *event);
   int contentX (core::MousePositionEvent *event);
   int contentY (core::MousePositionEvent *event);

   //core::Iterator *iterator (Content::Type mask, bool atEnd);

public:
   static int CLASS_ID;

   Image(const char *altText);
   ~Image();

   core::Iterator *iterator (core::Content::Type mask, bool atEnd);

   inline core::Imgbuf *getBuffer () { return buffer; }
   void setBuffer (core::Imgbuf *buffer, bool resize = false);

   void drawRow (int row);

   void finish ();
   void fatal ();

   void setIsMap ();
   void setUseMap (ImageMapsList *list, Object *key);

   /* This is a hack for the perhaps frivolous feature of drawing image map
    * shapes when there is no image to display. If the map is defined after
    * an image using an image map, and the actual image data has not been
    * loaded, tell the image to redraw.
    */
   void forceMapRedraw () { if (mapKey && ! buffer) queueDraw (); };
};

} // namespace dw

#endif // __DW_IMAGE_HH__
