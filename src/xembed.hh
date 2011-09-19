#ifndef __XEMBED_HH__
#define __XEMBED_HH__

#include <FL/Fl_Window.H>

#include "d_size.h"

class Xembed : public Fl_Window {
   private:
      uint32_t xid;
      void createInternal(uint32_t parent);
      void setXembedInfo(unsigned long flags);
      void sendXembedEvent(uint32_t message);

   public:
      Xembed(uint32_t xid, int _w, int _h) : Fl_Window(_w, _h) {
         this->xid = xid;
      };
      void show();
      int handle(int event);
};

#endif
