#!/bin/bash

set -euo pipefail

export FOOYIN_DIR=$PWD
export CC=$(brew --prefix llvm)/bin/clang
export CXX=$(brew --prefix llvm)/bin/clang++
export CXXFLAGS="-fexperimental-library"
export CMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix icu4c@78)"
export PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DICU_ROOT="$(brew --prefix icu4c@78)"

cmake --build build
cd build
cpack -G DragNDrop

mkdir ../dmg
mv ./*.dmg ../dmg
