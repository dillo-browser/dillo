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

#ifndef __DW_FLTKVIEWPORT_HH__
#define __DW_FLTKVIEWPORT_HH__

#include <FL/Fl_Group.H>
#include <FL/Fl_Scrollbar.H>

#include "core.hh"
#include "fltkcore.hh"
#include "fltkviewbase.hh"

namespace dw {
namespace fltk {

class FltkViewport: public FltkWidgetView
{
public:
   enum GadgetOrientation { GADGET_VERTICAL, GADGET_HORIZONTAL };

private:
   enum { SCROLLBAR_THICKNESS = 15 };

   int scrollX, scrollY;
   int scrollDX, scrollDY;
   int scrollbarOnLeft;
   int hasDragScroll, dragScrolling, dragX, dragY;
   int horScrolling, verScrolling;
   bool scrollbarPageMode;
   int pageOverlap;
   enum dw::core::ScrollCommand pageScrolling;
   float pageScrollInterval;
   float pageScrollDelay;

   Fl_Scrollbar *vscrollbar, *hscrollbar;

   GadgetOrientation gadgetOrientation[4];
   lout::container::typed::List <lout::object::TypedPointer < Fl_Widget> >
      *gadgets;

   void adjustScrollbarsAndGadgetsAllocation ();
   void adjustScrollbarValues ();
   void hscrollbarChanged ();
   void vscrollbarChanged ();
   void positionChanged ();

   static void hscrollbarCallback (Fl_Widget *hscrollbar, void *viewportPtr);
   static void vscrollbarCallback (Fl_Widget *vscrollbar, void *viewportPtr);

   void selectionScroll();
   static void selectionScroll(void *vport);

   void repeatPageScroll ();
   static void repeatPageScroll (void *data);

   void updateCanvasWidgets (int oldScrollX, int oldScrollY);
   static void draw_area (void *data, int x, int y, int w, int h);

protected:
   int translateViewXToCanvasX (int x);
   int translateViewYToCanvasY (int y);
   int translateCanvasXToViewX (int x);
   int translateCanvasYToViewY (int y);

public:
   FltkViewport (int x, int y, int w, int h, const char *label = 0);
   ~FltkViewport ();

   void resize(int x, int y, int w, int h);
   void draw ();
   int handle (int event);

   void setCanvasSize (int width, int ascent, int descent);

   bool usesViewport ();
   int getHScrollbarThickness ();
   int getVScrollbarThickness ();
   int getScrollbarOnLeft () {return scrollbarOnLeft; };
   void scroll(int dx, int dy);
   void scroll(dw::core::ScrollCommand cmd);
   void scrollTo (int x, int y);
   void setViewportSize (int width, int height,
                         int hScrollbarThickness, int vScrollbarThickness);
   void setScrollStep(int step);

   void setGadgetOrientation (bool hscrollbarVisible, bool vscrollbarVisible,
                              GadgetOrientation gadgetOrientation);
   void setDragScroll (bool enable) { hasDragScroll = enable ? 1 : 0; }
   void addGadget (Fl_Widget *gadget);
   void setScrollbarOnLeft (bool enable);
   void setScrollbarPageMode(bool enable);
   void setPageOverlap(int overlap);
};

} // namespace fltk
} // namespace dw

#endif // __DW_FLTKVIEWPORT_HH__

