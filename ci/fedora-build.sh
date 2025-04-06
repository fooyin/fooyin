#!/bin/bash

export FOOYIN_DIR=$PWD

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build
cd build
cpack -G RPM
mkdir ../rpm
mv *.rpm ../rpm
