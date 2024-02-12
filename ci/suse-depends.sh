#!/bin/bash -eux

source ci/setup.sh

$SUDO zypper -n install \
   lsb-release \
   rpm-build \
   cmake \
   ninja \
   gcc \
   tar \
   alsa-devel \
   taglib-devel \
   qt6-base-devel  \
   qt6-gui-devel \
   qt6-widgets-devel \
   qt6-concurrent-devel \
   qt6-network-devel \
   qt6-sql-devel \
   qt6-linguist-devel \
   ffmpeg-5-libavcodec-devel \
   ffmpeg-5-libavutil-devel \
   ffmpeg-5-libavfilter-devel \
   ffmpeg-5-libavformat-devel \
   ffmpeg-5-libswresample-devel \
   hicolor-icon-theme \
   desktop-file-utils \
   update-desktop-files