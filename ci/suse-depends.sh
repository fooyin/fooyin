#!/bin/bash -eux

source ci/setup.sh

$SUDO zypper -n remove busybox-which
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
   qt6-tools \
   ffmpeg-5-libavcodec-devel \
   ffmpeg-5-libavutil-devel \
   ffmpeg-5-libavfilter-devel \
   ffmpeg-5-libavformat-devel \
   ffmpeg-5-libswresample-devel \
   hicolor-icon-theme \
   desktop-file-utils \
   update-desktop-files