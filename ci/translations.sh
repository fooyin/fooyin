#!/bin/bash -eux

source ci/setup.sh

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    gnupg \
    qttools5-dev-tools

lupdate src -no-obsolete -I include -ts data/translations/fooyin_*.ts
lupdate src -no-obsolete -I include -ts -pluralonly data/translations/fooyin_en_GB.ts
