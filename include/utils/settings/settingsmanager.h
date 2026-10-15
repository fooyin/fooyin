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

#pragma once

#include "fyutils_export.h"

#include <utils/settings/settingsentry.h>

#include <QLoggingCategory>
#include <QMetaEnum>

#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>

Q_DECLARE_LOGGING_CATEGORY(SETTINGS)

class QMainWindow;

namespace Fooyin {
class SettingsEntry;
class SettingsDialogController;

template <auto key>
concept IsEnumType = std::is_enum_v<decltype(key)> && std::is_same_v<std::underlying_type_t<decltype(key)>, uint32_t>;

template <auto key>
    requires IsEnumType<key>
consteval int findType()
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
concept IsFloat = findType<key>() == Settings::Float && std::same_as<Value, float>;
template <auto key, typename Value>
concept IsDouble = findType<key>() == Settings::Double && std::same_as<Value, double>;
template <auto key, typename Value>
concept IsInt = findType<key>() == Settings::Int && std::same_as<Value, int>;
template <auto key, typename Value>
concept IsString
    = findType<key>() == Settings::String && (std::same_as<Value, QString> || std::same_as<Value, const char*>);
template <auto key, typename Value>
concept IsStringList = findType<key>() == Settings::StringList && std::same_as<Value, QStringList>;
template <auto key, typename Value>
concept IsByteArray = findType<key>() == Settings::ByteArray && std::same_as<Value, QByteArray>;

template <auto key, typename Value>
concept ValidValueType
    = IsVariant<key, Value> || IsBool<key, Value> || IsFloat<key, Value> || IsDouble<key, Value> || IsInt<key, Value>
   || IsString<key, Value> || IsStringList<key, Value> || IsByteArray<key, Value>;

/*!
 * Manages application settings, their default values and change notifications.
 *
 * Settings can be accessed in three ways:
 *
 * - String keys: register a setting with @c createSetting(), then use @c value() and @c set() with its key.
 *   Values are stored and delivered to subscribers as QVariant.
 * - Enum keys: register a setting with @c createSetting<key>(), then use @c value<key>() and @c set<key>().
 *   Enum keys can encode a Settings::Type to enable compile-time value checks and typed subscriber arguments.
 * - Direct file access: use @c fileValue(), @c fileSet(), @c fileContains() and @c fileRemove() without registration.
 *   These methods bypass registered values and do not notify subscribers.
 *
 * Register a setting before reading, changing or subscribing to it. Registration loads its saved value, if present,
 * or uses the supplied default. @c set() changes the value in memory; @c storeSettings() saves changed settings and
 * @c storeAllSettings() saves all registered settings. Settings registered with @c createTempSetting() are neither
 * loaded from nor saved to the file.
 *
 * Enum keys must use uint32_t as their underlying type and be exposed through Q_ENUM or Q_ENUM_NS. Combine each
 * enumerator with a Settings::Type value to specify its value type; untyped keys use QVariant.
 *
 * Use @c subscribe() with a receiver object and callback to receive changes. The receiver controls the connection's
 * lifetime and callback thread through Qt's automatic connection semantics. Notifications contain a snapshot of the
 * changed value, so a subsequent read may return a newer value.
 *
 * Registration, value access, persistence and subscription methods support concurrent calls. Notifications are emitted
 * in change order on the manager's thread, after releasing the settings lock, so callbacks can safely read or change
 * settings.
 *
 * Settings dialog creation, access and use are restricted to the main thread.
 *
 * @see Settings::Core::CoreSettings
 * @see Settings::Type
 */
class FYUTILS_EXPORT SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(const QString& settingsPath, QObject* parent = nullptr);

    /*!
     * Creates the settings dialog controller using @p mainWindow as the parent window.
     * @note this method must only be called on the main thread. The controller is available through @c
     * settingsDialog().
     */
    void createSettingsDialog(QMainWindow* mainWindow);

    /*!
     * Returns the settings dialog controller which can be used to open a specific settings page.
     * @note this will return a nullptr before the GuiApplication is initialised, so plugins should
     * only call this in the GuiPlugin @fn initialise method at the earliest.
     * This accessor and the returned controller must only be used on the main thread.
     */
    [[nodiscard]] SettingsDialogController* settingsDialog() const;

