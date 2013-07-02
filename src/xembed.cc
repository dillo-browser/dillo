/*
 * File: xembed.cc
 *
 * Copyright (C) 2009 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <ctype.h>

#define FL_INTERNALS
#include <FL/Fl_Window.H>
#include <FL/Fl.H>
#include <FL/x.H>

#include "xembed.hh"

#ifdef X_PROTOCOL

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

  Atom xembed_info_atom = XInternAtom (fl_display, "_XEMBED_INFO", false);

  buffer[0] = 1;
  buffer[1] = flags;

  XChangeProperty (fl_display,
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
   xclient.message_type = XInternAtom (fl_display, "_XEMBED", false);
   xclient.format = 32;
   xclient.data.l[0] = fl_event_time;
   xclient.data.l[1] = message;

   XSendEvent(fl_display, xid, False, NoEventMask, (XEvent *)&xclient);
   XSync(fl_display, False);
}

int
Xembed::handle(int e) {
   if (e == FL_PUSH)
      sendXembedEvent(XEMBED_REQUEST_FOCUS);

   return Fl_Window::handle(e);
}

static int event_handler(int e, Fl_Window *w) {
   Atom xembed_atom = XInternAtom (fl_display, "_XEMBED", false);

   if (fl_xevent->type == ClientMessage) {
      if (fl_xevent->xclient.message_type == xembed_atom) {
         long message = fl_xevent->xclient.data.l[1];

         switch (message) {
            case XEMBED_WINDOW_ACTIVATE:
               // Force a ConfigureNotify message so fltk can get the new
               // coordinates after a move of the embedder window.
               if (w)
                  w->resize(0,0, w->w(), w->h());
               break;
            case XEMBED_WINDOW_DEACTIVATE:
               break;
            default:
               break;
         }
      }
   }

   return Fl::handle_(e, w);
}

// TODO: Implement more XEMBED support;

void Xembed::show() {
   createInternal(xid);
   setXembedInfo(1);
   Fl::event_dispatch(event_handler);
   Fl_Window::show();
}

void Xembed::createInternal(uint32_t parent) {
   Fl_Window *window = this;
   Colormap colormap = fl_colormap;

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

   Fl_X::set_xid(window,
      XCreateWindow(fl_display,
         parent,
         X, Y, W, H,
         0, // borderwidth
         fl_visual->depth,
         InputOutput,
         fl_visual->visual,
         mask, &attr));
}

#else  // X_PROTOCOL

void
Xembed::setXembedInfo(unsigned long flags) {};

void
Xembed::sendXembedEvent(uint32_t message) {};

int
Xembed::handle(int e) {
   return Fl_Window::handle(e);
}

void
Xembed::show() {
   Fl_Window::show();
}

#endif
