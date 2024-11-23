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

using namespace Qt::StringLiterals;

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
    return u"QQ Music"_s;
}

void QQLyrics::search(const SearchParams& params)
{
    resetReply();
    m_data.clear();

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"key"_s, u"%1+%2"_s.arg(params.artist, params.title));
    urlQuery.addQueryItem(u"inCharset"_s, u"utf-8"_s);
    urlQuery.addQueryItem(u"outCharset"_s, u"utf-8"_s);

    const QNetworkRequest req = setupRequest(SearchUrl);

    qCDebug(LYRICS) << u"Sending request: %1?%2"_s.arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

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

    const QJsonObject dataObj  = obj.value("data"_L1).toObject();
    const QJsonObject songObj  = dataObj.value("song"_L1).toObject();
    const QJsonArray songArray = songObj.value("itemlist"_L1).toArray();

    if(songArray.isEmpty()) {
        emit searchResult({});
        return;
    }

    for(const auto& song : songArray) {
        const QJsonObject songItem = song.toObject();
        if(songItem.contains("mid"_L1)) {
            LyricData songData;
            songData.id     = songItem.value("mid"_L1).toString();
            songData.title  = songItem.value("name"_L1).toString();
            songData.artist = songItem.value("singer"_L1).toString();

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
    urlQuery.addQueryItem(u"songmid"_s, m_currentData->id);
    urlQuery.addQueryItem(u"inCharset"_s, u"utf-8"_s);
    urlQuery.addQueryItem(u"outCharset"_s, u"utf-8"_s);
    urlQuery.addQueryItem(u"format"_s, u"json"_s);
    urlQuery.addQueryItem(u"nobase64"_s, u"1"_s);
    urlQuery.addQueryItem(u"g_tk"_s, u"5381"_s);

    const QNetworkRequest req = setupRequest(LyricUrl);

    qCDebug(LYRICS) << u"Sending request: %1?%2"_s.arg(QLatin1String{LyricUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &QQLyrics::handleLyricReply);
}

void QQLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        resetReply();

        const QString lyrics = obj.value("lyric"_L1).toString();
        if(!lyrics.isEmpty()) {
            m_currentData->data = lyrics;
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
