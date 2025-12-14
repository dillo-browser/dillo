#!/bin/bash
#
# File: driver.sh
#
# Copyright (C) 2023-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -e
set -x

DILLOBINDIR=${DILLOBINDIR:-$TOP_BUILDDIR/src/}
LEAKFILTER=${LEAKFILTER:-$TOP_SRCDIR/test/html/leakfilter.awk}

DILLOBINDIR=$(readlink -f "$DILLOBINDIR")

# Clean asan options if set
unset ASAN_OPTIONS

if [ ! -e $DILLOBINDIR/dillo ]; then
  echo missing dillo binary, set DILLOBINDIR with the path to dillo bin directory
  exit 1
fi

if [ ! -e $DILLOBINDIR/dilloc ]; then
  echo missing dilloc binary, set DILLOCBINDIR with the path to dilloc bin directory
  exit 1
fi

# Make sure we use the correct dillo and dilloc
export PATH="$DILLOBINDIR:$PATH"

magick_bin="convert"
if command -v magick 2>&1 >/dev/null; then
  magick_bin="magick"
fi

function render_page() {
  htmlfile="$1"
  outpic="$2"
  outerr="$2.err"

  dillo -f "$htmlfile" 2> "$outerr" &
  export DILLO_PID=$!

  if ! dilloc wait 5; then
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
  dilloc quit

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

  test_name=$(basename "$html_file")
  wdir="${test_name}_wdir"

  # Clean any previous files
  rm -rf "$wdir"
  mkdir -p "$wdir"

  # Use a FIFO to read the display number
  mkfifo "$wdir/display.fifo"
  exec 6<> "$wdir/display.fifo"
  Xvfb -screen 5 1024x768x24 -displayfd 6 &
  xorgpid=$!

  # Always kill Xvfb on exit
  trap "kill $xorgpid" EXIT

  read dispnum < "$wdir/display.fifo"
  export DISPLAY=":$dispnum"

  render_page "$html_file" "$wdir/html.png"
  render_page "$ref_file" "$wdir/ref.png"

  # AE = Absolute Error count of the number of different pixels
  diffcount=$(compare -metric AE "$wdir/html.png" "$wdir/ref.png" "$wdir/diff.png" 2>&1 | cut -d ' ' -f 1 || true)

  # The test passes only if both images are identical
  if [ "$diffcount" = "0" ]; then
    echo "OK"
    ret=0
  else
    echo "FAIL"
    ret=1
  fi

  exec 6>&-
  rm "$wdir/display.fifo"

  if [ -z "$DILLO_TEST_LEAVE_FILES" ]; then
    rm -rf "$wdir"
  fi

  return $ret
}

test_file "$1"
exit $?