    // Returns @c true if any settings have been changed since program startup.
    [[nodiscard]] bool settingsHaveChanged() const;
    // Writes only changed settings to file, overwriting any existing values.
    void storeSettings();
    // Writes all settings to file, overwriting any existing values.
    void storeAllSettings();
    // Resets all settings to their default values.
    void resetAllSettings();

    /*!
     * Returns the current value of the setting at @p key if it exists, or an empty variant if not.
     * @note the setting must have been created using @fn createSetting(QString,QVariant).
     */
    QVariant value(QAnyStringView key) const;
    /*!
     * Sets the value of the setting at @p key.
     * @returns whether the setting was successfully changed.
     * @note the setting must have been created using @fn createSetting(QString,QVariant).
     */
    bool set(QAnyStringView key, const QVariant& value);
    /*!
     * Resets the value of the setting at @p key to default.
     * @returns whether the setting was successfully reset.
     * @note the setting must have been created using @fn createSetting(QString,QVariant).
     */
    bool reset(QAnyStringView key);
    /*!
     * Returns true if a setting at @p key exists.
     * @note the setting must have been created using @fn createSetting(QString,QVariant).
     */
    bool contains(QAnyStringView key) const;

    /*!
     * Returns the value of the setting at @p key from file if it exists, or an empty variant if not.
     * @note if using with a registered setting, the returned value may be different from the actual current value.
     */
    QVariant fileValue(QAnyStringView key, const QVariant& defaultValue = {}) const;
    /*!
     * Sets the value of the setting at @p key in the settings file.
     * @note this method is recommended to be used only for unregistered settings, as the value in the settings file
     * will be overwritten in @fn storeSettings.
     */
    bool fileSet(QAnyStringView key, const QVariant& value);
    /*!
     * Returns true if a setting at @p key exists in the settings file.
     */
    bool fileContains(QAnyStringView key) const;
    /*!
     * Removes the setting at @p key from the settings file.
     * @note this method is recommended to be used only for unregistered settings, as the value may be re-added in
     * @fn storeSettings.
     */
    void fileRemove(QAnyStringView key);

    /*!
     * Creates a setting at @p key, with the default value @p value.
     */
    void createSetting(const QString& key, const QVariant& value);
    /*!
     * Creates a temporary setting at @p key, with the default value @p value.
     * @note temporary settings are not saved to the settings file.
     */
    void createTempSetting(const QString& key, const QVariant& value);

    /*!
     * Creates a setting with the default value @p value.
     * @tparam key an enum value representing a setting key.
     * @tparam Value the default value of the setting.
     * @note the underlying type of the enum should be uint32_t, and Q_ENUM (or Q_ENUM_NS) should be used.
     */
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createSetting(Value value, const QString& settingKey)
    {
        createNewSetting<key>(value, settingKey, false);
    }

    /*!
     * Creates a setting with the default value @p value.
     * @tparam key an enum value representing a setting key.
     * @tparam Value the default value of the setting.
     * @note the underlying type of the enum should be uint32_t, and Q_ENUM (or Q_ENUM_NS) should be used.
     * @note temporary settings are not saved to the settings file.
     */
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void constexpr createTempSetting(const Value& value)
    {
        using Enum            = decltype(key);
        const auto meta       = QMetaEnum::fromType<Enum>();
        const auto keyString  = meta.valueToKey(key);
        const auto settingKey = std::string{"Temp/"} + keyString;

        createNewSetting<key>(value, QString::fromLatin1(settingKey), true);
    }

    /*!
     * Returns the value of the setting at the given key.
     * @tparam key an enum value representing a setting key.
     * @returns the current value of the setting (cast to the setting type or a variant).
     */
    template <auto key>
    auto value() const
    {
        const auto mapKey = getMapKey(key);

        const std::shared_lock lock{m_lock};

        QVariant settingValue;

        if(m_settings.contains(mapKey)) {
            if(auto* setting = m_settings.at(mapKey).get()) {
                settingValue = setting->value();
            }
        }

        const auto type = findType<key>();

        if constexpr(type == Settings::Bool) {
            return settingValue.toBool();
        }
        else if constexpr(type == Settings::Float) {
            return settingValue.toFloat();
        }
        else if constexpr(type == Settings::Double) {
            return settingValue.toDouble();
        }
        else if constexpr(type == Settings::Int) {
            return settingValue.toInt();
        }
        else if constexpr(type == Settings::String) {
            return settingValue.toString();
        }
        else if constexpr(type == Settings::StringList) {
            return settingValue.toStringList();
        }
        else if constexpr(type == Settings::ByteArray) {
            return settingValue.toByteArray();
        }
        else {
            return settingValue;
        }
    }

