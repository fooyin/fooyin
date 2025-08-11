#!/bin/sh

export FOOYIN_DIR=$PWD

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON

cmake --build build
cd build
cpack -G FREEBSD
mkdir ../pkg
mv *.pkg ../pkg
