#!/bin/bash

export FOOYIN_DIR=$PWD

export CC="$(brew --prefix llvm)/bin/clang"
export CXX="$(brew --prefix llvm)/bin/clang++"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix icu4c@76)"

cmake --build build
cd build
cpack -G TXZ
mkdir ../txz
mv *.tar.xz ../txz
