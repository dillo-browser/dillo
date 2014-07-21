/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "image.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"
#include "../lout/debug.hh"

namespace dw {

using namespace lout;

ImageMapsList::ImageMap::ImageMap ()
{
   shapesAndLinks = new container::typed::List <ShapeAndLink> (true);
   defaultLink = -1;
}

ImageMapsList::ImageMap::~ImageMap ()
{
   delete shapesAndLinks;
}

void ImageMapsList::ImageMap::draw (core::View *view,core::style::Style *style,
                                    int x, int y)
{
   container::typed::Iterator <ShapeAndLink> it;

   for (it = shapesAndLinks->iterator (); it.hasNext (); ) {
      ShapeAndLink *shapeAndLink = it.getNext ();

      shapeAndLink->shape->draw(view, style, x, y);
   }
}

void ImageMapsList::ImageMap::add (core::Shape *shape, int link) {
   ShapeAndLink *shapeAndLink = new ShapeAndLink ();
   shapeAndLink->shape = shape;
   shapeAndLink->link = link;
   shapesAndLinks->append (shapeAndLink);
}

int ImageMapsList::ImageMap::link (int x, int y) {
   container::typed::Iterator <ShapeAndLink> it;
   int link = defaultLink;

   for (it = shapesAndLinks->iterator (); it.hasNext (); ) {
      ShapeAndLink *shapeAndLink = it.getNext ();

      if (shapeAndLink->shape->isPointWithin (x, y)) {
         link = shapeAndLink->link;
         break;
      }
   }

   return link;
}

ImageMapsList::ImageMapsList ()
{
   imageMaps = new container::typed::HashTable <object::Object, ImageMap>
      (true, true);
   currentMap = NULL;
}

ImageMapsList::~ImageMapsList ()
{
   delete imageMaps;
}

/**
 * \brief Start a new map and make it the current one.
 *
 * This has to be called before dw::ImageMapsList::addShapeToCurrentMap.
 * "key" is owned by the image map list, so a copy should be passed, when
 * necessary.
 */
void ImageMapsList::startNewMap (object::Object *key)
{
   currentMap = new ImageMap ();
   imageMaps->put (key, currentMap);
}

/**
 * \brief Add a shape to the current map-
 *
 * "shape" is owned by the image map list, so a copy should be passed, when
 * necessary.
 */
void ImageMapsList::addShapeToCurrentMap (core::Shape *shape, int link)
{
   currentMap->add (shape, link);
}

/**
 * \brief Set default link for current map-
 */
void ImageMapsList::setCurrentMapDefaultLink (int link)
{
   currentMap->setDefaultLink (link);
}

void ImageMapsList::drawMap (lout::object::Object *key, core::View *view,
                             core::style::Style *style, int x, int y)
{
   ImageMap *map = imageMaps->get (key);

   if (map)
      map->draw(view, style, x, y);
}

int ImageMapsList::link (object::Object *key, int x, int y)
{
   int link = -1;
   ImageMap *map = imageMaps->get (key);

   if (map)
      link = map->link (x, y);

   return link;
}

// ----------------------------------------------------------------------

int Image::CLASS_ID = -1;

Image::Image(const char *altText)
{
   DBG_OBJ_CREATE ("dw::Image");
   registerName ("dw::Image", &CLASS_ID);
   this->altText = altText ? strdup (altText) : NULL;
   altTextWidth = -1; // not yet calculated
   buffer = NULL;
   bufWidth = bufHeight = -1;
   clicking = false;
   currLink = -1;
   mapList = NULL;
   mapKey = NULL;
   isMap = false;

   DBG_OBJ_SET_NUM ("bufWidth", bufWidth);
   DBG_OBJ_SET_NUM ("bufHeight", bufHeight);
}

Image::~Image()
{
   if (altText)
      free(altText);
   if (buffer)
      buffer->unref ();
   if (mapKey)
      delete mapKey;

   DBG_OBJ_DELETE ();
}

void Image::sizeRequestImpl (core::Requisition *requisition)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequestImpl");

   if (buffer) {
      requisition->width = buffer->getRootWidth ();
      requisition->ascent = buffer->getRootHeight ();
   } else
      requisition->width = requisition->ascent = 0;

   requisition->width += boxDiffWidth ();
   requisition->ascent += boxOffsetY ();

   requisition->descent = boxRestHeight ();

   correctRequisition (requisition, core::splitHeightPreserveDescent);

