#!/usr/bin/env bash

set -euo pipefail

mkdir -p sdk-check/relocated sdk-check/plugin
tar -xJf build/sdk/*.tar.xz -C sdk-check/relocated --strip-components=1
cp tests/core/plugins/CMakeLists.txt \
  tests/core/plugins/testplugin.cpp tests/core/plugins/testplugin.json.in \
  sdk-check/plugin/

cmake -S sdk-check/plugin -B sdk-check/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/sdk-check/relocated"
cmake --build sdk-check/build
cmake --install sdk-check/build --prefix "$PWD/sdk-check/relocated"

ldd -r sdk-check/relocated/lib/fooyin/plugins/fyplugin_testbundleplugin.so > sdk-check/ldd.txt
if grep -Eq 'not found|undefined symbol:' sdk-check/ldd.txt; then
  cat sdk-check/ldd.txt
  exit 1
fi

grep 'libfooyin_' sdk-check/ldd.txt
for library in sdk-check/relocated/lib/fooyin/libfooyin_*.so; do
  ldd -r "$library" > sdk-check/ldd.txt
  if grep -Eq 'not found|undefined symbol:' sdk-check/ldd.txt; then
    cat sdk-check/ldd.txt
    exit 1
  fi
done
