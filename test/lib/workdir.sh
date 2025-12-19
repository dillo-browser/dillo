#!/bin/bash
#
# Copyright (C) 2023-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# Prepare a working directory
function workdir_setup() {
  export WORKDIR="$(basename "$1").workdir"

  # Clean any previous workdir
  rm -rf "$WORKDIR"
  mkdir -p "$WORKDIR"
}

function workdir_clean() {
  if [ -z "${DILLO_KEEP_WORKDIR:-}" ]; then
    rm -rf "$WORKDIR"
  fi
}
