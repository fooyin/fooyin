#!/bin/bash -eux

source ci/setup.sh

export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
brew update
brew install \
    llvm \
    cmake \
    ninja \
    qt \
    taglib \
    ffmpeg \
    icu4c@76 \
    sdl2
