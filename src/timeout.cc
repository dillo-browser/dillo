/*
 * File: timeout.cc
 *
 * Copyright (C) 2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

// Simple ADT for timeout functions

#include <fltk/run.h>
#include "timeout.hh"

using namespace fltk;


// C++ functions with C linkage ----------------------------------------------

/*
 * Hook a one-time timeout function 'cb' after 't' seconds
 * with 'cbdata" as its data.
 */
void a_Timeout_add(float t, TimeoutCb_t cb, void *cbdata)
{
   add_timeout(t, cb, cbdata);
}

/*
 * To be called from iside the 'cb' function when it wants to keep running
 */
void a_Timeout_repeat(float t, TimeoutCb_t cb, void *cbdata)
{
   add_timeout(t, cb, cbdata);
}

/*
 * Stop running a timeout function
 */
void a_Timeout_remove()
{
   /* in FLTK, timeouts run one time by default */
}

