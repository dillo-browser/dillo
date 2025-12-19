#!/bin/bash
#
# Copyright (C) 2023-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

source "$TOP_SRCDIR/test/lib/xorg.sh"

function dillo_setup() {
  if [ "${dillo_setup_done:-}" ]; then
    return 0
  fi

  DILLOBINDIR="${DILLOBINDIR:-$TOP_BUILDDIR/src/}"
  DILLO="${DILLO:-$DILLOBINDIR/dillo}"
  DILLOC="${DILLOC:-$DILLOBINDIR/dilloc}"

  if [ ! -e "$DILLO" ]; then
    echo missing dillo binary, set DILLOBINDIR with the path to dillo bin directory
    exit 1
  fi

  if [ ! -e "$DILLOC" ]; then
    echo missing dilloc binary, set DILLOCBINDIR with the path to dilloc bin directory
    exit 1
  fi

  DILLO=$(readlink -f "$DILLO")
  DILLOC=$(readlink -f "$DILLOC")

  dillo_setup_done=1
}

function dilloc_start() {
  xorg_start_server
  dillo_setup

  # Cannot overwrite about:* pages
  $DILLO file:///dev/empty &

  export DILLO_PID=$!
  $DILLOC wait
}

function dilloc_stop() {
  $DILLOC quit
  xorg_stop_server
}