    /*!
     * Returns the persistent settings key associated with a registered enum setting.
     */
    template <auto key>
        requires IsEnumType<key>
    QString settingKey() const
    {
        const auto mapKey = getMapKey(key);
        const std::shared_lock lock{m_lock};
        if(!m_settings.contains(mapKey)) {
            return {};
        }

        const SettingsEntry* setting = m_settings.at(mapKey).get();
        return setting ? setting->key() : QString{};
    }

    /*!
     * Returns the default value associated with a registered enum setting.
     */
    template <auto key>
        requires IsEnumType<key>
    QVariant defaultValue() const
    {
        const auto mapKey = getMapKey(key);
        const std::shared_lock lock{m_lock};
        if(!m_settings.contains(mapKey)) {
            return {};
        }

        const SettingsEntry* setting = m_settings.at(mapKey).get();
        return setting ? setting->defaultValue() : QVariant{};
    }

    /*!
     * Sets the value of the setting at the given key.
     * @tparam key an enum value representing a setting key.
     * @tparam Value the value to set.
     * @returns true if the setting was successfully changed.
     * @note if the setting is changed, subscribers to this setting will be notified.
     */
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    bool set(Value value)
    {
        const QString mapKey = getMapKey(key);
        std::unique_lock lock{m_lock};
        const auto it = m_settings.find(mapKey);
        if(it == m_settings.end()) {
            return false;
        }

        SettingsEntry* setting = it->second.get();
        if(!setting->setValue(value)) {
            return false;
        }

        m_notifications.emplace_back(setting, setting->value());
        lock.unlock();
        notifySubscribers();
        return true;
    }

    /*!
     * Sets the value of the setting without notifying subscribers.
     * @tparam key an enum value representing a setting key.
     * @tparam Value the value to set.
     * @returns true if the stored value changed.
     */
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    bool setSilently(Value value)
    {
        const QString mapKey = getMapKey(key);
        const std::unique_lock lock{m_lock};
        if(!m_settings.contains(mapKey)) {
            return false;
        }

        SettingsEntry* setting = m_settings.at(mapKey).get();
        return setting && setting->setValue(value);
    }

    /*!
     * Resets the value of the setting at the given key.
     * @tparam key an enum value representing a setting key.
     * @returns true if the setting was successfully changed.
     * @note if the setting is changed, subscribers to this setting will be notified.
     */
    template <auto key>
    bool reset()
    {
        const auto mapKey = getMapKey(key);
        std::unique_lock lock{m_lock};
        const auto it = m_settings.find(mapKey);
        if(it == m_settings.end() || !it->second->reset()) {
            return false;
        }

        SettingsEntry* setting = it->second.get();
        fileRemove(setting->key());

        m_notifications.emplace_back(setting, setting->value());
        lock.unlock();
        notifySubscribers();
        return true;
    }

    /*!
     * Re-emits the current value of the setting to all subscribers without changing the stored value.
     * @tparam key an enum value representing a setting key.
     * @returns true if the setting exists and subscribers were notified.
     */
    template <auto key>
    bool refresh()
    {
        const auto mapKey = getMapKey(key);
        std::unique_lock lock{m_lock};
        const auto it = m_settings.find(mapKey);
        if(it == m_settings.end()) {
            return false;
        }

        SettingsEntry* setting = it->second.get();

        m_notifications.emplace_back(setting, setting->value());
        lock.unlock();
        notifySubscribers();
        return true;
    }

    /*!
     * Subscribes to a setting. A Qt signal-slot connection is made using the @p obj and @p func.
     * The @p func is then called whenever the setting is changed.
     * @param obj a QObject-derived class (controls connection lifetime).
     * @param func the 'slot' for the signal-slot connection.
     * @note this is for string key-based settings.
     */
    template <typename Obj, typename Func>
    void subscribe(QAnyStringView key, const Obj* obj, Func&& func)
    {
        const std::shared_lock lock{m_lock};

        const auto settingIt = settingEntry(key);
        if(settingIt != m_settings.cend()) {
            QObject::connect(settingIt->second.get(), &SettingsEntry::settingChangedVariant, obj,
                             std::forward<Func>(func));
        }
    }

