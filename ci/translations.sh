#!/bin/bash -eux

/usr/bin/lupdate6 src -no-obsolete -I include -ts data/translations/fooyin_*.ts
/usr/bin/lupdate6 src -no-obsolete -I include -ts -pluralonly data/translations/fooyin_en_GB.ts
