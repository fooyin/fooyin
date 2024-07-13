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

namespace Fooyin::Scrobbler {
LastFmService::LastFmService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : ScrobblerService{network, settings, parent}
{
    setName(QStringLiteral("LastFM"));
    setApiUrl(QStringLiteral("https://ws.audioscrobbler.com/2.0/"));
    setAuthUrl(QStringLiteral("https://www.last.fm/api/auth/"));
    setApiKey(QStringLiteral("b2d57c978a22be279cc3be6d67627fea"));
    setSecret(QStringLiteral("863d0b5b43b66d52ed9584ab9bbe7706"));
}
} // namespace Fooyin::Scrobbler
