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

constexpr auto SearchUrl = "https://music.163.com/api/cloudsearch/pc";
constexpr auto LyricUrl  = "https://interface3.music.163.com/api/song/lyric";

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
    urlQuery.addQueryItem(u"s"_s, u"%1+%2"_s.arg(params.artist, params.title));
    urlQuery.addQueryItem(u"type"_s, u"1"_s);
    urlQuery.addQueryItem(u"offset"_s, u"0"_s);
    urlQuery.addQueryItem(u"sub"_s, u"false"_s);
    urlQuery.addQueryItem(u"limit"_s, u"5"_s);

    const QNetworkRequest req = setupRequest(SearchUrl);

    qCDebug(LYRICS) << u"Sending request: %1?%2"_s.arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &NeteaseLyrics::handleSearchReply);
}

void NeteaseLyrics::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        emit searchResult(m_data);
        resetReply();
        return;
    }

    resetReply();

    const QJsonObject result = obj.value("result"_L1).toObject();
    const QJsonArray songs   = result.value("songs"_L1).toArray();

    if(songs.isEmpty()) {
        emit searchResult(m_data);
        return;
    }

    for(const auto& song : songs) {
        const QJsonObject songItem = song.toObject();
        if(songItem.contains("id"_L1)) {
            LyricData songData;
            songData.id    = QString::number(songItem.value("id"_L1).toInteger());
            songData.title = songItem.value("name"_L1).toString();
            songData.album = songItem.value("al"_L1).toObject().value("name"_L1).toString();

            QStringList trackArtists;
            const auto artists = songItem.value("ar"_L1).toArray();
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
        emit searchResult(m_data);
        return;
    }

    m_currentIndex = 0;
    makeLyricRequest();
}

void NeteaseLyrics::makeLyricRequest()
{
    if(m_currentIndex < 0 || std::cmp_greater_equal(m_currentIndex, m_data.size())) {
        emit searchResult(m_data);
        return;
    }

    const LyricData& data = m_data.at(m_currentIndex);

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"id"_s, data.id);
    urlQuery.addQueryItem(u"cp"_s, u"false"_s);
    urlQuery.addQueryItem(u"lv"_s, u"0"_s);
    urlQuery.addQueryItem(u"kv"_s, u"0"_s);
    urlQuery.addQueryItem(u"tv"_s, u"0"_s);
    urlQuery.addQueryItem(u"rv"_s, u"0"_s);
    urlQuery.addQueryItem(u"yv"_s, u"0"_s);
    urlQuery.addQueryItem(u"ytv"_s, u"0"_s);
    urlQuery.addQueryItem(u"yrv"_s, u"0"_s);
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

        if(m_currentIndex < 0 || std::cmp_greater_equal(m_currentIndex, m_data.size())) {
            emit searchResult(m_data);
            return;
        }

        const QJsonObject lrc = obj.value("lrc"_L1).toObject();
        const QString lyrics  = lrc.value("lyric"_L1).toString().trimmed();
        if(!lyrics.isEmpty() && lyrics != "暂无歌词"_L1) {
            LyricData& data = m_data.at(m_currentIndex);
            data.data       = lyrics;
        }
    }

    ++m_currentIndex;

    if(std::cmp_greater_equal(m_currentIndex, m_data.size())) {
        emit searchResult(m_data);
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
