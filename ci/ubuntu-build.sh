#!/bin/bash

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON

cmake --build build
cd build
cpack -G DEB
mkdir ../deb
mv *.deb ../deb
