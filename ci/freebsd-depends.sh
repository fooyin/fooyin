#!/bin/sh

sudo pkg update
sudo pkg upgrade -y
sudo pkg install -y \
     cmake \
     pkgconf \
     ninja \
     glib \
     icu \
     dbus \
     libgme \
     libxcb \
     libvgm \
     vulkan-headers \
     vulkan-loader \
     alsa-lib \
     qt6-base \
     qt6-svg \
     qt6-tools \
     ffmpeg \
     taglib \
     kdsingleapplication \
     pipewire \
     pipewire-spa-oss \
     sdl2 \
     libopenmpt \
     libarchive \
     libsndfile \
     libebur128
