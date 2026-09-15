#!/bin/bash -eux

source ci/setup.sh

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    gnupg \
    qt6-l10n-tools

/usr/lib/qt6/bin/lupdate src -no-obsolete -I include -ts data/translations/fooyin_*.ts
/usr/lib/qt6/bin/lupdate src -no-obsolete -I include -ts -pluralonly data/translations/fooyin_en_GB.ts
