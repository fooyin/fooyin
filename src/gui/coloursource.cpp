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

#include <gui/coloursource.h>

#include <utils/jsonutils.h>

#include <QCoreApplication>
#include <QMetaEnum>

using namespace Qt::StringLiterals;

namespace Fooyin {
QBrush resolveBrush(const ColourSource& colour, const QPalette& palette, QPalette::ColorGroup group)
{
    if(const auto* role = std::get_if<QPalette::ColorRole>(&colour)) {
        return palette.brush(group, *role);
    }
    return QBrush{std::get<QColor>(colour)};
}

QString colourSourceName(const ColourSource& colour)
{
    if(const auto* role = std::get_if<QPalette::ColorRole>(&colour)) {
        return QString::fromLatin1(QMetaEnum::fromType<QPalette::ColorRole>().valueToKey(*role));
    }
    return std::get<QColor>(colour).name(QColor::HexArgb);
}

QJsonObject colourSourceToJson(const ColourSource& colour)
{
    return {{std::holds_alternative<QPalette::ColorRole>(colour) ? u"role"_s : u"colour"_s, colourSourceName(colour)}};
}

std::expected<ColourSource, QString> readColourSource(const QJsonValue& value, const QString& path,
                                                      const ColourSource& fallback)
{
    if(value.isUndefined()) {
        return fallback;
    }

    const auto object = Utils::readObject(value, path, {"role"_L1, "colour"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    if(object->size() != 1) {
        return std::unexpected{Utils::errorWithPath(
            QCoreApplication::translate("ColourSource", "Choose either a palette role or a custom colour."), path)};
    }

    if(object->contains("role"_L1)) {
        const auto roleValue = object->value("role"_L1);
        const auto meta      = QMetaEnum::fromType<QPalette::ColorRole>();

        bool valid{false};
        const int role = meta.keyToValue(roleValue.toString().toLatin1().constData(), &valid);
        if(!roleValue.isString() || !valid || role < 0 || role >= QPalette::NColorRoles || role == QPalette::NoRole) {
            return std::unexpected{Utils::errorWithPath(
                QCoreApplication::translate("ColourSource", "Choose a valid palette colour role."), path + u".role"_s)};
        }
        return static_cast<QPalette::ColorRole>(role);
    }

    const auto text = object->value("colour"_L1).toString();
    const QColor colour{text};
    if(!text.startsWith(u'#') || (text.size() != 7 && text.size() != 9) || !colour.isValid()) {
        return std::unexpected{
            Utils::errorWithPath(QCoreApplication::translate("ColourSource", "Enter a colour as #RRGGBB or #AARRGGBB."),
                                 path + u".colour"_s)};
    }
    return colour;
}
} // namespace Fooyin
