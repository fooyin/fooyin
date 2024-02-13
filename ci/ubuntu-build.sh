#!/bin/bash

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=ON \
  -DBUILD_WERROR=ON

cmake --build build
cpack -G DEB
mkdir deb
mv build/*.deb deb
