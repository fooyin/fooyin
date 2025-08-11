#!/bin/sh

cd build/tests
GTEST_COLOR=1 ctest -V --output-on-failure
