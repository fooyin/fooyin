/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fileopsconfigdialog.h"

#include "fileopssettings.h"

#include <utils/settings/settingsmanager.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>

namespace Fooyin::FileOps {
FileOpsConfigDialog::FileOpsConfigDialog(SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_settings{settings}
    , m_confirmDelete{new QCheckBox(tr("Confirm before deleting tracks"), this)}
{
    setWindowTitle(tr("File Operations Settings"));
    setModal(true);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &FileOpsConfigDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &FileOpsConfigDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
                     &FileOpsConfigDialog::reset);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_confirmDelete, 0, 0);
    layout->addWidget(buttons, 1, 0);

    load();
}

void FileOpsConfigDialog::accept()
{
    m_settings->fileSet(Settings::ConfirmDelete, m_confirmDelete->isChecked());
    done(Accepted);
}

void FileOpsConfigDialog::load()
{
    m_confirmDelete->setChecked(m_settings->fileValue(Settings::ConfirmDelete, true).toBool());
}

void FileOpsConfigDialog::reset()
{
    m_settings->fileRemove(Settings::ConfirmDelete);
    load();
}
} // namespace Fooyin::FileOps
