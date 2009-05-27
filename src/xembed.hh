#ifndef __XEMBED_HH__
#define __XEMBED_HH__

#include <fltk/Window.h>

#include "d_size.h"

class Xembed : public fltk::Window {
      uint32_t xid;
      void create_internal(uint32_t parent);
      void setXembedInfo(unsigned long flags);
      void sendXembedEvent(uint32_t message);

   public:
      Xembed(uint32_t xid, int _w, int _h) : fltk::Window(_w, _h) {
           this->xid = xid;
      };
      void create();
      int handle(int event);
};

#endif
