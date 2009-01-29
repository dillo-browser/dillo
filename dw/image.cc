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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include "image.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

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
   registerName ("dw::Image", &CLASS_ID);
   this->altText = altText ? strdup (altText) : NULL;
   altTextWidth = -1; // not yet calculated
   buffer = NULL;
   clicking = false;
   currLink = -1;
   mapList = NULL;
   mapKey = NULL;
   isMap = false;
}

Image::~Image()
{
   if (altText)
      delete altText;
   if (buffer)
      buffer->unref ();
   if (mapKey)
      delete mapKey;
}

void Image::sizeRequestImpl (core::Requisition *requisition)
{
   if (buffer) {
      if (getStyle ()->height == core::style::LENGTH_AUTO &&
          core::style::isAbsLength (getStyle ()->width) &&
          buffer->getRootWidth () > 0) {
         // preserve aspect ratio when only width is given
         requisition->width = core::style::absLengthVal (getStyle ()->width);
         requisition->ascent = buffer->getRootHeight () *
                               requisition->width / buffer->getRootWidth ();
      } else if (getStyle ()->width == core::style::LENGTH_AUTO &&
                 core::style::isAbsLength (getStyle ()->height) &&
                 buffer->getRootHeight () > 0) {
         // preserve aspect ratio when only height is given
         requisition->ascent = core::style::absLengthVal (getStyle ()->height);
         requisition->width = buffer->getRootWidth () *
                               requisition->ascent / buffer->getRootHeight ();
      } else {
         requisition->width = buffer->getRootWidth ();
         requisition->ascent = buffer->getRootHeight ();
      }
      requisition->descent = 0;
   } else {
      if(altText && altText[0]) {
         if (altTextWidth == -1)
            altTextWidth =
               layout->textWidth (getStyle()->font, altText, strlen (altText));

         requisition->width = altTextWidth;
         requisition->ascent = getStyle()->font->ascent;
         requisition->descent = getStyle()->font->descent;
      } else {
         requisition->width = 0;
         requisition->ascent = 0;
         requisition->descent = 0;
      }
   }

   requisition->width += getStyle()->boxDiffWidth ();
   requisition->ascent += getStyle()->boxOffsetY ();
   requisition->descent += getStyle()->boxRestHeight ();
}

void Image::sizeAllocateImpl (core::Allocation *allocation)
{
   core::Imgbuf *oldBuffer;
   int dx, dy;

   /* if image is moved only */
   if (allocation->width == this->allocation.width &&
       allocation->ascent + allocation->descent == getHeight ())
      return;

   dx = getStyle()->boxDiffWidth ();
   dy = getStyle()->boxDiffHeight ();
#if 0
   MSG("boxDiffHeight = %d + %d, buffer=%p\n",
       getStyle()->boxOffsetY(), getStyle()->boxRestHeight(), buffer);
   MSG("getContentWidth() = allocation.width - style->boxDiffWidth ()"
       " = %d - %d = %d\n",
       this->allocation.width, getStyle()->boxDiffWidth(),
       this->allocation.width - getStyle()->boxDiffWidth());
   MSG("getContentHeight() = getHeight() - style->boxDiffHeight ()"
       " = %d - %d = %d\n", this->getHeight(), getStyle()->boxDiffHeight(),
       this->getHeight() - getStyle()->boxDiffHeight());
#endif
   if (buffer && (getContentWidth () > 0 || getContentHeight () > 0)) {
      // Zero content size : simply wait...
      // Only one dimension: naturally scale
      oldBuffer = buffer;
      buffer = oldBuffer->getScaledBuf (allocation->width - dx,
                                        allocation->ascent
                                        + allocation->descent - dy);
      oldBuffer->unref ();
   }
}

void Image::enterNotifyImpl (core::EventCrossing *event)
{
   // BUG: this is wrong for image maps, but the cursor position is unknown.
   currLink = getStyle()->x_link;

   if (currLink != -1) {
      (void) emitLinkEnter (currLink, -1, -1, -1);
   }
}

void Image::leaveNotifyImpl (core::EventCrossing *event)
{                                                          
   clicking = false;

   if (currLink != -1) {
      currLink = -1;
      (void) emitLinkEnter (-1, -1, -1, -1);
   }
}

bool Image::motionNotifyImpl (core::EventMotion *event)
{
   if (mapList) {
      /* client-side image map */
      int newLink = mapList->link (mapKey, event->xWidget, event->yWidget);
      if (newLink != currLink) {
         currLink = newLink;
         clicking = false;
         setCursor(newLink == -1 ? core::style::CURSOR_DEFAULT :
                                   core::style::CURSOR_POINTER);
         (void) emitLinkEnter (newLink, -1, -1, -1);
      }
   } else if (isMap && currLink != -1) {
      /* server-side image map */
      (void) emitLinkEnter (currLink, -1, event->xWidget, event->yWidget);
   }
   return true;
}

bool Image::buttonPressImpl (core::EventButton *event)
{
   bool ret = false;
   currLink = mapList ? mapList->link (mapKey, event->xWidget, event->yWidget):
      getStyle()->x_link;
   if (event->button == 3){
      (void)emitLinkPress(currLink, getStyle()->x_img, -1,-1,event);
      ret = true;
   } else if (event->button == 1 || currLink != -1){
      clicking = true;
      ret = true;
   }
   return ret;
}              

bool Image::buttonReleaseImpl (core::EventButton *event)
{
   currLink = mapList ? mapList->link (mapKey, event->xWidget, event->yWidget):
      getStyle()->x_link;
   if (clicking) {
      int x = isMap ? event->xWidget : -1;
      int y = isMap ? event->yWidget : -1;
      clicking = false;
      emitLinkClick (currLink, getStyle()->x_img, x, y, event);
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
      if(altText && altText[0]) {
         if (altTextWidth == -1)
            altTextWidth =
               layout->textWidth (getStyle()->font, altText, strlen (altText));
         
         core::View *clippingView = NULL, *usedView = view;
         if (allocation.width < altTextWidth ||
             allocation.ascent < getStyle()->font->ascent ||
             allocation.descent < getStyle()->font->descent) {
            clippingView = usedView =
               view->getClippingView (allocation.x + getStyle()->boxOffsetX (),
                                      allocation.y + getStyle()->boxOffsetY (),
                                      allocation.width
                                      - getStyle()->boxDiffWidth (),
                                      allocation.ascent + allocation.descent
                                      - getStyle()->boxDiffHeight ());
         }

         usedView->drawText (getStyle()->font, getStyle()->color,
                             core::style::Color::SHADING_NORMAL,
                             allocation.x + getStyle()->boxOffsetX (),
                             allocation.y + getStyle()->boxOffsetY ()
                             + getStyle()->font->ascent,
                             altText, strlen(altText));

         if(clippingView)
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

   if (resize)
      queueResize (0, true);

   if (wasAllocated () && getContentWidth () > 0 && getContentHeight () > 0) {
      // Only scale when both dimensions are known.
      this->buffer =
         buffer->getScaledBuf (getContentWidth (), getContentHeight ());
   } else {
      this->buffer = buffer;
      buffer->ref ();
   }

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
