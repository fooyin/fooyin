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

#include "filterfwd.h"

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Filters {
class FieldRegistry : public QObject
{
    Q_OBJECT

public:
    explicit FieldRegistry(Utils::SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] const IndexFieldMap& fields() const;

    FilterField addField(const FilterField& field);
    bool changeField(const FilterField& field);

    [[nodiscard]] FilterField fieldByIndex(int index) const;
    [[nodiscard]] FilterField fieldByName(const QString& name) const;

    bool removeByIndex(int index);

    void saveFields();
    void loadFields();

signals:
    void fieldChanged(const FilterField& field);

private:
    [[nodiscard]] int find(const QString& name) const;
    [[nodiscard]] int findAll(const QString& name) const;

    Utils::SettingsManager* m_settings;

    IndexFieldMap m_fields;
};

QDataStream& operator<<(QDataStream& stream, const FilterField& field);
QDataStream& operator>>(QDataStream& stream, FilterField& field);

QDataStream& operator<<(QDataStream& stream, const IndexFieldMap& fieldMap);
QDataStream& operator>>(QDataStream& stream, IndexFieldMap& fieldMap);
} // namespace Filters
} // namespace Fy