   if (buffer) {
      // If one dimension is set, preserve the aspect ratio (without
      // extraSpace/margin/border/padding). Notice that
      // requisition->descent could have been changed in
      // core::splitHeightPreserveDescent, so we do not make any
      // assumtions here about it (and requisition->ascent).

      // TODO Check again possible overflows. (Aren't buffer
      // dimensions limited to 2^15?)

      if (getStyle()->width == core::style::LENGTH_AUTO &&
          getStyle()->height != core::style::LENGTH_AUTO) {
         requisition->width =
            (requisition->ascent + requisition->descent - boxDiffHeight ())
            * buffer->getRootWidth () / buffer->getRootHeight ()
            + boxDiffWidth ();
      } else if (getStyle()->width != core::style::LENGTH_AUTO &&
                 getStyle()->height == core::style::LENGTH_AUTO) {
         requisition->ascent = (requisition->width + boxDiffWidth ())
            * buffer->getRootHeight () / buffer->getRootWidth ()
            + boxOffsetY ();
         requisition->descent = boxRestHeight ();
      }
   }         

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent, requisition->descent);
   DBG_OBJ_LEAVE ();
}

void Image::getExtremesImpl (core::Extremes *extremes)
{
   int width = (buffer ? buffer->getRootWidth () : 0) + boxDiffWidth ();

   // With percentage width, the image may be narrower than the buffer.
   extremes->minWidth =
      core::style::isPerLength (getStyle()->width) ? boxDiffWidth () : width;

   // (We ignore the same effect for the maximal width.)
   extremes->maxWidth = width;

   extremes->minWidthIntrinsic = extremes->minWidth;
   extremes->maxWidthIntrinsic = extremes->maxWidth;

   correctExtremes (extremes);
}

void Image::sizeAllocateImpl (core::Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize", 0, "sizeAllocateImpl", "%d, %d; %d * (%d + %d)",
                  allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   
   int newBufWidth = allocation->width - boxDiffWidth ();
   int newBufHeight =
      allocation->ascent + allocation->descent - boxDiffHeight ();

   if (buffer && newBufWidth > 0 && newBufHeight > 0 &&
       // Save some time when size did not change:
       (newBufWidth != bufWidth || newBufHeight != bufHeight)) {
      DBG_OBJ_MSG ("resize", 1, "replacing buffer");
      
      core::Imgbuf *oldBuffer = buffer;
      buffer = oldBuffer->getScaledBuf (newBufWidth, newBufHeight);
      oldBuffer->unref ();
      
      bufWidth = newBufWidth;
      bufHeight = newBufHeight;
      
      DBG_OBJ_ASSOC_CHILD (this->buffer);
      DBG_OBJ_SET_NUM ("bufWidth", bufWidth);
      DBG_OBJ_SET_NUM ("bufHeight", bufHeight);
   }
   
   DBG_OBJ_LEAVE ();
}

void Image::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");
   // Nothing to do.
   DBG_OBJ_LEAVE ();
}

void Image::enterNotifyImpl (core::EventCrossing *event)
{
   // BUG: this is wrong for image maps, but the cursor position is unknown.
   currLink = getStyle()->x_link;

   if (currLink != -1) {
      (void) layout->emitLinkEnter (this, currLink, -1, -1, -1);
   }
   Widget::enterNotifyImpl(event);
}

void Image::leaveNotifyImpl (core::EventCrossing *event)
{
   clicking = false;

   if (currLink != -1) {
      currLink = -1;
      (void) layout->emitLinkEnter (this, -1, -1, -1, -1);
   }
   Widget::leaveNotifyImpl(event);
}

/*
 * Return the coordinate relative to the contents.
 * If the event occurred in the surrounding box, return the value at the
 * edge of the contents instead.
 */
int Image::contentX (core::MousePositionEvent *event)
{
   int ret = event->xWidget - getStyle()->boxOffsetX();

   ret = misc::min(getContentWidth(), misc::max(ret, 0));
   return ret;
}

int Image::contentY (core::MousePositionEvent *event)
{
   int ret = event->yWidget - getStyle()->boxOffsetY();

   ret = misc::min(getContentHeight(), misc::max(ret, 0));
   return ret;
}

bool Image::motionNotifyImpl (core::EventMotion *event)
{
   if (mapList || isMap) {
      int x = contentX(event);
      int y = contentY(event);

      if (mapList) {
         /* client-side image map */
         int newLink = mapList->link (mapKey, x, y);
         if (newLink != currLink) {
            currLink = newLink;
            clicking = false;
            /* \todo Using MAP/AREA styles would probably be best */
            setCursor(newLink == -1 ? getStyle()->cursor :
                                      core::style::CURSOR_POINTER);
            (void) layout->emitLinkEnter (this, newLink, -1, -1, -1);
         }
      } else if (isMap && currLink != -1) {
         /* server-side image map */
         (void) layout->emitLinkEnter (this, currLink, -1, x, y);
      }
   }
   return true;
}

bool Image::buttonPressImpl (core::EventButton *event)
{
   bool ret = false;

   currLink = mapList ? mapList->link(mapKey,contentX(event),contentY(event)) :
      getStyle()->x_link;
   if (event->button == 3){
      (void)layout->emitLinkPress(this, currLink, getStyle()->x_img, -1, -1,
                                  event);
      ret = true;
   } else if (event->button == 1 || currLink != -1){
      clicking = true;
      ret = true;
   }
   return ret;
}

