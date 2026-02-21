#!/bin/bash
#
# Copyright (C) 2026 Rodrigo Arias Mallo <rodarima@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -eux

i="$SRCDIR/test.html"
o="$WORKDIR/output.html"

$DILLOC open "file://$(readlink -f $i)"
$DILLOC wait
$DILLOC title > "$o"

echo "Testing dilloc load command" | diff -up - "$o"
