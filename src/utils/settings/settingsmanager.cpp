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

constexpr auto SettingsDialogState = "Interface/SettingsDialogState";

namespace Fooyin {
SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsFile{new QSettings(settingsPath, QSettings::IniFormat, this)}
    , m_settingsDialog{new SettingsDialogController(this)}
{
    m_settingsDialog->loadState(fileValue(SettingsDialogState).toByteArray());
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
    m_settingsFile->clear();
    for(const auto& setting : m_settings | std::views::values) {
        if(setting->reset()) {
            setting->notifySubscribers();
        }
    }
}

QVariant SettingsManager::value(const QString& key) const
{
    m_lock.lockForRead();

    if(!m_settings.contains(key)) {
        m_lock.unlock();
        return {};
    }

    QVariant settingValue;

    if(auto* setting = m_settings.at(key)) {
        settingValue = setting->value();
    }

    m_lock.unlock();

    return settingValue;
}

bool SettingsManager::set(const QString& key, const QVariant& value)
{
    m_lock.lockForWrite();

    if(!m_settings.contains(key)) {
        m_lock.unlock();
        return false;
    }

    auto* setting = m_settings.at(key);

    const bool success = setting && setting->setValue(value);

    m_lock.unlock();

    if(success) {
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::reset(const QString& key)
{
    m_lock.lockForWrite();

    if(!m_settings.contains(key)) {
        m_lock.unlock();
        return false;
    }

    auto* setting = m_settings.at(key);

    const bool success = setting && setting->reset();

    m_lock.unlock();

    if(success) {
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::contains(const QString& key) const
{
    m_lock.lockForRead();

    const bool hasSetting = m_settings.contains(key);

    m_lock.unlock();

    return hasSetting;
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
    m_lock.lockForWrite();

    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        m_lock.unlock();
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    checkLoadSetting(setting);

    m_lock.unlock();
}

void SettingsManager::createTempSetting(const QString& key, const QVariant& value)
{
    m_lock.lockForWrite();

    if(m_settings.contains(key)) {
        qWarning() << "Setting has already been registered: " << key;
        m_lock.unlock();
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    setting->setIsTemporary(true);

    m_lock.unlock();
}

bool SettingsManager::settingExists(const QString& key) const
{
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
            setting->setValueSilently(diskValue);
        }
    }
}

void SettingsManager::saveSettings(bool onlyChanged)
{
    m_lock.lockForRead();

    for(const auto& [key, setting] : m_settings) {
        if((!onlyChanged || setting->wasChanged()) && !setting->isTemporary()) {
            const auto keyString = setting->key();
            if(!keyString.isEmpty()) {
                m_settingsFile->setValue(keyString, setting->value());
            }
        }
    }

    const auto dialogState = m_settingsDialog->saveState();

    m_lock.unlock();

    fileSet(SettingsDialogState, dialogState);

    m_settingsFile->sync();
}
} // namespace Fooyin

#include "utils/settings/moc_settingsmanager.cpp"
