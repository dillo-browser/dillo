#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <fltk/Window.h>
#include <fltk/x11.h>
#include <fltk/run.h>
#include <fltk/events.h>
#include <fltk/x.h>

#include "xembed.hh"

#if USE_X11
typedef enum { 
  XEMBED_EMBEDDED_NOTIFY        = 0,
  XEMBED_WINDOW_ACTIVATE        = 1,
  XEMBED_WINDOW_DEACTIVATE      = 2,
  XEMBED_REQUEST_FOCUS          = 3,
  XEMBED_FOCUS_IN               = 4,
  XEMBED_FOCUS_OUT              = 5,
  XEMBED_FOCUS_NEXT             = 6,
  XEMBED_FOCUS_PREV             = 7,
  XEMBED_GRAB_KEY               = 8,
  XEMBED_UNGRAB_KEY             = 9,
  XEMBED_MODALITY_ON            = 10,
  XEMBED_MODALITY_OFF           = 11,
} XEmbedMessageType;

void
Xembed::setXembedInfo(unsigned long flags)
{
  unsigned long buffer[2];

  Atom xembed_info_atom = XInternAtom (fltk::xdisplay, "_XEMBED_INFO", false);

  buffer[0] = 1;
  buffer[1] = flags;

  XChangeProperty (fltk::xdisplay,
     xid,
     xembed_info_atom, xembed_info_atom, 32,
     PropModeReplace,
     (unsigned char *)buffer, 2);
}

void
Xembed::sendXembedEvent(uint32_t message) {
   XClientMessageEvent xclient;

   memset (&xclient, 0, sizeof (xclient));
   xclient.window = xid;
   xclient.type = ClientMessage;
   xclient.message_type = XInternAtom (fltk::xdisplay, "_XEMBED", false);
   xclient.format = 32;
   xclient.data.l[0] = fltk::event_time;
   xclient.data.l[1] = message;

   XSendEvent(fltk::xdisplay, xid, False, NoEventMask, (XEvent *)&xclient);
   XSync(fltk::xdisplay, False);
}

int
Xembed::handle(int e) {
   if (e == fltk::PUSH)
      sendXembedEvent(XEMBED_REQUEST_FOCUS);

   return Window::handle(e);
}

static int event_handler(int e, fltk::Window *w) {
   Atom xembed_atom = XInternAtom (fltk::xdisplay, "_XEMBED", false);

   if (fltk::xevent.type == ClientMessage) {
      if (fltk::xevent.xclient.message_type == xembed_atom) {
         long message = fltk::xevent.xclient.data.l[1];

         switch (message) {
            case XEMBED_WINDOW_ACTIVATE:
               // Force a ConfigureNotify message so fltk can get the new
               // coordinates after a move of the embedder window.
               w->resize(0, 0, w->w(), w->h());
               break;
            case XEMBED_WINDOW_DEACTIVATE:
               break;
            default:
               break;
         }
      }
   }

   return 0;
}

// TODO; Implement more XEMBED support;

void Xembed::create() {
   createInternal(xid);
   setXembedInfo(1);
   fltk::add_event_handler(event_handler);
}

void Xembed::createInternal(uint32_t parent) {
   fltk::Window *window = this;
   Colormap colormap = fltk::xcolormap;

   XSetWindowAttributes attr;
   attr.border_pixel = 0;
   attr.colormap = colormap;
   attr.bit_gravity = 0; // StaticGravity;
   int mask = CWBorderPixel|CWColormap|CWEventMask|CWBitGravity;

   int W = window->w();
   if (W <= 0) W = 1; // X don't like zero...
   int H = window->h();
   if (H <= 0) H = 1; // X don't like zero...
   int X = window->x();
   int Y = window->y();

   attr.event_mask =
      ExposureMask | StructureNotifyMask
      | KeyPressMask | KeyReleaseMask | KeymapStateMask | FocusChangeMask
      | ButtonPressMask | ButtonReleaseMask
      | EnterWindowMask | LeaveWindowMask
      | PointerMotionMask;

   fltk::CreatedWindow *x = fltk::CreatedWindow::set_xid(window,
      XCreateWindow(fltk::xdisplay,
         parent,
         X, Y, W, H,
         0, // borderwidth
         fltk::xvisual->depth,
         InputOutput,
         fltk::xvisual->visual,
         mask, &attr));
}

#else  // USE_X11

void
Xembed::setXembedInfo(unsigned long flags) {};

void
Xembed::sendXembedEvent(uint32_t message) {};

int
Xembed::handle(int e) {
   return Window::handle(e);
}

void
Xembed::create() {
   Window::create();
}

#endif
