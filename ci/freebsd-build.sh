#!/bin/sh

export FOOYIN_DIR=$PWD
export CC=clang
export CXX=clang++
export CXXFLAGS="-fexperimental-library"
BUILD_CCACHE="${BUILD_CCACHE:-ON}"
BUILD_PCH="${BUILD_PCH:-OFF}"

if [ "$BUILD_CCACHE" = "ON" ]; then
  export CCACHE_DIR="${CCACHE_DIR:-$FOOYIN_DIR/.ccache}"
  export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$FOOYIN_DIR}"
  export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
  export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-pch_defines,time_macros}"
  mkdir -p "$CCACHE_DIR"
fi

cmake -S "$FOOYIN_DIR" \
  -G Ninja \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBUILD_TESTING=ON \
  -DBUILD_CCACHE="$BUILD_CCACHE" \
  -DBUILD_PCH="$BUILD_PCH"

cmake --build build
if [ "$BUILD_CCACHE" = "ON" ]; then
  ccache --show-stats || true
fi
cd build
cpack -G FREEBSD
mkdir ../pkg
mv *.pkg ../pkg
