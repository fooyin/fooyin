/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QThread>

#include <optional>
#include <ranges>

Q_LOGGING_CATEGORY(SETTINGS, "fy.settings")

namespace Fooyin {
namespace {
std::optional<QVariant> loadedSettingValue(const SettingsEntry& setting, const QVariant& diskValue)
{
    switch(setting.type()) {
        case Settings::Bool:
            return diskValue.toBool();
        case Settings::Float:
            return diskValue.toFloat();
        case Settings::Double:
            return diskValue.toDouble();
        case Settings::Int:
            return diskValue.toInt();
        case Settings::String:
            return diskValue.toString();
        case Settings::StringList:
            return diskValue.toStringList();
        case Settings::ByteArray:
            return diskValue.toByteArray();
        case Settings::Variant:
            if(diskValue.isValid()) {
                return diskValue;
            }
            break;
    }

    return {};
}
} // namespace

SettingsManager::SettingsManager(const QString& settingsPath, QObject* parent)
    : QObject{parent}
    , m_settingsPath{QFileInfo{settingsPath}.absoluteFilePath()}
    , m_notifying{false}
    , m_notificationScheduled{false}
    , m_settingsDialog{nullptr}
{ }

void SettingsManager::createSettingsDialog(QMainWindow* mainWindow)
{
    Q_ASSERT(QCoreApplication::instance() && QThread::currentThread() == qApp->thread());
    Q_ASSERT(QThread::currentThread() == thread());

    if(!m_settingsDialog) {
        m_settingsDialog = new SettingsDialogController(this, mainWindow);
    }
}

SettingsDialogController* SettingsManager::settingsDialog() const
{
    Q_ASSERT(QCoreApplication::instance() && QThread::currentThread() == qApp->thread());
    Q_ASSERT(QThread::currentThread() == thread());

    return m_settingsDialog;
}

bool SettingsManager::settingsHaveChanged() const
{
    const std::shared_lock lock{m_lock};

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
    std::unique_lock lock{m_lock};
    {
        QSettings settings{m_settingsPath, QSettings::IniFormat};
        settings.clear();
    }

    for(const auto& setting : m_settings | std::views::values) {
        if((*setting).reset()) {
            m_notifications.emplace_back(setting.get(), setting->value());
        }
    }

    lock.unlock();
    notifySubscribers();
}

QVariant SettingsManager::value(QAnyStringView key) const
{
    const std::shared_lock lock{m_lock};

    auto settingIt = settingEntry(key);
    if(settingIt == m_settings.cend()) {
        return false;
    }
    return settingIt->second->value();
}

bool SettingsManager::set(QAnyStringView key, const QVariant& value)
{
    std::unique_lock lock{m_lock};

    const auto it = settingEntry(key);
    if(it == m_settings.cend() || !it->second->setValue(value)) {
        return false;
    }

    SettingsEntry* setting = it->second.get();
    m_notifications.emplace_back(setting, setting->value());
    lock.unlock();
    notifySubscribers();
    return true;
}

bool SettingsManager::reset(QAnyStringView key)
{
    std::unique_lock lock{m_lock};

    const auto it = settingEntry(key);
    if(it == m_settings.cend() || !it->second->reset()) {
        return false;
    }

    SettingsEntry* setting = it->second.get();
    fileRemove(setting->key());
    m_notifications.emplace_back(setting, setting->value());
    lock.unlock();
    notifySubscribers();
    return true;
}

bool SettingsManager::contains(QAnyStringView key) const
{
    const std::shared_lock lock{m_lock};

    return std::ranges::any_of(m_settings, [key](const auto& setting) { return setting.second->key() == key; });
}

QVariant SettingsManager::fileValue(QAnyStringView key, const QVariant& defaultValue) const
{
    const QSettings settings{m_settingsPath, QSettings::IniFormat};
    return settings.value(key, defaultValue);
}

bool SettingsManager::fileSet(QAnyStringView key, const QVariant& value)
{
    QSettings settings{m_settingsPath, QSettings::IniFormat};
    if(settings.value(key) == value) {
        return false;
    }

    settings.setValue(key, value);

    return true;
}

bool SettingsManager::fileContains(QAnyStringView key) const
{
    const QSettings settings{m_settingsPath, QSettings::IniFormat};
    return settings.contains(key);
}

void SettingsManager::fileRemove(QAnyStringView key)
{
    QSettings settings{m_settingsPath, QSettings::IniFormat};
    settings.remove(key);
}

void SettingsManager::createSetting(const QString& key, const QVariant& value)
{
    const std::unique_lock lock{m_lock};

    if(settingExists(key)) {
        qCWarning(SETTINGS) << "Setting has already been registered:" << key;
        return;
    }

    addSetting(key, key, value, Settings::Variant, false);
}

void SettingsManager::createTempSetting(const QString& key, const QVariant& value)
{
    const std::unique_lock lock{m_lock};

    if(settingExists(key)) {
        qCWarning(SETTINGS) << "Setting has already been registered:" << key;
        return;
    }

    addSetting(key, key, value, Settings::Variant, true);
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
    if(keyString.isEmpty()) {
        return;
    }

    const QSettings settings{m_settingsPath, QSettings::IniFormat};
    if(!settings.contains(keyString)) {
        return;
    }

    if(const auto value = loadedSettingValue(*setting, settings.value(keyString))) {
        setting->setValueSilently(*value);
    }
}

void SettingsManager::saveSettings(bool onlyChanged)
{
    const std::shared_lock lock{m_lock};

    QSettings settings{m_settingsPath, QSettings::IniFormat};

    for(const auto& setting : m_settings | std::views::values) {
        if(setting && (!onlyChanged || setting->wasChanged()) && !setting->isTemporary()) {
            const auto keyString = setting->key();
            if(!keyString.isEmpty()) {
                settings.setValue(keyString, setting->value());
            }
        }
    }

    settings.sync();
}

void SettingsManager::addSetting(const QString& mapKey, const QString& key, const QVariant& value, Settings::Type type,
                                 bool isTemporary)
{
    auto setting = std::make_unique<SettingsEntry>(key, value, type);
    setting->moveToThread(thread());
    setting->setIsTemporary(isTemporary);
    if(!isTemporary) {
        checkLoadSetting(setting.get());
    }
    m_settings.emplace(mapKey, std::move(setting));
}

void SettingsManager::notifySubscribers()
{
    if(QThread::currentThread() == thread()) {
        dispatchNotifications();
        return;
    }

    {
        const std::unique_lock lock{m_lock};
        if(m_notifications.empty() || m_notifying || m_notificationScheduled) {
            return;
        }
        m_notificationScheduled = true;
    }

    QMetaObject::invokeMethod(this, &SettingsManager::dispatchNotifications, Qt::QueuedConnection);
}

void SettingsManager::dispatchNotifications()
{
    std::unique_lock lock{m_lock};

    m_notificationScheduled = false;
    if(m_notifying) {
        return;
    }
    m_notifying = true;

    while(!m_notifications.empty()) {
        Notification notification = std::move(m_notifications.front());
        m_notifications.pop_front();
        lock.unlock();
        notification.setting->notifySubscribers(notification.value);
        lock.lock();
    }

    m_notifying = false;
}
} // namespace Fooyin

#include "utils/settings/moc_settingsmanager.cpp"
