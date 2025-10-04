#!/bin/sh -eux

source ci/setup.sh

$SUDO pacman -Syyu --noconfirm
$SUDO pacman -S --noconfirm --needed \
       gcc \
       cmake \
       pkgconf \
       ninja \
       alsa-lib \
       icu \
       qt6-base \
       qt6-svg \
       qt6-tools \
       kdsingleapplication \
       qcoro \
       taglib \
       ffmpeg \
       pipewire \
       sdl2 \
       libopenmpt \
       libgme \
       libarchive \
       libsndfile \
       libebur128 \
       gtest
