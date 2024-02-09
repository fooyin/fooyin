#!/bin/bash

export FOOYIN_DIR=$PWD

mkdir -p /build
cd /build

cmake "$FOOYIN_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=1

cmake --build .
