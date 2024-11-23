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

#include "neteaselyrics.h"

#include <core/network/networkaccessmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

// TODO: Use offical API once we resolve issues with random search results
// constexpr auto SearchUrl = "https://music.163.com/api/search/get";
constexpr auto SearchUrl = "https://music.xianqiao.wang/neteaseapiv2/search";
// constexpr auto LyricUrl  = "https://music.163.com/api/song/lyric";
constexpr auto LyricUrl = "https://music.xianqiao.wang/neteaseapiv2/lyric";

namespace {
QNetworkRequest setupRequest(const char* url)
{
    QNetworkRequest req{QString::fromLatin1(url)};

    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);

    return req;
}
} // namespace

namespace Fooyin::Lyrics {
QString NeteaseLyrics::name() const
{
    return u"NetEase Cloud Music"_s;
}

void NeteaseLyrics::search(const SearchParams& params)
{
    resetReply();
    m_data.clear();

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"keywords"_s, u"%1+%2"_s.arg(params.artist, params.title));
    urlQuery.addQueryItem(u"type"_s, u"1"_s);
    urlQuery.addQueryItem(u"offset"_s, u"0"_s);
    urlQuery.addQueryItem(u"sub"_s, u"false"_s);
    urlQuery.addQueryItem(u"limit"_s, u"3"_s);
    urlQuery.addQueryItem(u"total"_s, u"true"_s);

    const QNetworkRequest req = setupRequest(SearchUrl);

    qCDebug(LYRICS) << u"Sending request: %1?%2"_s.arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &NeteaseLyrics::handleSearchReply);
}

void NeteaseLyrics::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        emit searchResult({});
        resetReply();
        return;
    }

    resetReply();

    const QJsonObject result = obj.value("result"_L1).toObject();
    const QJsonArray songs   = result.value("songs"_L1).toArray();

    if(songs.isEmpty()) {
        emit searchResult({});
        return;
    }

    for(const auto& song : songs) {
        const QJsonObject songItem = song.toObject();
        if(songItem.contains("id"_L1)) {
            LyricData songData;
            songData.id    = QString::number(songItem.value("id"_L1).toInteger());
            songData.title = songItem.value("name"_L1).toString();
            songData.album = songItem.value("album"_L1).toObject().value("name"_L1).toString();

            QStringList trackArtists;
            const auto artists = songItem.value("artists"_L1).toArray();
            for(const auto& artist : artists) {
                const QString artistName = artist.toObject().value("name"_L1).toString();
                if(!artistName.isEmpty()) {
                    trackArtists.push_back(artistName);
                }
            }

            if(!trackArtists.empty()) {
                songData.artist = trackArtists.join(", "_L1);
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

void NeteaseLyrics::makeLyricRequest()
{
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"id"_s, m_currentData->id);
    urlQuery.addQueryItem(u"lv"_s, u"-1"_s);
    urlQuery.addQueryItem(u"kv"_s, u"-1"_s);
    urlQuery.addQueryItem(u"tv"_s, u"-1"_s);
    urlQuery.addQueryItem(u"os"_s, u"pc"_s);

    const QNetworkRequest req = setupRequest(LyricUrl);

    qCDebug(LYRICS) << u"Sending request: %1?%2"_s.arg(QLatin1String{LyricUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &NeteaseLyrics::handleLyricReply);
}

void NeteaseLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        resetReply();

        const QJsonObject lrc = obj.value("lrc"_L1).toObject();
        const QString lyrics  = lrc.value("lyric"_L1).toString().trimmed();
        if(!lyrics.isEmpty()) {
            m_currentData->data = lyrics;
        }
    }

    ++m_currentData;

    if(m_currentData == m_data.end()) {
        emit searchResult(m_data);
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
