#!/bin/sh

set -x
set -e

DILLOBINDIR=${DILLOBINDIR:-$TOP_BUILDDIR/src/}
export PATH="$DILLOBINDIR:$PATH"

# Cannot overwrite about:* pages
dillo file:///dev/empty &
export DILLO_PID=$!

dilloc wait

d=$(mktemp -d)
cat >$d/input << EOF
<!doctype html>
<html>
  <head>
    <title>Testing dilloc load command</title>
  </head>
  <body>
    <p>This page has been written via the <code>dilloc load</code> command</p>
  </body>
</html>
EOF

dilloc load < $d/input
dilloc dump > $d/output
dilloc quit

rc=0
diff -up $d/input $d/output || rc=$?

rm $d/input $d/output
rmdir $d

exit $rc
