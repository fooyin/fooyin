#!/bin/bash -eux

source ci/setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

$SUDO apt-get update -qq
$SUDO apt-get install -y \
        g++ git cmake pkg-config ninja-build libasound2-dev libtag1-dev libglu1-mesa-dev \
        qt6-base-dev libqt6widgets6 libqt6gui6 libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
        libavcodec-dev libavfilter-dev libavformat-dev libavutil-dev libswresample-dev
