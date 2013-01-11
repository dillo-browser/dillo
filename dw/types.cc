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

using namespace lout;

namespace dw {
namespace core {

Rectangle::Rectangle (int x, int y, int width, int height)
{
   this->x = x;
   this->y = y;
   this->width = width;
   this->height = height;
}

/*
 * Draw rectangle in view relative to point (x,y).
 */
void Rectangle::draw (core::View *view, core::style::Style *style, int x,int y)
{
   const bool filled = false;

   view->drawRectangle(style->color, core::style::Color::SHADING_NORMAL,filled,
                       x + this->x, y + this->y, this->width, this->height);
}

/**
 * Return whether this rectangle and otherRect intersect. If yes,
 * return the intersection rectangle in dest.
 */
bool Rectangle::intersectsWith (Rectangle *otherRect, Rectangle *dest)
{
   bool doIntersect =
      this->x < otherRect->x + otherRect->width &&
      this->y < otherRect->y + otherRect->height &&
      otherRect->x < this->x + this->width &&
      otherRect->y < this->y + this->height;

   if (doIntersect) {
      dest->x = misc::max(this->x, otherRect->x);
      dest->y = misc::max(this->y, otherRect->y);
      dest->width  = misc::min(this->x + this->width,
                               otherRect->x + otherRect->width) - dest->x;
      dest->height = misc::min(this->y + this->height,
                               otherRect->y + otherRect->height) - dest->y;
   } else {
      dest->x = dest->y = dest->width = dest->height = 0;
   }

   return doIntersect;
}

/*
 * Return whether this is a subset of otherRect.
 */
bool Rectangle::isSubsetOf (Rectangle *otherRect)
{
   return
      x >= otherRect->x &&
      y >= otherRect->y &&
      x + width <= otherRect->x + otherRect->width &&
      y + height <= otherRect->y + otherRect->height;
}

bool Rectangle::isPointWithin (int x, int y)
{
   return
      x >= this->x && y >= this->y &&
      x < this->x + width && y < this->y + height;
}

// ----------------------------------------------------------------------

Circle::Circle (int x, int y, int radius)
{
   this->x = x;
   this->y = y;
   this->radius = radius;
}

/*
 * Draw circle in view relative to point (x,y).
 */
void Circle::draw (core::View *view, core::style::Style *style, int x, int y)
{
   const bool filled = false;

   view->drawArc(style->color, core::style::Color::SHADING_NORMAL, filled,
                 x + this->x, y + this->y, 2 * this->radius, 2 * this->radius,
                 0, 360);
}

bool Circle::isPointWithin (int x, int y)
{
   return
      (x - this->x) * (x - this->x) + (y - this->y) * (y - this->y)
      <= radius * radius;
}

// ----------------------------------------------------------------------

Polygon::Polygon ()
{
   points = new misc::SimpleVector<Point> (8);
   minx = miny = 0xffffff;
   maxx = maxy = -0xffffff;
}

Polygon::~Polygon ()
{
   delete points;
}

/*
 * Draw polygon in view relative to point (x,y).
 */
void Polygon::draw (core::View *view, core::style::Style *style, int x, int y)
{
   if (points->size()) {
      int i;
      const bool filled = false, convex = false;
      Point *pointArray = (Point *)malloc(points->size()*sizeof(struct Point));

      for (i = 0; i < points->size(); i++) {
         pointArray[i].x = x + points->getRef(i)->x;
         pointArray[i].y = y + points->getRef(i)->y;
      }
      view->drawPolygon(style->color, core::style::Color::SHADING_NORMAL,
                        filled, convex, pointArray, i);
      free(pointArray);
   }
}

void Polygon::addPoint (int x, int y)
{
   points->increase ();
   points->getRef(points->size () - 1)->x = x;
   points->getRef(points->size () - 1)->y = y;

   minx = misc::min(minx, x);
   miny = misc::min(miny, y);
   maxx = misc::max(maxx, x);
   maxy = misc::max(maxy, y);
}

/**
 * \brief Return, whether the line, limited by (ax1, ay1) and (ax2, ay2),
 *    crosses the unlimited line, determined by two points (bx1, by1) and
 *    (bx2, by2).
 */
bool Polygon::linesCross0(int ax1, int ay1, int ax2, int ay2,
                          int bx1, int by1, int bx2, int by2)
{
   /** TODO Some more description */
   // If the scalar product is 0, it means that one point is on the second
   // line, so we check for <= 0, not < 0.
   int z1 = zOfVectorProduct (ax1 - bx1, ay1 - by1, bx2 - bx1, by2 - by1);
   int z2 = zOfVectorProduct (ax2 - bx1, ay2 - by1, bx2 - bx1, by2 - by1);

   return (z1 <= 0 && z2 >= 0) || (z1 >= 0 && z2 <= 0);
}

/**
 * \brief Return, whether the line, limited by (ax1, ay1) and (ax2, ay2),
 *    crosses the line, limited by (bx1, by1) and (bx2, by2).
 */
bool Polygon::linesCross(int ax1, int ay1, int ax2, int ay2,
                         int bx1, int by1, int bx2, int by2)
{
   bool cross =
      linesCross0 (ax1, ay1, ax2, ay2, bx1, by1, bx2, by2) &&
      linesCross0 (bx1, by1, bx2, by2, ax1, ay1, ax2, ay2);
   _MSG("(%d, %d) - (%d, %d) and (%d, %d) - (%d, %d) cross? %s.\n",
        ax1, ay1, ax2, ay2, bx1, by1, bx2, by2, cross ? "Yes" : "No");
   return cross;
}

bool Polygon::isPointWithin (int x, int y)
{
   if (points->size () < 3 ||
       (x < minx || x > maxx || y < miny || y >= maxy))
      return false;
   else {
      int numCrosses = 0;
      for (int i = 0; i < points->size () - 1; i++) {
         if (linesCross (minx - 1, miny - 1, x, y,
                         points->getRef(i)->x, points->getRef(i)->y,
                         points->getRef(i + 1)->x, points->getRef(i + 1)->y))
            numCrosses++;
      }
      if (linesCross (minx - 1, miny - 1, x, y,
                      points->getRef(points->size () - 1)->x,
                      points->getRef(points->size () - 1)->y,
                      points->getRef(0)->x, points->getRef(0)->y))
         numCrosses++;

      return numCrosses % 2 == 1;
   }
}

Region::Region()
{
   rectangleList = new container::typed::List <Rectangle> (true);
}

Region::~Region()
{
   delete rectangleList;
}

/**
 * \brief Add a rectangle to the region and combine it with
 * existing rectangles if possible.
 * The number of rectangles is forced to be less than 16
 * by combining excessive rectangles.
 */
void Region::addRectangle (Rectangle *rPointer)
{
   container::typed::Iterator <Rectangle> it;
   Rectangle *r = new Rectangle (rPointer->x, rPointer->y,
      rPointer->width, rPointer->height);

   for (it = rectangleList->iterator (); it.hasNext (); ) {
      Rectangle *ownRect = it.getNext ();

      int combinedHeight =
         misc::max(r->y + r->height, ownRect->y + ownRect->height) -
         misc::min(r->y, ownRect->y);
      int combinedWidth =
         misc::max(r->x + r->width, ownRect->x + ownRect->width) -
         misc::min(r->x, ownRect->x);

      if (rectangleList->size() >= 16 ||
         combinedWidth * combinedHeight <=
         ownRect->width * ownRect->height + r->width * r->height) {

            r->x = misc::min(r->x, ownRect->x);
            r->y = misc::min(r->y, ownRect->y);
            r->width = combinedWidth;
            r->height = combinedHeight;

            rectangleList->removeRef (ownRect);
      }
   }

   rectangleList->append (r);
}

} // namespace core
} // namespace dw
