#!/bin/bash

export FOOYIN_DIR=$PWD

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=ON \
  -DFOOYIN_DEPLOY=ON

cmake --build build --target rpm