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

#include "appearanceutils.h"

#include <QCoreApplication>
#include <QMetaEnum>

using namespace Qt::StringLiterals;

namespace Fooyin::Appearance {
QJsonValue stateOverrides(const QJsonObject& object, QLatin1StringView state, QLatin1StringView base)
{
    auto value = object.value(state);
    if(state == base || (!value.isUndefined() && !value.isObject())) {
        return value;
    }

    auto inherited       = object.value(base).toObject();
    const auto overrides = value.toObject();
    for(auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        inherited.insert(it.key(), it.value());
    }
    return inherited;
}

QString numberError(const QString& path, double minimum, double maximum, bool integer)
{
    const auto message
        = integer
            ? QCoreApplication::translate("ThemeAppearance", "The value must be a whole number between %L1 and %L2.")
            : QCoreApplication::translate("ThemeAppearance", "The value must be a number between %L1 and %L2.");
    return errorWithPath(message.arg(minimum).arg(maximum), path);
}

std::expected<QPalette::ColorRole, QString> readRole(const QJsonObject& object, QLatin1StringView key,
                                                     const QString& path, QPalette::ColorRole fallback)
{
    const auto value = object.value(key);
    if(value.isUndefined()) {
        return fallback;
    }

    const auto meta = QMetaEnum::fromType<QPalette::ColorRole>();
    bool valid{false};
    const int candidate = meta.keyToValue(value.toString().toLatin1().constData(), &valid);
    if(!value.isString() || !valid || candidate < 0 || candidate >= QPalette::NColorRoles
       || candidate == QPalette::NoRole) {
        return std::unexpected{errorWithPath(
            QCoreApplication::translate("ThemeAppearance", "Choose a valid palette colour role."), path + u'.' + key)};
    }
    return static_cast<QPalette::ColorRole>(candidate);
}

std::expected<bool, QString> readBool(const QJsonObject& object, QLatin1StringView key, const QString& path,
                                      bool fallback)
{
    const auto value = object.value(key);
    if(value.isUndefined()) {
        return fallback;
    }

    if(!value.isBool()) {
        //: true and false are JSON boolean literals and must remain unchanged.
        return std::unexpected{errorWithPath(
            QCoreApplication::translate("ThemeAppearance", "The value must be true or false."), path + u'.' + key)};
    }
    return value.toBool();
}
} // namespace Fooyin::Appearance
