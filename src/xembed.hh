#ifndef __XIDWINDOW_HH__
#define __XIDWINDOW_HH__

#include <fltk/Window.h>

class Xembed : public fltk::Window {
   public:
      Xembed(int _w, int _h) : fltk::Window(_w, _h) {};
      void embed(uint32_t xid);
};

#endif
