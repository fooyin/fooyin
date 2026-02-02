#!/bin/sh

export FOOYIN_DIR=$PWD
export CC=clang
export CXX=clang++
export CXXFLAGS="-fexperimental-library"

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBUILD_TESTING=ON

cmake --build build
cd build
cpack -G FREEBSD
mkdir ../pkg
mv *.pkg ../pkg
