#!/bin/bash

BUILD_CCACHE="${BUILD_CCACHE:-ON}"
BUILD_PCH="${BUILD_PCH:-ON}"
INSTALL_HEADERS="${INSTALL_HEADERS:-OFF}"

cmake -S . \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DFETCH_PROJECTM=ON \
  -DENABLE_SYSTEM_GLM=ON \
  -DBUILD_CCACHE="$BUILD_CCACHE" \
  -DBUILD_PCH="$BUILD_PCH" \
  -DINSTALL_HEADERS="$INSTALL_HEADERS"

cmake --build build

if [ "$BUILD_CCACHE" = "ON" ]; then
  ccache --show-stats || true
fi

cd build

cpack_args=(-G DEB)
if [ "$INSTALL_HEADERS" = "ON" ]; then
  cpack_args+=(-D "CPACK_INSTALL_CMAKE_PROJECTS=$PWD;fooyin;fooyin;/")
fi
cpack "${cpack_args[@]}"

mkdir ../deb
mv *.deb ../deb
