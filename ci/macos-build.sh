#!/bin/bash

set -euo pipefail

export FOOYIN_DIR=$PWD
export SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
export CC=$(brew --prefix llvm)/bin/clang
export CXX=$(brew --prefix llvm)/bin/clang++
export CXXFLAGS="-fexperimental-library"
export PATH="$(brew --prefix bison)/bin:$(brew --prefix flex)/bin:$PATH"
export CMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix icu4c@78)"
export PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
BUILD_CCACHE="${BUILD_CCACHE:-ON}"
BUILD_PCH="${BUILD_PCH:-ON}"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCMAKE_OSX_SYSROOT="$SDKROOT" \
  -DICU_ROOT="$(brew --prefix icu4c@78)" \
  -DFETCH_PROJECTM=ON \
  -DBUILD_CCACHE="$BUILD_CCACHE" \
  -DBUILD_PCH="$BUILD_PCH"

cmake --build build
if [ "$BUILD_CCACHE" = "ON" ]; then
  ccache --show-stats || true
fi
cd build
cpack -G DragNDrop

mkdir ../dmg
mv ./*.dmg ../dmg
