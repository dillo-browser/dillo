#!/bin/bash
#
# Copyright (C) 2023-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# Xorg common functions to run Dillo inside a virtual screen.
# If $DILLO_DISPLAY is set, use that display instead.

function xorg_start_server() {
  if [ -z "${DILLO_DISPLAY:-}" ]; then
    export xdir="$(mktemp -d)"
    # Use a FIFO to read the display number
    mkfifo "$xdir/fifo"
    exec 6<> "$xdir/fifo"
    Xvfb -screen 5 1024x768x24 -displayfd 6 &
    export xorgpid=$!
    read dispnum < "$xdir/fifo"
    export DISPLAY=":$dispnum"
  fi
}

function xorg_stop_server() {
  if [ -n "${xorgpid:-}" ]; then
    exec 6>&-
    rm "$xdir/fifo"
    rmdir "$xdir"
    kill $xorgpid || true
  fi
}
