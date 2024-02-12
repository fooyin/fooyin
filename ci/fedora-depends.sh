#!/bin/bash -eux

# For ffmpeg-devel
dnf -y install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
dnf -y install https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

dnf -y --allowerasing install \
     @development-tools \
     redhat-lsb-core \
     rpmdevtools \
     tar \
     desktop-file-utils \
     cmake ninja-build \
     glib2-devel \
     libxkbcommon-x11-devel \
     libxkbcommon-devel \
     alsa-lib-devel \
     qt6-qtbase-devel \
     qt6-qtsvg-devel \
     qt6-qttools-devel \
     ffmpeg-devel \
     taglib-devel \
     qcoro-qt6-devel \
     kdsingleapplication-qt6-devel \
     pipewire-devel