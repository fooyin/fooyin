#!/bin/sh

pacman -S --noconfirm --needed \
    $MINGW_PACKAGE_PREFIX-clang \
    $MINGW_PACKAGE_PREFIX-cmake \
    $MINGW_PACKAGE_PREFIX-ninja \
    $MINGW_PACKAGE_PREFIX-qt6-base \
    $MINGW_PACKAGE_PREFIX-qt6-svg \
    $MINGW_PACKAGE_PREFIX-qt6-tools \
    $MINGW_PACKAGE_PREFIX-taglib \
    $MINGW_PACKAGE_PREFIX-ffmpeg \
    $MINGW_PACKAGE_PREFIX-libopenmpt \
    $MINGW_PACKAGE_PREFIX-libebur128 \
    $MINGW_PACKAGE_PREFIX-libsndfile \
    $MINGW_PACKAGE_PREFIX-libarchive