    /*!
     * Subscribes to a setting. A Qt signal-slot connection is made using the @p obj and @p func.
     * The @p func is then called whenever the setting is changed.
     * @param obj a QObject-derived class (controls connection lifetime).
     * @param func the 'slot' for the signal-slot connection.
     * @note this is for enum key-based settings.
     */
    template <auto key, typename Obj, typename Func>
    void subscribe(const Obj* obj, Func&& func)
    {
        const auto mapKey = getMapKey(key);
        const auto type   = findType<key>();

        const std::shared_lock lock{m_lock};

        if(m_settings.contains(mapKey)) {
            if constexpr(type == Settings::Variant) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedVariant, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::Bool) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedBool, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::Float) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedFloat, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::Double) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedDouble, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::Int) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedInt, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::String) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedString, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::StringList) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedStringList, obj,
                                 std::forward<Func>(func));
            }
            else if constexpr(type == Settings::ByteArray) {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedByteArray, obj,
                                 std::forward<Func>(func));
            }
            else {
                QObject::connect(m_settings.at(mapKey).get(), &SettingsEntry::settingChangedVariant, obj,
                                 std::forward<Func>(func));
            }
        }
    }

    /*!
     * Unsubscribes to a setting.
     * @param key the settings key used in @fn createSetting.
     * @param obj the object to disconnect from.
     * @note this is for string key-based settings.
     */
    template <typename Obj>
    void unsubscribe(QAnyStringView key, const Obj* obj)
    {
        const std::shared_lock lock{m_lock};

        const auto settingIt = settingEntry(key);
        if(settingIt != m_settings.cend()) {
            QObject::disconnect(settingIt->second.get(), nullptr, obj, nullptr);
        }
    }

    /*!
     * Unsubscribes to a setting.
     * @tparam key the settings key used in @fn createSetting
     * @param obj the object to disconnect from.
     * @note this is for enum key-based settings.
     */
    template <auto key, typename Obj>
    void unsubscribe(const Obj* obj)
    {
        const auto mapKey = getMapKey(key);

        const std::shared_lock lock{m_lock};

        if(m_settings.contains(mapKey)) {
            QObject::disconnect(m_settings.at(mapKey).get(), nullptr, obj, nullptr);
        }
    }

private:
    template <auto key, typename Value>
        requires ValidValueType<key, Value>
    void createNewSetting(const Value& value, const QString& settingKey, bool isTemporary)
    {
        using Enum           = decltype(key);
        const auto type      = static_cast<Settings::Type>(findType<key>());
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = QString::fromLatin1(meta.name());
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        const std::unique_lock lock{m_lock};

        if(m_settings.contains(mapKey) || settingExists(settingKey)) {
            QLoggingCategory log{"Settings"};
            qCWarning(log) << "Setting has already been registered:" << keyString;
            return;
        }

        addSetting(mapKey, settingKey, value, type, isTemporary);
    }

    template <typename Enum>
    auto constexpr getMapKey(Enum key) const
    {
        const auto meta      = QMetaEnum::fromType<Enum>();
        const auto enumName  = QString::fromLatin1(meta.name());
        const auto keyString = QString::fromLatin1(meta.valueToKey(key));
        const auto mapKey    = enumName + keyString;

        return QString::fromUtf8(mapKey.toUtf8());
    }

    using SettingsMap = std::map<QString, std::unique_ptr<SettingsEntry>>;

    SettingsMap::const_iterator settingEntry(QAnyStringView key) const;
    bool settingExists(QAnyStringView key) const;
    void checkLoadSetting(SettingsEntry* setting) const;
    void saveSettings(bool onlyChanged);

    void addSetting(const QString& mapKey, const QString& key, const QVariant& value, Settings::Type type,
                    bool isTemporary);
    void notifySubscribers();
    void dispatchNotifications();

    const QString m_settingsPath;
    SettingsMap m_settings;
    mutable std::shared_mutex m_lock;

    struct Notification
    {
        SettingsEntry* setting;
        QVariant value;
    };

    std::deque<Notification> m_notifications;
    bool m_notifying;
    bool m_notificationScheduled;

    SettingsDialogController* m_settingsDialog;
};
} // namespace Fooyin
