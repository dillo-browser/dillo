#ifndef __XEMBED_HH__
#define __XEMBED_HH__

#include <fltk/Window.h>

#include "d_size.h"

class Xembed : public fltk::Window {
   public:
      Xembed(int _w, int _h) : fltk::Window(_w, _h) {};
      void embed(uint32_t xid);
};

#endif
