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

#include "lrcliblyrics.h"

#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl = "https://lrclib.net/api/get";

namespace Fooyin::Lyrics {
QString LrcLibLyrics::name() const
{
    return u"LRCLIB"_s;
}

void LrcLibLyrics::search(const SearchParams& params)
{
    resetReply();

    QUrl url{QString::fromLatin1(ApiUrl)};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(encode(u"track_name"_s), encode(params.title));
    urlQuery.addQueryItem(encode(u"artist_name"_s), encode(params.artist));
    urlQuery.addQueryItem(encode(u"album_name"_s), encode(params.album));
    urlQuery.addQueryItem(encode(u"duration"_s), encode(QString::number(params.track.duration() / 1000)));
    url.setQuery(urlQuery);

    QNetworkRequest req{url};
    req.setRawHeader(
        "User-Agent",
        u"fooyin v%1 (https://www.fooyin.org)"_s.arg(settings()->value<Settings::Core::Version>()).toUtf8());

    qCDebug(LYRICS) << "Sending request" << url.toString();

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &LrcLibLyrics::handleLyricReply);
}

void LrcLibLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        resetReply();

        LyricData data;
        QString lyrics = obj.value("syncedLyrics"_L1).toString();
        if(!lyrics.isEmpty()) {
            data.data = lyrics;
        }
        else {
            lyrics = obj.value("plainLyrics"_L1).toString();
            if(!lyrics.isEmpty()) {
                data.data = lyrics;
            }
        }

        if(!data.data.isEmpty()) {
            data.title    = obj.value("trackName"_L1).toString();
            data.album    = obj.value("albumName"_L1).toString();
            data.artist   = obj.value("artistName"_L1).toString();
            data.duration = obj.value("duration"_L1).toInteger();
        }

        emit searchResult({data});
    }

    emit searchResult({});
}
} // namespace Fooyin::Lyrics
