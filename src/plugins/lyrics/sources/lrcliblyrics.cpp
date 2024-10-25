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

constexpr auto ApiUrl = "https://lrclib.net/api/get";

namespace Fooyin::Lyrics {
QString LrcLibLyrics::name() const
{
    return QStringLiteral("LRCLIB");
}

void LrcLibLyrics::search(const SearchParams& params)
{
    resetReply();

    QUrl url{QString::fromLatin1(ApiUrl)};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(encode(QStringLiteral("track_name")), encode(params.title));
    urlQuery.addQueryItem(encode(QStringLiteral("artist_name")), encode(params.artist));
    urlQuery.addQueryItem(encode(QStringLiteral("album_name")), encode(params.album));
    urlQuery.addQueryItem(encode(QStringLiteral("duration")), encode(QString::number(params.track.duration() / 1000)));
    url.setQuery(urlQuery);

    QNetworkRequest req{url};
    req.setRawHeader("User-Agent", QStringLiteral("fooyin v%1 (https://www.fooyin.org)")
                                       .arg(settings()->value<Settings::Core::Version>())
                                       .toUtf8());

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
        QString lyrics = obj.value(u"syncedLyrics").toString();
        if(!lyrics.isEmpty()) {
            data.data = lyrics.toUtf8();
        }
        else {
            lyrics = obj.value(u"plainLyrics").toString();
            if(!lyrics.isEmpty()) {
                data.data = lyrics.toUtf8();
            }
        }

        if(!data.data.isEmpty()) {
            data.title    = obj.value(u"trackName").toString();
            data.album    = obj.value(u"albumName").toString();
            data.artist   = obj.value(u"artistName").toString();
            data.duration = obj.value(u"duration").toInteger();
        }

        emit searchResult({data});
    }

    emit searchResult({});
}
} // namespace Fooyin::Lyrics
