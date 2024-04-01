/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <ranges>

namespace Fooyin {
SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsFile{new QSettings(settingsPath, QSettings::IniFormat, this)}
    , m_settingsDialog{new SettingsDialogController(this)}
{
    m_settingsDialog->restoreState();
}

SettingsDialogController* SettingsManager::settingsDialog() const
{
    return m_settingsDialog;
}

void SettingsManager::storeSettings()
{
    saveSettings(true);
}

void SettingsManager::storeAllSettings()
{
    saveSettings(false);
}

void SettingsManager::resetAllSettings()
{
    std::unique_lock lock(m_lock);

    m_settingsFile->clear();

    std::vector<SettingsEntry*> resetSettings;
    for(SettingsEntry* setting : m_settings | std::views::values) {
        if(setting && setting->reset()) {
            resetSettings.emplace_back(setting);
        }
    }

    lock.unlock();

    for(SettingsEntry* setting : resetSettings) {
        setting->notifySubscribers();
    }
}

QVariant SettingsManager::value(const QString& key) const
{
    const std::shared_lock lock(m_lock);

    if(!m_settings.contains(key)) {
        return {};
    }

    if(auto* setting = m_settings.at(key)) {
        return setting->value();
    }

    return {};
}

bool SettingsManager::set(const QString& key, const QVariant& value)
{
    std::unique_lock lock(m_lock);

    if(!m_settings.contains(key)) {
        return false;
    }

    auto* setting = m_settings.at(key);

    const bool success = setting && setting->setValue(value);

    lock.unlock();

    if(success) {
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::reset(const QString& key)
{
    std::unique_lock lock(m_lock);

    if(!m_settings.contains(key)) {
        return false;
    }

    auto* setting = m_settings.at(key);

    const bool success = setting && setting->reset();

    lock.unlock();

    if(success) {
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::contains(const QString& key) const
{
    const std::shared_lock lock(m_lock);

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
    const std::unique_lock lock(m_lock);

    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    checkLoadSetting(setting);
}

void SettingsManager::createTempSetting(const QString& key, const QVariant& value)
{
    const std::unique_lock lock(m_lock);

    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    setting->setIsTemporary(true);
}

bool SettingsManager::settingExists(const QString& key) const
{
    return std::ranges::any_of(m_settings,
                               [&key](const auto& setting) { return setting.second && setting.second->key() == key; });
}

void SettingsManager::checkLoadSetting(SettingsEntry* setting) const
{
    if(!setting || setting->isTemporary()) {
        return;
    }

    const auto keyString = setting->key();
    if(!keyString.isEmpty()) {
        const auto diskValue = m_settingsFile->value(keyString);
        if(!diskValue.isNull()) {
            setting->setValueSilently(diskValue);
        }
    }
}

void SettingsManager::saveSettings(bool onlyChanged)
{
    const std::shared_lock lock(m_lock);

    for(const auto& [key, setting] : m_settings) {
        if(setting && (!onlyChanged || setting->wasChanged()) && !setting->isTemporary()) {
            const auto keyString = setting->key();
            if(!keyString.isEmpty()) {
                m_settingsFile->setValue(keyString, setting->value());
            }
        }
    }

    m_settingsDialog->saveState();

    m_settingsFile->sync();
}
} // namespace Fooyin

#include "utils/settings/moc_settingsmanager.cpp"
