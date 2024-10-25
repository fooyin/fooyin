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

#include "qqlyrics.h"

#include <core/network/networkaccessmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

constexpr auto SearchUrl = "https://c.y.qq.com/splcloud/fcgi-bin/smartbox_new.fcg";
constexpr auto LyricUrl  = "https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg";

namespace {
QNetworkRequest setupRequest(const char* url)
{
    QNetworkRequest req{QString::fromLatin1(url)};
    req.setRawHeader("Referer", "https://y.qq.com/portal/player.html");
    return req;
}
} // namespace

namespace Fooyin::Lyrics {
QString QQLyrics::name() const
{
    return QStringLiteral("QQ Music");
}

void QQLyrics::search(const SearchParams& params)
{
    resetReply();
    m_data.clear();

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(encode(QStringLiteral("key")),
                          encode(QStringLiteral("%1+%2").arg(params.artist, params.title)));
    urlQuery.addQueryItem(encode(QStringLiteral("inCharset")), encode(QStringLiteral("utf-8")));
    urlQuery.addQueryItem(encode(QStringLiteral("outCharset")), encode(QStringLiteral("utf-8")));

    const QNetworkRequest req = setupRequest(SearchUrl);

    qCDebug(LYRICS) << QStringLiteral("Sending request: %1?%2").arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &QQLyrics::handleSearchReply);
}

void QQLyrics::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        emit searchResult({});
        resetReply();
        return;
    }

    resetReply();

    const QJsonObject dataObj  = obj.value(u"data").toObject();
    const QJsonObject songObj  = dataObj.value(u"song").toObject();
    const QJsonArray songArray = songObj.value(u"itemlist").toArray();

    if(songArray.isEmpty()) {
        emit searchResult({});
        return;
    }

    for(const auto& song : songArray) {
        const QJsonObject songItem = song.toObject();
        if(songItem.contains(u"mid")) {
            LyricData songData;
            songData.id       = songItem.value(u"mid").toString();
            songData.title    = songItem.value(u"name").toString();
            const auto artist = songItem.value(u"singer").toString();
            if(!artist.isEmpty()) {
                songData.artists.push_back(artist);
            }
            m_data.push_back(songData);
        }
    }

    if(m_data.empty()) {
        emit searchResult({});
        return;
    }

    m_currentData = m_data.begin();
    makeLyricRequest();
}

void QQLyrics::makeLyricRequest()
{
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("songmid"), encode(m_currentData->id));
    urlQuery.addQueryItem(encode(QStringLiteral("inCharset")), encode(QStringLiteral("utf-8")));
    urlQuery.addQueryItem(encode(QStringLiteral("outCharset")), encode(QStringLiteral("utf-8")));
    urlQuery.addQueryItem(encode(QStringLiteral("format")), encode(QStringLiteral("json")));
    urlQuery.addQueryItem(encode(QStringLiteral("nobase64")), encode(QStringLiteral("1")));
    urlQuery.addQueryItem(encode(QStringLiteral("g_tk")), encode(QStringLiteral("5381")));

    const QNetworkRequest req = setupRequest(LyricUrl);

    qCDebug(LYRICS) << QStringLiteral("Sending request: %1?%2").arg(QLatin1String{LyricUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &QQLyrics::handleLyricReply);
}

void QQLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        resetReply();

        const QString lyrics = obj.value(u"lyric").toString();
        if(!lyrics.isEmpty()) {
            m_currentData->data = lyrics.toUtf8();
        }
    }

    ++m_currentData;

    if(m_currentData == m_data.end()) {
        emit searchResult(m_data);
        m_data.clear();
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