bool Image::buttonReleaseImpl (core::EventButton *event)
{
   currLink = mapList ? mapList->link(mapKey,contentX(event),contentY(event)) :
      getStyle()->x_link;
   if (clicking) {
      int x = isMap ? contentX(event) : -1;
      int y = isMap ? contentY(event) : -1;
      clicking = false;
      layout->emitLinkClick (this, currLink, getStyle()->x_img, x, y, event);
      return true;
   }
   return false;
}

void Image::draw (core::View *view, core::Rectangle *area)
{
   int dx, dy;
   core::Rectangle content, intersection;

   drawWidgetBox (view, area, false);

   if (buffer) {
      dx = getStyle()->boxOffsetX ();
      dy = getStyle()->boxOffsetY ();
      content.x = dx;
      content.y = dy;
      content.width = getContentWidth ();
      content.height = getContentHeight ();

      if (area->intersectsWith (&content, &intersection))
         view->drawImage (buffer,
                          allocation.x + dx, allocation.y + dy,
                          intersection.x - dx, intersection.y - dy,
                          intersection.width, intersection.height);
   } else {
      core::View *clippingView;

      if (altText && altText[0]) {
         core::View *usedView = view;

         clippingView = NULL;

         if (altTextWidth == -1)
            altTextWidth =
               layout->textWidth (getStyle()->font, altText, strlen (altText));

         if ((getContentWidth() < altTextWidth) ||
             (getContentHeight() <
              getStyle()->font->ascent + getStyle()->font->descent)) {
            clippingView = usedView =
               view->getClippingView (allocation.x + getStyle()->boxOffsetX (),
                                      allocation.y + getStyle()->boxOffsetY (),
                                      getContentWidth(),
                                      getContentHeight());
         }

         usedView->drawSimpleWrappedText (getStyle()->font, getStyle()->color,
                             core::style::Color::SHADING_NORMAL,
                             allocation.x + getStyle()->boxOffsetX (),
                             allocation.y + getStyle()->boxOffsetY (),
                             getContentWidth(), getContentHeight(), altText);

         if (clippingView)
            view->mergeClippingView (clippingView);
      }
      if (mapKey) {
         clippingView = view->getClippingView (allocation.x +
                                               getStyle()->boxOffsetX (),
                                               allocation.y +
                                               getStyle()->boxOffsetY (),
                                               getContentWidth(),
                                               getContentHeight());
         mapList->drawMap(mapKey, clippingView, getStyle(),
                          allocation.x + getStyle()->boxOffsetX (),
                          allocation.y + getStyle()->boxOffsetY ());
         view->mergeClippingView (clippingView);
      }
   }

   /** TODO: draw selection */
}

core::Iterator *Image::iterator (core::Content::Type mask, bool atEnd)
{
   //return new core::TextIterator (this, mask, atEnd, altText);
   /** \bug Not implemented. */
   return new core::EmptyIterator (this, mask, atEnd);
}

void Image::setBuffer (core::Imgbuf *buffer, bool resize)
{
   core::Imgbuf *oldBuf = this->buffer;

   if (wasAllocated () && needsResize () &&
      getContentWidth () > 0 && getContentHeight () > 0) {
      // Don't create a new buffer for the transition from alt text to img,
      // and only scale when both dimensions are known.

      bufWidth = getContentWidth ();
      bufHeight = getContentHeight ();
      this->buffer = buffer->getScaledBuf (bufWidth, bufHeight);
   } else {
      this->buffer = buffer;
      bufWidth = buffer->getRootWidth ();
      bufHeight = buffer->getRootHeight ();
      buffer->ref ();
   }
   queueResize (0, true);

   DBG_OBJ_ASSOC_CHILD (this->buffer);
   DBG_OBJ_SET_NUM ("bufWidth", bufWidth);
   DBG_OBJ_SET_NUM ("bufHeight", bufHeight);

   if (oldBuf)
      oldBuf->unref ();
}

void Image::drawRow (int row)
{
   core::Rectangle area;

   assert (buffer != NULL);

   buffer->getRowArea (row, &area);
   if (area.width && area.height)
      queueDrawArea (area.x + getStyle()->boxOffsetX (),
                     area.y + getStyle()->boxOffsetY (),
                     area.width, area.height);
}

void Image::finish ()
{
   // Nothing to do; images are always drawn line by line.
}

void Image::fatal ()
{
   // Could display an error.
}


/**
 * \brief Sets image as server side image map.
 */
void Image::setIsMap ()
{
   isMap = true;
}


/**
 * \brief Sets image as client side image map.
 *
 * "list" is not owned by the image, the caller has to free it. "key"
 * is owned by the image, if it is used by the caller afterwards, a copy
 * should be passed.
 */
void Image::setUseMap (ImageMapsList *list, object::Object *key)
{
   mapList = list;
   if (mapKey && mapKey != key)
      delete mapKey;
   mapKey = key;
}

} // namespace dw
