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
 */

#include "cddaurl.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
bool isValidDiscId(const QString& discId)
{
    static const QRegularExpression expression{uR"(^[A-Za-z0-9._-]{28}$)"_s};
    return expression.match(discId).hasMatch();
}

QString cddaUrl(const QString& discId)
{
    if(!isValidDiscId(discId)) {
        return {};
    }
    return u"cdda:///%1"_s.arg(discId);
}

std::optional<QString> discIdFromCddaUrl(const QString& path)
{
    static constexpr QLatin1StringView Prefix{"cdda:///"};
    if(!path.startsWith(Prefix, Qt::CaseSensitive)) {
        return {};
    }

    QString discId = path.sliced(Prefix.size());
    if(!isValidDiscId(discId)) {
        return {};
    }

    return discId;
}
} // namespace Fooyin::Cdda
