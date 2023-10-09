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

#pragma once

#include "fyutils_export.h"

#include <utils/settings/settingsentry.h>
#include <utils/settings/settingtypes.h>

#include <QMetaEnum>
#include <QReadWriteLock>
#include <QSettings>

namespace Fy::Utils {
class SettingsDialogController;

template <auto key>
concept IsEnumType = std::is_enum_v<decltype(key)> && std::is_same_v<std::underlying_type_t<decltype(key)>, uint32_t>;

template <auto key>
    requires IsEnumType<key>
constexpr int findType()
{
    // Use last 4 bits to determine type
    const auto type = (key & 0xF0'00'00'00);
    return static_cast<int>(type);
}

template <auto key, typename Value>
concept IsVariant = findType<key>() == Settings::Type::Variant;

template <auto key, typename Value>
concept IsBool = findType<key>() == Settings::Type::Bool && std::same_as<Value, bool>;

template <auto key, typename Value>
concept IsDouble = findType<key>() == Settings::Type::Double && std::same_as<Value, double>;

template <auto key, typename Value>
concept IsInt = findType<key>() == Settings::Type::Int && std::same_as<Value, int>;

template <auto key, typename Value>
concept IsString
    = findType<key>() == Settings::Type::String && (std::same_as<Value, QString> || std::same_as<Value, const char*>);

template <auto key, typename Value>
concept IsByteArray = findType<key>() == Settings::Type::ByteArray && std::same_as<Value, QByteArray>;

template <auto key, typename Value>
concept ValidValueType = IsVariant<key, Value> || IsBool<key, Value> || IsDouble<key, Value> || IsInt<key, Value>
                      || IsString<key, Value> || IsByteArray<key, Value>;

class FYUTILS_EXPORT SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(const QString& settingsPath, QObject* parent = nullptr);

    void loadSettings();
    void storeSettings();

    [[nodiscard]] SettingsDialogController* settingsDialog() const;

    template <typename E>
    auto constexpr getMapKey(E key)
    {
        const auto meta      = QMetaEnum::fromType<E>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        return mapKey;
    }

    template <auto key, typename T, typename F>
    void constexpr connectTypeSignals(T obj, F func)
    {
        const auto mapKey = getMapKey(key);
        const auto type   = findType<key>();

        if(m_settings.contains(mapKey)) {
            if constexpr(type == Settings::Type::Variant) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedVariant, obj, func);
            }
            else if constexpr(type == Settings::Type::Bool) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedBool, obj, func);
            }
            else if constexpr(type == Settings::Type::Double) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedDouble, obj, func);
            }
            else if constexpr(type == Settings::Type::Int) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedInt, obj, func);
            }
            else if constexpr(type == Settings::Type::String) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedString, obj, func);
            }
            else if constexpr(type == Settings::Type::ByteArray) {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChangedByteArray, obj, func);
            }
            else {
                QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChanged, obj, func);
            }
        }
    }

    template <auto key, typename T>
    void constexpr subscribe(T* obj, void (T::*func)())
    {
        const auto mapKey = getMapKey(key);

        if(m_settings.contains(mapKey)) {
            QObject::connect(&m_settings.at(mapKey), &SettingsEntry::settingChanged, obj, func);
        }
    }

    template <auto key, typename T, typename R>
    void constexpr subscribe(T* obj, void (T::*func)(R ret))
    {
        connectTypeSignals<key>(obj, func);
    }

    template <auto key, typename T, typename L>
    void constexpr subscribe(T* obj, L const& lambda)
    {
        connectTypeSignals<key>(obj, lambda);
    }

    template <typename E, typename T>
    void constexpr unsubscribe(E key, T* obj)
    {
        const auto mapKey = getMapKey(key);

        if(m_settings.contains(mapKey)) {
            QObject::disconnect(&m_settings.at(mapKey), nullptr, obj, nullptr);
        }
    }

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createSetting(Value value, const QString& group = {})
    {
        using Enum           = decltype(key);
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(!m_settings.contains(mapKey)) {
            m_settings.emplace(mapKey, SettingsEntry{keyString, value, true, group});
        }
    }

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createTempSetting(const Value& value, const QString& group = {})
    {
        using Enum           = decltype(key);
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = meta.name();
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(!m_settings.contains(mapKey)) {
            m_settings.emplace(mapKey, SettingsEntry{keyString, value, false, group});
        }
    }

    template <auto key>
    auto value()
    {
        const auto mapKey = getMapKey(key);

        m_lock.lockForRead();

        const auto value = m_settings.contains(mapKey) ? m_settings.at(mapKey).value() : -1;
        const auto type  = findType<key>();

        m_lock.unlock();

        if constexpr(type == Settings::Type::Bool) {
            return value.toBool();
        }
        else if constexpr(type == Settings::Type::Double) {
            return value.toDouble();
        }
        else if constexpr(type == Settings::Type::Int) {
            return value.toInt();
        }
        else if constexpr(type == Settings::Type::String) {
            return value.toString();
        }
        else if constexpr(type == Settings::Type::ByteArray) {
            return value.toByteArray();
        }
        else {
            return value;
        }
    }

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void set(Value value)
    {
        const auto mapKey = getMapKey(key);

        m_lock.lockForWrite();

        if(!m_settings.contains(mapKey)) {
            m_lock.unlock();
            return;
        }

        SettingsEntry& setting = m_settings.at(mapKey);

        if(!setting.setValue(value)) {
            m_lock.unlock();
            return;
        }

        m_lock.unlock();

        const auto type = findType<key>();

        if constexpr(type == Settings::Type::Variant) {
            emit setting.settingChangedVariant(value);
        }
        else if constexpr(type == Settings::Type::Bool) {
            emit setting.settingChangedBool(value);
        }
        else if constexpr(type == Settings::Type::Double) {
            emit setting.settingChangedDouble(value);
        }
        else if constexpr(type == Settings::Type::Int) {
            emit setting.settingChangedInt(value);
        }
        else if constexpr(type == Settings::Type::String) {
            emit setting.settingChangedString(value);
        }
        else if constexpr(type == Settings::Type::ByteArray) {
            emit setting.settingChangedByteArray(value);
        }
        emit setting.settingChanged();
    }

    template <auto key>
    void reset()
    {
        const auto mapKey = getMapKey(key);

        m_lock.lockForWrite();

        if(!m_settings.contains(mapKey)) {
            m_lock.unlock();
            return;
        }

        SettingsEntry& setting = m_settings.at(mapKey);

        if(!setting.reset()) {
            m_lock.unlock();
            return;
        }

        const auto value = setting.value();

        m_lock.unlock();

        const auto type = findType<key>();

        if constexpr(type == Settings::Type::Variant) {
            emit setting.settingChangedVariant(value);
        }
        else if constexpr(type == Settings::Type::Bool) {
            emit setting.settingChangedBool(value.toBool());
        }
        else if constexpr(type == Settings::Type::Double) {
            emit setting.settingChangedDouble(value.toDouble());
        }
        else if constexpr(type == Settings::Type::Int) {
            emit setting.settingChangedInt(value.toInt());
        }
        else if constexpr(type == Settings::Type::String) {
            emit setting.settingChangedString(value.toString());
        }
        else if constexpr(type == Settings::Type::ByteArray) {
            emit setting.settingChangedByteArray(value.toByteArray());
        }
        emit setting.settingChanged();
    }

private:
    QSettings m_settingsFile;
    std::map<QString, SettingsEntry> m_settings;
    QReadWriteLock m_lock;

    SettingsDialogController* m_settingsDialog;
};
} // namespace Fy::Utils
