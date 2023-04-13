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

#include "fieldregistry.h"

#include "filtersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QIODevice>

namespace Fy::Filters {
FieldRegistry::FieldRegistry(Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

const IndexFieldMap& FieldRegistry::fields() const
{
    return m_fields;
}

FilterField FieldRegistry::addField(const FilterField& field)
{
    if(!field.isValid()) {
        return {};
    }

    FilterField newField{field};
    if(find(newField.name)) {
        auto count = find(newField.name + " (");
        newField.name += QString{" (%1)"}.arg(count);
    }
    newField.index = static_cast<int>(m_fields.size());

    return m_fields.emplace(newField.index, newField).first->second;
}

bool FieldRegistry::changeField(const FilterField& field)
{
    auto filterIt = std::find_if(m_fields.begin(), m_fields.end(), [field](const auto& filterField) {
        return filterField.first == field.index;
    });
    if(filterIt != m_fields.end()) {
        filterIt->second = field;
        emit fieldChanged(field);
        return true;
    }
    return false;
}

FilterField FieldRegistry::fieldByIndex(int index) const
{
    if(!m_fields.count(index)) {
        return {};
    }
    return m_fields.at(index);
}

FilterField FieldRegistry::fieldByName(const QString& name) const
{
    if(m_fields.empty()) {
        return {};
    }
    auto it = std::find_if(m_fields.begin(), m_fields.end(), [name](const auto& field) {
        return field.second.name == name;
    });
    if(it == m_fields.end()) {
        return m_fields.at(0);
    }
    return it->second;
}

bool FieldRegistry::removeByIndex(int index)
{
    if(!m_fields.count(index)) {
        return false;
    }
    m_fields.erase(index);
    return true;
}

void FieldRegistry::saveFields()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << m_fields;

    byteArray = byteArray.toBase64();
    m_settings->set<Settings::FilterFields>(byteArray);
}

void FieldRegistry::loadFields()
{
    QByteArray fields = m_settings->value<Settings::FilterFields>();
    fields            = QByteArray::fromBase64(fields);

    QDataStream in(&fields, QIODevice::ReadOnly);

    in >> m_fields;
}

int FieldRegistry::find(const QString& name) const
{
    return static_cast<int>(std::count_if(m_fields.cbegin(), m_fields.cend(), [name](const auto& field) {
        return field.second.name == name;
    }));
}

int FieldRegistry::findAll(const QString& name) const
{
    return static_cast<int>(std::count_if(m_fields.cbegin(), m_fields.cend(), [name](const auto& field) {
        return field.second.name.contains(name);
    }));
}

QDataStream& operator<<(QDataStream& stream, const FilterField& field)
{
    stream << field.index;
    stream << field.name;
    stream << field.field;
    stream << field.sortField;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, FilterField& field)
{
    stream >> field.index;
    stream >> field.name;
    stream >> field.field;
    stream >> field.sortField;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const IndexFieldMap& fieldMap)
{
    stream << static_cast<int>(fieldMap.size());
    for(const auto& [index, field] : fieldMap) {
        stream << index;
        stream << field;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, IndexFieldMap& fieldMap)
{
    int size;
    stream >> size;

    while(size > 0) {
        --size;

        FilterField field{};
        int index;
        stream >> index;
        stream >> field;
        fieldMap.emplace(index, field);
    }
    return stream;
}
} // namespace Fy::Filters
