/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include <QDataStream>
#include <QString>
#include <QUrl>

namespace Fooyin::Scrobbler {
struct ServiceDetails
{
    enum class CustomType : uint8_t
    {
        None = 0,
        AudioScrobbler,
        ListenBrainz
    };

    QString name;
    QUrl url;
    QString token;
    CustomType customType{CustomType::None};
    bool isEnabled{true};

    [[nodiscard]] bool isValid() const
    {
        return !name.isEmpty() && !url.isEmpty();
    }

    [[nodiscard]] bool isCustom() const
    {
        return customType != CustomType::None;
    }

    friend QDataStream& operator<<(QDataStream& stream, const ServiceDetails& service);
    friend QDataStream& operator>>(QDataStream& stream, ServiceDetails& service);
};
} // namespace Fooyin::Scrobbler
