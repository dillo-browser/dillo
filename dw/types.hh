#ifndef __DW_TYPES_HH__
#define __DW_TYPES_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

namespace style {
   class Style;
}

enum HPosition
{
   HPOS_LEFT,
   HPOS_CENTER,
   HPOS_RIGHT,
   HPOS_INTO_VIEW, /* scroll only, until the content in question comes
                    * into view */
   HPOS_NO_CHANGE
};

enum VPosition
{
   VPOS_TOP,
   VPOS_CENTER,
   VPOS_BOTTOM,
   VPOS_INTO_VIEW, /* scroll only, until the content in question comes
                    * into view */
   VPOS_NO_CHANGE
};

enum ScrollCommand {SCREEN_UP_CMD, SCREEN_DOWN_CMD, SCREEN_LEFT_CMD,
                    SCREEN_RIGHT_CMD, LINE_UP_CMD, LINE_DOWN_CMD,
                    LEFT_CMD, RIGHT_CMD, TOP_CMD, BOTTOM_CMD};

/*
 * Different "layers" may be highlighted in a widget.
 */
enum HighlightLayer
{
   HIGHLIGHT_SELECTION,
   HIGHLIGHT_FINDTEXT,
   HIGHLIGHT_NUM_LAYERS
};

struct Point
{
  int x;
  int y;
};

/**
 * \brief Abstract interface for different shapes.
 */
class Shape: public lout::object::Object
{
public:
   virtual bool isPointWithin (int x, int y) = 0;
   virtual void draw (core::View *view, core::style::Style *style, int x,
                      int y) = 0;
};

/**
 * \brief dw::core::Shape implemtation for simple rectangles.
 */
class Rectangle: public Shape
{
public:
   int x;
   int y;
   int width;
   int height;

   inline Rectangle () { }
   Rectangle (int x, int y, int width, int height);

   void draw (core::View *view, core::style::Style *style, int x, int y);
   bool intersectsWith (Rectangle *otherRect, Rectangle *dest);
   bool isSubsetOf (Rectangle *otherRect);
   bool isPointWithin (int x, int y);
   bool isEmpty () { return width <= 0 || height <= 0; };
};

/**
 * \brief dw::core::Shape implemtation for simple circles.
 */
class Circle: public Shape
{
public:
   int x, y, radius;

   Circle (int x, int y, int radius);

   void draw (core::View *view, core::style::Style *style, int x, int y);
   bool isPointWithin (int x, int y);
};

/**
 * \brief dw::core::Shape implemtation for polygons.
 */
class Polygon: public Shape
{
private:
   lout::misc::SimpleVector<Point> *points;
   int minx, miny, maxx, maxy;

   /**
    * \brief Return the z-coordinate of the vector product of two
    *    vectors, whose z-coordinate is 0 (so that x and y of
    *    the vector product is 0, too).
    */
   inline int zOfVectorProduct(int x1, int y1, int x2, int y2) {
      return x1 * y2 - x2 * y1;
   }

   bool linesCross0(int ax1, int ay1, int ax2, int ay2,
                    int bx1, int by1, int bx2, int by2);
   bool linesCross(int ax1, int ay1, int ax2, int ay2,
                   int bx1, int by1, int bx2, int by2);

public:
   Polygon ();
   ~Polygon ();

   void draw (core::View *view, core::style::Style *style, int x, int y);
   void addPoint (int x, int y);
   bool isPointWithin (int x, int y);
};

/**
 * Implementation for a point set.
 * Currently represented as a set of rectangles not containing
 * each other.
 * It is guaranteed that the rectangles returned by rectangles ()
 * cover all rectangles that were added with addRectangle ().
 */
class Region
{
private:
   lout::container::typed::List <Rectangle> *rectangleList;

public:
   Region ();
   ~Region ();

   void clear () { rectangleList->clear (); };

   void addRectangle (Rectangle *r);

   lout::container::typed::Iterator <Rectangle> rectangles ()
   {
      return rectangleList->iterator ();
   };
};

/**
 * \brief Represents the allocation, i.e. actual position and size of a
 *    dw::core::Widget.
 */
struct Allocation
{
   int x;
   int y;
   int width;
   int ascent;
   int descent;
};

struct Requisition
{
   int width;
   int ascent;
   int descent;
};

struct Extremes
{
   int minWidth;
   int maxWidth;
};

struct Content
{
   enum Type {
      START             = 1 << 0,
      END               = 1 << 1,
      TEXT              = 1 << 2,
      WIDGET            = 1 << 3,
      BREAK             = 1 << 4,
      ALL               = 0xff,
      REAL_CONTENT      = 0xff ^ (START | END),
      SELECTION_CONTENT = TEXT | WIDGET | BREAK
   };

   /* Content is embedded in struct Word therefore we
    * try to be space efficient.
    */
   short type;
   bool space;
   union {
      const char *text;
      Widget *widget;
      int breakSpace;
   };
};

} // namespace core
} // namespace dw

#endif // __DW_TYPES_HH__
