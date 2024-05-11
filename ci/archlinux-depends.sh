#!/bin/bash -eux

source ci/setup.sh

$SUDO pacman -Syyu --noconfirm
$SUDO pacman -S --noconfirm --needed \
       gcc cmake pkgconf ninja alsa-lib \
       qt6-base qt6-svg qt6-tools \
       kdsingleapplication taglib ffmpeg
