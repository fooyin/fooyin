#!/bin/bash -eux

source ci/setup.sh

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    gnupg \
    qttools5-dev-tools

FOOYIN_DIR="$PWD"

lupdate "$FOOYIN_DIR/src" -no-obsolete -I "$FOOYIN_DIR/include" -ts "$FOOYIN_DIR/data/translations/fooyin_*.ts"
lupdate "$FOOYIN_DIR/src" -no-obsolete -I "$FOOYIN_DIR/include" -ts -pluralonly "$FOOYIN_DIR/data/translations/fooyin_en_GB.ts"
