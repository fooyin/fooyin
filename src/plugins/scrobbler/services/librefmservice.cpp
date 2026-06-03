/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "librefmservice.h"

using namespace Qt::StringLiterals;

constexpr auto ApiUrl  = "https://libre.fm/2.0/";
constexpr auto AuthUrl = "https://libre.fm/api/auth/";

constexpr auto ApiKey    = "MjVkNGJiMDY4MWY3NjE3MzYyMTkxZTM4NGExZjlmOWE=";
constexpr auto ApiSecret = "Mjg4ZGJlNmUxMGY2NmM5ZDcyZDUyZGE0MmRmZTY0YTQ=";

namespace Fooyin::Scrobbler {
QUrl LibreFmService::url() const
{
    return isCustom() ? details().url : QString::fromLatin1(ApiUrl);
}

QUrl LibreFmService::authUrl() const
{
    return QString::fromLatin1(AuthUrl);
}

QString LibreFmService::apiKey() const
{
    return QString::fromLatin1(QByteArray::fromBase64(ApiKey));
}

QString LibreFmService::apiSecret() const
{
    return QString::fromLatin1(QByteArray::fromBase64(ApiSecret));
}
} // namespace Fooyin::Scrobbler
