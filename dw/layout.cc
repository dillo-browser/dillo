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



#include "core.hh"

#include "../lout/msg.h"
#include "../lout/debug.hh"
#include "../lout/misc.hh"

using namespace lout;
using namespace lout::container;
using namespace lout::object;

namespace dw {
namespace core {

bool Layout::LayoutImgRenderer::readyToDraw ()
{
   return true;
}

void Layout::LayoutImgRenderer::getBgArea (int *x, int *y, int *width,
                                           int *height)
{
   // TODO Actually not padding area, but visible area?
   getRefArea (x, y, width, height);
}

void Layout::LayoutImgRenderer::getRefArea (int *xRef, int *yRef, int *widthRef,
                                            int *heightRef)
{
   *xRef = 0;
   *yRef = 0;
   *widthRef = misc::max (layout->viewportWidth
                          - (layout->canvasHeightGreater ?
                             layout->vScrollbarThickness : 0),
                          layout->canvasWidth);
   *heightRef = misc::max (layout->viewportHeight
                           - layout->hScrollbarThickness,
                           layout->canvasAscent + layout->canvasDescent);
}

style::StyleImage *Layout::LayoutImgRenderer::getBackgroundImage ()
{
   return layout->bgImage;
}

style::BackgroundRepeat Layout::LayoutImgRenderer::getBackgroundRepeat ()
{
   return layout->bgRepeat;
}

style::BackgroundAttachment
   Layout::LayoutImgRenderer::getBackgroundAttachment ()
{
   return layout->bgAttachment;
}

style::Length Layout::LayoutImgRenderer::getBackgroundPositionX ()
{
   return layout->bgPositionX;
}

style::Length Layout::LayoutImgRenderer::getBackgroundPositionY ()
{
   return layout->bgPositionY;
}

void Layout::LayoutImgRenderer::draw (int x, int y, int width, int height)
{
   layout->queueDraw (x, y, width, height);
}

// ----------------------------------------------------------------------

void Layout::Receiver::canvasSizeChanged (int width, int ascent, int descent)
{
}

// ----------------------------------------------------------------------

bool Layout::Emitter::emitToReceiver (lout::signal::Receiver *receiver,
                                      int signalNo, int argc,
                                      lout::object::Object **argv)
{
   Receiver *layoutReceiver = (Receiver*)receiver;

   switch (signalNo) {
   case CANVAS_SIZE_CHANGED:
      layoutReceiver->canvasSizeChanged (((Integer*)argv[0])->getValue (),
                                         ((Integer*)argv[1])->getValue (),
                                         ((Integer*)argv[2])->getValue ());
      break;

   default:
      misc::assertNotReached ();
   }

   return false;
}

void Layout::Emitter::emitCanvasSizeChanged (int width,
                                             int ascent, int descent)
{
   Integer w (width), a (ascent), d (descent);
   Object *argv[3] = { &w, &a, &d };
   emitVoid (CANVAS_SIZE_CHANGED, 3, argv);
}

// ----------------------------------------------------------------------

bool Layout::LinkReceiver::enter (Widget *widget, int link, int img,
                                  int x, int y)
{
   return false;
}

bool Layout::LinkReceiver::press (Widget *widget, int link, int img,
                                  int x, int y, EventButton *event)
{
   return false;
}

bool Layout::LinkReceiver::release (Widget *widget, int link, int img,
                                    int x, int y, EventButton *event)
{
   return false;
}

bool Layout::LinkReceiver::click (Widget *widget, int link, int img,
                                    int x, int y, EventButton *event)
{
   return false;
}

// ----------------------------------------------------------------------

bool Layout::LinkEmitter::emitToReceiver (lout::signal::Receiver *receiver,
                                          int signalNo, int argc,
                                          lout::object::Object **argv)
{
   LinkReceiver *linkReceiver = (LinkReceiver*)receiver;

   switch (signalNo) {
   case ENTER:
      return linkReceiver->enter ((Widget*)argv[0],
                                  ((Integer*)argv[1])->getValue (),
                                  ((Integer*)argv[2])->getValue (),
                                  ((Integer*)argv[3])->getValue (),
                                  ((Integer*)argv[4])->getValue ());

   case PRESS:
      return linkReceiver->press ((Widget*)argv[0],
                                  ((Integer*)argv[1])->getValue (),
                                  ((Integer*)argv[2])->getValue (),
                                  ((Integer*)argv[3])->getValue (),
                                  ((Integer*)argv[4])->getValue (),
                                  (EventButton*)argv[5]);

   case RELEASE:
      return linkReceiver->release ((Widget*)argv[0],
                                    ((Integer*)argv[1])->getValue (),
                                    ((Integer*)argv[2])->getValue (),
                                    ((Integer*)argv[3])->getValue (),
                                    ((Integer*)argv[4])->getValue (),
                                    (EventButton*)argv[5]);

   case CLICK:
      return linkReceiver->click ((Widget*)argv[0],
                                  ((Integer*)argv[1])->getValue (),
                                  ((Integer*)argv[2])->getValue (),
                                  ((Integer*)argv[3])->getValue (),
                                  ((Integer*)argv[4])->getValue (),
                                  (EventButton*)argv[5]);

   default:
      misc::assertNotReached ();
   }
   return false;
}

bool Layout::LinkEmitter::emitEnter (Widget *widget, int link, int img,
                                     int x, int y)
{
   Integer ilink (link), iimg (img), ix (x), iy (y);
   Object *argv[5] = { widget, &ilink, &iimg, &ix, &iy };
   return emitBool (ENTER, 5, argv);
}

bool Layout::LinkEmitter::emitPress (Widget *widget, int link, int img,
                                     int x, int y, EventButton *event)
{
   Integer ilink (link), iimg (img), ix (x), iy (y);
   Object *argv[6] = { widget, &ilink, &iimg, &ix, &iy, event };
   return emitBool (PRESS, 6, argv);
}

bool Layout::LinkEmitter::emitRelease (Widget *widget, int link, int img,
                                       int x, int y, EventButton *event)
{
   Integer ilink (link), iimg (img), ix (x), iy (y);
   Object *argv[6] = { widget, &ilink, &iimg, &ix, &iy, event };
   return emitBool (RELEASE, 6, argv);
}

bool Layout::LinkEmitter::emitClick (Widget *widget, int link, int img,
                                     int x, int y, EventButton *event)
{
   Integer ilink (link), iimg (img), ix (x), iy (y);
   Object *argv[6] = { widget, &ilink, &iimg, &ix, &iy, event };
   return emitBool (CLICK, 6, argv);
}

// ---------------------------------------------------------------------

Layout::Anchor::~Anchor ()
{
   free(name);
}

// ---------------------------------------------------------------------

Layout::Layout (Platform *platform)
{
   this->platform = platform;
   view = NULL;
   topLevel = NULL;
   widgetAtPoint = NULL;

   DBG_OBJ_CREATE ("dw::core::Layout");

   bgColor = NULL;
   bgImage = NULL;
   cursor = style::CURSOR_DEFAULT;

   canvasWidth = canvasAscent = canvasDescent = 0;

   usesViewport = false;
   drawAfterScrollReq = false;
   scrollX = scrollY = 0;
   viewportWidth = viewportHeight = 0;
   hScrollbarThickness = vScrollbarThickness = 0;

   requestedAnchor = NULL;
   scrollIdleId = -1;
   scrollIdleNotInterrupted = false;

   anchorsTable =
      new container::typed::HashTable <object::String, Anchor> (true, true);

   resizeIdleId = -1;

   textZone = new misc::ZoneAllocator (16 * 1024);

   DBG_OBJ_ASSOC_CHILD (&findtextState);
   DBG_OBJ_ASSOC_CHILD (&selectionState);

   platform->setLayout (this);

   selectionState.setLayout(this);

   layoutImgRenderer = NULL;
}

Layout::~Layout ()
{
   widgetAtPoint = NULL;

   if (layoutImgRenderer) {
      if (bgImage)
         bgImage->removeExternalImgRenderer (layoutImgRenderer);
      delete layoutImgRenderer;
   }

   if (scrollIdleId != -1)
      platform->removeIdle (scrollIdleId);
   if (resizeIdleId != -1)
      platform->removeIdle (resizeIdleId);
   if (bgColor)
      bgColor->unref ();
   if (bgImage)
      bgImage->unref ();
   if (topLevel) {
      Widget *w = topLevel;
      topLevel = NULL;
      delete w;
   }
   delete platform;
   delete view;
   delete anchorsTable;
   delete textZone;

   DBG_OBJ_DELETE ();
}

void Layout::addWidget (Widget *widget)
{
   if (topLevel) {
      MSG_WARN("widget already set\n");
      return;
   }

   topLevel = widget;
   widget->layout = this;

   findtextState.setWidget (widget);

   canvasHeightGreater = false;
   setSizeHints ();
   queueResize ();
}

void Layout::removeWidget ()
{
   /**
    * \bug Some more attributes must be reset here.
    */
   topLevel = NULL;
   widgetAtPoint = NULL;
   canvasWidth = canvasAscent = canvasDescent = 0;
   scrollX = scrollY = 0;

   view->setCanvasSize (canvasWidth, canvasAscent, canvasDescent);
   if (view->usesViewport ())
      view->setViewportSize (viewportWidth, viewportHeight, 0, 0);
   view->queueDrawTotal ();

   setAnchor (NULL);
   updateAnchor ();

   emitter.emitCanvasSizeChanged (canvasWidth, canvasAscent, canvasDescent);

   findtextState.setWidget (NULL);
   selectionState.reset ();

   updateCursor ();
}

void Layout::setWidget (Widget *widget)
{
   DBG_OBJ_ASSOC_CHILD (widget);

   widgetAtPoint = NULL;
   if (topLevel) {
      Widget *w = topLevel;
      topLevel = NULL;
      delete w;
   }
   textZone->zoneFree ();
   addWidget (widget);

   updateCursor ();
}

/**
 * \brief Attach a view to the layout.
 *
 * It will become a child of the layout,
 * and so it will be destroyed, when the layout will be destroyed.
 */
void Layout::attachView (View *view)
{
   if (this->view)
      MSG_ERR("attachView: Multiple views for layout!\n");

   DBG_OBJ_ASSOC_CHILD (view);

   this->view = view;
   platform->attachView (view);

   /*
    * The layout of the view is set later, first, we "project" the current
    * state of the layout into the new view. A view must handle this without
    * a layout. See also at the end of this function.
    */
   if (bgColor)
      view->setBgColor (bgColor);
   view->setCursor (cursor);
   view->setCanvasSize (canvasWidth, canvasAscent, canvasDescent);

   if (view->usesViewport ()) {
      if (usesViewport) {
         view->scrollTo (scrollX, scrollY);
         view->setViewportSize (viewportWidth, viewportHeight,
                                hScrollbarThickness, vScrollbarThickness);
         hScrollbarThickness = misc::max (hScrollbarThickness,
                                          view->getHScrollbarThickness ());
         vScrollbarThickness = misc::max (vScrollbarThickness,
                                          view->getVScrollbarThickness ());
      }
      else {
         usesViewport = true;
         scrollX = scrollY = 0;
         viewportWidth = viewportHeight = 100; // random values
         hScrollbarThickness = view->getHScrollbarThickness ();
         vScrollbarThickness = view->getVScrollbarThickness ();
      }
   }

   /*
    * This is the last call within this function, so that it is safe for
    * the implementation of dw::core::View::setLayout, to call methods
    * of dw::core::Layout.
    */
   view->setLayout (this);
}

void Layout::detachView (View *view)
{
   if (this->view != view)
      MSG_ERR("detachView: this->view: %p view %p\n", this->view, view);

   view->setLayout (NULL);
   platform->detachView (view);
   this->view = NULL;
   /**
    * \todo Actually, viewportMarkerWidthDiff and
    *       viewportMarkerHeightDiff have to be recalculated here, since the
    *       effective (i.e. maximal) values may change, after the view has been
    *       detached. Same applies to the usage of viewports.
    */
}

void Layout::scroll(ScrollCommand cmd)
{
   if (view->usesViewport ())
      view->scroll(cmd);
}

/**
 * \brief Scrolls all viewports, so that the region [x, y, width, height]
 *    is seen, according to hpos and vpos.
 */
void Layout::scrollTo (HPosition hpos, VPosition vpos,
                       int x, int y, int width, int height)
{
   scrollTo0 (hpos, vpos, x, y, width, height, true);
}

void Layout::scrollTo0 (HPosition hpos, VPosition vpos,
                        int x, int y, int width, int height,
                        bool scrollingInterrupted)
{
   if (usesViewport) {
      _MSG("scrollTo (%d, %d, %s)\n",
           x, y, scrollingInterrupted ? "true" : "false");

      scrollTargetHpos = hpos;
      scrollTargetVpos = vpos;
      scrollTargetX = x;
      scrollTargetY = y;
      scrollTargetWidth = width;
      scrollTargetHeight = height;

      if (scrollIdleId == -1) {
         scrollIdleId = platform->addIdle (&Layout::scrollIdle);
         scrollIdleNotInterrupted = true;
      }

      scrollIdleNotInterrupted =
         scrollIdleNotInterrupted || !scrollingInterrupted;
   }
}

void Layout::scrollIdle ()
{
   bool xChanged = true;
   switch (scrollTargetHpos) {
   case HPOS_LEFT:
      scrollX = scrollTargetX;
      break;
   case HPOS_CENTER:
      scrollX =
         scrollTargetX
         - (viewportWidth - currVScrollbarThickness() - scrollTargetWidth) / 2;
      break;
   case HPOS_RIGHT:
      scrollX =
         scrollTargetX
         - (viewportWidth - currVScrollbarThickness() - scrollTargetWidth);
      break;
   case HPOS_INTO_VIEW:
      xChanged = calcScrollInto (scrollTargetX, scrollTargetWidth, &scrollX,
                                 viewportWidth - currVScrollbarThickness());
      break;
   case HPOS_NO_CHANGE:
      xChanged = false;
      break;
   }

   bool yChanged = true;
   switch (scrollTargetVpos) {
   case VPOS_TOP:
      scrollY = scrollTargetY;
      break;
   case VPOS_CENTER:
      scrollY =
         scrollTargetY
         - (viewportHeight - currHScrollbarThickness() - scrollTargetHeight)/2;
      break;
   case VPOS_BOTTOM:
      scrollY =
         scrollTargetY
         - (viewportHeight - currHScrollbarThickness() - scrollTargetHeight);
      break;
   case VPOS_INTO_VIEW:
      yChanged = calcScrollInto (scrollTargetY, scrollTargetHeight, &scrollY,
                                 viewportHeight - currHScrollbarThickness());
      break;
   case VPOS_NO_CHANGE:
      yChanged = false;
      break;
   }

   if (xChanged || yChanged) {
      adjustScrollPos ();
      view->scrollTo (scrollX, scrollY);
      if (drawAfterScrollReq) {
         drawAfterScrollReq = false;
         view->queueDrawTotal ();
      }
   }

   scrollIdleId = -1;
}

void Layout::adjustScrollPos ()
{
   scrollX = misc::min (scrollX,
      canvasWidth - (viewportWidth - vScrollbarThickness));
   scrollX = misc::max (scrollX, 0);

   scrollY = misc::min (scrollY,
      canvasAscent + canvasDescent - (viewportHeight - hScrollbarThickness));
   scrollY = misc::max (scrollY, 0);

   _MSG("adjustScrollPos: scrollX=%d scrollY=%d\n", scrollX, scrollY);
}

bool Layout::calcScrollInto (int requestedValue, int requestedSize,
                             int *value, int viewportSize)
{
   if (requestedSize > viewportSize) {
      // The viewport size is smaller than the size of the region which will
      // be shown. If the region is already visible, do not change the
      // position. Otherwise, show the left/upper border, this is most likely
      // what is needed.
      if (*value >= requestedValue &&
          *value + viewportSize < requestedValue + requestedSize)
         return false;
      else
         requestedSize = viewportSize;
   }

   if (requestedValue < *value) {
      *value = requestedValue;
      return true;
   } else if (requestedValue + requestedSize > *value + viewportSize) {
      *value = requestedValue - viewportSize + requestedSize;
      return true;
   } else
      return false;
}

void Layout::draw (View *view, Rectangle *area)
{
   Rectangle widgetArea, intersection, widgetDrawArea;

   // First of all, draw background image. (Unlike background *color*,
   // this is not a feature of the views.)
   if (bgImage != NULL && bgImage->getImgbufSrc() != NULL)
      style::drawBackgroundImage (view, bgImage, bgRepeat, bgAttachment,
                                  bgPositionX, bgPositionY,
                                  area->x, area->y, area->width,
                                  area->height, 0, 0,
                                  // Reference area: maximum of canvas size and
                                  // viewport size.
                                  misc::max (viewportWidth
                                             - (canvasHeightGreater ?
                                                vScrollbarThickness : 0),
                                             canvasWidth),
                                  misc::max (viewportHeight
                                             - hScrollbarThickness,
                                             canvasAscent + canvasDescent));

   if (scrollIdleId != -1) {
      /* scroll is pending, defer draw until after scrollIdle() */
      drawAfterScrollReq = true;

   } else if (topLevel) {
      /* Draw the top level widget. */
      widgetArea.x = topLevel->allocation.x;
      widgetArea.y = topLevel->allocation.y;
      widgetArea.width = topLevel->allocation.width;
      widgetArea.height = topLevel->getHeight ();

      if (area->intersectsWith (&widgetArea, &intersection)) {
         view->startDrawing (&intersection);

         /* Intersection in widget coordinates. */
         widgetDrawArea.x = intersection.x - topLevel->allocation.x;
         widgetDrawArea.y = intersection.y - topLevel->allocation.y;
         widgetDrawArea.width = intersection.width;
         widgetDrawArea.height = intersection.height;

         topLevel->draw (view, &widgetDrawArea);

         view->finishDrawing (&intersection);
      }
   }
}

int Layout::currHScrollbarThickness()
{
   return (canvasWidth > viewportWidth) ? hScrollbarThickness : 0;
}

int Layout::currVScrollbarThickness()
{
   return (canvasAscent + canvasDescent > viewportHeight) ?
          vScrollbarThickness : 0;
}

/**
 * Sets the anchor to scroll to.
 */
void Layout::setAnchor (const char *anchor)
{
   _MSG("setAnchor (%s)\n", anchor);

   if (requestedAnchor)
      free(requestedAnchor);
   requestedAnchor = anchor ? strdup (anchor) : NULL;
   updateAnchor ();
}

/**
 * Used, when the widget is not allocated yet.
 */
char *Layout::addAnchor (Widget *widget, const char* name)
{
   return addAnchor (widget, name, -1);
}

char *Layout::addAnchor (Widget *widget, const char* name, int y)
{
   String key (name);
   if (anchorsTable->contains (&key))
      return NULL;
   else {
      Anchor *anchor = new Anchor ();
      anchor->name = strdup (name);
      anchor->widget = widget;
      anchor->y = y;

      anchorsTable->put (new String (name), anchor);
      updateAnchor ();

      return anchor->name;
   }
}

void Layout::changeAnchor (Widget *widget, char* name, int y)
{
   String key (name);
   Anchor *anchor = anchorsTable->get (&key);
   assert (anchor);
   assert (anchor->widget == widget);
   anchor->y = y;
   updateAnchor ();
}

void Layout::removeAnchor (Widget *widget, char* name)
{
   String key (name);
   anchorsTable->remove (&key);
}

void Layout::updateAnchor ()
{
   Anchor *anchor;
   if (requestedAnchor) {
      String key (requestedAnchor);
      anchor = anchorsTable->get (&key);
   } else
      anchor = NULL;

   if (anchor == NULL) {
      /** \todo Copy comment from old docs. */
      if (scrollIdleId != -1 && !scrollIdleNotInterrupted) {
         platform->removeIdle (scrollIdleId);
         scrollIdleId = -1;
      }
   } else
      if (anchor->y != -1)
         scrollTo0 (HPOS_NO_CHANGE, VPOS_TOP, 0, anchor->y, 0, 0, false);
}

void Layout::setCursor (style::Cursor cursor)
{
   if (cursor != this->cursor) {
      this->cursor = cursor;
      view->setCursor (cursor);
   }
}

void Layout::updateCursor ()
{
   if (widgetAtPoint && widgetAtPoint->style)
      setCursor (widgetAtPoint->style->cursor);
   else
      setCursor (style::CURSOR_DEFAULT);
}

void Layout::setBgColor (style::Color *color)
{
   color->ref ();

   if (bgColor)
      bgColor->unref ();

   bgColor = color;

   if (view)
      view->setBgColor (bgColor);
}

void Layout::setBgImage (style::StyleImage *bgImage,
                         style::BackgroundRepeat bgRepeat,
                         style::BackgroundAttachment bgAttachment,
                         style::Length bgPositionX, style::Length bgPositionY)
{
   if (layoutImgRenderer && this->bgImage)
      this->bgImage->removeExternalImgRenderer (layoutImgRenderer);

   if (bgImage)
      bgImage->ref ();

   if (this->bgImage)
      this->bgImage->unref ();

   this->bgImage = bgImage;
   this->bgRepeat = bgRepeat;
   this->bgAttachment = bgAttachment;
   this->bgPositionX = bgPositionX;
   this->bgPositionY = bgPositionY;

   if (bgImage) {
      // Create instance of LayoutImgRenderer when needed. Until this
      // layout is deleted, "layoutImgRenderer" will be kept, since it
      // is not specific to the style, but only to this layout.
      if (layoutImgRenderer == NULL)
         layoutImgRenderer = new LayoutImgRenderer (this);
      bgImage->putExternalImgRenderer (layoutImgRenderer);
   }
}


void Layout::resizeIdle ()
{
   //static int calls = 0;
   //MSG(" Layout::resizeIdle calls = %d\n", ++calls);

   assert (resizeIdleId != -1);

   // Reset already here, since in this function, queueResize() may be
   // called again.
   resizeIdleId = -1;

   if (topLevel) {
      Requisition requisition;
      Allocation allocation;

      topLevel->sizeRequest (&requisition);

      allocation.x = allocation.y = 0;
      allocation.width = requisition.width;
      allocation.ascent = requisition.ascent;
      allocation.descent = requisition.descent;
      topLevel->sizeAllocate (&allocation);

      canvasWidth = requisition.width;
      canvasAscent = requisition.ascent;
      canvasDescent = requisition.descent;

      emitter.emitCanvasSizeChanged (canvasWidth, canvasAscent, canvasDescent);

      // Tell the view about the new world size.
      view->setCanvasSize (canvasWidth, canvasAscent, canvasDescent);
      //  view->queueDrawTotal (false);

      if (usesViewport) {
         int currHThickness = currHScrollbarThickness();
         int currVThickness = currVScrollbarThickness();

         if (!canvasHeightGreater &&
             canvasAscent + canvasDescent
             > viewportHeight - currHThickness) {
            canvasHeightGreater = true;
            setSizeHints ();
            /* May queue a new resize. */
            }

         // Set viewport sizes.
         view->setViewportSize (viewportWidth, viewportHeight,
                                currHThickness, currVThickness);
      }

      // views are redrawn via Widget::resizeDrawImpl ()

   }

   updateAnchor ();
}

void Layout::setSizeHints ()
{
   if (topLevel) {
      topLevel->setWidth (viewportWidth
                          - (canvasHeightGreater ? vScrollbarThickness : 0));
      topLevel->setAscent (viewportHeight - hScrollbarThickness);
      topLevel->setDescent (0);
   }
}

void Layout::queueDraw (int x, int y, int width, int height)
{
   Rectangle area;
   area.x = x;
   area.y = y;
   area.width = width;
   area.height = height;

   if (area.isEmpty ()) return;

   view->queueDraw (&area);
}

void Layout::queueDrawExcept (int x, int y, int width, int height,
   int ex, int ey, int ewidth, int eheight) {

   if (x == ex && y == ey && width == ewidth && height == eheight)
      return;

   // queueDraw() the four rectangles within rectangle (x, y, width, height)
   // around rectangle (ex, ey, ewidth, eheight).
   // Some or all of these may be empty.

   // upper left corner of the intersection rectangle
   int ix1 = misc::max (x, ex);
   int iy1 = misc::max (y, ey);
   // lower right corner of the intersection rectangle
   int ix2 = misc::min (x + width, ex + ewidth);
   int iy2 = misc::min (y + height, ey + eheight);

   queueDraw (x, y, width, iy1 - y);
   queueDraw (x, iy2, width, y + height - iy2);
   queueDraw (x, iy1, ix1 - x, iy2 - iy1);
   queueDraw (ix2, iy1, x + width - ix2, iy2 - iy1);
}

void Layout::queueResize ()
{
   if (resizeIdleId == -1) {
      view->cancelQueueDraw ();

      resizeIdleId = platform->addIdle (&Layout::resizeIdle);
   }
}


// Views

bool Layout::buttonEvent (ButtonEventType type, View *view, int numPressed,
                          int x, int y, ButtonState state, int button)

{
   EventButton event;

   moveToWidgetAtPoint (x, y, state);

   event.xCanvas = x;
   event.yCanvas = y;
   event.state = state;
   event.button = button;
   event.numPressed = numPressed;

   return processMouseEvent (&event, type);
}

/**
 * \brief This function is called by a view, to delegate a motion notify
 * event.
 *
 * Arguments are similar to dw::core::Layout::buttonPress.
 */
bool Layout::motionNotify (View *view,  int x, int y, ButtonState state)
{
   EventButton event;

   moveToWidgetAtPoint (x, y, state);

   event.xCanvas = x;
   event.yCanvas = y;
   event.state = state;

   return processMouseEvent (&event, MOTION_NOTIFY);
}

/**
 * \brief This function is called by a view, to delegate a enter notify event.
 *
 * Arguments are similar to dw::core::Layout::buttonPress.
 */
void Layout::enterNotify (View *view, int x, int y, ButtonState state)
{
   Widget *lastWidget;
   EventCrossing event;

   lastWidget = widgetAtPoint;
   moveToWidgetAtPoint (x, y, state);

   if (widgetAtPoint) {
      event.state = state;
      event.lastWidget = lastWidget;
      event.currentWidget = widgetAtPoint;
      widgetAtPoint->enterNotify (&event);
   }
}

/**
 * \brief This function is called by a view, to delegate a leave notify event.
 *
 * Arguments are similar to dw::core::Layout::buttonPress.
 */
void Layout::leaveNotify (View *view, ButtonState state)
{
#if 0
   Widget *lastWidget;
   EventCrossing event;

   lastWidget = widgetAtPoint;
   moveOutOfView (state);

   if (lastWidget) {
      event.state = state;
      event.lastWidget = lastWidget;
      event.currentWidget = widgetAtPoint;
      lastWidget->leaveNotify (&event);
   }
#else
   moveOutOfView (state);
#endif
}

/*
 * Return the widget at position (x, y). Return NULL, if there is no widget.
 */
Widget *Layout::getWidgetAtPoint (int x, int y)
{
   _MSG ("------------------------------------------------------------\n");
   _MSG ("widget at (%d, %d)\n", x, y);
   if (topLevel)
      return topLevel->getWidgetAtPoint (x, y, 0);
   else
      return NULL;
}


/*
 * Emit the necessary crossing events, when the mouse pointer has moved to
 * the given widget (by mouse or scrolling).
 */
void Layout::moveToWidget (Widget *newWidgetAtPoint, ButtonState state)
{
   Widget *ancestor, *w;
   Widget **track;
   int trackLen, i, i_a;
   EventCrossing crossingEvent;

   _MSG("moveToWidget: wap=%p nwap=%p\n",widgetAtPoint,newWidgetAtPoint);
   if (newWidgetAtPoint != widgetAtPoint) {
      // The mouse pointer has been moved into another widget.
      if (newWidgetAtPoint && widgetAtPoint)
         ancestor =
            newWidgetAtPoint->getNearestCommonAncestor (widgetAtPoint);
      else if (newWidgetAtPoint)
         ancestor = newWidgetAtPoint->getTopLevel ();
      else
         ancestor = widgetAtPoint->getTopLevel ();

      // Construct the track.
      trackLen = 0;
      if (widgetAtPoint)
         // first part
         for (w = widgetAtPoint; w != ancestor; w = w->getParent ())
            trackLen++;
      trackLen++; // for the ancestor
      if (newWidgetAtPoint)
         // second part
         for (w = newWidgetAtPoint; w != ancestor; w = w->getParent ())
            trackLen++;

      track = new Widget* [trackLen];
      i = 0;
      if (widgetAtPoint)
         /* first part */
         for (w = widgetAtPoint; w != ancestor; w = w->getParent ())
            track[i++] = w;
      i_a = i;
      track[i++] = ancestor;
      if (newWidgetAtPoint) {
         /* second part */
         i = trackLen - 1;
         for (w = newWidgetAtPoint; w != ancestor; w = w->getParent ())
            track[i--] = w;
      }
#if 0
      MSG("Track: %s[ ", widgetAtPoint ? "" : "nil ");
      for (i = 0; i < trackLen; i++)
         MSG("%s%p ", i == i_a ? ">" : "", track[i]);
      MSG("] %s\n", newWidgetAtPoint ? "" : "nil");
#endif

      /* Send events to the widgets on the track */
      for (i = 0; i < trackLen; i++) {
         crossingEvent.state = state;
         crossingEvent.currentWidget = widgetAtPoint; // ???
         crossingEvent.lastWidget = widgetAtPoint; // ???
         if (i < i_a) {
            track[i]->leaveNotify (&crossingEvent);
         } else if (i == i_a) { /* ancestor */
            /* Don't touch ancestor unless:
             *   - moving into/from NULL,
             *   - ancestor becomes the newWidgetAtPoint */
            if (i_a == trackLen-1 && !newWidgetAtPoint)
               track[i]->leaveNotify (&crossingEvent);
            else if ((i_a == 0 && !widgetAtPoint) ||
                     (i_a == trackLen-1 && newWidgetAtPoint))
               track[i]->enterNotify (&crossingEvent);
         } else {
            track[i]->enterNotify (&crossingEvent);
         }
      }

      delete[] track;

      widgetAtPoint = newWidgetAtPoint;
      updateCursor ();
   }
}

/**
 * \brief Common processing of press, release and motion events.
 *
 * This function depends on that move_to_widget_at_point()
 * has been called before.
 */
bool Layout::processMouseEvent (MousePositionEvent *event,
                                ButtonEventType type)
{
   Widget *widget;

   /*
    * If the event is outside of the visible region of the canvas, treat it
    * as occurring at the region's edge. Notably, this helps when selecting
    * text.
    */
   if (event->xCanvas < scrollX)
      event->xCanvas = scrollX;
   else {
      int maxX = scrollX + viewportWidth - currVScrollbarThickness() - 1;

      if (event->xCanvas > maxX)
         event->xCanvas = maxX;
   }
   if (event->yCanvas < scrollY)
      event->yCanvas = scrollY;
   else {
      int maxY = misc::min(scrollY + viewportHeight -currHScrollbarThickness(),
                           canvasAscent + canvasDescent) - 1;

      if (event->yCanvas > maxY)
         event->yCanvas = maxY;
   }

   widget = getWidgetAtPoint(event->xCanvas, event->yCanvas);

   for (; widget; widget = widget->getParent ()) {
      if (widget->isButtonSensitive ()) {
         event->xWidget = event->xCanvas - widget->getAllocation()->x;
         event->yWidget = event->yCanvas - widget->getAllocation()->y;

         switch (type) {
         case BUTTON_PRESS:
            return widget->buttonPress ((EventButton*)event);

         case BUTTON_RELEASE:
            return widget->buttonRelease ((EventButton*)event);

         case MOTION_NOTIFY:
            return widget->motionNotify ((EventMotion*)event);

         default:
            misc::assertNotReached ();
         }
      }
   }
   if (type == BUTTON_PRESS)
      return emitLinkPress (NULL, -1, -1, -1, -1, (EventButton*)event);
   else if (type == BUTTON_RELEASE)
      return emitLinkRelease(NULL, -1, -1, -1, -1, (EventButton*)event);

   return false;
}

/*
 * This function must be called by a view, when the user has manually changed
 * the viewport position. It is *not* called, when the layout has requested the
 * position change.
 */
void Layout::scrollPosChanged (View *view, int x, int y)
{
   if (x != scrollX || y != scrollY) {
      scrollX = x;
      scrollY = y;

      setAnchor (NULL);
      updateAnchor ();
   }
}

/*
 * This function must be called by a viewport view, when its viewport size has
 * changed. It is *not* called, when the layout has requested the size change.
 */
void Layout::viewportSizeChanged (View *view, int width, int height)
{
   _MSG("Layout::viewportSizeChanged w=%d h=%d new_w=%d new_h=%d\n",
        viewportWidth, viewportHeight, width, height);

   /* If the width has become higher, we test again, whether the vertical
    * scrollbar (so to speak) can be hidden again. */
   if (usesViewport && width > viewportWidth)
      canvasHeightGreater = false;

   /* if size changes, redraw this view.
    * TODO: this is a resize call (redraw/resize code needs a review). */
   if (viewportWidth != width || viewportHeight != height)
      queueResize();

   viewportWidth = width;
   viewportHeight = height;

   setSizeHints ();
}

} // namespace core
} // namespace dw
