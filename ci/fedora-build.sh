#!/bin/sh

export FOOYIN_DIR=$PWD
BUILD_CCACHE="${BUILD_CCACHE:-ON}"
BUILD_PCH="${BUILD_PCH:-ON}"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DFETCH_PROJECTM=ON \
  -DBUILD_CCACHE="$BUILD_CCACHE" \
  -DBUILD_PCH="$BUILD_PCH"

cmake --build build
if [ "$BUILD_CCACHE" = "ON" ]; then
  ccache --show-stats || true
fi
cd build
cpack -G RPM
mkdir ../rpm
mv *.rpm ../rpm
