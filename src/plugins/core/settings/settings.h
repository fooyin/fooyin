/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#pragma once

#include "settingsentry.h"

#include <QMetaEnum>
#include <QReadWriteLock>
#include <QSettings>

namespace Core {
class Settings : public QObject
{
    Q_OBJECT

public:
    enum Setting : uint32_t
    {
        Version            = 1,
        DatabaseVersion    = 2,
        FirstRun           = 3,
        LayoutEditing      = 4,
        Geometry           = 5,
        Layout             = 6,
        SplitterHandles    = 7,
        DiscHeaders        = 8,
        SplitDiscs         = 9,
        SimplePlaylist     = 10,
        PlaylistAltColours = 11,
        PlaylistHeader     = 12,
        PlaylistScrollBar  = 13,
        PlayMode           = 14,
        ElapsedTotal       = 15,
        FilterAltColours   = 16,
        FilterHeader       = 17,
        FilterScrollBar    = 18,
        InfoAltColours     = 19,
        InfoHeader         = 20,
        InfoScrollBar      = 21,
    };
    Q_ENUM(Setting);

    explicit Settings(QObject* parent = nullptr);
    ~Settings() override;
    Settings(const Settings& other)             = delete;
    Settings& operator=(const Settings& other)  = delete;
    Settings(const Settings&& other)            = delete;
    Settings& operator=(const Settings&& other) = delete;

    void loadSettings();
    void storeSettings();

    QString getKeyString(const SettingsEntry& setting);

    template <typename E, typename T>
    void constexpr subscribe(E key, T* obj, void (T::*func)())
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(m_settings.count(mapKey)) {
            QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChanged, obj, func);
        };
    }

    template <typename E, typename T>
    void constexpr unsubscribe(E key, T* obj)
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(m_settings.count(mapKey)) {
            QObject::disconnect(&m_settings.at(mapKey), nullptr, obj, nullptr);
        }
    }

    template <typename E>
    void constexpr createSetting(E key, const QVariant& value, const QString& group = {})
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(!m_settings.count(mapKey)) {
            m_settings.emplace(mapKey, SettingsEntry{keyString, value, true, group});
        }
    }

    template <typename E>
    void constexpr createTempSetting(E key, const QVariant& value, const QString& group = {})
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(!m_settings.count(mapKey)) {
            m_settings.emplace(mapKey, SettingsEntry{keyString, value, false, group});
        }
    }

    template <typename E>
    auto constexpr value(E key)
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(!m_settings.count(mapKey)) {
            return QVariant{};
        }

        m_lock.lockForRead();

        const auto value = m_settings.at(mapKey).value();

        m_lock.unlock();

        return value;
    }

    template <typename E, typename V>
    void constexpr set(E key, V value)
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        m_lock.lockForWrite();

        if(!m_settings.count(mapKey)) {
            m_lock.unlock();
            return;
        }
        m_settings.at(mapKey).setValue(value);

        m_lock.unlock();

        m_settings.at(mapKey).changedSetting();
    }

private:
    QSettings m_settingsFile;
    std::map<QString, SettingsEntry> m_settings;
    QReadWriteLock m_lock;
};
}; // namespace Core
