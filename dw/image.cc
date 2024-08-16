/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
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



//#define DEBUG_LEVEL 1
#include "image.hh"
#include "dlib/dlib.h"
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
   this->altText = altText ? dStrdup (altText) : NULL;
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

void Image::sizeRequestSimpl (core::Requisition *requisition)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequestImpl");

   DEBUG_MSG(1, "-- Image::sizeRequestSimpl() begins\n");

   /* First set the a naive size based on the image properties if given */

   if (buffer) {
      requisition->width = buffer->getRootWidth ();
      requisition->ascent = buffer->getRootHeight ();
      requisition->descent = 0;
   } else {
      if (altText && altText[0]) {
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

   requisition->width += boxDiffWidth ();
   requisition->ascent += boxOffsetY ();
   requisition->descent += boxRestHeight ();

   DEBUG_MSG(1, "initial requisition: w=%d, h=%d\n",
         requisition->width, requisition->ascent + requisition->descent);

   /* Then correct the size if needed, so it fits within the available space in
    * the container widget. */

   /* FIXME: The standard Widget::correctRequisition() doesn't take into account
    * any extra contraint like we want for the aspect ratio. It will only modify
    * the width and height until the image fits the available space. We probably
    * need a custom implementation of it to handle the aspect ratio constraint.
    */
   correctRequisition (requisition, core::splitHeightPreserveDescent, true,
                       true);

   DEBUG_MSG(1, "corrected requisition: w=%d, h=%d\n",
         requisition->width, requisition->ascent + requisition->descent);

   /* Finally, ensure that we don't distort the image unless the height and
    * width are fixed. For each dimension (width, height) of the image size,
    * there are two situations: the image has the size imposed by the containing
    * widget or the image sets its own size. */

   if (buffer) {
      // If one dimension is set, preserve the aspect ratio (without
      // extraSpace/margin/border/padding). Notice that
      // requisition->descent could have been changed in
      // core::splitHeightPreserveDescent, so we do not make any
      // assumtions here about it (and requisition->ascent).

      // TODO Check again possible overflows. (Aren't buffer
      // dimensions limited to 2^15?)

      bool wFixed = getStyle()->width != core::style::LENGTH_AUTO;
      bool hFixed = getStyle()->height != core::style::LENGTH_AUTO;

      /* Image dimensions */
      int w = buffer->getRootWidth ();
      int h = buffer->getRootHeight ();

      /* Reference size in case we are the root */
      int w_ref = -1, h_ref = -1;

      /* TODO: What if we have a quasiParent? */
      if (getParent() == NULL) {
         w_ref = layout->getWidthViewport();
         h_ref = layout->getHeightViewport();
      }

      /* FIXME: Cannot get private information from Layout to avoid the 
       * scrollbar width. */
      //int w_ref = layout->getWidthViewport() - (layout->canvasHeightGreater ?
      //                            layout->vScrollbarThickness : 0);

      DEBUG_MSG(1, "initial size: w=%d, h=%d\n", w, h);

      float ratio = (float) h / (float) w;

      DEBUG_MSG(1, "wFixed=%d, hFixed=%d\n", wFixed, hFixed);

      /* When the size is fixed, set w and h accordingly */
      if (hFixed)
         h = requisition->ascent + requisition->descent;

      if (wFixed)
         w = requisition->width;

      DEBUG_MSG(1, "size after fixed correction: w=%d, h=%d\n", w, h);

      /* Correct the image size to keep the aspect ratio, but only change the
       * dimensions that are not fixed, if any. */

      if (!wFixed && !hFixed) {
         DEBUG_MSG(1, "case: both can change\n");
         /* Both width and height are not set, so we can play with both dimensions. */
         int minWidth  = calcWidth(getStyle()->minWidth,  w_ref, getParent(), 0, true);
         int maxWidth  = calcWidth(getStyle()->maxWidth,  w_ref, getParent(), 0, true);
         int minHeight = calcHeight(getStyle()->minHeight, true, h_ref, getParent(), true);
         int maxHeight = calcHeight(getStyle()->maxHeight, true, h_ref, getParent(), true);

         DEBUG_MSG(1, "minWidth = %d, maxWidth = %d\n", minWidth, maxWidth);
         DEBUG_MSG(1, "minHeight= %d, maxHeight= %d\n", minHeight, maxHeight);

         /* Fix broken width by expanding the maximum */
         if (minWidth != -1 && maxWidth != -1 && minWidth > maxWidth)
            maxWidth = minWidth;

         /* Fix broken height by expanding the maximum */
         if (minHeight != -1 && maxHeight != -1 && minHeight > maxHeight)
            maxHeight = minHeight;

         DEBUG_MSG(1, "corrected minWidth = %d, maxWidth = %d\n", minWidth, maxWidth);
         DEBUG_MSG(1, "corrected minHeight= %d, maxHeight= %d\n", minHeight, maxHeight);

         /* Repeat 2 times fixing the aspect ratio, two more without */
         for (int i = 0; i < 4; i++) {
            DEBUG_MSG(1, "iteration %d, w=%d, h=%d\n", i, w, h);
            int old_w = w;

            /* Constraint the width first */
            if (maxWidth != -1 && w > maxWidth)
               w = maxWidth;
            else if (minWidth != -1 && w < minWidth)
               w = minWidth;

            /* Now we may have distorted the image, so try to fix the aspect ratio */
            if (w != old_w && i < 2)
               h = w * ratio;

            int old_h = h;

            /* Now constraint the height */
            if (maxHeight != -1 && h > maxHeight)
               h = maxHeight;
            else if (minHeight != -1 && h < minHeight)
               h = minHeight;

            /* Now we may have distorted the image again, so fix w */
            if (h != old_h && i < 2)
               w = h / ratio;

            DEBUG_MSG(1, "w=%d, h=%d\n", w, h);

            /* All contraints meet */
            if (old_h == h && old_w == w)
               break;
         }
      } else if (wFixed && !hFixed) {
         /* Only height can change */
         DEBUG_MSG(1, "case: only heigh can change\n");
         int minHeight = calcHeight(getStyle()->minHeight, true, h_ref, getParent(), true);
         int maxHeight = calcHeight(getStyle()->maxHeight, true, h_ref, getParent(), true);

         DEBUG_MSG(1, "minHeight= %d, maxHeight= %d\n", minHeight, maxHeight);

         /* Fix broken height by expanding the maximum */
         if (minHeight != -1 && maxHeight != -1 && minHeight > maxHeight)
            maxHeight = minHeight;

         DEBUG_MSG(1, "corrected minHeight= %d, maxHeight= %d\n", minHeight, maxHeight);

         /* Try preserving the ratio */
         h = w * ratio;

         /* Now constraint the height */
         if (maxHeight != -1 && h > maxHeight)
            h = maxHeight;
         else if (minHeight != -1 && h < minHeight)
            h = minHeight;
      } else if (!wFixed && hFixed) {
         /* Only width can change */
         DEBUG_MSG(1, "case: only width can change\n");
         int minWidth = calcWidth(getStyle()->minWidth, w_ref, getParent(), 0, true);
         int maxWidth = calcWidth(getStyle()->maxWidth, w_ref, getParent(), 0, true);

         DEBUG_MSG(1, "minWidth = %d, maxWidth = %d\n", minWidth, maxWidth);

         /* Fix broken width by expanding the maximum */
         if (minWidth != -1 && maxWidth != -1 && minWidth > maxWidth)
            maxWidth = minWidth;

         DEBUG_MSG(1, "corrected minWidth = %d, maxWidth = %d\n", minWidth, maxWidth);

         /* Try preserving the ratio */
         w = h / ratio;
         DEBUG_MSG(1, "by ratio, w=%d\n", w);

         /* Now constraint the width */
         if (maxWidth != -1 && w > maxWidth)
            w = maxWidth;
         else if (minWidth != -1 && w < minWidth)
            w = minWidth;
      } else {
         /* TODO: Both dimensions are fixed, however we may still be able to
          * adjust some if they use percent positions and the container widget
          * gives us some flexibility.
          *
          * The only situation in which we can still correct the aspect ratio is
          * that the image dimensions are specified as a percentage of the
          * container widget *and* the container widget doesn't force a size.
          */
         DEBUG_MSG(1, "case: both width and height are fixed\n");
      }

      DEBUG_MSG(1, "final: w=%d, h=%d\n", w, h);

      requisition->width = w + boxDiffWidth ();
      requisition->ascent = h + boxOffsetY ();
      requisition->descent = boxRestHeight ();
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent, requisition->descent);
   DBG_OBJ_LEAVE ();
}

void Image::getExtremesSimpl (core::Extremes *extremes)
{
   int contentWidth;
   if (buffer)
      contentWidth = buffer->getRootWidth ();
   else {
      if (altText && altText[0]) {
         if (altTextWidth == -1)
            altTextWidth =
               layout->textWidth (getStyle()->font, altText, strlen (altText));
         contentWidth = altTextWidth;
      } else
         contentWidth = 0;
   }

   int width = contentWidth + boxDiffWidth ();

   // With percentage width, the image may be narrower than the buffer.
   extremes->minWidth =
      core::style::isPerLength (getStyle()->width) ? boxDiffWidth () : width;

   // (We ignore the same effect for the maximal width.)
   extremes->maxWidth = width;

   extremes->minWidthIntrinsic = extremes->minWidth;
   extremes->maxWidthIntrinsic = extremes->maxWidth;

   correctExtremes (extremes, false);

   extremes->adjustmentWidth =
      misc::min (extremes->minWidthIntrinsic, extremes->minWidth);
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
   int ret = event->xWidget - boxOffsetX();

   ret = misc::min(getContentWidth(), misc::max(ret, 0));
   return ret;
}

int Image::contentY (core::MousePositionEvent *event)
{
   int ret = event->yWidget - boxOffsetY();

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

void Image::draw (core::View *view, core::Rectangle *area,
                  core::DrawingContext *context)
{
   int dx, dy;
   core::Rectangle content, intersection;

   drawWidgetBox (view, area, false);

   if (buffer) {
      dx = boxOffsetX ();
      dy = boxOffsetY ();
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
               view->getClippingView (allocation.x + boxOffsetX (),
                                      allocation.y + boxOffsetY (),
                                      getContentWidth(),
                                      getContentHeight());
         }

         usedView->drawSimpleWrappedText (getStyle()->font, getStyle()->color,
                             core::style::Color::SHADING_NORMAL,
                             allocation.x + boxOffsetX (),
                             allocation.y + boxOffsetY (),
                             getContentWidth(), getContentHeight(), altText);

         if (clippingView)
            view->mergeClippingView (clippingView);
      }
      if (mapKey) {
         clippingView = view->getClippingView (allocation.x + boxOffsetX (),
                                               allocation.y + boxOffsetY (),
                                               getContentWidth(),
                                               getContentHeight());
         mapList->drawMap(mapKey, clippingView, getStyle(),
                          allocation.x + boxOffsetX (),
                          allocation.y + boxOffsetY ());
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
      queueDrawArea (area.x + boxOffsetX (), area.y + boxOffsetY (), area.width,
                     area.height);
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
