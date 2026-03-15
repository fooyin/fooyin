#!/bin/bash

export FOOYIN_DIR=$PWD

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix icu4c@77)"

cmake --build build
cd build
cpack -G TXZ
mkdir ../txz
mv *.tar.xz ../txz
