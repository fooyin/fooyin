#!/bin/bash -eux

dnf -y update
dnf -y upgrade
dnf -y install --skip-broken \
     @development-tools \
     lsb_release \
     rpmdevtools \
     tar \
     desktop-file-utils \
     cmake \
     ninja-build \
     glib2-devel \
     libicu-devel \
     libxkbcommon-x11-devel \
     libxkbcommon-devel \
     alsa-lib-devel \
     qt6-qtbase-devel \
     qt6-qtsvg-devel \
     qt6-qttools-devel \
     libavcodec-free-devel \
     libavfilter-free-devel \
     libavformat-free-devel \
     libavutil-free-devel \
     libswresample-free-devel \
     taglib-devel \
     kdsingleapplication-qt6-devel \
     pipewire-devel \
     SDL2-devel \
     libopenmpt-devel \
     game-music-emu-devel \
     libarchive-devel \
     libsndfile-devel \
     libebur128-devel
