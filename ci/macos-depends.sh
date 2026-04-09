#!/bin/bash -eux

source ci/setup.sh

export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
brew update
brew install \
    cmake \
    ninja \
    qt \
    taglib \
    ffmpeg \
    icu4c@78 \
    sdl2 \
    libopenmpt \
    game-music-emu \
    libarchive \
    libsndfile \
    libebur128 \
    sound-touch \
    libsoxr \
    llvm
