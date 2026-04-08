#!/bin/bash

set -euo pipefail

export FOOYIN_DIR=$PWD
export CC=$(brew --prefix llvm)/bin/clang
export CXX=$(brew --prefix llvm)/bin/clang++
export CXXFLAGS="-fexperimental-library"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix icu4c@78)"

cmake --build build
cd build
cpack -G DragNDrop

mkdir ../dmg
mv ./*.dmg ../dmg
