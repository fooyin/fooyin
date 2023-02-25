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

#include "settingsentry.h"

namespace Fy::Utils {
SettingsEntry::SettingsEntry(QString name, const QVariant& value, bool writeToDisk, QString group)
    : m_name{std::move(name)}
    , m_value{value}
    , m_defaultValue{value}
    , m_group{std::move(group)}
    , m_writeToDisk{writeToDisk}
{ }

SettingsEntry::SettingsEntry(const SettingsEntry& other)
    : QObject{} // NOLINT
    , m_name{other.m_name}
    , m_value{other.m_value}
    , m_defaultValue{other.m_defaultValue}
    , m_group{other.m_group}
    , m_writeToDisk{other.m_writeToDisk}
{ }

QString SettingsEntry::name() const
{
    return m_name;
}

QVariant SettingsEntry::value() const
{
    return m_value;
}

QVariant SettingsEntry::defaultValue() const
{
    return m_defaultValue;
}

QString SettingsEntry::group() const
{
    return m_group;
}

bool SettingsEntry::writeToDisk() const
{
    return m_writeToDisk;
}

bool SettingsEntry::setValue(const QVariant& value)
{
    if(m_value == value) {
        return false;
    }
    m_value = value;
    return true;
}
} // namespace Fy::Utils
