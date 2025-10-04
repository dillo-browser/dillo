#!/bin/sh

set -e # Stop on first error
set -x # Print commands

./autogen.sh
# ----------------------------------------------------------------------
v=with-html-tests
mkdir build-$v install-$v
cd build-$v
../configure --prefix=$(readlink -f ../install-$v) --enable-html-tests
make
make install
DILLOBIN=$(readlink -f ../install-$v/bin/dillo) make check
# Check release fits in a floppy disk of 1.44 MB
make dist-gzip
size=$(stat -c %s dillo-*.tar.gz)
floppy=$((1474560 - 32*1024)) # Leave room for FAT table
echo "Floppy occupation: $(($size * 100 / $floppy)) %"
if [ $size -lt $floppy ]; then
  echo 'OK: Fits in floopy disk'
else
  echo "FAIL: Release size too big: $size / $floppy"
  exit 1
fi
cd ..
rm -rf build-$v install-$v
# ----------------------------------------------------------------------
v=distcheck
mkdir build-$v install-$v
cd build-$v
../configure --prefix=$(readlink -f ../install-$v) --enable-html-tests
make distcheck DISTCHECK_CONFIGURE_FLAGS=--enable-html-tests
cd ..
rm -rf build-$v install-$v
# ----------------------------------------------------------------------
v=asan
mkdir build-$v install-$v
cd build-$v
../configure CFLAGS="-fsanitize=address" CXXFLAGS="-fsanitize=address" \
  --prefix=$(readlink -f ../install-$v) --enable-html-tests
make
make install
DILLOBIN=$(readlink -f ../install-$v/bin/dillo) make check
cd ..
rm -rf build-$v install-$v
# ----------------------------------------------------------------------
exit 0
