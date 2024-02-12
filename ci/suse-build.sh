#!/bin/bash

export FOOYIN_DIR=$PWD
CMAKE_PATH=($PWD/cmake-*)
export PATH=$CMAKE_PATH/bin:$PATH

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=ON \
  -DFOOYIN_DEPLOY=ON

cmake --build build --target rpm