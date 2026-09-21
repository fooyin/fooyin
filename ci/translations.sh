#!/bin/bash -eux

dnf install -y \
    git \
    gnupg2 \
    qt6-linguist

/usr/bin/lupdate-qt6 src -no-obsolete -I include -ts data/translations/fooyin_*.ts
/usr/bin/lupdate-qt6 src -no-obsolete -I include -ts -pluralonly data/translations/fooyin_en_GB.ts
