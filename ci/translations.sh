#!/bin/bash -eux

pacman -Syu --noconfirm \
    git \
    gnupg \
    qt6-declarative \
    qt6-tools

/usr/bin/lupdate6 src -no-obsolete -I include -ts data/translations/fooyin_*.ts
/usr/bin/lupdate6 src -no-obsolete -I include -ts -pluralonly data/translations/fooyin_en_GB.ts
