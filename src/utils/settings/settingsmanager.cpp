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

Q_LOGGING_CATEGORY(SETTINGS, "fy.settings")

namespace Fooyin {
SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsFile{new QSettings(settingsPath, QSettings::IniFormat, this)}
    , m_settingsDialog{nullptr}
{ }

void SettingsManager::createSettingsDialog(QMainWindow* mainWindow)
{
    m_settingsDialog = new SettingsDialogController(this, mainWindow);
    m_settingsDialog->restoreState();
}

SettingsDialogController* SettingsManager::settingsDialog() const
{
    return m_settingsDialog;
}

bool SettingsManager::settingsHaveChanged() const
{
    return std::ranges::any_of(m_settings, [](const auto& setting) {
        return setting.second && setting.second->wasChanged() && !setting.second->isTemporary();
    });
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

QVariant SettingsManager::value(QAnyStringView key) const
{
    const std::shared_lock lock(m_lock);

    auto settingIt = settingEntry(key);
    if(settingIt == m_settings.cend()) {
        return false;
    }
    return settingIt->second->value();
}

bool SettingsManager::set(QAnyStringView key, const QVariant& value)
{
    std::unique_lock lock(m_lock);

    auto settingIt = settingEntry(key);
    if(settingIt == m_settings.cend()) {
        return false;
    }
    const auto& [_, setting] = *settingIt;

    const bool success = setting && setting->setValue(value);

    lock.unlock();

    if(success) {
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::reset(QAnyStringView key)
{
    std::unique_lock lock(m_lock);

    auto settingIt = settingEntry(key);
    if(settingIt == m_settings.cend()) {
        return false;
    }
    const auto& [_, setting] = *settingIt;

    const bool success = setting->reset();

    lock.unlock();

    if(success) {
        fileRemove(setting->key());
        setting->notifySubscribers();
    }

    return success;
}

bool SettingsManager::contains(QAnyStringView key) const
{
    const std::shared_lock lock(m_lock);

    return std::ranges::any_of(m_settings, [key](const auto& setting) { return setting.second->key() == key; });
}

QVariant SettingsManager::fileValue(QAnyStringView key, const QVariant& defaultValue) const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    return m_settingsFile->value(key, defaultValue);
#else
    return m_settingsFile->value(key.toString(), defaultValue);
#endif
}

bool SettingsManager::fileSet(QAnyStringView key, const QVariant& value)
{
    if(fileValue(key, {}) == value) {
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    m_settingsFile->setValue(key, value);
#else
    m_settingsFile->setValue(key.toString(), value);
#endif

    return true;
}

bool SettingsManager::fileContains(QAnyStringView key) const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    return m_settingsFile->contains(key);
#else
    return m_settingsFile->contains(key.toString());
#endif
}

void SettingsManager::fileRemove(QAnyStringView key)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    m_settingsFile->remove(key);
#else
    m_settingsFile->remove(key.toString());
#endif
}

void SettingsManager::createSetting(const QString& key, const QVariant& value)
{
    const std::unique_lock lock(m_lock);

    if(settingExists(key)) {
        qCWarning(SETTINGS) << "Setting has already been registered:" << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    checkLoadSetting(setting);
}

void SettingsManager::createTempSetting(const QString& key, const QVariant& value)
{
    const std::unique_lock lock(m_lock);

    if(settingExists(key)) {
        qCWarning(SETTINGS) << "Setting has already been registered:" << key;
        return;
    }

    auto* setting = m_settings.emplace(key, new SettingsEntry(key, value, this)).first->second;
    setting->setIsTemporary(true);
}

SettingsManager::SettingsMap::const_iterator SettingsManager::settingEntry(QAnyStringView key) const
{
    return std::ranges::find_if(m_settings,
                                [&key](const auto& setting) { return setting.second && setting.second->key() == key; });
}

bool SettingsManager::settingExists(QAnyStringView key) const
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

    if(m_settingsDialog) {
        m_settingsDialog->saveState();
    }

    m_settingsFile->sync();
}
} // namespace Fooyin

#include "utils/settings/moc_settingsmanager.cpp"
