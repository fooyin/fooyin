#!/bin/bash

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build
cd build
cpack -G DEB
mkdir ../deb
mv *.deb ../deb
