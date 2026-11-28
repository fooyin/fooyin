/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <QJsonValue>
#include <QMetaType>
#include <QPalette>

#include <cstdint>
#include <utility>
#include <vector>

namespace Fooyin {
struct AppearancePropertyEdit
{
    QString group;
    QString key;
    QJsonValue value{QJsonValue::Undefined};
};

struct AppearanceProperty
{
    enum class Type : uint8_t
    {
        Boolean = 0,
        OptionalBoolean,
        FontBold,
        Number,
        Choice,
        Colour,
        Brush,
        Group,
    };

    QString label;
    QString group;
    QString key;
    QJsonValue defaultValue;
    QJsonValue value{QJsonValue::Undefined};
    std::vector<QString> inheritanceGroups{};
    QJsonValue inheritedValue{QJsonValue::Undefined};

    bool stateBase{false};
    Type type{Type::Number};
    double minimum{0.0};
    double maximum{0.0};
    int decimals{0};
    QString suffix{};
    QString specialValueText{};
    QString toolTip{};
    std::vector<std::pair<QString, QString>> choices{};
    QPalette::ColorGroup colourGroup{QPalette::Active};
};

} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::AppearanceProperty)
