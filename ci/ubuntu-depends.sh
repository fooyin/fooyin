#!/bin/bash -eux

source ci/setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

$SUDO apt-get update -qq
$SUDO apt-get install -y \
        g++ git cmake pkg-config ninja-build debhelper lsb-release libglu1-mesa-dev libxkbcommon-dev dpkg-dev dh-make \
        libasound2-dev libtag1-dev \
        qt6-base-dev libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
        libavcodec-dev libavformat-dev libavutil-dev
