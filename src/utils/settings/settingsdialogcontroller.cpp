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
    auto* settingsDialog = new SettingsDialog{m_pages};
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    settingsDialog->openSettings();
}

void SettingsDialogController::openAtPage(const Id& page)
{
    auto* settingsDialog = new SettingsDialog{m_pages};
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    settingsDialog->openSettings();
    settingsDialog->openPage(page);
}

void SettingsDialogController::addPage(SettingsPage* page)
{
    m_pages.emplace_back(page);
}
} // namespace Fy::Utils
