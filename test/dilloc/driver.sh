#!/bin/bash
#
# Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -eux

source "$TOP_SRCDIR/test/lib/dilloc.sh"
source "$TOP_SRCDIR/test/lib/workdir.sh"

function driver_start() {
  workdir_setup "$1"
  dilloc_start
}

function driver_stop() {
  workdir_clean
  dilloc_stop
}

driver_start "$1"
trap driver_stop EXIT

source "$1"
