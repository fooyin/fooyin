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

namespace Fooyin {
class InfoItem : public TreeItem<InfoItem>
{
public:
    enum ItemType : uint16_t
    {
        Header = Qt::UserRole,
        Entry
    };

    enum Role : uint16_t
    {
        Type = Qt::UserRole + 5,
        Value
    };

    enum ValueType : uint8_t
    {
        Concat = 0,
        Average,
        Total,
        Max,
        Percentage
    };

    enum Option : uint16_t
    {
        None             = 0,
        Metadata         = 1 << 0,
        Location         = 1 << 1,
        General          = 1 << 2,
        ExtendedMetadata = 1 << 3,
        Other            = 1 << 4,
        Default          = (Metadata | Location | General | Other)
    };
    Q_DECLARE_FLAGS(Options, Option)

    using FormatUIntFunc  = std::function<QString(uint64_t)>;
    using FormatFloatFunc = std::function<QString(double)>;
    using FormatFunc      = std::variant<FormatUIntFunc, FormatFloatFunc>;

    InfoItem();
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType);
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType, FormatFunc numFunc);

    bool operator<(const InfoItem& other) const;

    [[nodiscard]] ItemType type() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QVariant value() const;

    void setIsFloat(bool isFloat);

    void addTrackValue(const QString& value);
    void addTrackValue(const QStringList& values);

private:
    ItemType m_type;
    ValueType m_valueType;
    bool m_isFloat;

    QString m_name;
    QStringList m_values;
    mutable QString m_value;

    FormatFunc m_formatNum;
};
} // namespace Fooyin
