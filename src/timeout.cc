/*
 * File: timeout.cc
 *
 * Copyright (C) 2005-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Simple ADT for timeout functions

#include <FL/Fl.H>
#include "timeout.hh"

// C++ functions with C linkage ----------------------------------------------

/*
 * Hook a one-time timeout function 'cb' after 't' seconds
 * with 'cbdata" as its data.
 */
void a_Timeout_add(float t, TimeoutCb_t cb, void *cbdata)
{
   Fl::add_timeout(t, cb, cbdata);
}

/*
 * To be called from inside the 'cb' function when it wants to keep running
 */
void a_Timeout_repeat(float t, TimeoutCb_t cb, void *cbdata)
{
   Fl::add_timeout(t, cb, cbdata);
}

/*
 * Stop running a timeout function
 */
void a_Timeout_remove()
{
   /* in FLTK, timeouts run one time by default */
}

