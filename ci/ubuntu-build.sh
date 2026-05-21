#!/bin/bash

BUILD_CCACHE="${BUILD_CCACHE:-ON}"
BUILD_PCH="${BUILD_PCH:-ON}"

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_CCACHE="$BUILD_CCACHE" \
  -DBUILD_PCH="$BUILD_PCH"

cmake --build build
if [ "$BUILD_CCACHE" = "ON" ]; then
  ccache --show-stats || true
fi
cd build
cpack -G DEB
mkdir ../deb
mv *.deb ../deb
