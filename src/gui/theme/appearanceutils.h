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

#include <utils/jsonutils.h>

#include <QJsonObject>
#include <QPalette>

#include <cmath>
#include <expected>
#include <initializer_list>
#include <type_traits>

namespace Fooyin::Appearance {
using Utils::errorWithPath;
using Utils::readObject;

QJsonValue stateOverrides(const QJsonObject& object, QLatin1StringView state,
                          QLatin1StringView base = QLatin1StringView{"normal"});

QString numberError(const QString& path, double minimum, double maximum, bool integer);

std::expected<QPalette::ColorRole, QString> readRole(const QJsonObject& object, QLatin1StringView key,
                                                     const QString& path, QPalette::ColorRole fallback);
std::expected<bool, QString> readBool(const QJsonObject& object, QLatin1StringView key, const QString& path,
                                      bool fallback);

template <typename Number>
std::expected<Number, QString> readNumber(const QJsonObject& object, QLatin1StringView key, const QString& path,
                                          Number fallback, double minimum, double maximum)
{
    const auto value = object.value(key);
    if(value.isUndefined()) {
        return fallback;
    }

    const double candidate = value.toDouble(-1.0);
    if(!value.isDouble() || !std::isfinite(candidate) || candidate < minimum || candidate > maximum
       || (std::is_integral_v<Number> && std::trunc(candidate) != candidate)) {
        return std::unexpected{
            numberError(path + QLatin1Char{'.'} + key, minimum, maximum, std::is_integral_v<Number>)};
    }

    return static_cast<Number>(candidate);
}
} // namespace Fooyin::Appearance
