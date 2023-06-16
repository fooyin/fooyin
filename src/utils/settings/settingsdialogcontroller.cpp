/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "settingsdialogcontroller.h"

#include "settingsdialog.h"

namespace Fy::Utils {
SettingsDialogController::SettingsDialogController(QObject* parent)
    : QObject{parent}
{ }

void SettingsDialogController::open()
{
    openAtPage({});
}

void SettingsDialogController::openAtPage(const Id& page)
{
    auto* settingsDialog = new SettingsDialog{m_pages};
    QObject::connect(settingsDialog, &QDialog::destroyed, this, [this, settingsDialog]() {
        m_geometry = settingsDialog->saveGeometry();
    });
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);

    if(m_geometry.isEmpty()) {
        settingsDialog->resize(750, 450);
    }
    else {
        settingsDialog->restoreGeometry(m_geometry);
    }
    settingsDialog->openSettings();

    if(page.isValid()) {
        settingsDialog->openPage(page);
    }
}

void SettingsDialogController::addPage(SettingsPage* page)
{
    m_pages.push_back(page);
}

QByteArray SettingsDialogController::geometry() const
{
    return m_geometry;
}

void SettingsDialogController::updateGeometry(const QByteArray& geometry)
{
    m_geometry = geometry;
}
} // namespace Fy::Utils
