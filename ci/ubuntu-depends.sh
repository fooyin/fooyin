#!/bin/bash -eux

source ci/setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

$SUDO apt-get update -qq
$SUDO apt-get install -y \
        g++ \
        git \
        cmake \
        pkg-config \
        ninja-build \
        debhelper \
        lsb-release \
        libicu-dev \
        libglu1-mesa-dev \
        libxkbcommon-dev \
        dpkg-dev \
        dh-make \
        zlib1g-dev \
        libasound2-dev \
        libpipewire-0.3-dev \
        libsdl2-dev \
        libtag1-dev \
        qt6-base-dev \
        libqt6svg6-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        qt6-l10n-tools \
        libavcodec-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswresample-dev \
        libopenmpt-dev \
        libgme-dev \
        libarchive-dev \
        libsndfile1-dev \
        libebur128-dev
