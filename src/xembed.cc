#include <fltk/Window.h>
#include <fltk/x11.h>
#include <fltk/run.h>
#include <fltk/x.h>

#include "xembed.hh"

using namespace fltk;
// TODO; Implement proper XEMBED support;
// http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
void Xembed::embed (uint32_t xid) {
#if USE_X11
   fltk::Widget *r = resizable();
   // WORKAROUND: Avoid jumping windows with tiling window managers (e.g. dwm)
   resize(1, 1);
   resizable(NULL);
   fltk::Window::show();
   fltk::Widget::hide();
   resizable(r);
   fltk::flush();
   XReparentWindow (fltk::xdisplay, fltk::xid(this), xid, 0, 0);
#endif
}
