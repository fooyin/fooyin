#!/bin/sh

sudo pkg update
sudo pkg upgrade -y
sudo pkg install -y \
     cmake-core \
     pkgconf \
     ninja \
     libgme \
     libvgm \
     vulkan-headers \
     alsa-lib \
     qt6-tools \
     qcoro-qt6 \
     ffmpeg \
     taglib \
     kdsingleapplication \
     pipewire-spa-oss \
     sdl2 \
     libopenmpt \
     libarchive \
     ebur128 \
     googletest
