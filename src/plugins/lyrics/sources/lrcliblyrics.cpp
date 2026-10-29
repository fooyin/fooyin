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

#include "lrcliblyrics.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimerEvent>
#include <QUrl>
#include <QUrlQuery>

#include <limits>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl       = "https://lrclib.net/api/get";
constexpr auto RequestDelay = 300;

namespace Fooyin::Lyrics {
QString LrcLibLyrics::name() const
{
    return u"LRCLIB"_s;
}

void LrcLibLyrics::search(const SearchParams& params)
{
    cancel();

    QUrl url{QString::fromLatin1(ApiUrl)};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(encode(u"track_name"_s), encode(params.title));
    urlQuery.addQueryItem(encode(u"artist_name"_s), encode(params.artist));
    urlQuery.addQueryItem(encode(u"album_name"_s), encode(params.album));
    urlQuery.addQueryItem(encode(u"duration"_s), encode(QString::number(params.track.duration() / 1000)));
    url.setQuery(urlQuery);

    m_requestUrl = url;
    m_requestTimer.start(RequestDelay, this);
}

void LrcLibLyrics::cancel()
{
    m_requestTimer.stop();
    resetReply();
}

void LrcLibLyrics::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_requestTimer.timerId()) {
        m_requestTimer.stop();
        sendRequest();
    }
    LyricSource::timerEvent(event);
}

void LrcLibLyrics::sendRequest()
{
    const QNetworkRequest req = makeNetworkRequest(m_requestUrl);

    qCDebug(LYRICS) << "Sending request" << m_requestUrl.toString();

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &LrcLibLyrics::handleLyricReply);
}

void LrcLibLyrics::handleLyricReply()
{
    auto* lyricReply = reply();
    if(!lyricReply) {
        return;
    }

    const int statusCode = lyricReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(statusCode == 429) {
        bool validRetryAfter{false};
        const qint64 retryAfter = lyricReply->rawHeader("Retry-After").trimmed().toLongLong(&validRetryAfter);

        resetReply();

        qint64 retryDelay{RequestDelay};
        if(validRetryAfter && retryAfter >= 0) {
            static constexpr qint64 MaxSeconds = std::numeric_limits<int>::max() / 1000;
            retryDelay                         = std::min(retryAfter, MaxSeconds) * 1000;
        }
        m_requestTimer.start(static_cast<int>(std::max(retryDelay, static_cast<qint64>(RequestDelay))), this);
        return;
    }

    QJsonObject obj;
    const bool success = getJsonFromReply(lyricReply, &obj);
    resetReply();

    if(success) {
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

        Q_EMIT searchResult({data});
        return;
    }

    Q_EMIT searchResult({});
}
} // namespace Fooyin::Lyrics
