#!/bin/bash

mkdir build
cd build

cmake .. \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=ON

cmake --build .
