/*
 * File: iowatch.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Simple ADT for watching file descriptor activity

#include <FL/Fl.H>
#include "iowatch.hh"

//
// Hook a Callback for a certain activities in a FD
//
void a_IOwatch_add_fd(int fd, int when, Fl_FD_Handler Callback,
                      void *usr_data = 0)
{
   if (fd >= 0)
      Fl::add_fd(fd, when, Callback, usr_data);
}

//
// Remove a Callback for a given FD (or just remove some events)
//
void a_IOwatch_remove_fd(int fd, int when)
{
   if (fd >= 0)
      Fl::remove_fd(fd, when);
}

