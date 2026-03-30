/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fileopsdeletedialog.h"

#include "fileopssettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::FileOps {

FileOpsDeleteDialog::FileOpsDeleteDialog(const TrackList& tracks, SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
{
    setWindowTitle(tr("Delete Files"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    const QString message = tracks.size() == 1
                              ? tr("Are you sure you want to delete \"%1\"?").arg(tracks.front().effectiveTitle())
                              : tr("Are you sure you want to delete %1 tracks?").arg(tracks.size());

    auto* label = new QLabel(message, this);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto* dontAsk = new QCheckBox(tr("Do not ask again"), this);
    layout->addWidget(dontAsk);

    auto* buttons      = new QDialogButtonBox(this);
    auto* deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
    auto* cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    cancelButton->setDefault(true);
    layout->addWidget(buttons);

    QObject::connect(deleteButton, &QPushButton::clicked, this, [this, dontAsk, settings]() {
        if(dontAsk->isChecked()) {
            settings->fileSet(Settings::ConfirmDelete, false);
        }
        accept();
    });
    QObject::connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

} // namespace Fooyin::FileOps
