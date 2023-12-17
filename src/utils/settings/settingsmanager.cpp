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

#include <utils/settings/settingsmanager.h>

#include <utils/settings/settingsdialogcontroller.h>

#include <QSettings>

constexpr auto SettingsDialogState = "Interface/SettingsDialogState";

namespace Fooyin {
SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsFile{new QSettings(settingsPath, QSettings::IniFormat, this)}
    , m_settingsDialog{new SettingsDialogController(this)}
{
    m_settingsDialog->loadState(m_settingsFile->value(SettingsDialogState).toByteArray());
}

SettingsDialogController* SettingsManager::settingsDialog() const
{
    return m_settingsDialog;
}

void SettingsManager::storeSettings()
{
    for(const auto& [key, setting] : m_settings) {
        if(setting->isTemporary()) {
            continue;
        }
        const auto keyString = setting->key();
        if(!keyString.isEmpty()) {
            m_settingsFile->setValue(keyString, setting->value());
        }
    }

    m_settingsFile->setValue(SettingsDialogState, m_settingsDialog->saveState());

    m_settingsFile->sync();
}

QVariant SettingsManager::value(const QString& key) const
{
    if(!m_settings.contains(key)) {
        return {};
    }

    return m_settings.at(key)->value();
}

bool SettingsManager::set(const QString& key, const QVariant& value)
{
    if(!m_settings.contains(key)) {
        return false;
    }

    return m_settings.at(key)->setValue(value);
}

bool SettingsManager::reset(const QString& key)
{
    if(!m_settings.contains(key)) {
        return false;
    }

    return set(key, m_settings.at(key)->defaultValue());
}

bool SettingsManager::contains(const QString& key) const
{
    return m_settings.contains(key);
}

QVariant SettingsManager::fileValue(const QString& key) const
{
    return m_settingsFile->value(key);
}

bool SettingsManager::fileSet(const QString& key, const QVariant& value)
{
    if(fileValue(key) == value) {
        return false;
    }

    m_settingsFile->setValue(key, value);
    return true;
}

bool SettingsManager::fileContains(const QString& key) const
{
    return m_settingsFile->contains(key);
}

void SettingsManager::fileRemove(const QString& key)
{
    m_settingsFile->remove(key);
}

void SettingsManager::createSetting(const QString& key, const QVariant& value)
{
    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    checkLoadSetting(setting);
}

void SettingsManager::createTempSetting(const QString& key, const QVariant& value)
{
    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    setting->setIsTemporary(true);
}

bool SettingsManager::settingExists(const QString& key) const
{
    if(m_settings.empty()) {
        return false;
    }

    return std::ranges::any_of(std::as_const(m_settings),
                               [&key](const auto& setting) { return setting.second->key() == key; });
}

void SettingsManager::checkLoadSetting(SettingsEntry* setting) const
{
    if(setting->isTemporary()) {
        return;
    }

    const auto keyString = setting->key();
    if(!keyString.isEmpty()) {
        const auto diskValue = m_settingsFile->value(keyString);
        if(!diskValue.isNull()) {
            setting->setValue(diskValue);
        }
    }
}
} // namespace Fooyin

#include "utils/settings/moc_settingsmanager.cpp"
