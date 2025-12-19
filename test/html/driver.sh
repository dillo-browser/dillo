#!/bin/bash
#
# Copyright (C) 2023-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -eux

source "$TOP_SRCDIR/test/lib/dilloc.sh"
source "$TOP_SRCDIR/test/lib/workdir.sh"
source "$TOP_SRCDIR/test/lib/xorg.sh"

function driver_start() {
  workdir_setup "$1"
  dillo_setup
  xorg_start_server

  # Check magick
  magick_bin="convert"
  if command -v magick 2>&1 >/dev/null; then
    magick_bin="magick"
  fi

  # File used to filter leaks
  LEAKFILTER=${LEAKFILTER:-$TOP_SRCDIR/test/html/leakfilter.awk}

  # Clean asan options if set
  unset ASAN_OPTIONS
}

function driver_stop() {
  workdir_clean
  xorg_stop_server
}

function render_page() {
  htmlfile="$1"
  outpic="$2"
  outerr="$2.err"

  $DILLO -f "$htmlfile" 2> "$outerr" &
  export DILLO_PID=$!

  if ! $DILLOC wait 5; then
    echo "cannot render page under 5 seconds" >&2
    exit 1
  fi

  # Capture only Dillo window
  winid=$(xwininfo -all -root | awk '/Dillo:/ {print $1}')
  if [ -z "$winid" ]; then
    echo "cannot find Dillo window" >&2
    exit 1
  fi

  xwd -id "$winid" -silent | ${magick_bin} xwd:- png:${outpic}

  # Exit cleanly
  $DILLOC quit

  # Dillo may fail due to leaks, but we will check them manually
  wait "$DILLO_PID" || true
  awk -f "$LEAKFILTER" < "$outerr"
}

function test_file() {
  html_file="$1"

  if [ ! -e "$html_file" ]; then
    echo "missing test file: $html_file"
    exit 1
  fi

  ref_file="${html_file%.html}.ref.html"
  if [ ! -e "$ref_file" ]; then
    echo "missing reference file: $ref_file"
    exit 1
  fi

  render_page "$html_file" "$WORKDIR/html.png"
  render_page "$ref_file" "$WORKDIR/ref.png"

  # AE = Absolute Error count of the number of different pixels
  diffcount=$(compare -metric AE "$WORKDIR/html.png" "$WORKDIR/ref.png" "$WORKDIR/diff.png" 2>&1 | cut -d ' ' -f 1 || true)

  # The test passes only if both images are identical
  if [ "$diffcount" = "0" ]; then
    echo "OK"
    return 0
  else
    echo "FAIL"
    return 1
  fi
}

driver_start $1
trap driver_stop EXIT
test_file "$1"
exit $?
