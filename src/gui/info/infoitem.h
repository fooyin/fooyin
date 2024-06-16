/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/treeitem.h>

#include <set>

namespace Fooyin {
class InfoItem : public TreeItem<InfoItem>
{
public:
    enum ItemType
    {
        Header = Qt::UserRole,
        Entry
    };

    enum Role
    {
        Type = Qt::UserRole + 5,
        Value
    };

    enum ValueType
    {
        Concat,
        Average,
        Total,
        Max,
        Percentage
    };

    enum Option
    {
        None     = 0,
        Metadata = 1 << 0,
        Location = 1 << 1,
        General  = 1 << 2,
        Default  = (Metadata | Location | General)
    };
    Q_DECLARE_FLAGS(Options, Option)

    using FormatFunc = std::function<QString(uint64_t)>;

    InfoItem();
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType);
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType, FormatFunc numFunc);

    [[nodiscard]] ItemType type() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QVariant value() const;

    void addTrackValue(uint64_t value);
    void addTrackValue(int value);
    void addTrackValue(const QString& value);
    void addTrackValue(const QStringList& values);

private:
    ItemType m_type;
    ValueType m_valueType;

    QString m_name;
    std::vector<uint64_t> m_numValues;
    mutable uint64_t m_numValue;
    std::set<QString> m_values;
    std::map<QString, int> m_percentValues;
    mutable QString m_value;

    FormatFunc m_formatNum;
};
} // namespace Fooyin
