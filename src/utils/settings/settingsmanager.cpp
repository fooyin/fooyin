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

#include "settingsmanager.h"

#include <utils/settings/settingsdialogcontroller.h>

#include <QSettings>

namespace Fy::Utils {
SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsFile{settingsPath, QSettings::IniFormat, this}
    , m_settingsDialog{new SettingsDialogController(this)}
{ }

void SettingsManager::loadSettings()
{
    for(auto& [key, setting] : m_settings) {
        if(!setting.writeToDisk()) {
            continue;
        }
        const auto keyString = getKeyString(setting);
        if(!keyString.isEmpty()) {
            const auto diskValue = m_settingsFile.value(keyString);
            if(!diskValue.isNull() && diskValue != setting.value()) {
                setting.setValue(diskValue);
            }
        }
    }
}

void SettingsManager::storeSettings()
{
    for(const auto& [key, setting] : m_settings) {
        if(!setting.writeToDisk()) {
            continue;
        }
        const auto keyString = getKeyString(setting);
        if(!keyString.isEmpty()) {
            m_settingsFile.setValue(keyString, setting.value());
        }
    }

    m_settingsFile.sync();
}

SettingsDialogController* SettingsManager::settingsDialog() const
{
    return m_settingsDialog;
}

QString SettingsManager::getKeyString(const SettingsEntry& setting)
{
    return setting.group() + "/" + setting.name();
}
} // namespace Fy::Utils
