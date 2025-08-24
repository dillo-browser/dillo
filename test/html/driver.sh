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

DILLOBIN=${DILLOBIN:-$TOP_BUILDDIR/src/dillo}
LEAKFILTER=${LEAKFILTER:-$TOP_SRCDIR/test/html/leakfilter.awk}

# Clean asan options if set
unset ASAN_OPTIONS

if [ ! -e $DILLOBIN ]; then
  echo missing dillo binary, set DILLOBIN with the path to dillo
  exit 1
fi

magick_bin="convert"
if command -v magick 2>&1 >/dev/null; then
  magick_bin="magick"
fi

function render_page() {
  htmlfile="$1"
  outpic="$2"
  outerr="$2.err"

  "$DILLOBIN" -f "$htmlfile" 2> "$outerr" &
  dillopid=$!

  # TODO: We need a better system to determine when the page loaded
  # This will poll for the window every 10th of a second for up to 5 seconds
  found_window=false
  for i in {0..50}; do
    sleep 0.1

    # Capture only Dillo window
    winid=$(xwininfo -all -root | awk '/Dillo:/ {print $1}')
    if [ ! -z "$winid" ]; then
      found_window=true
      # Wait some after the window appears to ensure rendering is done
      sleep ${DILLO_TEST_WAIT_TIME:-1}
      break
    fi
  done

  if ! $found_window; then
    echo "cannot find Dillo window" >&2
    exit 1
  fi
  xwd -id "$winid" -silent | ${magick_bin} xwd:- png:${outpic}

  # Exit cleanly
  kill -USR2 "$dillopid"

  # Dillo may fail due to leaks, but we will check them manually
  wait "$dillopid" || true
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
