#!/bin/bash

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PCH=ON \
  -DFOOYIN_DEPLOY=ON

cmake --build build --target deb
mkdir deb
mv ../*.deb deb
