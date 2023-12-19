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

#include <QMetaEnum>
#include <QReadWriteLock>

class QSettings;

namespace Fooyin {
class SettingsEntry;
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
concept IsVariant = findType<key>() == Settings::Variant;

template <auto key, typename Value>
concept IsBool = findType<key>() == Settings::Bool && std::same_as<Value, bool>;

template <auto key, typename Value>
concept IsDouble = findType<key>() == Settings::Double && std::same_as<Value, double>;

template <auto key, typename Value>
concept IsInt = findType<key>() == Settings::Int && std::same_as<Value, int>;

template <auto key, typename Value>
concept IsString
    = findType<key>() == Settings::String && (std::same_as<Value, QString> || std::same_as<Value, const char*>);

template <auto key, typename Value>
concept IsByteArray = findType<key>() == Settings::ByteArray && std::same_as<Value, QByteArray>;

template <auto key, typename Value>
concept ValidValueType = IsVariant<key, Value> || IsBool<key, Value> || IsDouble<key, Value> || IsInt<key, Value>
                      || IsString<key, Value> || IsByteArray<key, Value>;

class FYUTILS_EXPORT SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(const QString& settingsPath, QObject* parent = nullptr);

    [[nodiscard]] SettingsDialogController* settingsDialog() const;

    void storeSettings();

    QVariant value(const QString& key) const;
    bool set(const QString& key, const QVariant& value);
    bool reset(const QString& key);
    bool contains(const QString& key) const;

    QVariant fileValue(const QString& key) const;
    bool fileSet(const QString& key, const QVariant& value);
    bool fileContains(const QString& key) const;
    void fileRemove(const QString& key);

    void createSetting(const QString& key, const QVariant& value);
    void createTempSetting(const QString& key, const QVariant& value);

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createSetting(Value value, const QString& settingKey)
    {
        createNewSetting<key>(value, settingKey, false);
    }

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createTempSetting(const Value& value)
    {
        using Enum            = decltype(key);
        const auto meta       = QMetaEnum::fromType<Enum>();
        const auto keyString  = QString::fromLatin1(meta.valueToKey(key));
        const auto settingKey = "Temp/" + keyString;

        createNewSetting<key>(value, settingKey, true);
    }

    template <auto key>
    auto value()
    {
        const auto mapKey = getMapKey(key);

        m_lock.lockForRead();

        const auto value = m_settings.contains(mapKey) ? m_settings.at(mapKey)->value() : -1;
        const auto type  = findType<key>();

        m_lock.unlock();

        if constexpr(type == Settings::Bool) {
            return value.toBool();
        }
        else if constexpr(type == Settings::Double) {
            return value.toDouble();
        }
        else if constexpr(type == Settings::Int) {
            return value.toInt();
        }
        else if constexpr(type == Settings::String) {
            return value.toString();
        }
        else if constexpr(type == Settings::ByteArray) {
            return value.toByteArray();
        }
        else {
            return value;
        }
    }

    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    bool set(Value value)
    {
        const QString mapKey = getMapKey(key);

        m_lock.lockForWrite();

        if(!m_settings.contains(mapKey)) {
            m_lock.unlock();
            return false;
        }

        SettingsEntry* setting = m_settings.at(mapKey);

        const bool success = setting->setValue(value);

        m_lock.unlock();

        if(success) {
            setting->notifySubscribers();
        }

        return success;
    }

    template <auto key>
    bool reset()
    {
        const auto mapKey = getMapKey(key);

        m_lock.lockForWrite();

        if(!m_settings.contains(mapKey)) {
            m_lock.unlock();
            return false;
        }

        SettingsEntry* setting = m_settings.at(mapKey);

        const bool success = setting->reset();

        m_lock.unlock();

        return success;
    }

    template <typename T, typename Func>
    void subscribe(const QString& key, T receiver, Func&& func)
    {
        if(m_settings.contains(key)) {
            SettingsEntry* setting = m_settings.at(key);
            QObject::connect(setting, &SettingsEntry::settingChangedVariant, receiver, func);
        }
    }

    template <auto key, typename T, typename Func>
    void constexpr subscribe(T receiver, Func&& func)
    {
        connectTypeSignals<key>(receiver, func);
    }

    template <typename Enum>
    void constexpr unsubscribe(Enum key, QObject* obj)
    {
        const auto mapKey = getMapKey(key);

        if(m_settings.contains(mapKey)) {
            QObject::disconnect(m_settings.at(mapKey), nullptr, obj, nullptr);
        }
    }

private:
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createNewSetting(const Value& value, const QString& settingKey, bool isTemporary)
    {
        using Enum           = decltype(key);
        const auto type      = static_cast<Settings::Type>(findType<key>());
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = QString::fromLatin1(meta.name());
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        if(m_settings.contains(mapKey) || settingExists(settingKey)) {
            qWarning() << "Setting has already been registered: " << keyString;
            return;
        }

        m_settings.emplace(mapKey, new SettingsEntry(settingKey, value, type, this));
        auto* setting = m_settings.at(mapKey);
        if(isTemporary) {
            setting->setIsTemporary(isTemporary);
        }
        else {
            checkLoadSetting(setting);
        }
    }

    template <typename Enum>
    auto constexpr getMapKey(Enum key)
    {
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = QString::fromLatin1(meta.name());
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        return QString::fromUtf8(mapKey.toUtf8());
    }

    template <auto key, typename T, typename Func>
    void constexpr connectTypeSignals(T receiver, Func func)
    {
        const auto mapKey = getMapKey(key);
        const auto type   = findType<key>();

        if(m_settings.contains(mapKey)) {
            if constexpr(type == Settings::Variant) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedVariant, receiver, func);
            }
            else if constexpr(type == Settings::Bool) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedBool, receiver, func);
            }
            else if constexpr(type == Settings::Double) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedDouble, receiver, func);
            }
            else if constexpr(type == Settings::Int) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedInt, receiver, func);
            }
            else if constexpr(type == Settings::String) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedString, receiver, func);
            }
            else if constexpr(type == Settings::ByteArray) {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedByteArray, receiver, func);
            }
            else {
                QObject::connect(m_settings.at(mapKey), &SettingsEntry::settingChangedVariant, receiver, func);
            }
        }
    }

    bool settingExists(const QString& key) const;
    void checkLoadSetting(SettingsEntry* setting) const;

    QSettings* m_settingsFile;
    std::map<QString, SettingsEntry*> m_settings;
    QReadWriteLock m_lock;

    SettingsDialogController* m_settingsDialog;
};
} // namespace Fooyin
