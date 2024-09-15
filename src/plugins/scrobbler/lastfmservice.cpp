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

#include "lastfmservice.h"

#include "scrobblerauthsession.h"

#include <QUrlQuery>

constexpr auto ApiKey    = "YjJkNTdjOTc4YTIyYmUyNzljYzNiZTZkNjc2MjdmZWE=";
constexpr auto ApiSecret = "ODYzZDBiNWI0M2I2NmQ1MmVkOTU4NGFiOWJiZTc3MDY=";

namespace Fooyin::Scrobbler {
LastFmService::LastFmService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : ScrobblerService{network, settings, parent}
{
    setName(QStringLiteral("LastFM"));
    setApiUrl(QStringLiteral("https://ws.audioscrobbler.com/2.0/"));
    setAuthUrl(QStringLiteral("https://www.last.fm/api/auth/"));
    setApiKey(QString::fromLatin1(QByteArray::fromBase64(ApiKey)));
    setSecret(QString::fromLatin1(QByteArray::fromBase64(ApiSecret)));
}

void LastFmService::setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query)
{
    query.addQueryItem(QStringLiteral("api_key"), apiKey());
    query.addQueryItem(QStringLiteral("cb"), session->callbackUrl());
}
} // namespace Fooyin::Scrobbler
