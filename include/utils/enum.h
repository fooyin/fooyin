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

#pragma once

#include <QDebug>
#include <QMetaEnum>
#include <QString>

namespace Fooyin::Utils::Enum {
template <typename Value>
concept IsEnum = std::is_enum_v<Value>;

template <typename E>
    requires IsEnum<E>
std::optional<E> fromString(const QString& text)
{
    bool ok;
    auto result = static_cast<E>(QMetaEnum::fromType<E>().keyToValue(text.toUtf8(), &ok));
    if(!ok) {
        qDebug() << "Failed to convert enum " << text;
        return {};
    }
    return result;
}

template <typename E>
    requires IsEnum<E>
QString toString(E value)
{
    const int intValue = static_cast<int>(value);
    return QString::fromUtf8(QMetaEnum::fromType<E>().valueToKey(intValue));
}
} // namespace Fooyin::Utils::Enum
