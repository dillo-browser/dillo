#!/bin/bash
#
# Copyright (C) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -eux

yes "All work and no play makes Jack a dull boy" | head -n 500 | $DILLOC load
$DILLOC wait

for i in {1..10}; do
  stdbuf -oL $DILLOC dump | timeout 3 $DILLOC load
done
