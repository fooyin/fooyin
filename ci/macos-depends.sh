#!/bin/bash -eux

source ci/setup.sh

brew untap aws/tap || true

brew update
brew install \
    bison \
    flex \
    cmake \
    ninja \
    glm \
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
