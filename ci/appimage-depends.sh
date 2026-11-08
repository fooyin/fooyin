#!/usr/bin/env bash

set -euo pipefail

pacman -Syu --needed --noconfirm \
    alsa-lib \
    bison \
    ccache \
    cmake \
    curl \
    dbus \
    desktop-file-utils \
    ffmpeg \
    flac \
    flex \
    glm \
    icu \
    kdsingleapplication \
    kvantum \
    libarchive \
    libcdio \
    libcdio-paranoia \
    libebur128 \
    libgme \
    libopenmpt \
    libpulse \
    libsndfile \
    libsoxr \
    libvorbis \
    libx11 \
    libxcb \
    libxkbcommon \
    libxkbcommon-x11 \
    lxqt-qtplugin \
    mpg123 \
    ninja \
    opusfile \
    pipewire-audio \
    pipewire-jack \
    qcoro \
    qt6-base \
    qt6-imageformats \
    qt6-svg \
    qt6-tools \
    qt6-wayland \
    qt6ct \
    sdl2-compat \
    soundtouch \
    taglib \
    zlib

get-debloated-pkgs --add-common --prefer-nano ffmpeg-mini libdecor-mini '!' icu
