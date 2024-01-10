/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/settings/settingsentry.h>

namespace Fooyin {
SettingsEntry::SettingsEntry(QString key, const QVariant& value, QObject* parent)
    : SettingsEntry{std::move(key), value, Settings::Variant, parent}
{ }

SettingsEntry::SettingsEntry(QString key, const QVariant& value, Settings::Type type, QObject* parent)
    : QObject{parent}
    , m_key{std::move(key)}
    , m_type{type}
    , m_value{value}
    , m_defaultValue{value}
    , m_isTemporary{false}
{ }

QString SettingsEntry::key() const
{
    return m_key;
}

Settings::Type SettingsEntry::type() const
{
    return m_type;
}

QVariant SettingsEntry::value() const
{
    return m_value;
}

QVariant SettingsEntry::defaultValue() const
{
    return m_defaultValue;
}

bool SettingsEntry::isTemporary() const
{
    return m_isTemporary;
}

bool SettingsEntry::setValue(const QVariant& value)
{
    return std::exchange(m_value, value) != value;
}

void SettingsEntry::setIsTemporary(bool isTemporary)
{
    m_isTemporary = isTemporary;
}

void SettingsEntry::notifySubscribers()
{
    switch(m_type) {
        case(Settings::Variant):
            emit settingChangedVariant(m_value);
            break;
        case(Settings::Bool):
            emit settingChangedBool(m_value.toBool());
            break;
        case(Settings::Double):
            emit settingChangedDouble(m_value.toDouble());
            break;
        case(Settings::Int):
            emit settingChangedInt(m_value.toInt());
            break;
        case(Settings::String):
            emit settingChangedString(m_value.toString());
            break;
        case(Settings::ByteArray):
            emit settingChangedByteArray(m_value.toByteArray());
            break;
    }
}

bool SettingsEntry::reset()
{
    return setValue(m_defaultValue);
}
} // namespace Fooyin

#include "utils/settings/moc_settingsentry.cpp"
